#include "FLabProductionPreviewExecutor.h"

namespace Stoner::Demo
{
namespace
{
using RHI::ERHIResult;
using Renderer::FOutputTransformPlan;
using Renderer::FOutputTransformNativeFrameBinding;

class FLabPreviewExecutor final : public Renderer::IOutputTransformNativeFrameExecutor
{
public:
    FLabPreviewExecutor(Core::TSharedPtr<FLabProductionFrameContext> InContext,
        RHI::FRHIBorrowedAcquiredTarget InTarget,
        RHI::FRHIResolvedPresentationState InResolved,
        Core::FString InFingerprint, FLabPreviewCancelCallback InCancel)
        : Context(std::move(InContext)), Target(std::move(InTarget)),
          Resolved(std::move(InResolved)), Fingerprint(std::move(InFingerprint)),
          Cancel(std::move(InCancel)) {}

    bool SupportsTerminalUI() const noexcept override { return true; }
    bool SupportsDiagnosticWidgets() const noexcept override { return true; }

    ERHIResult Acquire(const FOutputTransformPlan&,
        FOutputTransformNativeFrameBinding&) override { return ERHIResult::Unsupported; }
    ERHIResult Submit(const FOutputTransformPlan&,
        const FOutputTransformNativeFrameBinding&) override { return ERHIResult::Unsupported; }
    ERHIResult WaitForCompletion(const FOutputTransformPlan&,
        const FOutputTransformNativeFrameBinding&) override { return ERHIResult::Unsupported; }
    ERHIResult CompleteReadback(const FOutputTransformPlan&,
        const FOutputTransformNativeFrameBinding&) override { return ERHIResult::Unsupported; }
    ERHIResult Present(const FOutputTransformPlan&,
        const FOutputTransformNativeFrameBinding&) override { return ERHIResult::Unsupported; }
    ERHIResult ReleaseAfterFailure(const FOutputTransformNativeFrameBinding&) noexcept override
        { return ERHIResult::Unsupported; }

    ERHIResult AcquirePreview(const FOutputTransformPlan& Plan,
        FOutputTransformNativeFrameBinding& Out) override
    {
        Out = {};
        if (bAcquired || !Matches(Plan) ||
            Context->GetFrameState(Target.Frame.FrameToken, Target.FrameSlotIndex) !=
                ELabProductionFrameState::Recorded)
            return ERHIResult::InvalidState;
        Out = {Target.Frame, Resolved, Target.Texture};
        bAcquired = true;
        return ERHIResult::Success;
    }

    ERHIResult RecordScheduleEvent(const FOutputTransformPlan& Plan,
        const Renderer::FOutputTransformGraphDeclaration& Declaration,
        const Renderer::FRenderGraphScheduleEvent& Event,
        const FOutputTransformNativeFrameBinding& Frame) override
    {
        if (!Matches(Plan, Frame) || !bAcquired || bSubmitted ||
            PassIndex >= Declaration.OrderedPasses.size() ||
            Event.PassIndex != Declaration.OrderedPasses[PassIndex].Index)
            return ERHIResult::InvalidState;
        // RecordFrame batches the scene and its prepared output/UI stages in one
        // existing Deferred command buffer. Here the exact compiled output
        // schedule is matched to that batch; it must not record a second draw.
        ++PassIndex;
        bScheduleComplete = PassIndex == Declaration.OrderedPasses.size();
        return ERHIResult::Success;
    }

    ERHIResult SubmitPreview(const FOutputTransformPlan& Plan,
        const FOutputTransformNativeFrameBinding& Frame, bool& Submitted) override
    {
        Submitted = false;
        if (!Matches(Plan, Frame) || !bScheduleComplete || bSubmitted)
            return ERHIResult::InvalidState;
        const auto Result = Context->SubmitFrame(Target.Frame.FrameToken, Target.FrameSlotIndex);
        bSubmitted = Result == ERHIResult::Success;
        Submitted = bSubmitted;
        return Result;
    }

    ERHIResult PollPreview(const FOutputTransformPlan& Plan,
        const FOutputTransformNativeFrameBinding& Frame, bool& Completed) override
    {
        Completed = false;
        if (!Matches(Plan, Frame) || !bSubmitted) return ERHIResult::InvalidState;
        return Context->PollRender(Target.Frame.FrameToken, Target.FrameSlotIndex, Completed);
    }

    ERHIResult RetirePreview(const FOutputTransformPlan& Plan,
        const FOutputTransformNativeFrameBinding& Frame) noexcept override
    {
        try
        {
            if (bReleased) return ERHIResult::Success;
            if (!Matches(Plan, Frame)) return ERHIResult::InvalidState;
            if (Context->GetFrameState(Target.Frame.FrameToken, Target.FrameSlotIndex) !=
                ELabProductionFrameState::PresentationQueued)
                return CancelFrame();
            const auto Result = Context->RetireRenderResources(Target.Frame.FrameToken, Target.FrameSlotIndex);
            ReleaseIfRetired();
            return Result;
        }
        catch (...) { return ERHIResult::Failed; }
    }

    ERHIResult ReleasePreviewAfterFailure(const FOutputTransformNativeFrameBinding& Frame) noexcept override
    {
        try
        {
            if (bReleased) return ERHIResult::Success;
            if (Frame.PresentationFrame != Target.Frame || Frame.FormalOutput != Target.Texture)
                return ERHIResult::InvalidState;
            return CancelFrame();
        }
        catch (...) { return ERHIResult::Failed; }
    }


    Core::uint32 GetOutstandingTerminalOwnerCount() const noexcept override
    {
        // Independent presentation leases belong to the session/context,
        // rather than this per-render ticket.
        return bReleased ? 0 : 1;
    }

private:
    bool Matches(const FOutputTransformPlan& Plan) const noexcept
    {
        return !bReleased && Context && Target.IsValid() &&
            Plan.FrameToken == Target.Frame.FrameToken && Plan.PlanFingerprint == Fingerprint;
    }
    bool Matches(const FOutputTransformPlan& Plan, const FOutputTransformNativeFrameBinding& Frame) const noexcept
    {
        return Matches(Plan) && Frame.PresentationFrame == Target.Frame &&
            Frame.FormalOutput == Target.Texture && Frame.ResolvedState == Resolved;
    }
    void ReleaseIfRetired() noexcept
    {
        if (Context->GetFrameState(Target.Frame.FrameToken, Target.FrameSlotIndex) ==
            ELabProductionFrameState::Free)
        {
            bReleased = true;
            Target = {};
            Cancel = {};
            Context.reset();
        }
    }
    ERHIResult CancelFrame()
    {
        if (!Cancel || !Context) return ERHIResult::InvalidState;
        RHI::FRHIRenderLease Render;
        if (bSubmitted && !Context->GetRenderLease(Target.Frame.FrameToken, Target.FrameSlotIndex, Render))
            return ERHIResult::NotReady;
        if (!bCancellationAcknowledged)
        {
            bool Acknowledged = false;
            const auto Result = Cancel(Target.Frame.FrameToken, Target.FrameSlotIndex,
                Render.CompletionFence, Acknowledged);
            if (Result != ERHIResult::Success || !Acknowledged)
                return Result == ERHIResult::Success ? ERHIResult::NotReady : Result;
            bCancellationAcknowledged = true;
        }
        if (Context->GetFrameState(Target.Frame.FrameToken, Target.FrameSlotIndex) !=
            ELabProductionFrameState::Cancelled)
        {
            const auto Cancelled = Context->CancelFrame(Target.Frame.FrameToken, Target.FrameSlotIndex);
            if (Cancelled != ERHIResult::Success) return Cancelled;
        }
        const auto Retired = Context->RetireCancelled(Target.Frame.FrameToken, Target.FrameSlotIndex);
        ReleaseIfRetired();
        return Retired;
    }
    Core::TSharedPtr<FLabProductionFrameContext> Context;
    RHI::FRHIBorrowedAcquiredTarget Target;
    RHI::FRHIResolvedPresentationState Resolved;
    Core::FString Fingerprint;
    FLabPreviewCancelCallback Cancel;
    Core::usize PassIndex = 0;
    bool bAcquired = false;
    bool bScheduleComplete = false;
    bool bSubmitted = false;
    bool bReleased = false;
    bool bCancellationAcknowledged = false;
};
} // namespace

Renderer::FOutputTransformPreviewResult RecordLabProductionPreview(
    const Core::TSharedPtr<FLabProductionFrameContext>& Context,
    const FProductionContentComposition& Composition, Core::uint32 Slot,
    const RHI::FRHIResolvedPresentationState& Resolved,
    FLabPreviewCancelCallback Cancel, Renderer::FOutputTransformPreviewTicket& OutTicket,
    const FLabProductionFrameContext::FPrepareUI& PrepareUI, Core::FString* OutReason)
{
    if (OutReason) OutReason->Clear();
    Renderer::FOutputTransformPreviewResult Failure;
    Failure.Result = Renderer::EOutputTransformResult::InvalidBinding;
    Failure.NativeResult = RHI::ERHIResult::InvalidState;
    RHI::FRHIBorrowedAcquiredTarget Target;
    if (!Context || !Cancel || OutTicket.GetTicketId() != 0 ||
        !Context->GetAcquiredTarget(Composition.FrameToken, Slot, Target) ||
        !Target.Frame.Matches(Resolved))
    { if (OutReason) *OutReason="preview acquired target identity mismatch"; return Failure; }
    const auto Recorded = Context->RecordFrame(Composition.FrameToken, Slot, Composition, OutReason, PrepareUI);
    if (Recorded != RHI::ERHIResult::Success)
    {
        Failure.NativeResult = Recorded;
        return Failure;
    }
    const auto* Resources = Context->GetResources(Composition.FrameToken, Slot);
    if (!Resources || !Resources->Bindings.Readbacks.empty())
    { if (OutReason) *OutReason="preview resources unavailable or contain readbacks"; return Failure; }
    auto OutputGraph=Resources->PreviewOutputGraph;
    if (!OutputGraph && !FProductionContentDeferredExecutionBuilder::BuildPreviewGraph(
        Composition,Resources->OutputSettings,
        Resources->OutputTransformPlan.TerminalUI ? &*Resources->OutputTransformPlan.TerminalUI : nullptr,
        Target.Frame.Format,OutputGraph))
    { if (OutReason) *OutReason="preview output graph construction failed"; return Failure; }
    const auto& Plan=OutputGraph->Plan;
    if (Plan.FrameToken!=Composition.FrameToken ||
        Plan.PlanFingerprint!=Resources->OutputTransformPlan.PlanFingerprint)
    { if (OutReason) *OutReason="preview output graph fingerprint mismatch"; return Failure; }
    Renderer::FOutputTransformExecutionBindings Bindings;
    Bindings.SceneColorExternalToken = Composition.FrameToken;
    Bindings.PreviewFrameExecutor = Core::MakeShared<FLabPreviewExecutor>(
        Context, Target, Resolved, Plan.PlanFingerprint, std::move(Cancel));
    return Renderer::FOutputTransformExecutor().RecordPreview(
        Plan, OutputGraph->Graph, OutputGraph->Declaration, Bindings, OutTicket);
}

} // namespace Stoner::Demo
