#include "Application/FInteractiveLabSession.h"
#include "Application/FInputOwnershipSnapshot.h"
#include "Core/FPlatformProcess.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <limits>
#include <mutex>
#include <thread>

namespace Stoner::Application
{
namespace
{
using namespace Stoner::Core;
using State = EInteractiveLabSessionState;
using Status = EInteractiveLabServiceStatus;
using Phase = EInteractiveLabServicePhase;
using Assurance = EInteractiveLabShutdownAssurance;
using Clock = std::chrono::steady_clock;
std::atomic<uint64> NextSession{1};
uint64 Milliseconds()
{
    return static_cast<uint64>(std::chrono::duration_cast<std::chrono::milliseconds>(
        Clock::now().time_since_epoch()).count());
}
FString BoundedText(const FString& Text, std::size_t Limit)
{
    const auto View = Text.View();
    if (View.size() <= Limit) return Text;
    while (Limit > 0 && (static_cast<unsigned char>(View[Limit]) & 0xc0) == 0x80) --Limit;
    return FString(std::string(View.substr(0, Limit)));
}
bool Pending(Status S) { return S == Status::NotReady; }
bool ValidExtent(FWindowExtent E)
{
    return E.Width <= 4096 && E.Height <= 4096 &&
        static_cast<uint64>(E.Width) * E.Height <= 7864320;
}
}

struct FInteractiveLabSession::FImpl
{
    FWindow* Window = nullptr;
    FInputManager* Input = nullptr;
    FFreeCameraController Camera;
    FWindowDisplayState Display;
    FInteractiveLabSessionCallbacks Callbacks;
    FInteractiveLabSessionConfig Config;
    State SessionState = State::Starting;
    Assurance ShutdownAssurance = Assurance::None;
    uint64 SessionId = 0, NextRequest = 1, LastClock = 0, TransitionStart = 0;
    FInteractiveLabTransitionIntent PendingIntent, ActiveIntent;
    FWindowDisplayState ActiveDisplay;
    bool bActivePoll = false, bDrainOnly = false, bFreshInterval = true;
    bool bLook = false;
    std::array<bool, GInputKeyCount> Quarantine{};
    bool bRightQuarantined = false;
    FString FirstFailure;
    FApplicationDiagnosticLog Diagnostics;
    uint64 DiagnosticCount = 0;

    struct FTerminal
    {
        std::mutex Mutex;
        std::condition_variable Changed;
        FInteractiveLabServiceResponse Response;
        FString Failure;
        bool bDone = false;
        bool bTimedOut = false;
        Clock::time_point Started;
        // The callback owns the native bindings until both threads have joined.
        FInteractiveLabSessionCallbacks::FServiceCallback Service;
        FInteractiveLabServiceRequest Request;
    };
    TUniquePtr<FTerminal> Terminal;
    std::thread Worker, Watchdog;

    uint64 Now()
    {
        const auto Value = Config.MonotonicMilliseconds ? Config.MonotonicMilliseconds() : Milliseconds();
        LastClock = std::max(LastClock, Value);
        return LastClock;
    }
    void AddDiagnostic(FApplicationDiagnosticRecord Record)
    {
        ++DiagnosticCount;
        Record.StableCode = BoundedText(Record.StableCode, 64);
        Record.SubjectName = BoundedText(Record.SubjectName, 128);
        Record.Message = BoundedText(Record.Message, 832);
        auto& Records = Diagnostics.GetMutableRecords();
        if (Records.size() == 256) Records.erase(Records.begin());
        Records.push_back(std::move(Record));
    }
    void CollectDiagnostics()
    {
        for (const auto& R : Window->GetDiagnostics().GetRecords()) AddDiagnostic(R);
        for (const auto& R : Input->GetDiagnostics().GetRecords()) AddDiagnostic(R);
        Window->GetMutableDiagnostics().Clear();
        Input->GetMutableDiagnostics().Clear();
    }
    void Fail(const FString& Reason)
    {
        if (!FirstFailure.IsEmpty()) return;
        FirstFailure = BoundedText(Reason.IsEmpty() ? FString("lab-native-service-failed") : Reason, 1024);
        AddDiagnostic({EApplicationDiagnosticSeverity::Error, EApplicationDiagnosticCategory::Loop,
            EApplicationResult::RuntimeUnavailable, "APP-LAB-FIRST-FAILURE", "InteractiveLab", FirstFailure});
    }
    void ReleaseInput()
    {
        for (auto K : Input->GetState().GetHeldKeys())
            if (IsKnownKey(K)) Quarantine[static_cast<std::size_t>(K)] = true;
        bRightQuarantined |= Input->GetState().IsMouseButtonHeld(EMouseButton::Right);
        bLook = false; bFreshInterval = true;
        (void)Window->SetCursorMode(ECursorMode::Normal);
    }
    FInteractiveLabServiceRequest Request(Phase P) const
    {
        FInteractiveLabServiceRequest Q;
        Q.Phase = P; Q.SessionId = SessionId;
        Q.Display = Display; Q.DisplayGeneration = Display.DisplayGeneration;
        return Q;
    }
    void StartTerminal()
    {
        if (Terminal) return;
        SessionState = FirstFailure.IsEmpty() ? State::Draining : State::Failed;
        // No callback may run on the event thread after this ownership handoff.
        try { Terminal = MakeUnique<FTerminal>(); }
        catch (...) { FPlatformProcess::TerminateCurrentProcess(125); }
        Terminal->Started = Clock::now();
        Terminal->Service = std::move(Callbacks.Service);
        Terminal->Request = Request(Phase::Drain);
        Terminal->Request.bTerminalOnly = true;
        Terminal->Failure = FirstFailure;
        auto* Job = Terminal.get();
        const auto DrainLimit = std::chrono::milliseconds(Config.DrainTimeoutMilliseconds);
        const auto WatchdogLimit = std::chrono::milliseconds(Config.TerminalWatchdogMilliseconds);
        try
        {
            // A separate watchdog also bounds a blocked event-service thread.
            Watchdog = std::thread([Job, DrainLimit, WatchdogLimit] {
                std::unique_lock Lock(Job->Mutex);
                if (Job->Changed.wait_until(Lock, Job->Started + DrainLimit, [Job] { return Job->bDone; })) return;
                Job->bTimedOut = true;
                if (Job->Failure.IsEmpty()) Job->Failure = "lab-terminal-drain-timed-out";
                if (Job->Changed.wait_until(Lock, Job->Started + WatchdogLimit, [Job] { return Job->bDone; })) return;
                Job->Response.ShutdownAssurance = Assurance::Forced;
                const auto Owners = Job->Response.RetainedOwnerCount;
                Lock.unlock();
                std::fprintf(stderr, "lab-terminal-watchdog: Forced; retained-owners=%llu; failed\n",
                    static_cast<unsigned long long>(Owners));
                FPlatformProcess::TerminateCurrentProcess(124);
            });
            ReleaseInput();
            Worker = std::thread([Job, DrainLimit] {
                bool bTerminalPhase = false;
                for (;;)
                {
                    FInteractiveLabServiceResponse R;
                    try { R = Job->Service(Job->Request); }
                    catch (...) { R.Status = Status::Failed; R.FirstFailure = "lab-terminal-callback-threw"; }
                    {
                        std::lock_guard Lock(Job->Mutex);
                        if (R.Status == Status::DeviceLost || R.bDeviceLost)
                            R.ShutdownAssurance = Assurance::DeviceLost;
                        Job->Response = R;
                        if (Clock::now() >= Job->Started + DrainLimit && Job->Failure.IsEmpty())
                        {
                            Job->bTimedOut = true;
                            Job->Failure = "lab-terminal-drain-timed-out";
                        }
                        if (Job->Failure.IsEmpty() && !R.FirstFailure.IsEmpty()) Job->Failure = R.FirstFailure;
                        if (Job->Failure.IsEmpty() && R.Status != Status::Success && !Pending(R.Status))
                            Job->Failure = FInteractiveLabSession::ToString(R.Status);
                        if (bTerminalPhase && R.bCompleted && R.RetainedOwnerCount == 0 &&
                            (R.Status == Status::Success || R.Status == Status::Failed || R.Status == Status::DeviceLost) &&
                            (R.ShutdownAssurance == Assurance::Proven || R.ShutdownAssurance == Assurance::IdleAssumed ||
                             R.ShutdownAssurance == Assurance::DeviceLost))
                        {
                            if ((R.bDeviceLost || R.ShutdownAssurance == Assurance::DeviceLost) && Job->Failure.IsEmpty())
                                Job->Failure = "lab-terminal-device-lost";
                            Job->bDone = true;
                            Job->Changed.notify_all();
                            return;
                        }
                    }
                    if (!bTerminalPhase && ((R.bCompleted && R.Status == Status::Success) ||
                        (!Pending(R.Status) && R.Status != Status::Success) || Clock::now() >= Job->Started + DrainLimit))
                    {
                        bTerminalPhase = true;
                        Job->Request.Phase = Phase::TerminalCleanup;
                        Job->Request.bPoll = false;
                    }
                    else Job->Request.bPoll = true;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            });
        }
        catch (...)
        {
            // Owners have already transferred. Do not unwind them into live
            // native work if the process cannot create its terminal workers.
            std::fputs("lab-terminal-thread-start-failed\n", stderr);
            FPlatformProcess::TerminateCurrentProcess(125);
        }
    }
    void PollTerminal()
    {
        bool Done = false;
        {
            std::lock_guard Lock(Terminal->Mutex);
            if (!Terminal->Failure.IsEmpty()) Fail(Terminal->Failure);
            Done = Terminal->bDone;
            if (!FirstFailure.IsEmpty()) SessionState = State::Failed;
            ShutdownAssurance = Done ? Terminal->Response.ShutdownAssurance : Assurance::None;
        }
        if (!Done) return;
        Worker.join(); Watchdog.join();
        Terminal->Service = {};
        SessionState = State::Closed;
        Input->Clear();
    }
};

FInteractiveLabSession::FInteractiveLabSession() : Impl(Core::MakeUnique<FImpl>()) {}
FInteractiveLabSession::~FInteractiveLabSession()
{
    if (!Impl || !Impl->Window) return;
    if (Impl->SessionState != State::Closed)
    {
        Impl->StartTerminal();
        // The independent watchdog bounds this join even if native idle hangs.
        if (Impl->Worker.joinable()) Impl->Worker.join();
        if (Impl->Watchdog.joinable()) Impl->Watchdog.join();
    }
}

EApplicationResult FInteractiveLabSession::Initialize(FWindow& W, FInputManager& I,
    const FFreeCameraState& C, FInteractiveLabSessionCallbacks Callbacks, FInteractiveLabSessionConfig Config)
{
    if (Impl->Window || !W.IsActive() || !Callbacks.Service) return EApplicationResult::InvalidLifecycle;
    if (Config.TransitionTimeoutMilliseconds == 0 || Config.TransitionTimeoutMilliseconds > 5000 ||
        Config.DrainTimeoutMilliseconds == 0 || Config.DrainTimeoutMilliseconds > 5000 ||
        Config.TerminalWatchdogMilliseconds < Config.DrainTimeoutMilliseconds || Config.TerminalWatchdogMilliseconds > 10000 ||
        Config.BusyServiceMilliseconds == 0 || Config.BusyServiceMilliseconds > 16 ||
        Config.MinimizedServiceMilliseconds == 0 || Config.MinimizedServiceMilliseconds > 50)
        return EApplicationResult::ValidationFailed;
    const auto D = W.GetDisplayState();
    if (!D.IsValid() || !ValidExtent(D.DrawableExtent) || !Impl->Camera.Initialize(C, D))
        return EApplicationResult::ValidationFailed;
    auto Identity = NextSession.load();
    do { if (Identity == 0 || Identity == std::numeric_limits<uint64>::max()) return EApplicationResult::InvalidLifecycle; }
    while (!NextSession.compare_exchange_weak(Identity, Identity + 1));
    Impl->SessionId = Identity; Impl->Window = &W; Impl->Input = &I;
    Impl->Display = D; Impl->Callbacks = std::move(Callbacks); Impl->Config = std::move(Config);
    Impl->SessionState = D.DrawableExtent.IsPositive() ? State::Ready : State::PausedZeroExtent;
    (void)Impl->Now();
    return EApplicationResult::Success;
}

EApplicationResult FInteractiveLabSession::RequestTransition(FInteractiveLabTransitionIntent Intent)
{
    if (!Impl->Window || Impl->Terminal || Impl->SessionState == State::Closed) return EApplicationResult::InvalidLifecycle;
    if (!ValidExtent(Intent.DrawableExtent) || Intent.DisplayGeneration != Impl->Display.DisplayGeneration)
        return EApplicationResult::ValidationFailed;
    if (Intent.RequestId == 0) Intent.RequestId = Impl->NextRequest;
    if (!Intent.IsValid() || Intent.RequestId < Impl->NextRequest || Intent.RequestId == std::numeric_limits<uint64>::max())
        return EApplicationResult::ValidationFailed;
    Impl->NextRequest = Intent.RequestId + 1;
    if (!Impl->PendingIntent.IsValid() && !Impl->ActiveIntent.IsValid()) Impl->TransitionStart = Impl->Now();
    Impl->PendingIntent = Intent;
    Impl->ReleaseInput();
    Impl->SessionState = Intent.DrawableExtent.IsPositive() ? State::TransitionPending : State::PausedZeroExtent;
    return EApplicationResult::Success;
}
EApplicationResult FInteractiveLabSession::BeginDrain()
{
    if (!Impl->Window || Impl->Terminal || Impl->SessionState == State::Closed) return EApplicationResult::InvalidLifecycle;
    if (Impl->bDrainOnly) return EApplicationResult::Success;
    Impl->ReleaseInput(); Impl->bDrainOnly = true; Impl->bActivePoll = false;
    Impl->TransitionStart = Impl->Now(); Impl->SessionState = State::Draining;
    return EApplicationResult::Success;
}
EApplicationResult FInteractiveLabSession::RequestExit(const Core::FString& Failure)
{
    if (!Impl->Window) return EApplicationResult::InvalidLifecycle;
    if (!Failure.IsEmpty()) Impl->Fail(Failure);
    if (Impl->SessionState != State::Closed) Impl->StartTerminal();
    return EApplicationResult::Success;
}

EApplicationResult FInteractiveLabSession::Service(double DeltaSeconds)
{
    auto& S = *Impl;
    if (!S.Window || S.SessionState == State::Closed) return EApplicationResult::InvalidLifecycle;
    const auto PreviousDisplay = S.Display;
    const auto Events = S.Window->PollEvents();
    auto Raw = S.Window->PollInputEvents();
    S.Display = S.Window->GetDisplayState();
    bool Lost = std::any_of(Events.begin(), Events.end(), [](const auto& E) {
        return E.EventType == EWindowEventType::FocusLost || E.EventType == EWindowEventType::Minimized;
    });
    Lost |= std::any_of(Raw.begin(), Raw.end(), [](const auto& E) { return E.EventType == EInputEventType::FocusLost; });
    if (Lost || !S.Display.bFocused || !S.Display.DrawableExtent.IsPositive()) S.ReleaseInput();
    bool Overflow = Raw.size() > FInputManager::MaximumEventsPerInterval ||
        std::any_of(Raw.begin(), Raw.end(), [](const auto& E) { return E.EventType == EInputEventType::Overflow; });
    if (Overflow) S.ReleaseInput();
    S.Input->QueueEvents(Raw);
    S.Input->PollFrame(S.Window->GetLifecycleState(), S.Display.bFocused);
    Overflow |= S.Input->DidOverflow();
    if (Overflow)
    {
        S.ReleaseInput();
        S.Quarantine.fill(true);
        S.Quarantine[0] = false;
        S.bRightQuarantined = true;
        Raw.clear();
    }
    S.CollectDiagnostics();
    const auto& Input = S.Input->GetState();
    for (auto K : Input.GetReleasedKeys()) if (IsKnownKey(K)) S.Quarantine[static_cast<std::size_t>(K)] = false;
    for (const auto& E : Raw)
    {
        if (E.EventType == EInputEventType::KeyUp && IsKnownKey(E.Key)) S.Quarantine[static_cast<std::size_t>(E.Key)] = false;
        if (E.EventType == EInputEventType::MouseButtonUp && E.MouseButton == EMouseButton::Right) S.bRightQuarantined = false;
    }
    if (Input.WasMouseButtonReleased(EMouseButton::Right)) S.bRightQuarantined = false;
    if (!S.Display.bFocused || !S.Display.DrawableExtent.IsPositive() || Lost || Overflow) S.ReleaseInput();
    if (S.Window->IsCloseRequested() || S.Window->IsDestroyed()) (void)RequestExit();
    if (S.Terminal)
    {
        S.PollTerminal();
        return S.FirstFailure.IsEmpty() ? EApplicationResult::Success : EApplicationResult::RuntimeUnavailable;
    }
    if (!std::isfinite(DeltaSeconds) || DeltaSeconds < 0) return EApplicationResult::InvalidInput;
    const bool ExtentChanged = PreviousDisplay.DrawableExtent != S.Display.DrawableExtent ||
        PreviousDisplay.DisplayGeneration != S.Display.DisplayGeneration;
    if (!ValidExtent(S.Display.DrawableExtent))
    {
        S.ReleaseInput();
        S.SessionState = State::PausedZeroExtent;
        return EApplicationResult::ValidationFailed;
    }
    if (ExtentChanged)
    {
        FInteractiveLabTransitionIntent Intent{0, S.Display.DisplayGeneration, S.Display.DrawableExtent};
        if (RequestTransition(Intent) != EApplicationResult::Success)
        { S.Fail("lab-invalid-drawable"); S.StartTerminal(); return EApplicationResult::ValidationFailed; }
        if (!PreviousDisplay.DrawableExtent.IsPositive() || !ValidExtent(PreviousDisplay.DrawableExtent))
            S.TransitionStart = S.Now();
    }
    if (!S.Display.DrawableExtent.IsPositive() || S.Display.bMinimized)
    { S.SessionState = State::PausedZeroExtent; return EApplicationResult::Success; }
    if (S.SessionState == State::PausedZeroExtent)
    {
        S.TransitionStart = S.Now(); // zero extent suspends transition progress
        S.SessionState = S.PendingIntent.IsValid() ? State::TransitionPending : State::Ready;
        S.bFreshInterval = true;
    }
    if (S.PendingIntent.IsValid() || S.ActiveIntent.IsValid() || S.bDrainOnly)
    {
        S.ReleaseInput();
        if (S.Now() - S.TransitionStart >= (S.bDrainOnly ? S.Config.DrainTimeoutMilliseconds : S.Config.TransitionTimeoutMilliseconds))
        { S.Fail("lab-transition-timed-out"); S.StartTerminal(); return EApplicationResult::RuntimeUnavailable; }
        if (!S.bDrainOnly && !S.ActiveIntent.IsValid())
        {
            S.ActiveIntent = S.PendingIntent; S.ActiveDisplay = S.Display;
            S.bActivePoll = false;
        }
        auto Q = S.Request(S.bDrainOnly ? Phase::Drain : Phase::Transition);
        Q.bPoll = S.bActivePoll;
        if (!S.bDrainOnly)
        {
            Q.Transition = S.ActiveIntent; Q.RequestId = S.ActiveIntent.RequestId;
            Q.Display = S.ActiveDisplay; Q.DisplayGeneration = S.ActiveIntent.DisplayGeneration;
        }
        FInteractiveLabServiceResponse R;
        try { R = S.Callbacks.Service(Q); }
        catch (...) { R.Status = Status::Failed; R.FirstFailure = "lab-service-callback-threw"; }
        if (!R.FirstFailure.IsEmpty() || (R.Status != Status::Success && !Pending(R.Status)) || R.bDeviceLost)
        {
            S.Fail(R.FirstFailure.IsEmpty() ? FString(ToString(R.Status)) : R.FirstFailure);
            S.StartTerminal(); return EApplicationResult::RuntimeUnavailable;
        }
        S.bActivePoll = R.bAccepted;
        if (!R.bCompleted || R.Status != Status::Success) return EApplicationResult::Success;
        if (S.bDrainOnly) S.bDrainOnly = false;
        else
        {
            if (S.PendingIntent.RequestId == S.ActiveIntent.RequestId) S.PendingIntent = {};
            S.ActiveIntent = {};
        }
        S.bActivePoll = false;
        if (S.PendingIntent.IsValid()) return EApplicationResult::Success;
        S.SessionState = State::Ready; S.bFreshInterval = true;
    }
    FFreeCameraActions A;
    if (S.Display.bFocused && Input.IsFocused() && !Lost && !Overflow)
    {
        const auto Held = [&](EKey K) { return Input.IsKeyHeld(K) && !S.Quarantine[static_cast<std::size_t>(K)]; };
        A.ForwardAxis = float(Held(EKey::W)) - float(Held(EKey::S));
        A.RightAxis = float(Held(EKey::D)) - float(Held(EKey::A));
        A.UpAxis = float(Held(EKey::E)) - float(Held(EKey::Q));
        A.bFast = Held(EKey::LeftShift) || Held(EKey::RightShift);
        if (Input.WasKeyPressed(EKey::Escape)) S.ReleaseInput();
        if (!Input.IsMouseButtonHeld(EMouseButton::Right)) S.bLook = false;
        if (Input.WasMouseButtonPressed(EMouseButton::Right) && !S.bRightQuarantined && !Input.WasKeyPressed(EKey::Escape))
        {
            S.bLook = S.Window->SetCursorMode(ECursorMode::Disabled) == EApplicationResult::Success;
            S.bFreshInterval = true;
        }
        if (!S.bLook) (void)S.Window->SetCursorMode(ECursorMode::Normal);
        A.bLookCaptured = S.bLook;
        if (S.bLook && !S.bFreshInterval) { A.LookDeltaX = Input.GetPointerDeltaX(); A.LookDeltaY = Input.GetPointerDeltaY(); }
        for (const auto& E : Raw) if (E.EventType == EInputEventType::Scroll) A.ScrollDeltaY += E.DeltaY;
    }
    if (!S.Display.bFocused || !Input.IsFocused()) { A = {}; S.bFreshInterval = true; }
    (void)S.Camera.Update(A, S.Display, S.bFreshInterval ? 0.0 : DeltaSeconds);
    S.bFreshInterval = !S.Display.bFocused || !Input.IsFocused();
    S.SessionState = State::Running;
    return EApplicationResult::Success;
}

EApplicationResult FInteractiveLabSession::ExecuteCameraCommand(EInteractiveLabCameraCommand C) noexcept
{
    if (!Impl->Window || Impl->Terminal || Impl->SessionState == State::Closed) return EApplicationResult::InvalidLifecycle;
    if (C == EInteractiveLabCameraCommand::ReleaseCapture) { Impl->ReleaseInput(); return EApplicationResult::Success; }
    if (C != EInteractiveLabCameraCommand::Reset || !Impl->Camera.Reset(Impl->Display)) return EApplicationResult::ValidationFailed;
    return EApplicationResult::Success;
}
State FInteractiveLabSession::GetState() const noexcept { return Impl->SessionState; }
Assurance FInteractiveLabSession::GetShutdownAssurance() const noexcept { return Impl->ShutdownAssurance; }
const FFreeCameraState& FInteractiveLabSession::GetCameraState() const noexcept { return Impl->Camera.GetState(); }
const FWindowDisplayState& FInteractiveLabSession::GetDisplayState() const noexcept { return Impl->Display; }
const FInputState& FInteractiveLabSession::GetInputState() const noexcept
{ static const FInputState Empty; return Impl->Input ? Impl->Input->GetState() : Empty; }
const FInteractiveLabTransitionIntent& FInteractiveLabSession::GetPendingTransition() const noexcept { return Impl->PendingIntent; }
bool FInteractiveLabSession::HasPendingTransition() const noexcept { return Impl->PendingIntent.IsValid(); }
bool FInteractiveLabSession::IsInputFreshRequired() const noexcept { return Impl->bFreshInterval; }
bool FInteractiveLabSession::IsTerminalWorkerRunning() const noexcept
{
    if (!Impl->Terminal) return false;
    std::lock_guard Lock(Impl->Terminal->Mutex);
    return !Impl->Terminal->bDone;
}
uint32 FInteractiveLabSession::GetRecommendedServiceWaitMilliseconds() const noexcept
{ return Impl->SessionState == State::PausedZeroExtent ? Impl->Config.MinimizedServiceMilliseconds : Impl->Config.BusyServiceMilliseconds; }
const FString& FInteractiveLabSession::GetFirstFailure() const noexcept { return Impl->FirstFailure; }
const FApplicationDiagnosticLog& FInteractiveLabSession::GetDiagnostics() const noexcept { return Impl->Diagnostics; }
uint64 FInteractiveLabSession::GetDiagnosticCount() const noexcept { return Impl->DiagnosticCount; }
const char* FInteractiveLabSession::ToString(State S) noexcept
{
    switch (S) {
#define LAB_STATE(X) case State::X: return #X
    LAB_STATE(Starting); LAB_STATE(Ready); LAB_STATE(Running); LAB_STATE(PausedZeroExtent);
    LAB_STATE(TransitionPending); LAB_STATE(Draining); LAB_STATE(Failed); LAB_STATE(Closed);
#undef LAB_STATE
    } return "Unknown";
}
const char* FInteractiveLabSession::ToString(Assurance A) noexcept
{
    switch (A) {
#define LAB_ASSURANCE(X) case Assurance::X: return #X
    LAB_ASSURANCE(None); LAB_ASSURANCE(IdleAssumed); LAB_ASSURANCE(Proven); LAB_ASSURANCE(Forced); LAB_ASSURANCE(DeviceLost);
#undef LAB_ASSURANCE
    } return "Unknown";
}
const char* FInteractiveLabSession::ToString(Status S) noexcept
{
    switch (S) {
#define LAB_STATUS(X) case Status::X: return #X
    LAB_STATUS(Success); LAB_STATUS(NotReady); LAB_STATUS(Unsupported); LAB_STATUS(Invalid);
    LAB_STATUS(TimedOut); LAB_STATUS(Failed); LAB_STATUS(DeviceLost);
#undef LAB_STATUS
    } return "Unknown";
}
} // namespace Stoner::Application
