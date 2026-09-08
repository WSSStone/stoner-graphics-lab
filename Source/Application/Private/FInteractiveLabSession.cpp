#include "Application/FInteractiveLabSession.h"
#include "Application/FInputOwnershipSnapshot.h"
#include "Core/FPlatformProcess.h"
#include "FImGuiLabAdapter.h"
#include "FLabInputRouter.h"
#include "FLabSettingsController.h"
#include "FLabPresetCodec.h"
#include "FLabPresetStore.h"

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
    FLabInputRouter Router;
    TUniquePtr<FLabSettingsController> Settings;
    std::optional<FLabPresetWorkload> PresetWorkload;
    std::optional<FLabPreset> PendingPreset;
    TUniquePtr<FLabSettingsController> BeforePreset;
    std::optional<FFreeCameraController> PresetCamera;
    FString PresetFailure;
    TArray<FString> ReadOnlyPresetPaths;
    std::optional<FLabPresetStoreConfig> PresetExports;
    FLabPresetSourceContext PresetSourceContext;
    uint64 SettingsStart = 0;
    TArray<FLabControlSection> ControlSections;
    std::optional<FLabRuntimeInfo> RuntimeInfo;
    bool bInvokingControl = false, bControlsFrozen = false;
    TUniquePtr<FImGuiLabAdapter> UI;
    FInteractiveLabUICallbacks UICallbacks;
    uint64 UIFrameId = 0;
    bool bUIEnabled = false, bUIConfigured = false;
    FString UIFailure;
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
    bool CanEditNavigation() const noexcept
    {
        return Window && !Terminal && (SessionState == State::Ready || SessionState == State::Running) &&
            !Display.bMinimized && Display.DrawableExtent.IsPositive() &&
            !PendingIntent.IsValid() && !ActiveIntent.IsValid() && !bDrainOnly &&
            !PendingPreset && (!Settings || !Settings->GetActive());
    }
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
        bLook = false; bFreshInterval = true;
        Router.CancelInteraction();
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
        PendingPreset.reset(); PresetCamera.reset(); BeforePreset.reset();
        UI.reset(); UICallbacks = {}; bUIEnabled = false;
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
    (void)Impl->Router.Resolve({}, {}, D.bFocused);
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
    Impl->PendingPreset.reset(); Impl->PresetCamera.reset(); Impl->BeforePreset.reset();
    if (!Failure.IsEmpty()) Impl->Fail(Failure);
    if (Impl->SessionState != State::Closed) Impl->StartTerminal();
    return EApplicationResult::Success;
}

EApplicationResult FInteractiveLabSession::Service(double DeltaSeconds, bool bRenderEligible)
{
    auto& S = *Impl;
    if (!S.Window || S.SessionState == State::Closed) return EApplicationResult::InvalidLifecycle;
    S.bControlsFrozen = true;
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
    Raw = S.Input->GetFrameEvents();
    if (Overflow)
    {
        S.ReleaseInput();
        Raw.clear();
    }
    S.CollectDiagnostics();
    const auto& Input = S.Input->GetState();
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
        (void)S.Router.Resolve(Raw,{},false,Overflow);
        if (S.UI) S.UI->Suspend();
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
    FUILabCapture Capture;
    if (S.UI && S.bUIEnabled)
    {
        if (!S.Display.DrawableExtent.IsPositive() || S.Display.bMinimized || Overflow)
            S.UI->Suspend();
        else
        {
            const bool Eligible = bRenderEligible && !S.PendingIntent.IsValid() &&
                !S.ActiveIntent.IsValid() && !S.bDrainOnly &&
                (!S.Settings || (!S.Settings->GetActive() && !S.Settings->IsPaused()));
            S.UICallbacks.BeginFrame(++S.UIFrameId, Eligible);
            FLabPresetActions Presets;
            Presets.bPending = S.PendingPreset.has_value();
            Presets.bNativeActive = S.Settings && S.Settings->GetActive().has_value();
            Presets.bCanExport = S.PresetExports && !Presets.bPending && !Presets.bNativeActive &&
                !S.Display.bMinimized && S.Display.DrawableExtent.IsPositive();
            Presets.Failure = &S.PresetFailure;
            Presets.Import = [this](const FString& Path) { return RequestPresetFile(Path); };
            Presets.Cancel = [this] { return CancelPreset(); };
            Presets.Export = [this](const FString& Name,bool Overwrite) {
                const auto Result = ExportPreset(Name,Overwrite);
                if (Result.bPublished) return FString(Result.Status.IsSuccess()
                    ? "Preset exported." : "Preset published; durability confirmation failed. Do not retry as a new export.");
                return FString(Result.bTemporaryRetained ? "Export failed; temporary cleanup requires attention." : "Export rejected; destination was not published.");
            };
            const auto UIResult = S.UI->Frame(Raw,S.Display,DeltaSeconds,Eligible,S.ControlSections,
                [this](const FString& Section,const FString& Control) { return InvokeSectionControl(Section,Control); },
                S.Settings && !S.PendingPreset && !S.Settings->GetActive() && !S.PendingIntent.IsValid() && !S.ActiveIntent.IsValid(),
                S.RuntimeInfo ? &*S.RuntimeInfo : nullptr,&S.Camera.GetState(),GetRequestedSettings(),
                GetPendingSettings(),GetEffectiveSettings(),&GetSettingsFailure(),
                [this,&S](const FLabSettingsSnapshot& Edit) {
                    auto Candidate = Edit;
                    Candidate.CameraRevision = S.Camera.GetState().CameraRevision;
                    Candidate.DisplayGeneration = S.Display.DisplayGeneration;
                    return RequestSettings(Candidate);
                },S.Settings ? &S.Settings->GetCapabilities() : nullptr,
                [this](float Speed,float Fov) { return SetNavigationParameters(Speed,Fov) == EApplicationResult::Success; },
                [this] { return ExecuteCameraCommand(EInteractiveLabCameraCommand::Reset) == EApplicationResult::Success; },
                S.PresetWorkload ? &Presets : nullptr);
            Capture = S.UI->GetCapture();
            if (UIResult == EApplicationResult::Success) S.UIFailure.Clear();
            else if (S.UI->GetTextureResult() != Stoner::RHI::ERHIResult::NotReady)
                S.UIFailure = S.UI->GetTextureDiagnostic();
        }
    }
    const auto Routed = S.Router.Resolve(Raw,Capture,
        S.Display.bFocused && S.Display.DrawableExtent.IsPositive() && !S.Display.bMinimized,Overflow);
    if (Routed.bCancelInteraction)
    {
        if (S.UI) S.UI->Suspend();
        S.ReleaseInput();
    }
    if (Capture.bHideUIRequested && S.bUIConfigured) (void)SetUIEnabled(false);
    else if (Routed.bToggleUI && S.bUIConfigured) (void)SetUIEnabled(!S.bUIEnabled);
    if (!S.Display.DrawableExtent.IsPositive() || S.Display.bMinimized)
    { S.SessionState = State::PausedZeroExtent; return EApplicationResult::Success; }
    if (S.SessionState == State::PausedZeroExtent)
    {
        S.TransitionStart = S.Now(); // zero extent suspends transition progress
        if (S.Settings && S.Settings->GetActive()) S.SettingsStart = S.Now();
        S.SessionState = S.PendingIntent.IsValid() ? State::TransitionPending : State::Ready;
        S.bFreshInterval = true;
    }
    if (S.Settings && S.Settings->GetActive() && S.Now() - S.SettingsStart >= S.Config.TransitionTimeoutMilliseconds)
    { S.Fail("lab-settings-transition-timed-out"); S.StartTerminal(); return EApplicationResult::RuntimeUnavailable; }
    if (S.PendingIntent.IsValid() || S.ActiveIntent.IsValid() || S.bDrainOnly)
    {
        S.ReleaseInput();
        if (S.Now() - S.TransitionStart >= (S.bDrainOnly ? S.Config.DrainTimeoutMilliseconds : S.Config.TransitionTimeoutMilliseconds))
        { S.Fail("lab-transition-timed-out"); S.StartTerminal(); return EApplicationResult::RuntimeUnavailable; }
        if (S.Settings && S.Settings->GetActive()) return EApplicationResult::Success;
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
    if (S.Settings && S.Settings->GetActive() && S.Settings->GetActive()->bRequiresOutputTransition)
    {
        S.ReleaseInput(); S.SessionState = State::TransitionPending;
        return EApplicationResult::Success;
    }
    FFreeCameraActions A = Routed.Actions;
    if (Routed.bCancelInteraction || Routed.bToggleUI || Capture.bHideUIRequested) A = {};
    if (A.bLookCaptured)
    {
        const bool Captured = S.Window->SetCursorMode(ECursorMode::Disabled) == EApplicationResult::Success;
        if (!S.bLook) { S.bFreshInterval = true; S.Router.InvalidatePointerBaseline(); }
        S.bLook = Captured;
        if (!Captured) { S.Router.CancelInteraction(); A.bLookCaptured = false; }
    }
    else
    {
        S.bLook = false;
        (void)S.Window->SetCursorMode(ECursorMode::Normal);
    }
    if (S.bFreshInterval) A.LookDeltaX = A.LookDeltaY = 0;
    if (!S.Display.bFocused || !Input.IsFocused()) { A = {}; S.bFreshInterval = true; }
    if (!S.PendingPreset) (void)S.Camera.Update(A, S.Display, S.bFreshInterval ? 0.0 : DeltaSeconds);
    S.bFreshInterval = !S.Display.bFocused || !Input.IsFocused();
    S.SessionState = State::Running;
    return EApplicationResult::Success;
}

bool FInteractiveLabSession::UpdateRuntimeInfo(const FLabRuntimeInfo& Info)
{
    auto& S = *Impl;
    if (!S.Window || S.Terminal || S.SessionState == State::Closed || !std::isfinite(Info.ExposureStops) ||
        Info.ExposureStops < -16 || Info.ExposureStops > 16 || Info.RenderCompleted > Info.Submitted ||
        Info.PresentQueued > Info.Submitted || Info.UIFrames > Info.Submitted || Info.SceneFallbackFrames > Info.Submitted)
        return false;
    for (const auto* Text : {&Info.Workload,&Info.RootIdentity,&Info.CookedGeneration,&Info.RequestedProfile,
        &Info.EffectiveProfile,&Info.TransformVersion,&Info.Failure})
        if (Text->View().size() > 1024 || Text->View().find('\0') != std::string_view::npos) return false;
    if (Info.Workload.IsEmpty() || Info.RootIdentity.IsEmpty() || Info.CookedGeneration.IsEmpty() ||
        Info.RequestedProfile.IsEmpty() || Info.EffectiveProfile.IsEmpty() || Info.TransformVersion.IsEmpty()) return false;
    S.RuntimeInfo = Info;
    return true;
}

bool FInteractiveLabSession::RegisterControlSection(const FLabControlSection& Section)
{
    auto& S = *Impl;
    if (!S.Window || S.Terminal || S.SessionState != State::Ready || S.bControlsFrozen || S.bInvokingControl ||
        S.ControlSections.size() >= 8) return false;
    const auto Text = [](const FString& V, bool Identity) {
        if (V.IsEmpty() || V.View().size() > 128) return false;
        return std::all_of(V.View().begin(),V.View().end(),[&](unsigned char C) {
            return Identity ? ((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') ||
                (C >= '0' && C <= '9') || C == '-' || C == '_' || C == '.') : (C >= 32 && C != 127 && C != '#');
        });
    };
    if (!Text(Section.Id,true) || !Text(Section.Title,false)) return false;
    std::size_t Commands = Section.Commands.size(), Views = Section.DebugViews.size();
    for (const auto& Existing : S.ControlSections)
    {
        if (Existing.Id == Section.Id) return false;
        Commands += Existing.Commands.size(); Views += Existing.DebugViews.size();
    }
    if (Commands > 64 || Views > 32) return false;
    TArray<FString> Ids;
    const auto Item = [&](const FString& Id,const FString& Label) {
        if (!Text(Id,true) || !Text(Label,false) || std::find(Ids.begin(),Ids.end(),Id) != Ids.end()) return false;
        Ids.push_back(Id); return true;
    };
    for (const auto& Command : Section.Commands)
        if (!Command.PrepareEdit || !Item(Command.Id,Command.Label)) return false;
    for (const auto& View : Section.DebugViews)
        if (!View.Selection.IsValid() || !Item(View.Id,View.Label)) return false;
    S.ControlSections.push_back(Section);
    return true;
}
bool FInteractiveLabSession::InvokeSectionControl(const FString& SectionId, const FString& ControlId)
{
    auto& S = *Impl;
    if (!S.Settings || S.PendingPreset || S.Terminal || S.SessionState == State::Closed || S.bInvokingControl ||
        S.Settings->GetActive() || S.PendingIntent.IsValid() || S.ActiveIntent.IsValid()) return false;
    auto Candidate = S.Settings->GetRequested();
    Candidate.CameraRevision = S.Camera.GetState().CameraRevision;
    Candidate.DisplayGeneration = S.Display.DisplayGeneration;
    for (const auto& Section : S.ControlSections) if (Section.Id == SectionId)
    {
        for (const auto& Command : Section.Commands) if (Command.Id == ControlId)
        {
            S.bInvokingControl = true;
            bool Prepared = false;
            try { Prepared = Command.PrepareEdit(Candidate); } catch (...) { Prepared = false; }
            S.bInvokingControl = false;
            return Prepared && RequestSettings(Candidate);
        }
        for (const auto& View : Section.DebugViews) if (View.Id == ControlId)
        { Candidate.DebugBypass = View.Selection; return RequestSettings(Candidate); }
    }
    return false;
}
uint32 FInteractiveLabSession::GetControlSectionCount() const noexcept
{ return static_cast<uint32>(Impl->ControlSections.size()); }

bool FInteractiveLabSession::ConfigureSettings(const FLabSettingsSnapshot& Initial, const FLabSettingsCapabilities& Caps)
{
    auto& S = *Impl;
    if (!S.Window || S.Terminal || S.Settings || S.SessionState == State::Closed ||
        Caps.DisplayGeneration != S.Display.DisplayGeneration || Initial.CameraRevision != S.Camera.GetState().CameraRevision)
        return false;
    auto Candidate = MakeUnique<FLabSettingsController>();
    if (!Candidate->Initialize(Initial,Caps)) return false;
    S.Settings = std::move(Candidate);
    return true;
}
bool FInteractiveLabSession::ConfigurePresetWorkload(const FLabPresetWorkload& Workload)
{
    auto& S = *Impl;
    if (!S.Settings || S.PresetWorkload || S.Terminal || Workload.Revision.IsEmpty() ||
        Workload.ProductionRoot.IsEmpty() || Workload.Revision.View().size() > 4096 ||
        Workload.ProductionRoot.View().size() > 4096 || Workload.SourceIdentityDigest.View().size() != 64) return false;
    S.PresetWorkload = Workload;
    return true;
}
bool FInteractiveLabSession::RequestPreset(const FLabPreset& Preset)
{
    auto& S = *Impl;
    if (!S.Settings || !S.PresetWorkload || S.Terminal || S.bDrainOnly || S.SessionState == State::Closed ||
        S.BeforePreset || S.Settings->GetActive()) return false;
    if (Preset.Workload != *S.PresetWorkload) { S.PresetFailure = "Preset workload mismatch"; return false; }
    TArray<uint8> Bytes;
    if (!FLabPresetCodec::Encode(Preset,S.Settings->GetCapabilities().DebugStages,Bytes,S.PresetFailure)) return false;
    auto Settings = *S.Settings;
    auto Candidate = S.Settings->GetRequested();
    Candidate.RequestedProfileId = Preset.Output.RequestedProfileId;
    Candidate.SdrToneMapVersion = Preset.Output.SdrToneMapVersion;
    Candidate.HdrViewingVersion = Preset.Output.HdrViewingVersion;
    Candidate.ExposureStops = Preset.Output.ExposureStops;
    Candidate.UIWhiteMultiplier = Preset.Output.UIWhiteMultiplier;
    Candidate.DebugBypass = Preset.Output.DebugBypass;
    Candidate.DisplayGeneration = Settings.GetCapabilities().DisplayGeneration;
    Candidate.CameraRevision = S.Camera.GetState().CameraRevision;
    if (!Settings.RequestStrict(Candidate)) { S.PresetFailure = Settings.GetFailure(); return false; }
    S.PendingPreset = Preset; S.PresetFailure.Clear(); S.ReleaseInput();
    return true;
}
bool FInteractiveLabSession::RequestPresetFile(const FString& Path)
{
    auto& S = *Impl;
    if (!S.PresetWorkload || !S.Settings || S.Terminal || S.BeforePreset || S.bDrainOnly) return false;
    if (Path.IsEmpty() || Path.View().size() > 4096 || Path.View().find('\0') != std::string_view::npos)
    { S.PresetFailure = "Invalid preset input path"; return false; }
    FString Canonical;
    if (!FPlatformFileSystem::CanonicalizeExistingPath(Path,Canonical).IsSuccess())
    { S.PresetFailure = "Preset input path unavailable"; return false; }
    if (std::find(S.ReadOnlyPresetPaths.begin(),S.ReadOnlyPresetPaths.end(),Canonical) == S.ReadOnlyPresetPaths.end())
    {
        if (S.ReadOnlyPresetPaths.size() >= 16) { S.PresetFailure = "Read-only preset path budget exhausted"; return false; }
        S.ReadOnlyPresetPaths.push_back(Canonical);
    }
    FLabPreset Candidate;
    const auto Status = FLabPresetStore::Read(Path,*S.PresetWorkload,S.Settings->GetCapabilities().DebugStages,Candidate);
    if (!Status.IsSuccess()) { S.PresetFailure = Status.Context; return false; }
    return RequestPreset(Candidate);
}
bool FInteractiveLabSession::ConfigurePresetExports(const FLabPresetStoreConfig& Config, const FLabPresetSourceContext& Context)
{
    auto& S = *Impl;
    if (!S.PresetWorkload || !S.Settings || S.PresetExports || S.Terminal || Config.ExportRoot.IsEmpty() ||
        Config.ExportRoot.View().size() > 4096 || Config.ProtectedPaths.empty() || Config.ProtectedPaths.size() > 48) return false;
    if (Config.ExportRoot.View().find('\0') != std::string_view::npos) return false;
    for (const auto& Path : Config.ProtectedPaths)
        if (Path.IsEmpty() || Path.View().size() > 4096 || Path.View().find('\0') != std::string_view::npos) return false;
    FLabPreset Candidate{*S.PresetWorkload,Context,S.Camera.GetState(),S.Settings->GetRequested()};
    Candidate.SourceContext.DrawableExtent = S.Display.DrawableExtent;
    TArray<uint8> Bytes;
    if (!FLabPresetCodec::Encode(Candidate,S.Settings->GetCapabilities().DebugStages,Bytes,S.PresetFailure)) return false;
    S.PresetExports = Config; S.PresetSourceContext = Context;
    return true;
}
FLabPresetExportResult FInteractiveLabSession::ExportPreset(const FString& Filename, bool Overwrite)
{
    auto& S = *Impl;
    FLabPresetExportResult Result;
    if (!S.PresetExports || !S.PresetWorkload || !S.Settings || S.Terminal || S.bDrainOnly ||
        S.SessionState == State::Closed || S.PendingPreset || S.Settings->GetActive() ||
        S.Display.bMinimized || !S.Display.DrawableExtent.IsPositive())
    {
        S.PresetFailure = "Preset export requires a configured, stable drawable session";
        Result.Status = {EPlatformFileResult::InvalidArgument,0,S.PresetFailure};
        return Result;
    }
    auto Config = *S.PresetExports;
    for (const auto& Path : S.ReadOnlyPresetPaths)
        if (std::find(Config.ProtectedPaths.begin(),Config.ProtectedPaths.end(),Path) == Config.ProtectedPaths.end())
            Config.ProtectedPaths.push_back(Path);
    FLabPreset Candidate{*S.PresetWorkload,S.PresetSourceContext,S.Camera.GetState(),S.Settings->GetRequested()};
    Candidate.SourceContext.DrawableExtent = S.Display.DrawableExtent;
    Result = FLabPresetStore::Export(Config,Filename,Candidate,S.Settings->GetCapabilities().DebugStages,Overwrite);
    S.PresetFailure = Result.Status.IsSuccess() ? FString{} : Result.Status.Context;
    return Result;
}
bool FInteractiveLabSession::CancelPreset()
{
    auto& S = *Impl;
    if (!S.PendingPreset || S.BeforePreset) return false;
    S.PendingPreset.reset(); S.PresetFailure.Clear(); S.ReleaseInput();
    return true;
}
bool FInteractiveLabSession::HasPendingPreset() const noexcept { return Impl->PendingPreset.has_value(); }
const FString& FInteractiveLabSession::GetPresetFailure() const noexcept { return Impl->PresetFailure; }

bool FInteractiveLabSession::RequestSettings(const FLabSettingsSnapshot& Request)
{
    auto& S = *Impl;
    return S.Settings && !S.PendingPreset && !S.Terminal && !S.bInvokingControl && S.SessionState != State::Closed &&
        Request.DisplayGeneration == S.Display.DisplayGeneration && S.Settings->Request(Request);
}
bool FInteractiveLabSession::RefreshSettingsCapabilities(const FLabSettingsCapabilities& Caps, bool FormerUsable)
{
    auto& S = *Impl;
    if (!S.Settings || S.Terminal || S.SessionState == State::Closed || Caps.DisplayGeneration != S.Display.DisplayGeneration) return false;
    if (S.BeforePreset) (void)S.BeforePreset->RefreshCapabilities(Caps,FormerUsable);
    return S.Settings->RefreshCapabilities(Caps,FormerUsable);
}
const FLabSettingsTransaction* FInteractiveLabSession::BeginSettingsTransaction(bool Eligible)
{
    auto& S = *Impl;
    if (!S.Settings || S.Terminal || S.PendingIntent.IsValid() || S.ActiveIntent.IsValid() || S.bDrainOnly ||
        (S.SessionState != State::Ready && S.SessionState != State::Running) ||
        S.Display.bMinimized || !S.Display.DrawableExtent.IsPositive()) return nullptr;
    if (S.Settings->GetPending() && S.Settings->GetPending()->DisplayGeneration != S.Display.DisplayGeneration) return nullptr;
    if (S.BeforePreset && S.Settings->GetActive()) return nullptr;
    if (S.PendingPreset && !S.BeforePreset && Eligible)
    {
        if (S.Settings->GetActive() || S.Settings->GetCapabilities().DisplayGeneration != S.Display.DisplayGeneration) return nullptr;
        auto Camera = S.Camera;
        auto Settings = MakeUnique<FLabSettingsController>(*S.Settings);
        auto Candidate = S.Settings->GetRequested();
        const auto& Output = S.PendingPreset->Output;
        Candidate.RequestedProfileId = Output.RequestedProfileId;
        Candidate.SdrToneMapVersion = Output.SdrToneMapVersion;
        Candidate.HdrViewingVersion = Output.HdrViewingVersion;
        Candidate.ExposureStops = Output.ExposureStops;
        Candidate.UIWhiteMultiplier = Output.UIWhiteMultiplier;
        Candidate.DebugBypass = Output.DebugBypass;
        Candidate.DisplayGeneration = S.Display.DisplayGeneration;
        if (!Camera.RestorePreset(S.PendingPreset->Camera,S.Display))
        { S.PresetFailure = "Preset camera cannot be prepared for current drawable"; S.PendingPreset.reset(); return nullptr; }
        Candidate.CameraRevision = Camera.GetState().CameraRevision;
        if (!Settings->RequestStrict(Candidate))
        { S.PresetFailure = Settings->GetFailure(); S.PendingPreset.reset(); return nullptr; }
        S.PresetCamera = Camera;
        S.BeforePreset = std::move(S.Settings); S.Settings = std::move(Settings);
    }
    const auto* Transaction = S.Settings->BeginEligible(Eligible);
    if (Transaction) S.SettingsStart = S.Now();
    else if (S.BeforePreset)
    {
        S.PresetFailure = S.Settings->GetFailure();
        S.Settings = std::move(S.BeforePreset); S.PresetCamera.reset(); S.PendingPreset.reset();
    }
    return Transaction;
}
bool FInteractiveLabSession::CompleteSettingsTransaction(uint64 Token, bool Success, bool FormerUsable)
{
    auto& S = *Impl;
    if (!S.Settings || S.Terminal || S.SessionState == State::Closed) return false;
    if (S.BeforePreset)
    {
        const auto& Active = S.Settings->GetActive();
        if (!Active || Active->Token != Token) return false;
        const bool Current = Active->Settings.DisplayGeneration == S.Display.DisplayGeneration &&
            S.PresetCamera && S.PresetCamera->GetState().DrawableExtent == S.Display.DrawableExtent &&
            !S.Display.bMinimized && S.Display.DrawableExtent.IsPositive();
        const bool Completed = S.Settings->Complete(Token,Success && Current,FormerUsable);
        const bool Publish = Completed && Success && Current;
        if (Publish) { S.Camera = *S.PresetCamera; S.BeforePreset.reset(); S.PresetFailure.Clear(); }
        else
        {
            S.PresetFailure = Current ? "Preset native output preparation failed" : "Preset display changed before commit";
            S.Settings = std::move(S.BeforePreset);
            if (!Current || !FormerUsable) S.Settings->MarkOutputUnusable();
        }
        S.PendingPreset.reset(); S.PresetCamera.reset(); S.ReleaseInput();
        return Publish;
    }
    const auto& Active = S.Settings->GetActive();
    if (Active && Active->Token == Token && Active->Settings.DisplayGeneration != S.Display.DisplayGeneration)
    {
        (void)S.Settings->Complete(Token,false,false);
        return false;
    }
    return S.Settings->Complete(Token,Success,FormerUsable);
}
const FLabSettingsSnapshot* FInteractiveLabSession::GetEffectiveSettings() const noexcept
{ return Impl->BeforePreset ? &Impl->BeforePreset->GetEffective() : Impl->Settings ? &Impl->Settings->GetEffective() : nullptr; }
const FLabSettingsSnapshot* FInteractiveLabSession::GetRequestedSettings() const noexcept
{ return Impl->BeforePreset ? &Impl->BeforePreset->GetRequested() : Impl->Settings ? &Impl->Settings->GetRequested() : nullptr; }
const FLabSettingsSnapshot* FInteractiveLabSession::GetPendingSettings() const noexcept
{ const auto* Settings = Impl->BeforePreset ? Impl->BeforePreset.get() : Impl->Settings.get(); return Settings && Settings->GetPending() ? &*Settings->GetPending() : nullptr; }
const FString& FInteractiveLabSession::GetSettingsFailure() const noexcept
{ static const FString Empty; return Impl->Settings ? Impl->Settings->GetFailure() : Empty; }
bool FInteractiveLabSession::IsSettingsPaused() const noexcept
{ return Impl->Settings && Impl->Settings->IsPaused(); }

EApplicationResult FInteractiveLabSession::ConfigureUI(FInteractiveLabUICallbacks Callbacks, bool bEnabled)
{
    if (!Impl->Window || Impl->Terminal || Impl->bUIConfigured ||
        !Callbacks.PreflightEnable || !Callbacks.BeginFrame || !Callbacks.PrepareTexture || !Callbacks.AcquireTexture)
        return EApplicationResult::InvalidLifecycle;
    Impl->UICallbacks = std::move(Callbacks); Impl->bUIConfigured = true;
    return SetUIEnabled(bEnabled);
}
EApplicationResult FInteractiveLabSession::SetUIEnabled(bool bEnabled)
{
    auto& S = *Impl;
    if (!S.bUIConfigured || S.Terminal || S.SessionState == State::Closed)
        return EApplicationResult::InvalidLifecycle;
    if (S.bUIEnabled == bEnabled) return EApplicationResult::Success;
    if (bEnabled)
    {
        auto Result = S.UICallbacks.PreflightEnable();
        if (Result == EApplicationResult::Success && !S.UI)
        {
            auto Candidate = MakeUnique<FImGuiLabAdapter>();
            Result = Candidate->Initialize(*S.Window,S.UICallbacks.PrepareTexture);
            if (Result == EApplicationResult::Success) S.UI = std::move(Candidate);
        }
        if (Result != EApplicationResult::Success)
        { S.UIFailure = "ui-enable-preflight-failed"; return Result; }
    }
    S.bUIEnabled = bEnabled; S.UIFailure.Clear();
    if (S.UI) S.UI->Suspend();
    S.ReleaseInput();
    return EApplicationResult::Success;
}
bool FInteractiveLabSession::IsUIEnabled() const noexcept { return Impl->bUIEnabled; }
Stoner::Core::uint64 FInteractiveLabSession::GetSessionId() const noexcept { return Impl->SessionId; }
const FInputOwnershipSnapshot& FInteractiveLabSession::GetInputOwnership() const noexcept { return Impl->Router.GetOwnership(); }
const FString& FInteractiveLabSession::GetUIFailure() const noexcept { return Impl->UIFailure; }
Stoner::RHI::ERHIResult FInteractiveLabSession::ExtractUIDrawSnapshot(Stoner::Renderer::FUIDrawSnapshot& Out) const
{
    if (!Impl->UI || !Impl->bUIEnabled || Impl->Terminal || Out.GetSessionId() != Impl->SessionId)
        return Stoner::RHI::ERHIResult::InvalidState;
    return Impl->UI->ExtractSnapshot(Impl->UICallbacks.AcquireTexture,Out);
}

EApplicationResult FInteractiveLabSession::ExecuteCameraCommand(EInteractiveLabCameraCommand C) noexcept
{
    if (!Impl->Window || Impl->Terminal || Impl->SessionState == State::Closed) return EApplicationResult::InvalidLifecycle;
    if (C == EInteractiveLabCameraCommand::ReleaseCapture) { Impl->ReleaseInput(); return EApplicationResult::Success; }
    if (!Impl->CanEditNavigation()) return EApplicationResult::InvalidLifecycle;
    if (C != EInteractiveLabCameraCommand::Reset || !Impl->Camera.Reset(Impl->Display)) return EApplicationResult::ValidationFailed;
    Impl->ReleaseInput();
    return EApplicationResult::Success;
}
EApplicationResult FInteractiveLabSession::SetNavigationParameters(float Speed, float Fov) noexcept
{
    if (!Impl->CanEditNavigation()) return EApplicationResult::InvalidLifecycle;
    return Impl->Camera.SetNavigationParameters(Speed,Fov,Impl->Display)
        ? EApplicationResult::Success : EApplicationResult::ValidationFailed;
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
