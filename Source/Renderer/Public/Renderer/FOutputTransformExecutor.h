#pragma once

#include "Renderer/FHDRPostProcessPipeline.h"
#include "RHI/FRHIPresentationFrame.h"
#include "RHI/FRHIResolvedPresentationState.h"
#include "RHI/IRHITexture.h"

namespace Stoner::Renderer
{

struct FOutputTransformNativeFrameBinding
{
    Stoner::RHI::FRHIPresentationFrame PresentationFrame;
    Stoner::RHI::FRHIResolvedPresentationState ResolvedState;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> FormalOutput;
};

// Backend-neutral execution seam. Implementations own only RHI objects and
// synchronization; Renderer remains the sole owner of stage/color policy.
class IOutputTransformNativeFrameExecutor
{
public:
    virtual ~IOutputTransformNativeFrameExecutor() = default;
    [[nodiscard]] virtual bool SupportsTerminalUI() const noexcept { return false; }
    [[nodiscard]] virtual bool SupportsDiagnosticWidgets() const noexcept { return false; }

    virtual Stoner::RHI::ERHIResult Acquire(
        const FOutputTransformPlan& Plan,
        FOutputTransformNativeFrameBinding& OutFrame) = 0;
    virtual Stoner::RHI::ERHIResult RecordScheduleEvent(
        const FOutputTransformPlan& Plan,
        const FOutputTransformGraphDeclaration& Declaration,
        const FRenderGraphScheduleEvent& Event,
        const FOutputTransformNativeFrameBinding& Frame) = 0;
    virtual Stoner::RHI::ERHIResult Submit(
        const FOutputTransformPlan& Plan,
        const FOutputTransformNativeFrameBinding& Frame) = 0;

    // Preview has an explicit asynchronous admission seam.  A legacy/formal
    // executor must not be used as a synchronous preview fallback: the
    // default is Unsupported and is reached before any frame is acquired.
    // Non-success leaves OutFrame empty. The adapter independently retains
    // any pending acquisition and supports retry for the same frame/slot.
    // Its session owner must span retries and drain pending work on close.
    virtual Stoner::RHI::ERHIResult AcquirePreview(
        const FOutputTransformPlan& Plan,
        FOutputTransformNativeFrameBinding& OutFrame)
    {
        (void)Plan;
        (void)OutFrame;
        return Stoner::RHI::ERHIResult::Unsupported;
    }
    virtual Stoner::RHI::ERHIResult SubmitPreview(
        const FOutputTransformPlan& Plan,
        const FOutputTransformNativeFrameBinding& Frame,
        bool& bOutSubmitted)
    {
        (void)Plan;
        (void)Frame;
        bOutSubmitted = false;
        return Stoner::RHI::ERHIResult::Unsupported;
    }
    virtual Stoner::RHI::ERHIResult PollPreview(
        const FOutputTransformPlan& Plan,
        const FOutputTransformNativeFrameBinding& Frame,
        bool& bOutCompleted)
    {
        (void)Plan;
        (void)Frame;
        bOutCompleted = false;
        return Stoner::RHI::ERHIResult::Unsupported;
    }
    virtual Stoner::RHI::ERHIResult RetirePreview(
        const FOutputTransformPlan& Plan,
        const FOutputTransformNativeFrameBinding& Frame) noexcept
    {
        (void)Plan;
        (void)Frame;
        return Stoner::RHI::ERHIResult::Unsupported;
    }
    virtual Stoner::RHI::ERHIResult ReleasePreviewAfterFailure(
        const FOutputTransformNativeFrameBinding& Frame) noexcept
    {
        (void)Frame;
        return Stoner::RHI::ERHIResult::Unsupported;
    }
    virtual Stoner::RHI::ERHIResult WaitForCompletion(
        const FOutputTransformPlan& Plan,
        const FOutputTransformNativeFrameBinding& Frame) = 0;
    virtual Stoner::RHI::ERHIResult CompleteReadback(
        const FOutputTransformPlan& Plan,
        const FOutputTransformNativeFrameBinding& Frame) = 0;
    virtual Stoner::RHI::ERHIResult Present(
        const FOutputTransformPlan& Plan,
        const FOutputTransformNativeFrameBinding& Frame) = 0;
    virtual Stoner::RHI::ERHIResult ReleaseAfterFailure(
        const FOutputTransformNativeFrameBinding& Frame) noexcept = 0;
    [[nodiscard]] virtual Stoner::Core::uint32
    GetOutstandingTerminalOwnerCount() const noexcept = 0;
};

enum class EOutputTransformPreviewState : Stoner::Core::uint8
{
    Invalid,
    Recorded,
    Queued,
    RenderCompleted,
    Failed,
    Retired
};

[[nodiscard]] const char* ToString(EOutputTransformPreviewState State) noexcept;

class FOutputTransformPreviewTicketState;

// Opaque, shared ownership token for one asynchronous preview recording.
// State is private so callers cannot forge completion, retirement or owner
// values by editing a public frame token or boolean.
struct FOutputTransformPreviewTicket
{
public:
    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] Stoner::Core::uint64 GetTicketId() const noexcept;
    [[nodiscard]] Stoner::Core::uint64 GetFrameToken() const noexcept;
    [[nodiscard]] EOutputTransformPreviewState GetState() const noexcept;

private:
    Stoner::Core::TSharedPtr<FOutputTransformPreviewTicketState> State;
    friend class FOutputTransformExecutor;
};

struct FOutputTransformExecutionBindings
{
    Stoner::Core::uint64 SceneColorExternalToken = 0;
    bool bFailTransientResolution = false;
    bool bFailSchedule = false;
    bool bFailReadback = false;
    bool bFailDiagnosticReadback = false;
    bool bFailPresentation = false;
    bool bRequireNativeExecution = false;
    IOutputTransformNativeFrameExecutor* NativeFrameExecutor = nullptr;
    // Preview requires shared ownership across Record/Submit/Poll/Retire so
    // an in-flight native executor cannot be destroyed when a caller drops a
    // local raw pointer. The raw pointer above remains the formal compatibility
    // seam and is intentionally not accepted by RecordPreview.
    Stoner::Core::TSharedPtr<IOutputTransformNativeFrameExecutor>
        PreviewFrameExecutor;
};

struct FOutputTransformExecutionResult
{
    EOutputTransformResult Result = EOutputTransformResult::InvalidGraph;
    EOutputTransformPlanState FinalState = EOutputTransformPlanState::Failed;
    bool bFormalOutputPublished = false;
    Stoner::Core::uint64 PublishedFormalOutputId = 0;
    Stoner::Core::uint64 FrameToken = 0;
    Stoner::Core::FString PlanFingerprint;
    Stoner::Core::uint32 ExecutedPassCount = 0;
    Stoner::Core::uint32 ExecutedFullscreenPassCount = 0;
    Stoner::Core::uint32 ExecutedFullImageVisitCount = 0;
    Stoner::Core::uint32 GpuReadbackCopyCount = 0;
    Stoner::Core::uint32 CpuReadbackInitiationCount = 0;
    bool bDiagnosticBypassProduced = false;
    FOutputTransformDiagnosticBypassRecord DiagnosticBypass;
    Stoner::Core::uint32 DiagnosticGpuReadbackCopyCount = 0;
    Stoner::Core::uint32 DiagnosticCpuReadbackInitiationCount = 0;
    Stoner::RHI::ERHIResult NativeResult =
        Stoner::RHI::ERHIResult::Success;
    Stoner::RHI::FRHIPresentationFrame PresentationFrame;
    Stoner::RHI::FRHIResolvedPresentationState ResolvedPresentationState;
    bool bNativeFrameAcquired = false;
    bool bNativeSubmitted = false;
    bool bNativeCompletionObserved = false;
    bool bNativeReadbackCompleted = false;
    bool bNativePresented = false;
    bool bNativeReleasedAfterFailure = false;
    Stoner::Core::uint32 OutstandingTerminalOwnerCount = 0;
    FOutputTransformDiagnosticLog Diagnostics;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return Result == EOutputTransformResult::Success &&
            bFormalOutputPublished &&
            FinalState == EOutputTransformPlanState::Published;
    }
};

struct FOutputTransformPreviewResult
{
    EOutputTransformResult Result = EOutputTransformResult::InvalidGraph;
    EOutputTransformPreviewState State =
        EOutputTransformPreviewState::Invalid;
    Stoner::RHI::ERHIResult NativeResult =
        Stoner::RHI::ERHIResult::Failed;
    Stoner::Core::uint64 TicketId = 0;
    Stoner::Core::uint64 FrameToken = 0;
    bool bQueued = false;
    bool bRenderCompleted = false;
    bool bRetired = false;
    bool bFormalOutputPublished = false;
    Stoner::Core::uint32 OutstandingOwnerCount = 0;
    FOutputTransformExecutionResult Execution;
    FOutputTransformDiagnosticLog Diagnostics;

    // This describes whether the requested operation was accepted by the
    // preview seam. It intentionally does not imply render completion,
    // presentation, or formal evidence publication.
    [[nodiscard]] bool Accepted() const noexcept
    {
        return Result == EOutputTransformResult::Success && bQueued;
    }
    [[nodiscard]] bool Completed() const noexcept
    {
        return Result == EOutputTransformResult::Success &&
            bRenderCompleted;
    }
    [[nodiscard]] bool Retired() const noexcept { return bRetired; }
};

class FOutputTransformExecutor
{
public:
    [[nodiscard]] FOutputTransformExecutionResult Execute(
        const FOutputTransformPlan& Plan,
        FRenderGraph& Graph,
        const FOutputTransformGraphDeclaration& Declaration,
        const FOutputTransformExecutionBindings& Bindings) const;

    [[nodiscard]] FOutputTransformPreviewResult RecordPreview(
        const FOutputTransformPlan& Plan,
        FRenderGraph& Graph,
        const FOutputTransformGraphDeclaration& Declaration,
        const FOutputTransformExecutionBindings& Bindings,
        FOutputTransformPreviewTicket& OutTicket) const;
    [[nodiscard]] FOutputTransformPreviewResult SubmitPreview(
        FOutputTransformPreviewTicket& Ticket) const;
    [[nodiscard]] FOutputTransformPreviewResult PollPreview(
        FOutputTransformPreviewTicket& Ticket) const;
    [[nodiscard]] FOutputTransformPreviewResult RetirePreview(
        FOutputTransformPreviewTicket& Ticket) const;
};

} // namespace Stoner::Renderer
