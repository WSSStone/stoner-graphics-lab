#include "Application/FInteractiveLabSession.h"
#include "Application/FLabSettingsSnapshot.h"
#include "Renderer/FOutputTransformSettings.h"
#include "Core/FPlatformProcess.h"
#include "FWindowDriver.h"
#include "Renderer/FUITextureRequest.h"
#include "Renderer/FUITextureLease.h"

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
void TestSettingsSession()
{
    using namespace Stoner::Renderer;
    FFixture F;
    uint64 Transitions = 0;
    Check(F.Start([&](const auto& Q) { if (Q.Phase == Phase::Transition) ++Transitions; return Complete(); }),
        "settings session starts with native lifecycle callback");
    FLabSettingsSnapshot Initial;
    Initial.CameraRevision = F.S.GetCameraState().CameraRevision;
    Initial.SettingsRevision = Initial.OutputModeGeneration = 1;
    Initial.DisplayGeneration = F.S.GetDisplayState().DisplayGeneration;
    Initial.RequestedProfileId = Initial.EffectiveProfileId = "Sdr.sRGB.v1";
    Initial.SdrToneMapVersion = GDefaultSDRToneMapVersion; Initial.HdrViewingVersion = GInitialHDRViewingVersion;
    FLabSettingsCapabilities Caps; Caps.DisplayGeneration = Initial.DisplayGeneration;
    Caps.Outputs = {{"Sdr.sRGB.v1",100,100}};
    Check(F.S.ConfigureSettings(Initial,Caps) && !F.S.ConfigureSettings(Initial,Caps),
        "session owns one settings controller initialized to its current camera and display");
    auto Edit = Initial; Edit.ExposureStops = 2;
    Check(F.S.RequestSettings(Edit) && F.S.GetPendingSettings() && !F.S.BeginSettingsTransaction(false),
        "session retains pending edits while rendering is busy");
    (void)F.S.Service(0);
    const auto* Active = F.S.BeginSettingsTransaction(true);
    const auto Token = Active ? Active->Token : 0;
    Check(Active && F.S.GetEffectiveSettings()->ExposureStops == 0,
        "session exposes immutable transaction before native completion");
    F.Driver->Width = 640;
    (void)F.S.Service(0);
    Check(Transitions == 0 && !F.S.BeginSettingsTransaction(true),
        "resize cannot enter native service while settings transaction is outstanding");
    Check(!F.S.CompleteSettingsTransaction(Token,true,true) && F.S.IsSettingsPaused() &&
        F.S.GetEffectiveSettings()->ExposureStops == 0,
        "display event rejects stale settings completion even before capability refresh");
    (void)F.S.Service(0);
    Check(Transitions == 1,"resize proceeds after stale settings native work has completed");
    Caps.DisplayGeneration = F.S.GetDisplayState().DisplayGeneration;
    Check(F.S.RefreshSettingsCapabilities(Caps,false),"session admits only current-generation capability refresh");
    Active = F.S.BeginSettingsTransaction(true);
    Check(Active && F.S.CompleteSettingsTransaction(Active->Token,true,false) &&
        !F.S.IsSettingsPaused() && F.S.GetEffectiveSettings()->ExposureStops == 2,
        "latest settings resume after refreshed output completion");
    Edit = *F.S.GetEffectiveSettings(); Edit.ExposureStops = 3;
    Check(F.S.RequestSettings(Edit),"session accepts next settings intent");
    Active = F.S.BeginSettingsTransaction(true);
    Check(Active != nullptr,"timeout fixture has an actual outstanding transaction");
    F.Clock += F.Config.TransitionTimeoutMilliseconds;
    (void)F.S.Service(0);
    Check(F.S.GetFirstFailure() == "lab-settings-transition-timed-out" && !F.S.RequestSettings(Edit),
        "settings transition deadline enters bounded terminal cleanup and rejects further edits");
    Check(Close(F.S),"settings session completes terminal cleanup");
}
void TestUISession()
{
    using namespace Stoner::Renderer;
    using Stoner::RHI::ERHIResult;
    FFixture F;
    Check(F.Start(), "UI session fixture initializes");
    uint64 EnableCalls = 0, TextureCalls = 0, TextureGeneration = 0;
    bool AllowEnable = true;
    FInteractiveLabUICallbacks UI;
    UI.PreflightEnable = [&] { ++EnableCalls; return AllowEnable ? EApplicationResult::Success : EApplicationResult::RuntimeUnavailable; };
    UI.BeginFrame = [](uint64,bool) {};
    UI.PrepareTexture = [&](const FUITextureRequest& Request) {
        ++TextureCalls;
        FUITextureResult Result;
        Result.RequestId = Request.RequestId; Result.Result = ERHIResult::Success;
        Result.State = Request.Operation == EUITextureOperation::Destroy ? EUITextureState::Destroyed : EUITextureState::Prepared;
        Result.TextureId = Request.Operation == EUITextureOperation::Destroy ? Request.TextureId : FUITextureId{1,++TextureGeneration};
        return Result;
    };
    UI.AcquireTexture = [](FUITextureId) { return FUITextureLease{}; };
    Check(F.S.ConfigureUI(std::move(UI),false) == EApplicationResult::Success && !F.S.IsUIEnabled() &&
        EnableCalls == 0 && TextureCalls == 0, "UI-off session creates no UI context or texture requests");
    F.Driver->QueueEvent(FInputEvent::KeyDown(EKey::W));
    (void)F.S.Service(0); (void)F.S.Service(0.25);
    const auto BeforeToggle = F.S.GetCameraState().Position;
    F.Driver->QueueEvent(FInputEvent::KeyDown(EKey::F1));
    (void)F.S.Service(0.25); (void)F.S.Service(0.25);
    Check(F.S.IsUIEnabled() && EnableCalls == 1 && TextureCalls > 0 &&
        F.S.GetCameraState().Position == BeforeToggle,
        "F1 enables a preflighted real UI context and quarantines held camera movement");
    F.Driver->QueueEvent(FInputEvent::KeyUp(EKey::F1));
    F.Driver->QueueEvent(FInputEvent::KeyUp(EKey::W));
    F.Driver->QueueEvent(FInputEvent::PointerMove(60,70));
    F.Driver->QueueEvent(FInputEvent::MouseDown(EMouseButton::Left));
    F.Driver->QueueEvent(FInputEvent::KeyDown(EKey::W));
    (void)F.S.Service(0.25);
    Check(F.S.GetInputOwnership().KeyboardOwner == EInputOwner::UI &&
        F.S.GetCameraState().Position == BeforeToggle,
        "actual same-interval text activation wins before the session camera can move");
    const auto BeforeBusy = TextureCalls;
    F.Driver->QueueEvent(FInputEvent::MouseUp(EMouseButton::Left));
    F.Driver->QueueEvent(FInputEvent::Text('x'));
    F.Driver->QueueEvent(FInputEvent::KeyDown(EKey::F1));
    (void)F.S.Service(0.25,false);
    Check(F.S.IsUIEnabled() && F.S.GetInputOwnership().KeyboardOwner == EInputOwner::UI &&
        F.S.GetCameraState().Position == BeforeToggle && TextureCalls == BeforeBusy,
        "busy session still gives text editing F1 ownership without speculative camera or texture work");
    F.Driver->QueueEvent(FInputEvent::KeyDown(EKey::Escape)); (void)F.S.Service(0);
    F.Driver->QueueEvent(FInputEvent::KeyUp(EKey::F1));
    F.Driver->QueueEvent(FInputEvent::KeyDown(EKey::F1)); (void)F.S.Service(0.25); (void)F.S.Service(0.25);
    Check(!F.S.IsUIEnabled() && !F.W.IsCloseRequested() && F.S.GetCameraState().Position == BeforeToggle,
        "Escape cancels the widget; hiding UI preserves held-key quarantine and never exits");
    F.Driver->QueueEvent(FInputEvent::KeyUp(EKey::W));
    F.Driver->QueueEvent(FInputEvent::KeyDown(EKey::W)); (void)F.S.Service(0.25);
    Check(F.S.GetCameraState().Position.X > BeforeToggle.X,
        "fresh release and press rearms the session camera after UI ownership");
    Check(F.S.SetUIEnabled(true) == EApplicationResult::Success, "UI can be restored after keyboard navigation");
    F.Driver->QueueEvent(FInputEvent::KeyUp(EKey::W)); (void)F.S.Service(0);
    F.Driver->QueueEvent(FInputEvent::PointerMove(60,92));
    F.Driver->QueueEvent(FInputEvent::MouseDown(EMouseButton::Left)); (void)F.S.Service(0);
    F.Driver->QueueEvent(FInputEvent::MouseUp(EMouseButton::Left)); (void)F.S.Service(0);
    Check(!F.S.IsUIEnabled(), "visible panel Hide UI control disables composition and releases input ownership");
    AllowEnable = false;
    Check(F.S.SetUIEnabled(true) == EApplicationResult::RuntimeUnavailable && !F.S.IsUIEnabled() &&
        !F.S.GetUIFailure().IsEmpty() && F.S.GetFirstFailure().IsEmpty(),
        "later UI preflight failure keeps the scene session active with a UI diagnostic");
    Check(Close(F.S), "UI context closes before terminal callback ownership transfer");
}
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
void TestTerminalFailureBoundaries()
{
    FFixture F;
    std::atomic<bool> Release{false}, Entered{false};
    Check(F.Start([&](const auto& Q) {
        auto R = Complete(Assurance::DeviceLost);
        if (Q.Phase == Phase::TerminalCleanup)
        {
            Entered = true;
            R.Status = Status::DeviceLost; R.bDeviceLost = true;
            R.RetainedOwnerCount = Release ? 0 : 1;
            R.FirstFailure = "injected-native-device-loss";
        }
        return R;
    }), "terminal device-loss fixture starts");
    (void)F.S.RequestExit();
    const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!Entered && std::chrono::steady_clock::now() < Deadline) std::this_thread::yield();
    (void)F.S.Service(0);
    Check(Entered && F.S.GetState() != State::Closed,
        "completed terminal response with retained native owners cannot close the session");
    Release = true;
    Check(Close(F.S) && F.S.GetShutdownAssurance() == Assurance::DeviceLost &&
        F.S.GetFirstFailure() == "injected-native-device-loss",
        "device-loss cleanup preserves the first failure and distinct assurance after owner release");

    FFixture Drain;
    Drain.Config.DrainTimeoutMilliseconds = 20;
    Check(Drain.Start([](const auto& Q) {
        auto R = Complete(Assurance::IdleAssumed);
        if (Q.Phase == Phase::Drain)
        { R.bCompleted = false; R.Status = Status::NotReady; R.RetainedOwnerCount = 1; }
        return R;
    }), "finite terminal drain fixture starts");
    Check(Close(Drain.S) && Drain.S.GetShutdownAssurance() == Assurance::IdleAssumed &&
        Drain.S.GetFirstFailure() == "lab-terminal-drain-timed-out",
        "successful compatibility cleanup cannot erase an earlier terminal drain timeout");
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
    TestSettingsSession();
    TestUISession(); TestSession(); TestTransitions(); TestTerminalOwnership(); TestTerminalFailureBoundaries(); TestTimeout();
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
