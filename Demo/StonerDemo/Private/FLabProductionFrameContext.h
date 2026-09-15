#pragma once

#include "Asset/AssetMinimal.h"
#include "FProductionContentComposition.h"
#include "FProductionContentDeferredExecution.h"
#include "FLabCaptureQueue.h"
#include "Renderer/FOutputTransformExecutor.h"
#include "Renderer/FOutputTransformSettings.h"
#include "Renderer/FStaticModelRealization.h"
#include "RHI/FRHIPresentationFrame.h"
#include "RHI/ERHIPresentationRetirement.h"
#include "RHI/IRHICommandBuffer.h"
#include "RHI/IRHIFence.h"
#include "RHI/IRHIDevice.h"
#include <functional>

namespace Stoner::Demo
{

// These limits are part of the interactive lab contract.  They intentionally
// live with the frame owner so a resize cannot bypass the resource admission
// check by calling the deferred builder directly.
struct FLabProductionFrameLimits
{
    static constexpr Core::uint32 MaxSlots = 2;
    static constexpr Core::uint32 MaxDrawableAxis = 4096;
    static constexpr Core::uint64 MaxDrawablePixels = 7864320;
    static constexpr Core::uint64 MaxAttachmentBytes =
        1024ULL * 1024ULL * 1024ULL;
};

enum class ELabProductionFrameState : Core::uint8
{
    Free,
    Reserved,
    Acquired,
    Recording,
    Recorded,
    Submitted,
    RenderCompleted,
    PresentationQueued,
    Cancelled,
    Failed
};

[[nodiscard]] const char* ToString(ELabProductionFrameState State) noexcept;

struct FLabProductionFrameContextConfig
{
    Core::TSharedPtr<RHI::IRHIDevice> Device;
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot> SceneLease;
    FProductionContentComposition Composition;
    Core::TArray<Core::TSharedPtr<const Asset::FShaderAsset>> RenderShaders;
    Core::TArray<Core::TSharedPtr<const Asset::FShaderPayloadAsset>>
        RenderShaderPayloads;
    Asset::FAssetTargetProfileEvidence TargetEvidence;
    Renderer::FOutputTransformSettings OutputSettings;
};

struct FLabProductionFrameContextSnapshot
{
    RHI::FRHINativeExecutionStatistics NativeOperations;
    Core::uint64 SubmittedFrameCount = 0;
    Core::uint64 RenderCompletedFrameCount = 0;
    Core::uint64 RenderRetiredFrameCount = 0;
    Core::uint64 ProvenPresentationReleaseCount = 0;
    Core::uint32 ActiveSlotCount = 0;
    Core::uint32 BusySlotCount = 0;
    Core::uint32 RetainedPresentationCount = 0;
    Core::uint64 ActiveAttachmentBytes = 0;
    Core::uint64 PeakAttachmentBytes = 0;
    FLabCaptureStatistics Captures;
    Core::uint64 LastFrameToken = 0;
    Core::uint32 LastFrameSlot = 0;
    ELabProductionFrameState LastFrameState =
        ELabProductionFrameState::Free;
    Core::FString FailureReason;
    RHI::ERHIResult LastUIPreparationResult = RHI::ERHIResult::Success;
    bool bInitialized = false;
    bool bPausedZeroExtent = false;
    bool bFailed = false;
};

// Demo-owned orchestration for the two deferred preview slots.  Native
// acquire/present remains behind the T029 backend-neutral bridge; this class
// only accepts exact borrowed targets and typed completion leases and owns the
// RHI frame resources that surround them.
class FLabProductionFrameContext
{
public:
    FLabProductionFrameContext();
    ~FLabProductionFrameContext();
    FLabProductionFrameContext(const FLabProductionFrameContext&) = delete;
    FLabProductionFrameContext& operator=(
        const FLabProductionFrameContext&) = delete;

    [[nodiscard]] RHI::ERHIResult Initialize(
        const FLabProductionFrameContextConfig& Config,
        Core::FString* OutReason = nullptr);

    [[nodiscard]] ELabCaptureStatus RequestCapture(const FLabCaptureRequest&, Core::uint64 Now);
    [[nodiscard]] bool CancelCapture(Core::uint64 RequestId);
    // Called at the explicit copy insertion point while the exact slot command
    // is recording. Ordinary RecordFrame never implicitly requests a capture.
    [[nodiscard]] ELabCaptureStatus PrepareCapture(const FLabCaptureFrame&, Core::uint32 SlotIndex,
        Core::uint64 Now, FLabCapturePrepared& Out);
    [[nodiscard]] bool ProcessCapture(Core::uint64 ServiceFrame, Core::uint64 Now,
        const FLabCaptureQueue::FReadback&, FLabCaptureCompletion& Out);

    // A zero extent pauses admission while retaining all live owners.  A
    // non-zero change is admitted only after all slot render uses have drained.
    [[nodiscard]] RHI::ERHIResult Reconfigure(
        Core::uint32 Width,
        Core::uint32 Height,
        Core::FString* OutReason = nullptr);

    // Stage ordinary output parameters for future recordings. Existing queued
    // slots retain their captured settings and buffers until render completion.
    [[nodiscard]] RHI::ERHIResult UpdateOutputSettings(
        const Renderer::FOutputTransformSettings& Settings, Core::FString* OutReason = nullptr);

    // After native output recreation, retire idle render-only bundles even
    // when extent is unchanged. Independent presentation leases stay retained.
    [[nodiscard]] RHI::ERHIResult ReconfigureOutputSettings(
        const Renderer::FOutputTransformSettings& Settings, Core::FString* OutReason = nullptr);

    // BeginFrame binds the exact acquired target to a reusable slot.  The
    // target is borrowed and remains backend-owned; this call never invalidates
    // it.  Repeating the same token/slot while pending is idempotent.
    [[nodiscard]] RHI::ERHIResult BeginFrame(
        Core::uint64 FrameToken,
        Core::uint32 SlotIndex,
        const RHI::FRHIBorrowedAcquiredTarget& Target,
        Core::FString* OutReason = nullptr);

    // Reserve a slot before native acquisition completes. Repeating the same
    // token/slot is idempotent and retains the pending acquisition ownership.
    [[nodiscard]] RHI::ERHIResult ReserveFrame(
        Core::uint64 FrameToken,
        Core::uint32 SlotIndex,
        Core::FString* OutReason = nullptr);

    // Updates camera/frame parameters and records the existing scene plus
    // output chain into the slot command buffer.  It does not recreate shader,
    // pipeline or scene attachment resources when the extent is unchanged.
    using FPrepareUI = std::function<RHI::ERHIResult(
        const FProductionContentDeferredExecutionResources& Resources,
        Core::uint64 AvailableAttachmentBytes,
        Core::TSharedPtr<Renderer::FUIRenderFrame>& OutFrame,
        Core::TSharedPtr<FProductionContentPreviewGraph>& OutGraph)>;
    [[nodiscard]] RHI::ERHIResult RecordFrame(
        Core::uint64 FrameToken,
        Core::uint32 SlotIndex,
        const FProductionContentComposition& FrameComposition,
        Core::FString* OutReason = nullptr,
        const FPrepareUI& PrepareUI = {});

    // Submit uses the backend's asynchronous queue. Success is admission only;
    // render completion is observed separately by PollRender.
    [[nodiscard]] const FProductionContentDeferredExecutionResources*
    GetResources(Core::uint64 FrameToken, Core::uint32 SlotIndex) const noexcept;
    [[nodiscard]] RHI::ERHIResult SubmitFrame(
        Core::uint64 FrameToken,
        Core::uint32 SlotIndex,
        Core::FString* OutReason = nullptr);

    [[nodiscard]] bool GetAcquiredTarget(
        Core::uint64 FrameToken,
        Core::uint32 SlotIndex,
        RHI::FRHIBorrowedAcquiredTarget& OutTarget) const noexcept;

    // Returns the exact slot completion fence only after the context has
    // observed it complete.  The fence remains associated with this frame
    // until QueuePresentation/Cancel plus retirement resets it, so a backend
    // typed presentation overload can validate the real completion proof.
    [[nodiscard]] bool GetRenderLease(
        Core::uint64 FrameToken,
        Core::uint32 SlotIndex,
        RHI::FRHIRenderLease& OutLease) const noexcept;

    // Polls the slot-owned completion fence.  Completion is observed from the
    // fence itself; callers cannot inject a boolean completion result.
    [[nodiscard]] RHI::ERHIResult PollRender(
        Core::uint64 FrameToken,
        Core::uint32 SlotIndex,
        bool& bOutCompleted,
        Core::FString* OutReason = nullptr);

    // QueuePresentation accepts only a typed lease matching this exact frame.
    // It retains the borrowed target and lease independently of the reusable
    // render slot until PollPresentation observes the lease fence.
    [[nodiscard]] RHI::ERHIResult QueuePresentation(
        Core::uint64 FrameToken,
        Core::uint32 SlotIndex,
        const RHI::FRHIPresentationLease& Lease,
        Core::FString* OutReason = nullptr);
    [[nodiscard]] RHI::ERHIResult PollPresentation(
        Core::uint64 FrameToken,
        Core::uint32 SlotIndex,
        bool& bOutRetired,
        Core::FString* OutReason = nullptr);

    // Cancellation is logical only.  An acquired or reserved target stays
    // retained until the backend reports a matching cancellation
    // acknowledgement through RetireCancelled.  This does not claim native
    // generation retirement or presentation completion.
    [[nodiscard]] RHI::ERHIResult CancelFrame(
        Core::uint64 FrameToken,
        Core::uint32 SlotIndex,
        Core::FString* OutReason = nullptr);
    [[nodiscard]] RHI::ERHIResult RetireCancelled(
        Core::uint64 FrameToken,
        Core::uint32 SlotIndex,
        Core::FString* OutReason = nullptr);

    [[nodiscard]] ELabProductionFrameState GetFrameState(
        Core::uint64 FrameToken, Core::uint32 SlotIndex) const noexcept;
    [[nodiscard]] FLabProductionFrameContextSnapshot Snapshot() const;

    // Release render resources after render completion while preserving any
    // presentation lease records.  This is also where the T027 harness
    // retires/resets its deferred submission owner. The context destructor
    // never performs blocking native cleanup.
    [[nodiscard]] RHI::ERHIResult RetireRenderResources(
        Core::uint64 FrameToken,
        Core::uint32 SlotIndex,
        Core::FString* OutReason = nullptr);
    [[nodiscard]] RHI::ERHIResult Shutdown(
        Core::FString* OutReason = nullptr) noexcept;

    // Terminal host cleanup after the device has independently completed
    // teardown. Pending presentation records are discarded without signaling
    // their fences or claiming a proven release. Never legal on an active device.
    [[nodiscard]] RHI::ERHIResult ReleaseAfterDeviceShutdown(
        RHI::ERHIShutdownAssurance Assurance, Core::FString* OutReason = nullptr) noexcept;

private:
    struct FImpl;
    Core::TUniquePtr<FImpl> Impl_;
};

} // namespace Stoner::Demo
