#include "Core/SGPlatform.h"
#include "Renderer/FOutputTransformPlan.h"
#include "RHI/RHIMinimal.h"

#if SG_PLATFORM_MAC
// These are production Metal ownership/completion types.  The test leaves the
// native event/queue null and drives the same completion path that Metal's
// command-buffer handler uses; it therefore proves retention and timeout
// behavior without claiming a native device or presentation result.
#include "../Source/Backend/Metal/Private/FMetalCommandBuffer.h"
#include "../Source/Backend/Metal/Private/FMetalDeviceOwnerState.h"
#include "../Source/Backend/Metal/Private/FMetalQueue.h"
#include "../Source/Backend/Metal/Private/FMetalSubmission.h"
#include "../Source/Backend/Metal/Private/FMetalSynchronization.h"
#endif

#include <iostream>
#include <memory>
#include <mutex>
#include <type_traits>

#if SG_PLATFORM_MAC
namespace Stoner::Backend::Metal::Private
{

struct FMetalQueueDeferredTestAccess
{
    static bool Reserve(FMetalQueue& Queue) noexcept
    {
        return Queue.TryReserveDeferredSlot();
    }

    static void Release(FMetalQueue& Queue) noexcept
    {
        Queue.ReleaseDeferredSlot();
    }

    static bool Attach(
        FMetalQueue& Queue,
        const Core::TSharedPtr<FMetalSubmission>& Submission) noexcept
    {
        try
        {
            std::lock_guard Lock(Queue.Mutex_);
            Queue.Submissions_.push_back(Submission);
            ++Queue.SubmittedCount_;
            return true;
        }
        catch (const std::bad_alloc&)
        {
            return false;
        }
    }
};

} // namespace Stoner::Backend::Metal::Private
#endif

namespace
{

using namespace Stoner::Core;
using namespace Stoner::RHI;

struct FDeferredSubmissionTestState
{
    int Failed = 0;
};

void Record(
    FDeferredSubmissionTestState& State,
    bool bPassed,
    const char* Name)
{
    if (!bPassed)
    {
        ++State.Failed;
    }
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

class FContractFence final : public IRHIFence
{
public:
    [[nodiscard]] ERHIFenceState GetState() const noexcept override
    {
        return State_;
    }

    [[nodiscard]] bool IsSignaled() const noexcept override
    {
        return State_ == ERHIFenceState::Signaled ||
            State_ == ERHIFenceState::Waited;
    }

    ERHIResult Wait(uint64 TimeoutMicroseconds = 0) override
    {
        if (!IsSignaled())
        {
            return TimeoutMicroseconds == 0
                ? ERHIResult::NotReady
                : ERHIResult::Timeout;
        }
        State_ = ERHIFenceState::Waited;
        return ERHIResult::Success;
    }

    ERHIResult Reset() override
    {
        State_ = ERHIFenceState::Unsignaled;
        return ERHIResult::Success;
    }

    ERHIResult Signal() override
    {
        State_ = ERHIFenceState::Signaled;
        return ERHIResult::Success;
    }

private:
    ERHIFenceState State_ = ERHIFenceState::Unsignaled;
};

class FContractTexture final : public IRHITexture
{
public:
    FContractTexture()
    {
        Desc_.Width = 640;
        Desc_.Height = 360;
        Desc_.Format = ERHIFormat::R8G8B8A8_UNorm;
        Desc_.Usage = ERHITextureUsage::ColorAttachment |
            ERHITextureUsage::Present;
    }

    [[nodiscard]] const FRHITextureDesc& GetDesc() const noexcept override
    {
        return Desc_;
    }

    [[nodiscard]] ERHITextureDimension GetDimension() const noexcept override
    {
        return Desc_.Dimension;
    }

    [[nodiscard]] ERHIFormat GetFormat() const noexcept override
    {
        return Desc_.Format;
    }

    [[nodiscard]] ERHITextureUsage GetUsage() const noexcept override
    {
        return Desc_.Usage;
    }

    [[nodiscard]] ERHIResourceLifecycleState GetLifecycleState()
        const noexcept override
    {
        return State_;
    }

    ERHIResult Invalidate() override
    {
        State_ = ERHIResourceLifecycleState::Invalidated;
        return ERHIResult::Success;
    }

private:
    FRHITextureDesc Desc_;
    ERHIResourceLifecycleState State_ =
        ERHIResourceLifecycleState::Valid;
};

FRHIPresentationFrame MakeFrame(uint32 ImageIndex = 3)
{
    FRHIPresentationFrame Frame;
    Frame.FrameToken = 101;
    Frame.ModeGeneration = 7;
    Frame.SwapchainImageGeneration = 11;
    Frame.ImageIndex = ImageIndex;
    Frame.Width = 640;
    Frame.Height = 360;
    Frame.Format = ERHIFormat::R8G8B8A8_UNorm;
    Frame.ColorSpace = ERHIPresentationColorSpace::SrgbNonlinear;
    Frame.DisplayAdaptation = ERHIPresentationDisplayAdaptation::None;
    Frame.MetadataDigest = "deferred-submission-test";
    return Frame;
}

void TestCapabilityAndFormalDefaults(
    FDeferredSubmissionTestState& State)
{
    const FRHIDeviceCapabilities DeviceCapabilities;
    const FRHIPresentationCapabilities PresentationCapabilities;
    const Stoner::Renderer::FOutputTransformPlan DefaultPlan;

    Record(State,
        !DeviceCapabilities.bSupportsDeferredSubmission &&
            !PresentationCapabilities.bSupportsIndependentPresentationCompletion,
        "deferred and independent-presentation capabilities default unsupported");

    Record(State,
        !PresentationCapabilities.bSupportsIndependentPresentationCompletion,
        "absent independent presentation completion remains unsupported");

    Record(State,
        DefaultPlan.ExecutionPurpose ==
                Stoner::Renderer::EFrameExecutionPurpose::FormalValidation &&
            DefaultPlan.ReadbackSelection ==
                Stoner::Renderer::EFrameReadbackSelection::Formal,
        "formal output execution and readback remain the default");
}

void TestBorrowedTargetAndIndependentLeases(
    FDeferredSubmissionTestState& State)
{
    const FRHIPresentationFrame Frame = MakeFrame();
    const TSharedPtr<IRHITexture> Texture = MakeShared<FContractTexture>();
    const TSharedPtr<IRHIFence> CompletionFence =
        MakeShared<FContractFence>();

    FRHIBorrowedAcquiredTarget SlotZero;
    SlotZero.Texture = Texture;
    SlotZero.Frame = Frame;
    SlotZero.FrameSlotIndex = 0;

    FRHIBorrowedAcquiredTarget SlotOne = SlotZero;
    SlotOne.FrameSlotIndex = 1;

    FRHIRenderLease RenderLease;
    RenderLease.Frame = Frame;
    RenderLease.FrameSlotIndex = 0;
    RenderLease.CompletionFence = CompletionFence;

    FRHIPresentationLease PresentationLease;
    PresentationLease.Frame = Frame;
    PresentationLease.PresentationCompletionFence = CompletionFence;

    Record(State,
        SlotZero.IsValid() && SlotOne.IsValid() &&
            RenderLease.IsValid() && PresentationLease.IsValid(),
        "valid acquired target has independent render and presentation leases");
    Record(State,
        RenderLease.Matches(SlotZero) && !RenderLease.Matches(SlotOne) &&
            PresentationLease.Matches(SlotZero) &&
            PresentationLease.Matches(SlotOne),
        "render lease keeps slot identity while presentation lease is image based");

    FRHIBorrowedAcquiredTarget StaleTarget = SlotZero;
    ++StaleTarget.Frame.ModeGeneration;
    Record(State,
        !SlotZero.Matches(StaleTarget) &&
            !PresentationLease.Matches(StaleTarget) &&
            !RenderLease.Matches(StaleTarget),
        "stale acquired provenance fails target and lease matching");

    FRHIBorrowedAcquiredTarget UnrelatedTextureTarget = SlotZero;
    UnrelatedTextureTarget.Texture = MakeShared<FContractTexture>();
    Record(State,
        !SlotZero.Matches(UnrelatedTextureTarget),
        "borrowed target rejects an unrelated texture with equal metadata");

    FRHIBorrowedAcquiredTarget OutOfRangeTarget = SlotZero;
    OutOfRangeTarget.Frame.ImageIndex = MaxRHIPresentationImageLeases;
    Record(State,
        !OutOfRangeTarget.IsValid(),
        "borrowed target rejects an image lease index beyond the bound");
}

#if SG_PLATFORM_MAC
void TestMetalDelayedRenderAndPresentationCompletion(
    FDeferredSubmissionTestState& State)
{
    using namespace Stoner::Backend::Metal::Private;

    const auto Owner = MakeShared<FMetalDeviceOwnerState>(1801);
    auto Commands = MakeShared<FMetalCommandBuffer>(
        Owner, ERHIQueueType::Graphics);
    const bool bRecorded =
        Commands->Begin() == ERHIResult::Success &&
        Commands->RecordBarrier() == ERHIResult::Success &&
        Commands->End() == ERHIResult::Success;
    TArray<FMetalCommandRecord> Records;
    const bool bPrepared = bRecorded && Commands->PrepareSubmission(Records);

    auto RenderFence = MakeShared<FMetalFence>(Owner, nullptr, false);
    const uint64 RenderEpoch = RenderFence->ReserveSubmissionSignal();
    const auto WeakCommands = TWeakPtr<FMetalCommandBuffer>(Commands);
    const auto WeakRenderFence = TWeakPtr<FMetalFence>(RenderFence);

    const FRHIPresentationFrame Frame = MakeFrame();
    FRHIRenderLease RenderLease;
    RenderLease.Frame = Frame;
    RenderLease.FrameSlotIndex = 0;
    RenderLease.CompletionFence = RenderFence;

    auto PresentationFence = MakeShared<FMetalFence>(Owner, nullptr, false);
    auto RenderFinishedSemaphore = MakeShared<FMetalSemaphore>(Owner, nullptr);
    const auto WeakPresentationFence =
        TWeakPtr<FMetalFence>(PresentationFence);
    const auto WeakRenderFinishedSemaphore =
        TWeakPtr<FMetalSemaphore>(RenderFinishedSemaphore);
    FRHIPresentationLease PresentationLease;
    PresentationLease.Frame = Frame;
    PresentationLease.RenderFinishedSemaphore = RenderFinishedSemaphore;
    PresentationLease.PresentationCompletionFence = PresentationFence;

    const bool bAdmission = Owner->TryBeginSubmission();
    FMetalSubmission Submission(
        Owner, Commands, std::move(Records), {}, {}, {},
        RenderFence, RenderEpoch);
    Commands.reset();
    RenderFence.reset();
    PresentationFence.reset();
    RenderFinishedSemaphore.reset();

    auto PendingCommand = WeakCommands.lock();
    const bool bPending = bAdmission && bPrepared &&
        !Submission.IsComplete() &&
        Submission.Wait(1000) == ERHIResult::Timeout &&
        PendingCommand &&
        PendingCommand->GetState() == ERHICommandBufferState::Submitted &&
        !WeakRenderFence.expired() &&
        Owner->Inspect().InFlightSubmissionCount == 1;
    PendingCommand.reset();
    RenderLease.CompletionFence.reset();
    Record(State, bPending,
        "Metal production submission remains pending after finite timeout");

    auto CompletedCommand = WeakCommands.lock();
    auto CompletedFence = WeakRenderFence.lock();
    Submission.Complete(true);
    const bool bRenderCompleted =
        Submission.IsComplete() &&
        Submission.Wait(1000) == ERHIResult::Success &&
        CompletedCommand &&
        CompletedCommand->GetState() == ERHICommandBufferState::Resettable &&
        CompletedFence && CompletedFence->IsSignaled() &&
        CompletedFence->Wait(1000) == ERHIResult::Success &&
        Owner->Inspect().InFlightSubmissionCount == 0;
    Record(State, bRenderCompleted,
        "Metal production completion signals render fence and releases submission ownership");
    CompletedCommand.reset();
    CompletedFence.reset();

    const bool bPresentationStillPending =
        PresentationLease.IsValid() &&
        WeakCommands.expired() && WeakRenderFence.expired() &&
        !WeakPresentationFence.expired() &&
        !WeakRenderFinishedSemaphore.expired() &&
        PresentationLease.PresentationCompletionFence->Wait(1000) ==
            ERHIResult::Timeout;
    Record(State, bPresentationStillPending,
        "render completion leaves the independent presentation lease pending");

    const bool bPresentationCompleted =
        PresentationLease.PresentationCompletionFence->Signal() ==
            ERHIResult::Success &&
        PresentationLease.PresentationCompletionFence->Wait(1000) ==
            ERHIResult::Success;
    PresentationLease = {};
    Record(State,
        bPresentationCompleted && WeakPresentationFence.expired() &&
            WeakRenderFinishedSemaphore.expired(),
        "presentation completion independently releases image lease ownership");
}

void TestMetalFailedCompletion(
    FDeferredSubmissionTestState& State)
{
    using namespace Stoner::Backend::Metal::Private;

    const auto Owner = MakeShared<FMetalDeviceOwnerState>(1802);
    auto Commands = MakeShared<FMetalCommandBuffer>(
        Owner, ERHIQueueType::Graphics);
    const bool bRecorded =
        Commands->Begin() == ERHIResult::Success &&
        Commands->RecordBarrier() == ERHIResult::Success &&
        Commands->End() == ERHIResult::Success;
    TArray<FMetalCommandRecord> Records;
    const bool bPrepared = bRecorded && Commands->PrepareSubmission(Records);

    auto Fence = MakeShared<FMetalFence>(Owner, nullptr, false);
    const uint64 FenceEpoch = Fence->ReserveSubmissionSignal();
    const auto WeakCommands = TWeakPtr<FMetalCommandBuffer>(Commands);
    const auto WeakFence = TWeakPtr<FMetalFence>(Fence);
    auto FailureFence = Fence;
    const bool bAdmission = Owner->TryBeginSubmission();
    FMetalSubmission Submission(
        Owner, Commands, std::move(Records), {}, {}, {}, Fence, FenceEpoch);
    Commands.reset();
    Fence.reset();

    auto PendingCommand = WeakCommands.lock();
    const bool bRetainedBeforeCompletion = bAdmission && bPrepared &&
        !Submission.IsComplete() &&
        Submission.Wait(1000) == ERHIResult::Timeout &&
        FailureFence->Wait(0) == ERHIResult::NotReady &&
        PendingCommand && !WeakFence.expired() &&
        Owner->Inspect().InFlightSubmissionCount == 1;
    PendingCommand.reset();
    Record(State, bRetainedBeforeCompletion,
        "failed Metal submission retains production owners through timeout");

    Submission.Complete(false);
    const auto Inspection = Owner->Inspect();
    Record(State,
        Submission.IsComplete() &&
            Submission.Wait(1000) == ERHIResult::Failed &&
            FailureFence->GetState() == ERHIFenceState::Unsignaled &&
            !FailureFence->IsSignaled() &&
            FailureFence->Wait(0) == ERHIResult::Failed &&
            FailureFence->Wait(1000) == ERHIResult::Failed &&
            FailureFence->ReserveSubmissionSignal() == 0 &&
            FailureFence->Signal() == ERHIResult::InvalidState &&
            Inspection.bTerminalFailure &&
            Inspection.TerminalFailureReason ==
                "metal-command-buffer-failed" &&
            Inspection.InFlightSubmissionCount == 0 &&
            WeakCommands.expired(),
        "failed Metal completion reports failure and releases retained owners");

    const uint64 ReuseEpoch = FailureFence->Reset() == ERHIResult::Success
        ? FailureFence->ReserveSubmissionSignal() : 0;
    FailureFence->CompleteSubmissionSignal(ReuseEpoch, true);
    Record(State,
        ReuseEpoch != 0 && FailureFence->IsSignaled() &&
            FailureFence->Wait(1000) == ERHIResult::Success &&
            FailureFence->GetState() == ERHIFenceState::Waited,
        "explicit fence reset clears terminal failure and permits reuse");
    FailureFence.reset();
    Record(State, WeakFence.expired(),
        "failed submission fence ownership releases after caller drops its reference");
}
#else
void TestMetalDelayedRenderAndPresentationCompletion(
    FDeferredSubmissionTestState& State)
{
    // The production FMetalSubmission/FMetalFence implementation is
    // macOS-only. Public contract tests above still run on every platform;
    // native Metal evidence is intentionally not synthesized on other hosts.
    (void)State;
}

void TestMetalFailedCompletion(
    FDeferredSubmissionTestState& State)
{
    (void)State;
}
#endif

#if SG_PLATFORM_MAC
void TestMetalQueueDeferredAdmission(
    FDeferredSubmissionTestState& State)
{
    using namespace Stoner::Backend::Metal::Private;

    const auto Owner = MakeShared<FMetalDeviceOwnerState>(1803);
    FRHIDeviceCapabilities Capabilities;
    Capabilities.bSupportsDeferredSubmission = true;
    FMetalQueue Queue(
        Owner, ERHIQueueType::Graphics, nullptr, Capabilities);
    auto Commands = MakeShared<FMetalCommandBuffer>(
        Owner, ERHIQueueType::Graphics);
    const bool bRecorded =
        Commands->Begin() == ERHIResult::Success &&
        Commands->RecordBarrier() == ERHIResult::Success &&
        Commands->End() == ERHIResult::Success;
    auto Fence = MakeShared<FMetalFence>(Owner, nullptr, false);
    auto Signal = MakeShared<FMetalSemaphore>(Owner, nullptr);
    const bool bReservedFirst =
        FMetalQueueDeferredTestAccess::Reserve(Queue);
    const bool bReservedSecond =
        FMetalQueueDeferredTestAccess::Reserve(Queue);
    const auto Result = Queue.SubmitDeferred(
        Commands, {}, {Signal}, Fence);
    const auto FullInspection = Queue.Inspect();
    const bool bRejectedWithoutMutation = bRecorded &&
        bReservedFirst && bReservedSecond &&
        Result == ERHIResult::NotReady &&
        Commands->GetState() == ERHICommandBufferState::Completed &&
        Fence->GetState() == ERHIFenceState::Unsignaled &&
        Signal->GetState() == ERHISemaphoreState::Unsignaled &&
        FullInspection.AcceptedCount == 0 &&
        FullInspection.PendingCount == 2 &&
        FullInspection.RenderCompletedCount == 0;
    Record(State, bRejectedWithoutMutation,
        "full deferred admission rejects before mutating command or sync state");

    FMetalQueueDeferredTestAccess::Release(Queue);
    const bool bReused = FMetalQueueDeferredTestAccess::Reserve(Queue);
    const auto ReusedInspection = Queue.Inspect();
    Record(State,
        bReused && ReusedInspection.AcceptedCount == 0 &&
            ReusedInspection.PendingCount == 2,
        "deferred admission reuses a released lab slot without accepting work");

    FMetalQueueDeferredTestAccess::Release(Queue);
    FMetalQueueDeferredTestAccess::Release(Queue);
    Record(State, Queue.Inspect().PendingCount == 0,
        "deferred admission reservations release back to an empty queue");
}

void TestMetalQueueCompletionInspection(
    FDeferredSubmissionTestState& State)
{
    using namespace Stoner::Backend::Metal::Private;

    const auto Owner = MakeShared<FMetalDeviceOwnerState>(1804);
    FRHIDeviceCapabilities Capabilities;
    Capabilities.bSupportsDeferredSubmission = true;
    FMetalQueue Queue(
        Owner, ERHIQueueType::Graphics, nullptr, Capabilities);
    auto Commands = MakeShared<FMetalCommandBuffer>(
        Owner, ERHIQueueType::Graphics);
    const bool bRecorded =
        Commands->Begin() == ERHIResult::Success &&
        Commands->RecordBarrier() == ERHIResult::Success &&
        Commands->End() == ERHIResult::Success;
    TArray<FMetalCommandRecord> Records;
    const bool bPrepared = bRecorded && Commands->PrepareSubmission(Records);
    auto Fence = MakeShared<FMetalFence>(Owner, nullptr, false);
    const uint64 FenceEpoch = Fence->ReserveSubmissionSignal();
    const bool bAdmission = Owner->TryBeginSubmission();
    auto Submission = MakeShared<FMetalSubmission>(
        Owner, Commands, std::move(Records),
        TArray<TSharedPtr<FMetalSemaphore>>{},
        TArray<TSharedPtr<FMetalSemaphore>>{},
        TArray<uint64>{}, Fence, FenceEpoch);
    const bool bAttached = bPrepared && bAdmission &&
        FMetalQueueDeferredTestAccess::Attach(Queue, Submission);
    const auto PendingInspection = Queue.Inspect();
    Submission->Complete(true);
    const auto CompletedInspection = Queue.Inspect();
    Record(State,
        bAttached && PendingInspection.AcceptedCount == 1 &&
            PendingInspection.PendingCount == 1 &&
            PendingInspection.RenderCompletedCount == 0 &&
            CompletedInspection.AcceptedCount == 1 &&
            CompletedInspection.PendingCount == 0 &&
            CompletedInspection.RenderCompletedCount == 1,
        "queue inspection separates accepted pending and actual render completion");
}

void TestMetalQueueHasProductionDeferredOverride(
    FDeferredSubmissionTestState& State)
{
    using namespace Stoner::Backend::Metal::Private;
    using FDeferredSubmit = ERHIResult (FMetalQueue::*)(
        const TSharedPtr<IRHICommandBuffer>&,
        const TArray<TSharedPtr<IRHISemaphore>>&,
        const TArray<TSharedPtr<IRHISemaphore>>&,
        const TSharedPtr<IRHIFence>&);

    // &FMetalQueue::SubmitDeferred has the base-member type until T018 adds
    // the real override. This is a compile-time production-seam check rather
    // than a queue double that could pass without exercising Metal ownership.
    Record(State,
        std::is_same_v<decltype(&FMetalQueue::SubmitDeferred), FDeferredSubmit>,
        "Metal queue exposes a production SubmitDeferred override");
}
#endif

} // namespace

int RunRHIDeferredSubmissionTests()
{
    FDeferredSubmissionTestState State;
    TestCapabilityAndFormalDefaults(State);
    TestBorrowedTargetAndIndependentLeases(State);
    TestMetalDelayedRenderAndPresentationCompletion(State);
    TestMetalFailedCompletion(State);

#if SG_PLATFORM_MAC
    TestMetalQueueDeferredAdmission(State);
    TestMetalQueueCompletionInspection(State);
    TestMetalQueueHasProductionDeferredOverride(State);
#endif

    return State.Failed;
}
