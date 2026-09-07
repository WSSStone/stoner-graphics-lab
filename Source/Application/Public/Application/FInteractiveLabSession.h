#pragma once

#include "Application/FApplicationDiagnostics.h"
#include "Application/FFreeCameraController.h"
#include "Application/FInputManager.h"
#include "Application/FWindow.h"
#include "Core/CoreMinimal.h"

#include <functional>

namespace Stoner::Application
{

// This state belongs to the Application coordinator.  It deliberately has no
// RHI or platform presentation values; the Demo supplies those through the
// value callback below.
enum class EInteractiveLabSessionState : Stoner::Core::uint8
{
    Starting,
    Ready,
    Running,
    PausedZeroExtent,
    TransitionPending,
    Draining,
    Failed,
    Closed
};

enum class EInteractiveLabShutdownAssurance : Stoner::Core::uint8
{
    None,
    IdleAssumed,
    Proven,
    Forced,
    DeviceLost
};

enum class EInteractiveLabServicePhase : Stoner::Core::uint8
{
    Transition,
    Drain,
    TerminalCleanup
};

enum class EInteractiveLabServiceStatus : Stoner::Core::uint8
{
    Success,
    NotReady,
    Unsupported,
    Invalid,
    TimedOut,
    Failed,
    DeviceLost
};

enum class EInteractiveLabCameraCommand : Stoner::Core::uint8
{
    Reset,
    ReleaseCapture
};

struct FInteractiveLabTransitionIntent
{
    // Zero is invalid after admission.  RequestTransition assigns a fresh
    // value when the caller leaves RequestId at zero.
    Stoner::Core::uint64 RequestId = 0;
    Stoner::Core::uint64 DisplayGeneration = 0;
    FWindowExtent DrawableExtent;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return RequestId != 0 && DisplayGeneration != 0;
    }
};

struct FInteractiveLabServiceRequest
{
    EInteractiveLabServicePhase Phase = EInteractiveLabServicePhase::Transition;
    Stoner::Core::uint64 SessionId = 0;
    Stoner::Core::uint64 RequestId = 0;
    Stoner::Core::uint64 DisplayGeneration = 0;
    FWindowDisplayState Display;
    FInteractiveLabTransitionIntent Transition;
    bool bPoll = false;
    bool bTerminalOnly = false;

    [[nodiscard]] bool IsValid() const noexcept
    {
        if (SessionId == 0 || DisplayGeneration == 0) return false;
        if (Phase == EInteractiveLabServicePhase::Transition)
            return RequestId != 0 && Transition.IsValid();
        return true;
    }
};

struct FInteractiveLabServiceResponse
{
    EInteractiveLabServiceStatus Status = EInteractiveLabServiceStatus::Unsupported;
    // Accepted means the backend owns the request. Completed is independent
    // and is only true when the callback has real completion evidence.
    bool bAccepted = false;
    bool bCompleted = false;
    bool bDeviceLost = false;
    EInteractiveLabShutdownAssurance ShutdownAssurance =
        EInteractiveLabShutdownAssurance::None;
    Stoner::Core::uint64 RetainedOwnerCount = 0;
    Stoner::Core::FString FirstFailure;
};

struct FInteractiveLabSessionConfig
{
    Stoner::Core::uint64 TransitionTimeoutMilliseconds = 5000;
    Stoner::Core::uint64 DrainTimeoutMilliseconds = 5000;
    Stoner::Core::uint64 TerminalWatchdogMilliseconds = 10000;
    Stoner::Core::uint32 BusyServiceMilliseconds = 16;
    Stoner::Core::uint32 MinimizedServiceMilliseconds = 50;

    // An empty clock selects the production steady clock.  Tests may
    // provide a monotonic millisecond source; decreasing values are clamped
    // by the session and never move a deadline backwards.
    std::function<Stoner::Core::uint64()> MonotonicMilliseconds;
};

struct FInteractiveLabSessionCallbacks
{
    // Transition and Drain callbacks must be bounded/non-blocking and may be
    // called repeatedly with bPoll=true. TerminalCleanup is the one callback
    // that runs on the session's terminal worker and may perform qualified
    // terminal-only idle cleanup. A callback that captures native ownership
    // must keep that ownership live until it reports completion.
    using FServiceCallback = std::function<FInteractiveLabServiceResponse(
        const FInteractiveLabServiceRequest&)>;

    FServiceCallback Service;
};

class FInteractiveLabSession
{
public:
    FInteractiveLabSession();
    ~FInteractiveLabSession();

    FInteractiveLabSession(const FInteractiveLabSession&) = delete;
    FInteractiveLabSession& operator=(const FInteractiveLabSession&) = delete;
    FInteractiveLabSession(FInteractiveLabSession&&) = delete;
    FInteractiveLabSession& operator=(FInteractiveLabSession&&) = delete;

    // Window and input manager must outlive the session. Service exclusively
    // consumes their events; callers must not poll the input manager in parallel.
    [[nodiscard]] EApplicationResult Initialize(
        FWindow& Window,
        FInputManager& InputManager,
        const FFreeCameraState& InitialCamera,
        FInteractiveLabSessionCallbacks Callbacks = {},
        FInteractiveLabSessionConfig Config = {});

    // Polls window/input, performs one UI-off camera update and makes at most
    // one bounded transition/drain callback. It never sleeps.
    [[nodiscard]] EApplicationResult Service(double DeltaSeconds);

    [[nodiscard]] EApplicationResult RequestTransition(
        FInteractiveLabTransitionIntent Intent);
    [[nodiscard]] EApplicationResult BeginDrain();
    [[nodiscard]] EApplicationResult RequestExit(
        const Stoner::Core::FString& FirstFailure = {});

    [[nodiscard]] EApplicationResult ExecuteCameraCommand(
        EInteractiveLabCameraCommand Command) noexcept;

    [[nodiscard]] EInteractiveLabSessionState GetState() const noexcept;
    [[nodiscard]] EInteractiveLabShutdownAssurance GetShutdownAssurance() const noexcept;
    [[nodiscard]] const FFreeCameraState& GetCameraState() const noexcept;
    [[nodiscard]] const FWindowDisplayState& GetDisplayState() const noexcept;
    [[nodiscard]] const FInputState& GetInputState() const noexcept;
    [[nodiscard]] const FInteractiveLabTransitionIntent& GetPendingTransition() const noexcept;
    [[nodiscard]] bool HasPendingTransition() const noexcept;
    [[nodiscard]] bool IsInputFreshRequired() const noexcept;
    [[nodiscard]] bool IsTerminalWorkerRunning() const noexcept;
    [[nodiscard]] Stoner::Core::uint32 GetRecommendedServiceWaitMilliseconds() const noexcept;
    [[nodiscard]] const Stoner::Core::FString& GetFirstFailure() const noexcept;
    [[nodiscard]] const FApplicationDiagnosticLog& GetDiagnostics() const noexcept;
    [[nodiscard]] Stoner::Core::uint64 GetDiagnosticCount() const noexcept;

    [[nodiscard]] static const char* ToString(
        EInteractiveLabSessionState State) noexcept;
    [[nodiscard]] static const char* ToString(
        EInteractiveLabShutdownAssurance Assurance) noexcept;
    [[nodiscard]] static const char* ToString(
        EInteractiveLabServiceStatus Status) noexcept;

private:
    struct FImpl;
    Stoner::Core::TUniquePtr<FImpl> Impl;
};

} // namespace Stoner::Application
