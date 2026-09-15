#pragma once

#include "Application/FApplicationDiagnostics.h"
#include "Application/FFreeCameraController.h"
#include "Application/FInputManager.h"
#include "Application/FWindow.h"
#include "Core/CoreMinimal.h"
#include "Application/FInputOwnershipSnapshot.h"


#include <functional>

namespace Stoner::Renderer
{
class FUIDrawSnapshot;
class FUITextureLease;
struct FUITextureId;
struct FUITextureRequest;
struct FUITextureResult;
}
namespace Stoner::RHI { enum class ERHIResult; }

namespace Stoner::Application
{

struct FLabPreset;
struct FLabPresetWorkload;
struct FLabPresetStoreConfig;
struct FLabPresetSourceContext;
struct FLabPresetExportResult;
struct FLabSettingsSnapshot;
struct FLabSettingsCapabilities;
struct FLabSettingsTransaction;
struct FLabControlSection;
struct FLabRuntimeInfo;
struct FLabSessionStatistics
{
    Stoner::Core::uint64 InputOverflowIntervals = 0;
    Stoner::Core::uint64 CursorCaptureFailures = 0;
    Stoner::Core::uint64 UIEnableFailures = 0;
    Stoner::Core::uint64 CapabilityPauses = 0;
    Stoner::Core::uint64 DiagnosticEvictions = 0;
};

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

// Value-only bridge to the Demo-owned Renderer UI session. No third-party
// context or graphics resource crosses this Application boundary.
struct FInteractiveLabUICallbacks
{
    std::function<EApplicationResult()> PreflightEnable;
    std::function<void(Stoner::Core::uint64, bool)> BeginFrame;
    std::function<Stoner::Renderer::FUITextureResult(const Stoner::Renderer::FUITextureRequest&)> PrepareTexture;
    std::function<Stoner::Renderer::FUITextureLease(Stoner::Renderer::FUITextureId)> AcquireTexture;
};

struct FLabCaptureActions
{
    std::function<Stoner::Core::FString(const Stoner::Core::FString&,bool,bool)> Request;
    std::function<Stoner::Core::FString()> Status;
};

class FInteractiveLabSession
{
public:
    [[nodiscard]] bool ConfigureCaptureActions(FLabCaptureActions Actions);
    [[nodiscard]] Stoner::Core::FString RequestCaptureExport(const Stoner::Core::FString& Name,
        bool bIncludeUI=false,bool bNumeric=false);
    [[nodiscard]] Stoner::Core::FString GetCaptureExportStatus() const;
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

    [[nodiscard]] EApplicationResult ConfigureUI(FInteractiveLabUICallbacks Callbacks, bool bEnabled);
    [[nodiscard]] EApplicationResult SetUIEnabled(bool bEnabled);
    [[nodiscard]] bool IsUIEnabled() const noexcept;
    [[nodiscard]] Stoner::Core::uint64 GetSessionId() const noexcept;
    [[nodiscard]] const FInputOwnershipSnapshot& GetInputOwnership() const noexcept;
    [[nodiscard]] const Stoner::Core::FString& GetUIFailure() const noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult ExtractUIDrawSnapshot(
        Stoner::Renderer::FUIDrawSnapshot& OutSnapshot) const;

    [[nodiscard]] bool UpdateRuntimeInfo(const FLabRuntimeInfo&);

    // Startup-only registration: <=8 sections, <=64 commands and <=32 debug
    // views in total; identities/labels <=128 bytes. Invocation uses the same
    // settings transaction path as built-in controls, never direct GPU work.
    [[nodiscard]] bool RegisterControlSection(const FLabControlSection&);
    [[nodiscard]] bool InvokeSectionControl(const Stoner::Core::FString& SectionId, const Stoner::Core::FString& ControlId);
    [[nodiscard]] Stoner::Core::uint32 GetControlSectionCount() const noexcept;

    // The composition root polls these value transactions after Service and
    // before admitting a frame. It owns native preparation and reports its
    // actual completion; no success is inferred from merely queueing work.
    [[nodiscard]] bool ConfigureSettings(const FLabSettingsSnapshot&, const FLabSettingsCapabilities&);
    [[nodiscard]] bool ConfigurePresetWorkload(const FLabPresetWorkload&);
    [[nodiscard]] bool RequestPreset(const FLabPreset&);
    [[nodiscard]] bool RequestPresetFile(const Stoner::Core::FString&);
    [[nodiscard]] bool ConfigurePresetExports(const FLabPresetStoreConfig&, const FLabPresetSourceContext&);
    [[nodiscard]] FLabPresetExportResult ExportPreset(const Stoner::Core::FString& Filename, bool bOverwrite = false);
    [[nodiscard]] bool CancelPreset();
    [[nodiscard]] bool HasPendingPreset() const noexcept;
    [[nodiscard]] const Stoner::Core::FString& GetPresetFailure() const noexcept;

    [[nodiscard]] bool RequestSettings(const FLabSettingsSnapshot&);
    [[nodiscard]] bool RefreshSettingsCapabilities(const FLabSettingsCapabilities&, bool bFormerOutputUsable);
    [[nodiscard]] const FLabSettingsTransaction* BeginSettingsTransaction(bool bRenderEligible);
    [[nodiscard]] bool CompleteSettingsTransaction(Stoner::Core::uint64 Token, bool bSuccess, bool bFormerOutputUsable);
    [[nodiscard]] const FLabSettingsSnapshot* GetEffectiveSettings() const noexcept;
    [[nodiscard]] const FLabSettingsSnapshot* GetRequestedSettings() const noexcept;
    [[nodiscard]] const FLabSettingsSnapshot* GetPendingSettings() const noexcept;
    [[nodiscard]] const Stoner::Core::FString& GetSettingsFailure() const noexcept;
    [[nodiscard]] bool IsSettingsPaused() const noexcept;

    // Polls window/input, builds UI before one camera update and makes at most
    // one bounded transition/drain callback. It never sleeps.
    [[nodiscard]] EApplicationResult Service(double DeltaSeconds, bool bRenderEligible = true);

    [[nodiscard]] EApplicationResult RequestTransition(
        FInteractiveLabTransitionIntent Intent);
    [[nodiscard]] EApplicationResult BeginDrain();
    [[nodiscard]] EApplicationResult RequestExit(
        const Stoner::Core::FString& FirstFailure = {});

    [[nodiscard]] EApplicationResult ExecuteCameraCommand(
        EInteractiveLabCameraCommand Command) noexcept;
    [[nodiscard]] EApplicationResult SetNavigationParameters(float MovementSpeed, float VerticalFovRadians) noexcept;

    [[nodiscard]] EInteractiveLabSessionState GetState() const noexcept;
    [[nodiscard]] EInteractiveLabShutdownAssurance GetShutdownAssurance() const noexcept;
    [[nodiscard]] const FFreeCameraState& GetCameraState() const noexcept;
    [[nodiscard]] const FCameraChangeSet& GetCameraChangeSet() const noexcept;
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
    [[nodiscard]] FLabSessionStatistics GetStatistics() const noexcept;

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
