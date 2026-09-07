#include "Application/FInteractiveLabSession.h"
#include "Core/FPlatformProcess.h"
#include "FWindowDriver.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

namespace
{
using namespace Stoner::Application;
using namespace Stoner::Core;
using State = EInteractiveLabSessionState;
using Status = EInteractiveLabServiceStatus;
using Phase = EInteractiveLabServicePhase;
using Assurance = EInteractiveLabShutdownAssurance;
int Failures = 0;
void Check(bool Passed, const char* Name)
{
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
    if (!Passed) ++Failures;
}
FFreeCameraState Camera()
{
    FFreeCameraState C;
    C.CameraRevision = 1;
    C.Position = FVector3::Zero();
    C.VerticalFovRadians = FMath::DegreesToRadians(60.0f);
    C.NearPlane = 0.1f; C.FarPlane = 100.0f; C.MovementSpeed = 1.5f;
    C.DrawableExtent = {1024, 1024};
    C.View = FMatrix4x4::Identity();
    C.Projection = FMatrix4x4(0, 1.7320508f, 0, 0, 0, 0, -1.7320508f, 0,
        1.001001f, 0, 0, -0.1001001f, 1, 0, 0, 0);
    C.ViewProjection = C.Projection;
    return C;
}
FInteractiveLabServiceResponse Complete(Assurance A = Assurance::Proven)
{
    FInteractiveLabServiceResponse R;
    R.Status = Status::Success; R.bAccepted = true; R.bCompleted = true;
    R.ShutdownAssurance = A;
    return R;
}
bool Close(FInteractiveLabSession& S)
{
    (void)S.RequestExit();
    const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (S.GetState() != State::Closed && std::chrono::steady_clock::now() < Deadline)
    {
        (void)S.Service(0.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return S.GetState() == State::Closed;
}
class FInputDriver final : public IWindowDriver
{
public:
    TArray<FInputEvent> Events;
    uint32 Width = 320, Height = 240;
    const char* GetDriverName() const noexcept override { return "LabFixture"; }
    EWindowRuntimeAvailability GetRuntimeAvailability() const noexcept override { return EWindowRuntimeAvailability::Available; }
    uint32 GetDrawableWidth() const noexcept override { return Width; }
    uint32 GetDrawableHeight() const noexcept override { return Height; }
    EApplicationResult SetCursorMode(ECursorMode) override { return EApplicationResult::Success; }
    TArray<FWindowEvent> ConsumeWindowEvents() override { return {}; }
    TArray<FInputEvent> ConsumeInputEvents() override { auto Copy = std::move(Events); Events.clear(); return Copy; }
    void QueueEvent(const FInputEvent& E) { Events.push_back(E); }
};
struct FFixture
{
    FWindow W;
    FInputManager I;
    FInputDriver* Driver = nullptr;
    FInteractiveLabSession S;
    uint64 Clock = 100;
    FInteractiveLabSessionConfig Config;
    FFixture()
    {
        FWindowDesc D; D.Title = "Lab lifecycle"; D.ClientWidth = 320; D.ClientHeight = 240;
        (void)W.Create(D);
        auto InputDriver = std::make_unique<FInputDriver>();
        Driver = InputDriver.get();
        FWindowTestAccess::InstallDriver(W, std::move(InputDriver));
        Config.MonotonicMilliseconds = [this] { return Clock; };
    }
    bool Start(FInteractiveLabSessionCallbacks::FServiceCallback Callback = {})
    {
        if (!Callback) Callback = [](const auto&) { return Complete(); };
        return S.Initialize(W, I, Camera(), {std::move(Callback)}, Config) == EApplicationResult::Success;
    }
};
void TestSession()
{
    FFixture F;
    Check(F.Start() && F.S.GetState() == State::Ready, "session initializes current drawable camera in Ready");
    F.Driver->QueueEvent(FInputEvent::KeyDown(EKey::W));
    (void)F.S.Service(0.25);
    Check(F.S.GetCameraState().Position == FVector3::Zero(), "first active camera interval is zero");
    (void)F.S.Service(0.25);
    Check(F.S.GetCameraState().Position.X > 0.0f, "fresh viewport key drives the camera");
    F.Driver->QueueEvent(FInputEvent::KeyDown(EKey::Escape));
    (void)F.S.Service(0.0);
    Check(!F.W.IsCloseRequested() && F.S.GetState() == State::Running,
        "Escape cancels capture without exiting the lab");
    const auto Before = F.S.GetCameraState().Position;
    F.W.QueueEvent(FWindowEvent::Minimized());
    (void)F.S.Service(0.25);
    Check(F.S.GetState() == State::PausedZeroExtent &&
        F.S.GetRecommendedServiceWaitMilliseconds() <= 50 && F.S.GetCameraState().Position == Before,
        "minimized session services input without moving or rendering");
    F.Clock += 60000;
    F.Driver->Width = 640; F.Driver->Height = 360;
    F.W.QueueEvent(FWindowEvent::Restored(640, 360));
    F.W.QueueEvent(FWindowEvent::FocusGained());
    (void)F.S.Service(0.25);
    (void)F.S.Service(0.25);
    Check(F.S.GetCameraState().Position == Before && F.S.GetCameraState().DrawableExtent == FWindowExtent{640,360},
        "long paused interval resumes with current aspect and quarantines held movement");
    F.Driver->QueueEvent(FInputEvent::KeyUp(EKey::W)); (void)F.S.Service(0.0);
    F.Driver->QueueEvent(FInputEvent::KeyDown(EKey::W)); (void)F.S.Service(0.25);
    Check(F.S.GetCameraState().Position.X > Before.X, "release and new press rearm navigation after pause");
    F.Driver->Width = 5000;
    Check(F.S.Service(0.0) == EApplicationResult::ValidationFailed && F.S.GetFirstFailure().IsEmpty(),
        "unsupported drawable pauses without poisoning later recovery");
    F.Driver->Width = 640;
    (void)F.S.Service(0.0);
    Check(F.S.GetState() == State::Running, "valid extent recovers from a rejected drawable");
    F.W.QueueEvent(FWindowEvent::FocusLost());
    for (int N = 0; N < 400; ++N) (void)F.S.Service(0.0);
    Check(F.S.GetDiagnosticCount() >= 400 && F.S.GetDiagnostics().GetRecords().size() <= 256 &&
        F.W.GetDiagnostics().IsEmpty() && F.I.GetDiagnostics().IsEmpty(),
        "session keeps one bounded diagnostic ring and preserves monotonic totals");
    Check(Close(F.S) && F.S.GetShutdownAssurance() == Assurance::Proven,
        "explicit exit closes only after terminal completion");
}
void TestTransitions()
{
    FFixture F;
    bool Ready = false;
    int TransitionCalls = 0;
    uint64 ActiveId = 0;
    Check(F.Start([&](const auto& Q) {
        if (Q.Phase != Phase::Transition) return Complete();
        ++TransitionCalls;
        if (!Q.bPoll) ActiveId = Q.RequestId;
        if (Q.bPoll && Q.RequestId != ActiveId) ++Failures;
        auto R = Complete(); R.bCompleted = Ready; R.Status = Ready ? Status::Success : Status::NotReady; return R;
    }), "transition fixture starts");
    auto D = F.S.GetDisplayState();
    (void)F.S.RequestTransition({1, D.DisplayGeneration, D.DrawableExtent});
    (void)F.S.Service(0);
    (void)F.S.RequestTransition({2, D.DisplayGeneration, D.DrawableExtent});
    (void)F.S.RequestTransition({3, D.DisplayGeneration, D.DrawableExtent});
    Check(F.S.RequestTransition({4, D.DisplayGeneration, {5000, 1}}) == EApplicationResult::ValidationFailed &&
        F.S.GetPendingTransition().RequestId == 3,
        "invalid output intent preserves the latest valid pending request");
    Ready = true;
    (void)F.S.Service(0); (void)F.S.Service(0);
    Check(ActiveId == 3 && !F.S.HasPendingTransition() && TransitionCalls == 3,
        "one active and one latest pending transition complete with exact identity");
    Check(Close(F.S), "transition fixture closes");
}
void TestTerminalOwnership()
{
    FFixture F;
    std::atomic<bool> Release{false}, Entered{false};
    std::atomic<int> Calls{0};
    const auto MainThread = std::this_thread::get_id();
    Check(F.Start([&](const auto& Q) {
        ++Calls;
        if (Q.Phase == Phase::TerminalCleanup)
        {
            if (std::this_thread::get_id() == MainThread) return FInteractiveLabServiceResponse{};
            Entered = true;
            while (!Release) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return Complete(Assurance::IdleAssumed);
        }
        return Complete();
    }), "terminal owner fixture starts");
    (void)F.S.RequestExit();
    const auto Limit = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!Entered && std::chrono::steady_clock::now() < Limit) std::this_thread::yield();
    const int PreviousCalls = Calls;
    const auto Start = std::chrono::steady_clock::now();
    for (int N = 0; N < 20; ++N) (void)F.S.Service(0.0);
    Check(Entered && F.S.IsTerminalWorkerRunning() && Calls == PreviousCalls &&
        std::chrono::steady_clock::now() - Start < std::chrono::milliseconds(100),
        "blocked terminal idle leaves event service live without concurrent backend calls");
    Release = true;
    Check(Close(F.S) && F.S.GetShutdownAssurance() == Assurance::IdleAssumed,
        "compatibility cleanup retains IdleAssumed without inventing proof");
}
void TestTimeout()
{
    FFixture F;
    Check(F.Start([](const auto& Q) {
        if (Q.Phase == Phase::Transition) { auto R = Complete(); R.Status = Status::NotReady; R.bCompleted = false; return R; }
        return Complete();
    }), "timeout fixture starts");
    auto D = F.S.GetDisplayState();
    (void)F.S.RequestTransition({1, D.DisplayGeneration, D.DrawableExtent});
    (void)F.S.Service(0);
    F.Clock += 5000;
    (void)F.S.Service(0);
    Check(!F.S.GetFirstFailure().IsEmpty(), "transition deadline latches a failure before terminal cleanup");
    const auto First = F.S.GetFirstFailure();
    Check(Close(F.S) && F.S.GetFirstFailure() == First,
        "later cleanup success does not erase the first timeout");
}
} // namespace

int RunInteractiveLabLifecycleTests()
{
    Failures = 0;
    TestSession(); TestTransitions(); TestTerminalOwnership(); TestTimeout();
    return Failures == 0 ? 0 : 1;
}

int RunInteractiveLabWatchdogChild()
{
    FFixture F;
    F.Config.DrainTimeoutMilliseconds = 20;
    F.Config.TerminalWatchdogMilliseconds = 100;
    if (!F.Start([](const auto& Q) {
        FInteractiveLabServiceResponse R;
        R.Status = Status::NotReady; R.RetainedOwnerCount = 17;
        if (Q.Phase == Phase::TerminalCleanup)
            std::this_thread::sleep_for(std::chrono::seconds(30));
        return R;
    })) return 65;
    (void)F.S.RequestExit();
    // Stop servicing the session as well: watchdog timing is independent of
    // both the native cleanup worker and the Application event thread.
    std::this_thread::sleep_for(std::chrono::seconds(30));
    return 66;
}
int RunInteractiveLabWatchdogTests(const char* Executable)
{
    FProcessExecutionRequest Request;
    Request.ExecutablePath = FString(Executable);
    Request.Arguments = {FString("--interactive-lab-watchdog-child")};
    Request.Limits.TimeoutMilliseconds = 2000;
    const auto R = FPlatformProcess::Execute(Request);
    const bool Passed = R.Status == EProcessExecutionStatus::Completed && R.ExitCode == 124 &&
        R.StandardError.ToStdString().find("Forced; retained-owners=17; failed") != std::string::npos;
    std::cout << (Passed ? "[PASS] " : "[FAIL] ")
        << "independent session watchdog exits a failed child with retained native-owner diagnostics\n";
    return Passed ? 0 : 1;
}
