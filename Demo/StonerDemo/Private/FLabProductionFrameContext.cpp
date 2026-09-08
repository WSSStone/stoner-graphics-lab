#include "FLabProductionFrameContext.h"

#include "FProductionSubmissionHarness.h"
#include "RHI/FRHIFormatInfo.h"
#include "RHI/IRHISemaphore.h"
#include "Renderer/FDeferredFrameExecutor.h"
#include "Renderer/FUIRenderSession.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace Stoner::Demo
{
namespace
{

using namespace Stoner::Core;
using namespace Stoner::RHI;

void Fail(FString* OutReason, const char* Reason)
{
    if (OutReason) *OutReason = Reason;
}

bool IsValidDrawable(uint32 Width, uint32 Height) noexcept
{
    if (Width == 0 || Height == 0 ||
        Width > FLabProductionFrameLimits::MaxDrawableAxis ||
        Height > FLabProductionFrameLimits::MaxDrawableAxis)
        return false;
    return static_cast<uint64>(Width) * static_cast<uint64>(Height) <=
        FLabProductionFrameLimits::MaxDrawablePixels;
}

bool IsZeroDrawable(uint32 Width, uint32 Height) noexcept
{
    // Window systems can report one zero drawable axis while a surface is
    // being minimized. Treat either axis as paused and retain all owners.
    return Width == 0 || Height == 0;
}

bool AddBytes(uint64 Left, uint64 Right, uint64& Out) noexcept
{
    constexpr uint64 Max = std::numeric_limits<uint64>::max();
    if (Right > Max - Left) return false;
    Out = Left + Right;
    return true;
}

constexpr uint32 MaxRetainedPresentationLeases =
    FLabProductionFrameLimits::MaxSlots * RHI::MaxRHIPresentationImageLeases;

uint64 GetAttachmentBytes(
    const FProductionContentDeferredExecutionResources& Resources) noexcept
{
    uint64 Total = 0;
    for (const auto& Texture : Resources.OwnedTextures)
    {
        if (!Texture) return 0;
        FRHITextureFootprint Footprint;
        const auto& Desc = Texture->GetDesc();
        if (Desc.Dimension != ERHITextureDimension::Texture2D ||
            Desc.Depth != 1 || Desc.MipLevels != 1 || Desc.ArrayLayers != 1 ||
            Desc.SampleCount != ERHISampleCount::One)
            return 0;
        if (!TryGetRHITextureFootprint(
                Desc.Format, Desc.Width, Desc.Height, Desc.Depth,
                Footprint))
            return 0;
        // A texture can be referenced by more than one binding, but each
        // owned texture appears once in OwnedTextures.  Count the allocation
        // once, using its actual format footprint.
        uint64 NewTotal = 0;
        if (!AddBytes(Total, Footprint.TotalBytes, NewTotal)) return 0;
        Total = NewTotal;
    }
    return Total;
}

bool AddAttachmentBytes(
    ERHIFormat Format, uint32 Width, uint32 Height, uint64& InOutTotal)
    noexcept
{
    FRHITextureFootprint Footprint;
    if (!TryGetRHITextureFootprint(Format, Width, Height, 1, Footprint))
        return false;
    uint64 NewTotal = 0;
    if (!AddBytes(InOutTotal, Footprint.TotalBytes, NewTotal)) return false;
    InOutTotal = NewTotal;
    return true;
}

bool GetPlannedAttachmentBytes(
    const Renderer::FDeferredFramePlan& Plan,
    uint32 Width, uint32 Height, uint64& OutBytes) noexcept
{
    OutBytes = 0;
    // These are exactly the eight textures created by Build for one preview
    // slot: five deferred attachments, the final output, and the two
    // intermediate output-transform targets. All are fixed 2D/one-mip/
    // one-layer/single-sample allocations in this path.
    constexpr ERHIFormat FixedFormats[] = {
        ERHIFormat::R8G8B8A8_UNorm,
        ERHIFormat::R16G16B16A16_Float,
        ERHIFormat::R16G16B16A16_Float,
        ERHIFormat::D32_Float,
        ERHIFormat::R16G16B16A16_Float};
    for (const ERHIFormat Format : FixedFormats)
    {
        if (!AddAttachmentBytes(Format, Width, Height, OutBytes))
            return false;
    }
    // The final output format is selected by the deferred plan. Count it
    // separately rather than assuming the current RGBA16F choice.
    return AddAttachmentBytes(Plan.Output.Format, Width, Height, OutBytes) &&
        AddAttachmentBytes(ERHIFormat::R16G16B16A16_Float, Width, Height,
            OutBytes) &&
        AddAttachmentBytes(ERHIFormat::R16G16B16A16_Float, Width, Height,
            OutBytes);
}

} // namespace

const char* ToString(ELabProductionFrameState State) noexcept
{
    switch (State)
    {
    case ELabProductionFrameState::Free: return "Free";
    case ELabProductionFrameState::Reserved: return "Reserved";
    case ELabProductionFrameState::Acquired: return "Acquired";
    case ELabProductionFrameState::Recording: return "Recording";
    case ELabProductionFrameState::Recorded: return "Recorded";
    case ELabProductionFrameState::Submitted: return "Submitted";
    case ELabProductionFrameState::RenderCompleted: return "RenderCompleted";
    case ELabProductionFrameState::PresentationQueued:
        return "PresentationQueued";
    case ELabProductionFrameState::Cancelled: return "Cancelled";
    case ELabProductionFrameState::Failed: return "Failed";
    }
    return "Unknown";
}

struct FLabProductionFrameContext::FImpl
{
    struct FSlot
    {
        FProductionContentDeferredExecutionResources Resources;
        TSharedPtr<IRHIFence> RenderFence;
        FRHIBorrowedAcquiredTarget Target;
        uint64 FrameToken = 0;
        ELabProductionFrameState State = ELabProductionFrameState::Free;
        TSharedPtr<Renderer::FUIRenderFrame> UIFrame;
        bool bUIReleased = false;
        bool bResourcesBuilt = false;
        bool bRecorded = false;
        bool bSubmitted = false;
        bool bRenderComplete = false;
        bool bPresentationQueued = false;
        bool bCancelRequested = false;
        bool bSubmissionRetired = false;
        RHI::ERHIResult SubmissionRetireResult = RHI::ERHIResult::Success;
        bool bCommandResetFailed = false;
        RHI::ERHIResult CommandResetFailureResult = RHI::ERHIResult::Success;
    };

    struct FPresentationRecord
    {
        uint64 FrameToken = 0;
        uint32 SlotIndex = 0;
        FRHIBorrowedAcquiredTarget Target;
        FRHIPresentationLease Lease;
    };

    TSharedPtr<IRHIDevice> Device;
    Core::TUniquePtr<FProductionSubmissionHarness> SubmissionHarness;
    TSharedPtr<const Renderer::FStaticModelRenderSnapshot> SceneLease;
    FLabProductionFrameContextConfig Config;
    std::array<FSlot, FLabProductionFrameLimits::MaxSlots> Slots;
    TArray<FPresentationRecord> Presentations;
    FString FailureReason;
    RHI::ERHIResult LastUIPreparationResult = RHI::ERHIResult::Success;
    uint64 SubmittedFrameCount = 0;
    uint64 RenderCompletedFrameCount = 0;
    uint64 RenderRetiredFrameCount = 0;
    uint64 ProvenPresentationReleaseCount = 0;
    uint64 ActiveAttachmentBytes = 0;
    uint64 PeakAttachmentBytes = 0;
    uint64 LastFrameToken = 0;
    uint32 LastFrameSlot = 0;
    ELabProductionFrameState LastFrameState =
        ELabProductionFrameState::Free;
    uint32 Width = 0;
    uint32 Height = 0;
    bool bInitialized = false;
    bool bPausedZeroExtent = false;
    bool bFailed = false;
    bool bShutdownStarted = false;

    struct FRetireSubmissionResult
    {
        RHI::ERHIResult Result = RHI::ERHIResult::InvalidState;
        bool bRetired = false;
    };

    void SetFailure(const char* Reason)
    {
        const bool bHasFirstFailure = bFailed && !FailureReason.IsEmpty();
        bFailed = true;
        if (!bHasFirstFailure)
            FailureReason = Reason ? Reason :
                "interactive production frame failure";
    }

    FSlot* FindSlot(uint64 Token, uint32 SlotIndex) noexcept
    {
        if (SlotIndex >= Slots.size()) return nullptr;
        FSlot& Slot = Slots[SlotIndex];
        return Slot.FrameToken == Token && Token != 0 ? &Slot : nullptr;
    }

    const FSlot* FindSlot(uint64 Token, uint32 SlotIndex) const noexcept
    {
        if (SlotIndex >= Slots.size()) return nullptr;
        const FSlot& Slot = Slots[SlotIndex];
        return Slot.FrameToken == Token && Token != 0 ? &Slot : nullptr;
    }

    void RefreshAttachmentBytes() noexcept
    {
        ActiveAttachmentBytes = 0;
        for (const FSlot& Slot : Slots)
        {
            if (!Slot.bResourcesBuilt) continue;
            uint64 NewTotal = 0;
            if (!AddBytes(ActiveAttachmentBytes,
                    Slot.Resources.AttachmentBytes, NewTotal))
            {
                ActiveAttachmentBytes = 0;
                bFailed = true;
                return;
            }
            ActiveAttachmentBytes = NewTotal;
        }
        PeakAttachmentBytes = std::max(PeakAttachmentBytes,
            ActiveAttachmentBytes);
    }

    bool IsRenderSlotReusable(const FSlot& Slot) const noexcept
    {
        return Slot.State == ELabProductionFrameState::Free;
    }

    void ClearFailure() noexcept
    {
        bFailed = false;
        FailureReason.Clear();
    }

    RHI::ERHIResult GetRetainedRetireResult(const FSlot& Slot) const noexcept
    {
        // RetireDeferred's first result precedes the separate command-buffer
        // reset. Preserve it when both operations reported a failure; a
        // command reset failure is the retained result when submission
        // retirement itself succeeded.
        if (Slot.SubmissionRetireResult != RHI::ERHIResult::Success)
            return Slot.SubmissionRetireResult;
        return Slot.bCommandResetFailed
            ? Slot.CommandResetFailureResult
            : RHI::ERHIResult::Success;
    }

    FRetireSubmissionResult ResetCommand(FSlot& Slot) noexcept
    {
        if (!Slot.Resources.Bindings.CommandBuffer)
            return {RHI::ERHIResult::InvalidState, false};
        RHI::ERHIResult Reset = RHI::ERHIResult::Failed;
        try
        {
            Reset = Slot.Resources.Bindings.CommandBuffer->Reset();
        }
        catch (...)
        {
            Reset = RHI::ERHIResult::Failed;
        }
        if (Reset == RHI::ERHIResult::NotReady)
            return {RHI::ERHIResult::NotReady, false};
        if (Reset != RHI::ERHIResult::Success)
        {
            // NotReady is retryable and deliberately is not latched. Every
            // other reset result is a real failure and must remain visible
            // after a later successful reset frees the slot.
            if (!Slot.bCommandResetFailed)
            {
                Slot.bCommandResetFailed = true;
                Slot.CommandResetFailureResult = Reset;
            }
            return {GetRetainedRetireResult(Slot), false};
        }
        if (Slot.UIFrame)
        {
            // Reset has discarded all unsubmitted commands before upload cancellation.
            if (!Slot.bUIReleased)
            {
                const auto Released = Slot.bSubmitted ? Slot.UIFrame->ReleaseCompleted()
                    : Slot.UIFrame->CancelAfterCommandDiscard();
                if (Released != ERHIResult::Success) return {Released, false};
                Slot.bUIReleased = true;
            }
            const auto Removed = FProductionContentDeferredExecutionBuilder::BindPreviewUI(nullptr, Slot.Resources);
            if (Removed != ERHIResult::Success) return {Removed, false};
            Slot.UIFrame.reset(); Slot.bUIReleased = false;
            Slot.Resources.AttachmentBytes = GetAttachmentBytes(Slot.Resources);
            RefreshAttachmentBytes();
        }
        return {GetRetainedRetireResult(Slot), true};
    }

    FRetireSubmissionResult RetireSubmission(FSlot& Slot) noexcept
    {
        if (!Slot.bSubmitted)
        {
            // A recorded but never submitted command still has to be reset
            // before its persistent slot is reused.
            if (Slot.bRecorded)
            {
                return ResetCommand(Slot);
            }
            return {RHI::ERHIResult::Success, true};
        }
        if (Slot.bSubmissionRetired)
        {
            return ResetCommand(Slot);
        }
        if (!Slot.bRenderComplete || !SubmissionHarness ||
            !Slot.RenderFence || !Slot.Resources.Bindings.CommandBuffer)
            return {RHI::ERHIResult::NotReady, false};

        if (Slot.UIFrame && !Slot.bUIReleased)
        {
            const auto Released = Slot.UIFrame->ReleaseCompleted();
            if (Released != ERHIResult::Success) return {Released, false};
            Slot.bUIReleased = true;
        }
        const FProductionDeferredSubmissionResult Retire =
            SubmissionHarness->RetireDeferred(Slot.RenderFence);
        if (!Retire.bRetired)
        {
            return {Retire.Result == RHI::ERHIResult::Success
                    ? RHI::ERHIResult::NotReady : Retire.Result, false};
        }
        Slot.bSubmissionRetired = true;
        Slot.SubmissionRetireResult = Retire.Result;
        // RetireDeferred resets the fence only after observed completion and
        // removes its native owner. Reset the command buffer separately; a
        // reset failure keeps this slot retained for terminal diagnostics.
        return ResetCommand(Slot);
    }
};

FLabProductionFrameContext::FLabProductionFrameContext()
    : Impl_(Core::MakeUnique<FImpl>())
{
    // Keep the bounded presentation bookkeeping allocation outside the frame
    // path.  A presentation lease is still copied only after the caller has
    // completed native admission; this reserve merely makes that copy
    // capacity-stable for the contract's eight retained leases.
    if (Impl_)
        Impl_->Presentations.reserve(MaxRetainedPresentationLeases);
}

FLabProductionFrameContext::~FLabProductionFrameContext()
{
    // Shutdown is deliberately non-blocking.  The explicit Shutdown call is
    // the only successful owner-release path; destruction does not invent a
    // render or presentation proof and does not wait on a native queue.
}

RHI::ERHIResult FLabProductionFrameContext::Initialize(
    const FLabProductionFrameContextConfig& Config,
    FString* OutReason)
{
    if (OutReason) OutReason->Clear();
    if (!Impl_ || Impl_->bInitialized || !Config.Device ||
        !Config.Device->IsActive() ||
        !Config.SceneLease ||
        Config.SceneLease->GetDraws().empty() ||
        Config.TargetEvidence.Validate() != Asset::EAssetResult::Success ||
        Config.RenderShaders.empty() || Config.RenderShaderPayloads.empty())
    {
        Fail(OutReason, "invalid interactive production frame context input");
        return RHI::ERHIResult::InvalidState;
    }
    const uint32 Width = Config.Composition.DeferredInputs.View.Extent.Width;
    const uint32 Height = Config.Composition.DeferredInputs.View.Extent.Height;
    if (!(IsZeroDrawable(Width, Height) || IsValidDrawable(Width, Height)))
    {
        Fail(OutReason, "interactive drawable exceeds the frozen bounds");
        return RHI::ERHIResult::InvalidState;
    }

    // Keep configuration in one transaction.  No RHI allocation happens
    // until BeginFrame supplies a valid borrowed target for a slot.
    Impl_->Config = Config;
    Impl_->Device = Config.Device;
    Impl_->SceneLease = Config.SceneLease;
    auto SubmissionHarness = Core::MakeUnique<FProductionSubmissionHarness>();
    const RHI::ERHIResult HarnessResult =
        SubmissionHarness->Initialize(Config.Device);
    if (HarnessResult != RHI::ERHIResult::Success)
    {
        Fail(OutReason, "interactive deferred submission harness initialization failed");
        Impl_->Device.reset();
        Impl_->SceneLease.reset();
        Impl_->Config = {};
        return HarnessResult;
    }
    Impl_->SubmissionHarness = std::move(SubmissionHarness);
    Impl_->Width = Width;
    Impl_->Height = Height;
    Impl_->bPausedZeroExtent = IsZeroDrawable(Width, Height);
    Impl_->bInitialized = true;
    Impl_->ClearFailure();
    return Impl_->bPausedZeroExtent
        ? RHI::ERHIResult::NotReady : RHI::ERHIResult::Success;
}

RHI::ERHIResult FLabProductionFrameContext::Reconfigure(
    uint32 Width, uint32 Height, FString* OutReason)
{
    if (OutReason) OutReason->Clear();
    if (!Impl_ || !Impl_->bInitialized || Impl_->bFailed ||
        Impl_->bShutdownStarted)
    {
        Fail(OutReason, "interactive frame context is unavailable");
        return RHI::ERHIResult::InvalidState;
    }
    if (!IsZeroDrawable(Width, Height) && !IsValidDrawable(Width, Height))
    {
        Fail(OutReason, "interactive drawable exceeds the frozen bounds");
        return RHI::ERHIResult::InvalidState;
    }
    if (Width == Impl_->Width && Height == Impl_->Height)
    {
        Impl_->bPausedZeroExtent = IsZeroDrawable(Width, Height);
        return Impl_->bPausedZeroExtent
            ? RHI::ERHIResult::NotReady : RHI::ERHIResult::Success;
    }
    for (const auto& Slot : Impl_->Slots)
    {
        if (!Impl_->IsRenderSlotReusable(Slot))
        {
            Fail(OutReason, "resize waits for all frame render uses to drain");
            return RHI::ERHIResult::NotReady;
        }
    }

    // The old slot attachments are render-only.  Presentation targets are
    // retained separately by the backend-neutral leases and are excluded from
    // this slot budget.
    for (auto& Slot : Impl_->Slots)
    {
        if (Slot.bResourcesBuilt)
        {
            Slot.Resources.Release();
            Slot.bResourcesBuilt = false;
            Slot.RenderFence.reset();
        }
    }
    Impl_->RefreshAttachmentBytes();
    Impl_->Width = Width;
    Impl_->Height = Height;
    Impl_->Config.Composition.DeferredInputs.View.Extent = {Width, Height};
    Impl_->Config.Composition.DeferredInputs.Output.Extent = {Width, Height};
    Impl_->bPausedZeroExtent = IsZeroDrawable(Width, Height);
    return Impl_->bPausedZeroExtent
        ? RHI::ERHIResult::NotReady : RHI::ERHIResult::Success;
}

RHI::ERHIResult FLabProductionFrameContext::BeginFrame(
    uint64 FrameToken,
    uint32 SlotIndex,
    const FRHIBorrowedAcquiredTarget& Target,
    FString* OutReason)
{
    if (OutReason) OutReason->Clear();
    if (!Impl_ || !Impl_->bInitialized || Impl_->bFailed ||
        Impl_->bShutdownStarted || FrameToken == 0 ||
        SlotIndex >= Impl_->Slots.size())
    {
        Fail(OutReason, "invalid interactive frame admission");
        return RHI::ERHIResult::InvalidState;
    }
    if (Impl_->bPausedZeroExtent)
    {
        Fail(OutReason, "zero drawable pauses frame admission");
        return RHI::ERHIResult::NotReady;
    }
    if (!Target.IsValid() || Target.Frame.FrameToken != FrameToken ||
        Target.FrameSlotIndex != SlotIndex ||
        Target.Frame.Width != Impl_->Width ||
        Target.Frame.Height != Impl_->Height)
    {
        Fail(OutReason, "borrowed target does not match the frame context");
        return RHI::ERHIResult::InvalidState;
    }

    FImpl::FSlot& Slot = Impl_->Slots[SlotIndex];
    const bool bReserved = Slot.FrameToken == FrameToken &&
        Slot.State == ELabProductionFrameState::Reserved;
    if (Slot.FrameToken == FrameToken &&
        (Slot.State == ELabProductionFrameState::Acquired ||
         Slot.State == ELabProductionFrameState::Recording ||
         Slot.State == ELabProductionFrameState::Recorded))
    {
        if (Slot.Target.Matches(Target)) return RHI::ERHIResult::Success;
        Fail(OutReason, "same frame token was paired with another target");
        return RHI::ERHIResult::InvalidState;
    }
    if (!bReserved && !Impl_->IsRenderSlotReusable(Slot))
    {
        Fail(OutReason, "both bounded frame slots are busy");
        return RHI::ERHIResult::NotReady;
    }

    FProductionContentComposition Candidate = Impl_->Config.Composition;
    Candidate.FrameToken = FrameToken;
    Candidate.DeferredInputs.View.Extent = {Target.Frame.Width,
        Target.Frame.Height};
    Candidate.DeferredInputs.Output.Extent = {Target.Frame.Width,
        Target.Frame.Height};
    Candidate.DeferredInputs.FrameId =
        Core::FString("LabDeferredFrame");

    if (!Slot.bResourcesBuilt)
    {
        // Prepare the plan and account for exactly the fixed attachments the
        // preview builder will create before entering any device allocation.
        // The post-build footprint check below remains mandatory because a
        // driver/RHI implementation may reject or realize a different
        // descriptor shape.
        Renderer::FDeferredFramePlan PlannedPlan;
        Renderer::FDeferredRendererConfiguration RendererConfig;
        RendererConfig.bEnableValidationReadback = false;
        if (Renderer::FDeferredRenderer(RendererConfig).PrepareFrame(
                Candidate.DeferredInputs, PlannedPlan) !=
                Renderer::EDeferredResult::Success)
        {
            Fail(OutReason, "interactive preview frame planning failed");
            return RHI::ERHIResult::InvalidState;
        }
        uint64 PlannedAttachmentBytes = 0;
        if (!GetPlannedAttachmentBytes(PlannedPlan, Target.Frame.Width,
                Target.Frame.Height, PlannedAttachmentBytes) ||
            PlannedAttachmentBytes == 0 ||
            PlannedAttachmentBytes >
                FLabProductionFrameLimits::MaxAttachmentBytes)
        {
            Fail(OutReason, "interactive slot attachment budget exceeded");
            return RHI::ERHIResult::Failed;
        }
        if (Impl_->ActiveAttachmentBytes >
                FLabProductionFrameLimits::MaxAttachmentBytes -
                    PlannedAttachmentBytes)
        {
            Fail(OutReason, "interactive aggregate attachment budget exceeded");
            return RHI::ERHIResult::NotReady;
        }

        FProductionContentDeferredExecutionBuildOptions Options;
        Options.ExecutionPurpose =
            Renderer::EFrameExecutionPurpose::InteractivePreview;
        Options.ReadbackSelection =
            Renderer::EFrameReadbackSelection::None;
        Options.BorrowedFinalOutput = Target.Texture;
        Options.SceneLease = Impl_->SceneLease;
        FProductionContentDeferredExecutionResources CandidateResources;
        const RHI::ERHIResult BuildResult =
            FProductionContentDeferredExecutionBuilder::Build(
                Impl_->Device, *Impl_->SceneLease, Candidate, 
                Impl_->Config.RenderShaders, Impl_->Config.RenderShaderPayloads,
                Impl_->Config.TargetEvidence, Impl_->Config.OutputSettings,
                CandidateResources, OutReason, Options);
        if (BuildResult != RHI::ERHIResult::Success)
            return BuildResult;
        CandidateResources.AttachmentBytes =
            GetAttachmentBytes(CandidateResources);
        if (CandidateResources.AttachmentBytes == 0 ||
            CandidateResources.AttachmentBytes != PlannedAttachmentBytes ||
            CandidateResources.AttachmentBytes >
                FLabProductionFrameLimits::MaxAttachmentBytes)
        {
            CandidateResources.Release();
            Fail(OutReason, "interactive slot attachment budget exceeded");
            return RHI::ERHIResult::Failed;
        }
        if (Impl_->ActiveAttachmentBytes >
                FLabProductionFrameLimits::MaxAttachmentBytes -
                    CandidateResources.AttachmentBytes)
        {
            CandidateResources.Release();
            Fail(OutReason, "interactive aggregate attachment budget exceeded");
            return RHI::ERHIResult::NotReady;
        }
        auto Fence = Impl_->Device->CreateFence(false);
        if (!Fence.Succeeded())
        {
            CandidateResources.Release();
            Fail(OutReason, "interactive slot completion fence creation failed");
            return Fence.Result;
        }
        Slot.Resources = std::move(CandidateResources);
        Slot.RenderFence = std::move(Fence.Object);
        Slot.bResourcesBuilt = true;
        Impl_->RefreshAttachmentBytes();
    }
    else
    {
        const RHI::ERHIResult Rebind =
            FProductionContentDeferredExecutionBuilder::RebindPreviewOutput(
                Impl_->Device, Target.Texture, Slot.Resources, OutReason);
        if (Rebind != RHI::ERHIResult::Success) return Rebind;
    }

    Slot.Target = Target;
    Slot.FrameToken = FrameToken;
    Slot.State = ELabProductionFrameState::Acquired;
    Slot.bRecorded = false;
    Slot.bSubmitted = false;
    Slot.bRenderComplete = false;
    Slot.bPresentationQueued = false;
    Slot.bCancelRequested = false;
    Slot.bSubmissionRetired = false;
    Slot.SubmissionRetireResult = RHI::ERHIResult::Success;
    Slot.bCommandResetFailed = false;
    Slot.CommandResetFailureResult = RHI::ERHIResult::Success;
    Impl_->LastFrameToken = FrameToken;
    Impl_->LastFrameSlot = SlotIndex;
    Impl_->LastFrameState = Slot.State;
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FLabProductionFrameContext::ReserveFrame(
    uint64 FrameToken, uint32 SlotIndex, FString* OutReason)
{
    if (OutReason) OutReason->Clear();
    if (!Impl_ || !Impl_->bInitialized || Impl_->bFailed ||
        Impl_->bShutdownStarted || FrameToken == 0 ||
        SlotIndex >= Impl_->Slots.size())
    {
        Fail(OutReason, "invalid interactive frame reservation");
        return RHI::ERHIResult::InvalidState;
    }
    if (Impl_->bPausedZeroExtent)
    {
        Fail(OutReason, "zero drawable pauses frame reservation");
        return RHI::ERHIResult::NotReady;
    }
    FImpl::FSlot& Slot = Impl_->Slots[SlotIndex];
    if (Slot.FrameToken == FrameToken &&
        Slot.State == ELabProductionFrameState::Reserved)
        return RHI::ERHIResult::Success;
    if (!Impl_->IsRenderSlotReusable(Slot))
    {
        Fail(OutReason, "both bounded frame slots are busy");
        return RHI::ERHIResult::NotReady;
    }
    Slot.FrameToken = FrameToken;
    Slot.Target = {};
    Slot.State = ELabProductionFrameState::Reserved;
    Slot.bRecorded = false;
    Slot.bSubmitted = false;
    Slot.bRenderComplete = false;
    Slot.bPresentationQueued = false;
    Slot.bCancelRequested = false;
    Slot.bSubmissionRetired = false;
    Slot.SubmissionRetireResult = RHI::ERHIResult::Success;
    Slot.bCommandResetFailed = false;
    Slot.CommandResetFailureResult = RHI::ERHIResult::Success;
    Impl_->LastFrameToken = FrameToken;
    Impl_->LastFrameSlot = SlotIndex;
    Impl_->LastFrameState = Slot.State;
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FLabProductionFrameContext::UpdateOutputSettings(
    const Renderer::FOutputTransformSettings& Settings, FString* OutReason)
{
    if (OutReason) OutReason->Clear();
    if (!Impl_ || !Impl_->bInitialized || Impl_->bFailed || Impl_->bShutdownStarted) return ERHIResult::InvalidState;
    const auto Validated = Renderer::FOutputTransformSettingsValidator().Validate(Settings);
    const auto& Old = Impl_->Config.OutputSettings;
    if (!Validated.Succeeded() || Settings.bRequireReadback || !Settings.bRequirePresentation ||
        Settings.DiagnosticBypass.Mode != Renderer::EOutputTransformDebugBypassMode::Disabled ||
        !Settings.PreTonemapOperations.IsEmpty() || !Settings.PostTonemapOperations.IsEmpty())
    { Fail(OutReason,"invalid ordinary preview output settings"); return ERHIResult::InvalidState; }
    if (Settings.OutputDeviceProfileId != Old.OutputDeviceProfileId ||
        Settings.PreferredNativeEncoding != Old.PreferredNativeEncoding || Settings.NativeReferenceWhiteNits != Old.NativeReferenceWhiteNits)
    { Fail(OutReason,"output profile change requires presentation recreation"); return ERHIResult::ResizeRequired; }
    Impl_->Config.OutputSettings = Settings;
    return ERHIResult::Success;
}

RHI::ERHIResult FLabProductionFrameContext::RecordFrame(
    uint64 FrameToken, uint32 SlotIndex,
    const FProductionContentComposition& FrameComposition,
    FString* OutReason, const FPrepareUI& PrepareUI)
{
    if (OutReason) OutReason->Clear();
    if (!Impl_ || !Impl_->bInitialized || Impl_->bFailed ||
        Impl_->bShutdownStarted)
    {
        Fail(OutReason, "interactive frame context is unavailable");
        return RHI::ERHIResult::InvalidState;
    }
    FImpl::FSlot* Slot = Impl_->FindSlot(FrameToken, SlotIndex);
    if (!Slot || Slot->State != ELabProductionFrameState::Acquired)
    {
        Fail(OutReason, "record requires an acquired frame");
        return RHI::ERHIResult::InvalidState;
    }
    Slot->State = ELabProductionFrameState::Recording;
    // Mark before validation/execution so a partial recording failure is
    // reset by the later logical-cancel acknowledgement.
    Slot->bRecorded = true;
    if (FrameComposition.FrameToken != FrameToken ||
        FrameComposition.DeferredInputs.View.Extent.Width != Impl_->Width ||
        FrameComposition.DeferredInputs.View.Extent.Height != Impl_->Height ||
        FrameComposition.DeferredInputs.Output.Extent.Width != Impl_->Width ||
        FrameComposition.DeferredInputs.Output.Extent.Height != Impl_->Height)
    {
        Slot->State = ELabProductionFrameState::Acquired;
        Fail(OutReason, "frame composition does not match acquired target");
        return RHI::ERHIResult::InvalidState;
    }
    const FProductionContentComposition& Candidate = FrameComposition;
    const auto& PendingSettings = Impl_->Config.OutputSettings;
    const auto& RecordedSettings = Slot->Resources.OutputSettings;
    const bool SettingsChanged = PendingSettings.ManualExposureStops != RecordedSettings.ManualExposureStops ||
        PendingSettings.SDRToneMapVersion != RecordedSettings.SDRToneMapVersion ||
        PendingSettings.HDRViewingVersion != RecordedSettings.HDRViewingVersion;
    const RHI::ERHIResult Update =
        FProductionContentDeferredExecutionBuilder::UpdatePreviewFrame(
            Impl_->Device, *Impl_->SceneLease, Impl_->SceneLease, Candidate,
            Slot->Resources, OutReason, SettingsChanged ? &PendingSettings : nullptr);
    if (Update != RHI::ERHIResult::Success)
    {
        Slot->State = ELabProductionFrameState::Failed;
        Impl_->SetFailure("interactive preview frame parameter update failed");
        return Update;
    }
    Impl_->LastUIPreparationResult = ERHIResult::Success;
    if (PrepareUI)
    {
        const uint64 Available = FLabProductionFrameLimits::MaxAttachmentBytes -
            std::min(Impl_->ActiveAttachmentBytes, FLabProductionFrameLimits::MaxAttachmentBytes);
        // Reject before invoking a preparer that could allocate a new target.
        const uint64 Required = static_cast<uint64>(Impl_->Width) * Impl_->Height * 8ULL;
        TSharedPtr<Renderer::FUIRenderFrame> UI;
        auto Prepared = ERHIResult::Unavailable;
        try
        {
            if (Required <= Available)
                Prepared = PrepareUI(Slot->Resources.Bindings.OutputTransformStages.back().Input, Available, UI);
        }
        catch (const std::bad_alloc&) { Prepared = ERHIResult::Unavailable; }
        if (Prepared == ERHIResult::Success && UI && UI->HasDraws())
        {
            Prepared = FProductionContentDeferredExecutionBuilder::BindPreviewUI(UI, Slot->Resources);
            if (Prepared == ERHIResult::Success)
            {
                Slot->UIFrame = UI;
                Slot->Resources.AttachmentBytes += Required;
                Impl_->RefreshAttachmentBytes();
            }
        }
        if (UI && !Slot->UIFrame)
            (void)UI->CancelAfterCommandDiscard(); // command is still idle
        Impl_->LastUIPreparationResult = Prepared;
        if (Prepared != ERHIResult::Success && Prepared != ERHIResult::NotReady &&
            Prepared != ERHIResult::Unavailable)
        {
            Slot->State = ELabProductionFrameState::Failed;
            Impl_->SetFailure("interactive UI preflight failed");
            Fail(OutReason, "interactive UI preflight failed");
            return Prepared;
        }
    }
    const Renderer::FDeferredFrameExecutionResult Execution =
        Renderer::FDeferredFrameExecutor().Execute(
            Slot->Resources.Plan, Slot->Resources.Graph,
            Slot->Resources.Bindings);
    if (!Execution.Succeeded())
    {
        Slot->State = ELabProductionFrameState::Failed;
        Impl_->SetFailure("interactive deferred frame recording failed");
        Fail(OutReason, "interactive deferred frame recording failed");
        return Execution.NativeResult != ERHIResult::Success ? Execution.NativeResult : ERHIResult::Failed;
    }
    Slot->State = ELabProductionFrameState::Recorded;
    Impl_->LastFrameState = Slot->State;
    return RHI::ERHIResult::Success;
}

const FProductionContentDeferredExecutionResources*
FLabProductionFrameContext::GetResources(
    uint64 FrameToken, uint32 SlotIndex) const noexcept
{
    const FImpl::FSlot* Slot = Impl_ ? Impl_->FindSlot(FrameToken, SlotIndex)
                                    : nullptr;
    return Slot && (Slot->State == ELabProductionFrameState::Acquired ||
                    Slot->State == ELabProductionFrameState::Recording ||
                    Slot->State == ELabProductionFrameState::Recorded ||
                    Slot->State == ELabProductionFrameState::Submitted ||
                    Slot->State == ELabProductionFrameState::RenderCompleted ||
                    Slot->State == ELabProductionFrameState::PresentationQueued)
        ? &Slot->Resources : nullptr;
}

RHI::ERHIResult FLabProductionFrameContext::SubmitFrame(
    uint64 FrameToken, uint32 SlotIndex, FString* OutReason)
{
    if (OutReason) OutReason->Clear();
    if (!Impl_ || !Impl_->bInitialized || Impl_->bFailed ||
        Impl_->bShutdownStarted)
    {
        Fail(OutReason, "interactive deferred submission is unavailable");
        return RHI::ERHIResult::InvalidState;
    }
    FImpl::FSlot* Slot = Impl_ ? Impl_->FindSlot(FrameToken, SlotIndex) : nullptr;
    if (!Slot || Slot->State != ELabProductionFrameState::Recorded ||
        !Slot->RenderFence || !Impl_->SubmissionHarness ||
        !Slot->Resources.Bindings.CommandBuffer)
    {
        Fail(OutReason, "deferred submission requires one recorded frame");
        return RHI::ERHIResult::InvalidState;
    }
    const TArray<TSharedPtr<IRHISemaphore>> Waits =
        Slot->Target.AcquireSemaphore
            ? TArray<TSharedPtr<IRHISemaphore>>{Slot->Target.AcquireSemaphore}
            : TArray<TSharedPtr<IRHISemaphore>>{};
    const FProductionDeferredSubmissionResult Submission =
        Impl_->SubmissionHarness->SubmitDeferred(
            Slot->Resources.Bindings.CommandBuffer, Waits, {},
            Slot->RenderFence);
    if (!Submission.bSubmissionAccepted)
    {
        if (Submission.Result == RHI::ERHIResult::NotReady ||
            Submission.Result == RHI::ERHIResult::Unavailable ||
            Submission.Result == RHI::ERHIResult::ResizeRequired)
            return Submission.Result;
        Slot->State = ELabProductionFrameState::Failed;
        Impl_->SetFailure("interactive deferred submission failed");
        Fail(OutReason, "interactive deferred submission failed");
        return Submission.Result;
    }
    ++Impl_->SubmittedFrameCount;
    Slot->bSubmitted = true;
    Slot->State = ELabProductionFrameState::Submitted;
    Impl_->LastFrameState = Slot->State;
    if (Slot->UIFrame)
    {
        const auto Committed = Slot->UIFrame->Commit(Slot->RenderFence);
        if (Committed != ERHIResult::Success)
        {
            Impl_->SetFailure("submitted UI frame ownership commit failed");
            Fail(OutReason, "submitted UI frame ownership commit failed");
            return Submission.Result != ERHIResult::Success ? Submission.Result : Committed;
        }
    }
    if (Submission.Result != RHI::ERHIResult::Success)
    {
        Impl_->SetFailure("interactive deferred submission reported a native failure");
        Fail(OutReason, "interactive deferred submission reported a native failure");
    }
    return Submission.Result;
}

bool FLabProductionFrameContext::GetAcquiredTarget(
    uint64 FrameToken, uint32 SlotIndex, FRHIBorrowedAcquiredTarget& OutTarget)
    const noexcept
{
    OutTarget = {};
    const FImpl::FSlot* Slot = Impl_ ? Impl_->FindSlot(FrameToken, SlotIndex)
                                    : nullptr;
    if (!Slot || !Slot->Target.IsValid() ||
        (Slot->State != ELabProductionFrameState::Acquired &&
         Slot->State != ELabProductionFrameState::Recording &&
         Slot->State != ELabProductionFrameState::Recorded &&
         Slot->State != ELabProductionFrameState::Submitted &&
         Slot->State != ELabProductionFrameState::RenderCompleted &&
         Slot->State != ELabProductionFrameState::PresentationQueued))
        return false;
    OutTarget = Slot->Target;
    return true;
}

bool FLabProductionFrameContext::GetRenderLease(
    uint64 FrameToken, uint32 SlotIndex, FRHIRenderLease& OutLease) const noexcept
{
    OutLease = {};
    const FImpl::FSlot* Slot = Impl_ ? Impl_->FindSlot(FrameToken, SlotIndex)
                                    : nullptr;
    if (!Slot || !Slot->Target.IsValid() || !Slot->RenderFence ||
        !Slot->bSubmitted || !Slot->bRenderComplete ||
        (Slot->State != ELabProductionFrameState::RenderCompleted &&
         Slot->State != ELabProductionFrameState::PresentationQueued))
        return false;
    OutLease.Frame = Slot->Target.Frame;
    OutLease.FrameSlotIndex = SlotIndex;
    OutLease.CompletionFence = Slot->RenderFence;
    return OutLease.IsValid();
}

RHI::ERHIResult FLabProductionFrameContext::PollRender(
    uint64 FrameToken, uint32 SlotIndex, bool& bOutCompleted,
    FString* OutReason)
{
    bOutCompleted = false;
    if (OutReason) OutReason->Clear();
    FImpl::FSlot* Slot = Impl_ ? Impl_->FindSlot(FrameToken, SlotIndex) : nullptr;
    if (!Slot || !Slot->bSubmitted || !Slot->RenderFence ||
        (Slot->State != ELabProductionFrameState::Submitted &&
         Slot->State != ELabProductionFrameState::PresentationQueued &&
         Slot->State != ELabProductionFrameState::RenderCompleted &&
         Slot->State != ELabProductionFrameState::Cancelled &&
         Slot->State != ELabProductionFrameState::Failed))
    {
        Fail(OutReason, "render polling requires an admitted frame");
        return RHI::ERHIResult::InvalidState;
    }
    if (Slot->bSubmissionRetired)
    {
        // RetireDeferred removes the harness record. A command-buffer reset
        // may still be retryable, so never query the erased record again;
        // completion was already observed before this state was entered.
        bOutCompleted = Slot->bRenderComplete;
        const RHI::ERHIResult Retained =
            Impl_->GetRetainedRetireResult(*Slot);
        if (Retained != RHI::ERHIResult::Success)
        {
            Impl_->SetFailure("interactive render retirement reported a failure");
            Fail(OutReason, "interactive render retirement reported a failure");
        }
        return Retained;
    }
    if (!Impl_->SubmissionHarness)
    {
        Fail(OutReason, "render polling requires its submission harness");
        return RHI::ERHIResult::InvalidState;
    }
    const FProductionDeferredSubmissionResult Poll =
        Impl_->SubmissionHarness->PollDeferred(Slot->RenderFence);
    bOutCompleted = Poll.bCompletionObserved;
    if (Poll.bCompletionObserved)
    {
        // Completion and execution result are independent facts. Preserve
        // the completed owner even when the harness reports its first native
        // failure, so terminal cleanup can still drain/reset it later.
        if (!Slot->bRenderComplete) ++Impl_->RenderCompletedFrameCount;
        Slot->bRenderComplete = true;
        if (Slot->State == ELabProductionFrameState::Submitted)
            Slot->State = ELabProductionFrameState::RenderCompleted;
        if (Slot->UIFrame && !Slot->bUIReleased)
        {
            const auto Released = Slot->UIFrame->ReleaseCompleted();
            if (Released != ERHIResult::Success)
            {
                Impl_->SetFailure("completed UI frame release failed");
                Fail(OutReason, "completed UI frame release failed");
                return Poll.Result != ERHIResult::Success ? Poll.Result : Released;
            }
            Slot->bUIReleased = true;
        }
        if (Poll.Result != RHI::ERHIResult::Success)
        {
            Slot->State = ELabProductionFrameState::Failed;
            Impl_->SetFailure("interactive render completion reported a failure");
            Fail(OutReason, "interactive render completion reported a failure");
        }
        return Poll.Result;
    }
    if (Poll.Result != RHI::ERHIResult::NotReady &&
        Poll.Result != RHI::ERHIResult::Success)
    {
        Slot->State = ELabProductionFrameState::Failed;
        Impl_->SetFailure("interactive render completion reported a failure");
        Fail(OutReason, "interactive render completion reported a failure");
        return Poll.Result;
    }
    return Poll.Result;
}

RHI::ERHIResult FLabProductionFrameContext::QueuePresentation(
    uint64 FrameToken, uint32 SlotIndex, const FRHIPresentationLease& Lease,
    FString* OutReason)
{
    if (OutReason) OutReason->Clear();
    if (!Impl_ || !Impl_->bInitialized || Impl_->bFailed ||
        Impl_->bShutdownStarted)
    {
        Fail(OutReason, "interactive presentation is unavailable");
        return RHI::ERHIResult::InvalidState;
    }
    FImpl::FSlot* Slot = Impl_ ? Impl_->FindSlot(FrameToken, SlotIndex) : nullptr;
    if (!Slot || !Slot->bSubmitted || !Slot->bRenderComplete ||
        Slot->bPresentationQueued ||
        (Slot->State != ELabProductionFrameState::RenderCompleted &&
         Slot->State != ELabProductionFrameState::PresentationQueued) ||
        !Lease.Matches(Slot->Target))
    {
        Fail(OutReason, "presentation lease does not match the completed frame");
        return RHI::ERHIResult::InvalidState;
    }
    if (Impl_->Presentations.size() >= MaxRetainedPresentationLeases)
    {
        Fail(OutReason, "bounded presentation lease capacity is full");
        return RHI::ERHIResult::NotReady;
    }
    Impl_->Presentations.push_back({FrameToken, SlotIndex, Slot->Target, Lease});
    Slot->bPresentationQueued = true;
    Slot->State = ELabProductionFrameState::PresentationQueued;
    Impl_->LastFrameState = Slot->State;
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FLabProductionFrameContext::PollPresentation(
    uint64 FrameToken, uint32 SlotIndex, bool& bOutRetired, FString* OutReason)
{
    bOutRetired = false;
    if (OutReason) OutReason->Clear();
    if (!Impl_) return RHI::ERHIResult::InvalidState;
    const auto Found = std::find_if(
        Impl_->Presentations.begin(), Impl_->Presentations.end(),
        [FrameToken, SlotIndex](const FImpl::FPresentationRecord& Record)
        {
            return Record.FrameToken == FrameToken &&
                Record.SlotIndex == SlotIndex;
        });
    if (Found == Impl_->Presentations.end())
    {
        Fail(OutReason, "presentation lease record is unavailable");
        return RHI::ERHIResult::InvalidState;
    }
    if (!Found->Lease.PresentationCompletionFence ||
        !Found->Lease.PresentationCompletionFence->IsSignaled())
        return RHI::ERHIResult::NotReady;
    ++Impl_->ProvenPresentationReleaseCount;
    Impl_->Presentations.erase(Found);
    bOutRetired = true;
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FLabProductionFrameContext::CancelFrame(
    uint64 FrameToken, uint32 SlotIndex, FString* OutReason)
{
    if (OutReason) OutReason->Clear();
    FImpl::FSlot* Slot = Impl_ ? Impl_->FindSlot(FrameToken, SlotIndex) : nullptr;
    if (!Slot || Slot->bPresentationQueued ||
        (Slot->State != ELabProductionFrameState::Reserved &&
         Slot->State != ELabProductionFrameState::Acquired &&
         Slot->State != ELabProductionFrameState::Recorded &&
         Slot->State != ELabProductionFrameState::RenderCompleted &&
         !(Slot->State == ELabProductionFrameState::Failed &&
            (!Slot->bSubmitted || Slot->bRenderComplete))))
    {
        Fail(OutReason, "frame cancellation is invalid after presentation queue");
        return RHI::ERHIResult::InvalidState;
    }
    if (Slot->bSubmitted && !Slot->bRenderComplete)
    {
        Fail(OutReason, "submitted frame must complete rendering before cancel");
        return RHI::ERHIResult::NotReady;
    }
    Slot->bCancelRequested = true;
    Slot->State = ELabProductionFrameState::Cancelled;
    Impl_->LastFrameState = Slot->State;
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FLabProductionFrameContext::RetireCancelled(
    uint64 FrameToken, uint32 SlotIndex, FString* OutReason)
{
    if (OutReason) OutReason->Clear();
    FImpl::FSlot* Slot = Impl_ ? Impl_->FindSlot(FrameToken, SlotIndex) : nullptr;
    if (!Slot || Slot->State != ELabProductionFrameState::Cancelled ||
        (Slot->bSubmitted && !Slot->bRenderComplete))
    {
        Fail(OutReason, "cancelled frame still has live render ownership");
        return RHI::ERHIResult::NotReady;
    }
    const FImpl::FRetireSubmissionResult Retire =
        Impl_->RetireSubmission(*Slot);
    if (!Retire.bRetired)
    {
        if (Retire.Result != RHI::ERHIResult::NotReady)
        {
            Impl_->SetFailure("cancelled frame retirement failed");
            Fail(OutReason, "cancelled frame retirement failed");
        }
        return Retire.Result;
    }
    // This is a backend acknowledgement for the exact logical cancellation
    // token. It is intentionally independent of native generation
    // retirement and carries no presentation-success evidence. A native
    // first failure remains the returned result even when its owners have
    // since drained and the slot can be reused.
    const RHI::ERHIResult RetireResult = Retire.Result;
    if (RetireResult != RHI::ERHIResult::Success)
        Impl_->SetFailure("cancelled frame retirement reported a failure");
    if (Slot->bSubmitted) ++Impl_->RenderRetiredFrameCount;
    Slot->Target = {};
    Slot->FrameToken = 0;
    Slot->State = ELabProductionFrameState::Free;
    Slot->bRecorded = false;
    Slot->bSubmitted = false;
    Slot->bRenderComplete = false;
    Slot->bCancelRequested = false;
    Slot->bSubmissionRetired = false;
    Slot->SubmissionRetireResult = RHI::ERHIResult::Success;
    Slot->bCommandResetFailed = false;
    Slot->CommandResetFailureResult = RHI::ERHIResult::Success;
    return RetireResult;
}

ELabProductionFrameState FLabProductionFrameContext::GetFrameState(
    uint64 FrameToken, uint32 SlotIndex) const noexcept
{
    const FImpl::FSlot* Slot = Impl_ ? Impl_->FindSlot(FrameToken, SlotIndex)
                                    : nullptr;
    return Slot ? Slot->State : ELabProductionFrameState::Free;
}

FLabProductionFrameContextSnapshot FLabProductionFrameContext::Snapshot() const
{
    FLabProductionFrameContextSnapshot Out;
    if (!Impl_) return Out;
    if (Impl_->Device) Out.NativeOperations = Impl_->Device->GetRuntimeSnapshot().NativeOperations;
    Out.SubmittedFrameCount = Impl_->SubmittedFrameCount;
    Out.RenderCompletedFrameCount = Impl_->RenderCompletedFrameCount;
    Out.RenderRetiredFrameCount = Impl_->RenderRetiredFrameCount;
    Out.ProvenPresentationReleaseCount = Impl_->ProvenPresentationReleaseCount;
    Out.LastUIPreparationResult = Impl_->LastUIPreparationResult;
    Out.ActiveAttachmentBytes = Impl_->ActiveAttachmentBytes;
    Out.PeakAttachmentBytes = Impl_->PeakAttachmentBytes;
    Out.RetainedPresentationCount =
        static_cast<uint32>(Impl_->Presentations.size());
    Out.LastFrameToken = Impl_->LastFrameToken;
    Out.LastFrameSlot = Impl_->LastFrameSlot;
    Out.LastFrameState = Impl_->LastFrameState;
    Out.FailureReason = Impl_->FailureReason;
    Out.bInitialized = Impl_->bInitialized;
    Out.bPausedZeroExtent = Impl_->bPausedZeroExtent;
    Out.bFailed = Impl_->bFailed;
    for (const FImpl::FSlot& Slot : Impl_->Slots)
    {
        if (Slot.bResourcesBuilt) ++Out.ActiveSlotCount;
        if (Slot.State != ELabProductionFrameState::Free)
            ++Out.BusySlotCount;
    }
    return Out;
}

RHI::ERHIResult FLabProductionFrameContext::RetireRenderResources(
    uint64 FrameToken, uint32 SlotIndex, FString* OutReason)
{
    if (OutReason) OutReason->Clear();
    FImpl::FSlot* Slot = Impl_ ? Impl_->FindSlot(FrameToken, SlotIndex) : nullptr;
    if (!Slot || !Slot->bRenderComplete)
    {
        Fail(OutReason, "render resources require observed render completion");
        return RHI::ERHIResult::NotReady;
    }
    if (!Slot->bPresentationQueued && !Slot->bCancelRequested)
    {
        Fail(OutReason, "unpresented frame must be cancelled before retirement");
        return RHI::ERHIResult::NotReady;
    }
    const FImpl::FRetireSubmissionResult Retire =
        Impl_->RetireSubmission(*Slot);
    if (!Retire.bRetired)
    {
        if (Retire.Result != RHI::ERHIResult::NotReady)
        {
            Impl_->SetFailure("render resource retirement failed");
            Fail(OutReason, "render resource retirement failed");
        }
        return Retire.Result;
    }
    const RHI::ERHIResult RetireResult = Retire.Result;
    if (RetireResult != RHI::ERHIResult::Success)
        Impl_->SetFailure("render resource retirement reported a failure");
    if (Slot->bSubmitted) ++Impl_->RenderRetiredFrameCount;
    Slot->Target = {};
    Slot->FrameToken = 0;
    Slot->State = ELabProductionFrameState::Free;
    Slot->bRecorded = false;
    Slot->bSubmitted = false;
    Slot->bRenderComplete = false;
    Slot->bPresentationQueued = false;
    Slot->bCancelRequested = false;
    Slot->bSubmissionRetired = false;
    Slot->SubmissionRetireResult = RHI::ERHIResult::Success;
    Slot->bCommandResetFailed = false;
    Slot->CommandResetFailureResult = RHI::ERHIResult::Success;
    return RetireResult;
}

RHI::ERHIResult FLabProductionFrameContext::Shutdown(FString* OutReason) noexcept
{
    if (OutReason) OutReason->Clear();
    if (!Impl_ || !Impl_->bInitialized) return RHI::ERHIResult::Success;
    Impl_->bShutdownStarted = true;
    for (const auto& Slot : Impl_->Slots)
    {
        if (Slot.State != ELabProductionFrameState::Free)
        {
            Fail(OutReason, "interactive frame context still owns a frame");
            return RHI::ERHIResult::NotReady;
        }
    }
    if (!Impl_->Presentations.empty())
    {
        Fail(OutReason, "interactive presentation leases are still retained");
        return RHI::ERHIResult::NotReady;
    }
    // Verify the harness has no deferred native owners before releasing any
    // slot resources it may still reference. A retryable NotReady leaves all
    // owners intact and can be retried after the caller drains them.
    if (Impl_->SubmissionHarness &&
        Impl_->SubmissionHarness->Release() != RHI::ERHIResult::Success)
    {
        Fail(OutReason, "interactive deferred submission owners are retained");
        return RHI::ERHIResult::NotReady;
    }
    for (auto& Slot : Impl_->Slots)
    {
        if (Slot.bResourcesBuilt)
        {
            Slot.Resources.Release();
            Slot.bResourcesBuilt = false;
        }
        Slot.RenderFence.reset();
    }
    Impl_->RefreshAttachmentBytes();
    Impl_->bInitialized = false;
    Impl_->Device.reset();
    Impl_->SubmissionHarness.reset();
    Impl_->SceneLease.reset();
    Impl_->Config = {};
    return RHI::ERHIResult::Success;
}

RHI::ERHIResult FLabProductionFrameContext::ReleaseAfterDeviceShutdown(
    RHI::ERHIShutdownAssurance Assurance, FString* OutReason) noexcept
{
    if (OutReason) OutReason->Clear();
    if (!Impl_ || !Impl_->bInitialized) return RHI::ERHIResult::Success;
    if (!Impl_->Device || Impl_->Device->GetState() != RHI::ERHIDeviceState::Shutdown ||
        (Assurance != RHI::ERHIShutdownAssurance::Proven &&
         Assurance != RHI::ERHIShutdownAssurance::IdleAssumed &&
         Assurance != RHI::ERHIShutdownAssurance::DeviceLost))
    {
        Fail(OutReason, "terminal host release requires completed device teardown and qualified assurance");
        return RHI::ERHIResult::InvalidState;
    }
    // Native teardown owns its own proofs. Do not poll/reset presentation
    // fences after it, or turn compatibility cleanup into completion evidence.
    Impl_->bShutdownStarted = true;
    Impl_->Presentations.clear();
    for (auto& Slot : Impl_->Slots)
    {
        if (Slot.bResourcesBuilt) Slot.Resources.Release();
        Slot = {};
    }
    Impl_->SubmissionHarness.reset();
    Impl_->SceneLease.reset();
    Impl_->Config = {};
    Impl_->Device.reset();
    Impl_->RefreshAttachmentBytes();
    Impl_->bInitialized = false;
    return RHI::ERHIResult::Success;
}

} // namespace Stoner::Demo
