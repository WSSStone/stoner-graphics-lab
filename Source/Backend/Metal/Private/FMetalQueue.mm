#include "FMetalQueue.h"

#include "FMetalBlitCommandEncoder.h"
#include "FMetalCommandBuffer.h"
#include "FMetalComputeCommandEncoder.h"
#include "FMetalFailureInjector.h"
#include "FMetalRenderCommandEncoder.h"
#include "FMetalSynchronization.h"

#import <Metal/Metal.h>

#include <algorithm>
#include <new>

namespace Stoner::Backend::Metal::Private
{
namespace
{

template <typename T>
bool HasDuplicate(const Core::TArray<Core::TSharedPtr<T>>& Values) noexcept
{
    for (Core::usize Index = 0; Index < Values.size(); ++Index)
        for (Core::usize Other = 0; Other < Index; ++Other)
            if (Values[Index].get() == Values[Other].get()) return true;
    return false;
}

} // namespace

FMetalQueue::FMetalQueue(
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
    RHI::ERHIQueueType QueueType,
    void* RetainedNativeQueue,
    RHI::FRHIDeviceCapabilities Capabilities) noexcept
    : FMetalNativeObject(
          std::move(Owner), EMetalOwnershipCategory::Command),
      QueueType_(QueueType),
      NativeQueue_(RetainedNativeQueue), Capabilities_(std::move(Capabilities))
{
}

FMetalQueue::~FMetalQueue()
{
    (void)WaitIdle();
    (void)InvalidateObject();
    if (NativeQueue_) (void)(__bridge_transfer id<MTLCommandQueue>)NativeQueue_;
    NativeQueue_ = nullptr;
}

RHI::ERHIQueueType FMetalQueue::GetQueueType() const noexcept
{
    return QueueType_;
}

Core::uint32 FMetalQueue::GetSubmittedCommandBufferCount() const noexcept
{
    std::lock_guard Lock(Mutex_);
    return SubmittedCount_;
}

RHI::ERHIResult FMetalQueue::Submit(
    const Core::TSharedPtr<RHI::IRHICommandBuffer>& CommandBuffer,
    const Core::TArray<Core::TSharedPtr<RHI::IRHISemaphore>>& WaitSemaphores,
    const Core::TArray<Core::TSharedPtr<RHI::IRHISemaphore>>& SignalSemaphores,
    const Core::TSharedPtr<RHI::IRHIFence>& Fence)
{
    if (FMetalFailureInjector::ShouldFail(
            EMetalFailurePoint::CommandSubmission))
    {
        GetOwner()->RecordDiagnostic(
            Core::FString("Submit"), Core::FString("queue"),
            RHI::ERHIResult::Failed,
            Core::FString(ToStableName(
                EMetalFailurePoint::CommandSubmission)),
            0, 0, {}, Core::FString("recoverable"));
        return RHI::ERHIResult::Failed;
    }
    const auto Commands =
        std::dynamic_pointer_cast<FMetalCommandBuffer>(CommandBuffer);
    if (!IsCompatible(GetOwner()) || !Commands ||
        !Commands->IsCompatibleWith(GetOwner()) ||
        Commands->GetCompatibleQueueType() != QueueType_ ||
        Commands->GetState() != RHI::ERHICommandBufferState::Completed ||
        HasDuplicate(WaitSemaphores) || HasDuplicate(SignalSemaphores))
        return RHI::ERHIResult::InvalidState;

    Core::TArray<Core::TSharedPtr<FMetalSemaphore>> Waits;
    Core::TArray<Core::TSharedPtr<FMetalSemaphore>> Signals;
    Core::TArray<Core::uint64> WaitEpochs;
    try
    {
        for (const auto& Base : WaitSemaphores)
        {
            auto Value = std::dynamic_pointer_cast<FMetalSemaphore>(Base);
            if (!Value || !Value->IsCompatible(GetOwner()))
                return RHI::ERHIResult::InvalidState;
            Waits.push_back(std::move(Value));
        }
        for (const auto& Base : SignalSemaphores)
        {
            if (std::find(WaitSemaphores.begin(), WaitSemaphores.end(), Base) !=
                WaitSemaphores.end())
                return RHI::ERHIResult::InvalidState;
            auto Value = std::dynamic_pointer_cast<FMetalSemaphore>(Base);
            if (!Value || !Value->CanSignalForSubmission(GetOwner()))
                return RHI::ERHIResult::InvalidState;
            Signals.push_back(std::move(Value));
        }
    }
    catch (const std::bad_alloc&)
    {
        return RHI::ERHIResult::Failed;
    }
    const auto NativeFence = std::dynamic_pointer_cast<FMetalFence>(Fence);
    if (Fence && (!NativeFence ||
        !NativeFence->CanSignalForSubmission(GetOwner())))
        return RHI::ERHIResult::InvalidState;
    try
    {
        WaitEpochs.reserve(Waits.size());
        for (const auto& Wait : Waits)
        {
            const Core::uint64 Epoch = Wait->ReserveSubmissionWait();
            if (Epoch == 0)
            {
                for (Core::usize Index = 0; Index < WaitEpochs.size(); ++Index)
                    Waits[Index]->CancelSubmissionWait(WaitEpochs[Index]);
                return RHI::ERHIResult::NotReady;
            }
            WaitEpochs.push_back(Epoch);
        }
    }
    catch (const std::bad_alloc&)
    {
        for (Core::usize Index = 0; Index < WaitEpochs.size(); ++Index)
            Waits[Index]->CancelSubmissionWait(WaitEpochs[Index]);
        return RHI::ERHIResult::Failed;
    }
    if (!GetOwner()->TryBeginSubmission())
    {
        for (Core::usize Index = 0; Index < WaitEpochs.size(); ++Index)
            Waits[Index]->CancelSubmissionWait(WaitEpochs[Index]);
        return RHI::ERHIResult::InvalidState;
    }

    Core::TArray<FMetalCommandRecord> Records;
    if (!Commands->PrepareSubmission(Records))
    {
        for (Core::usize Index = 0; Index < WaitEpochs.size(); ++Index)
            Waits[Index]->CancelSubmissionWait(WaitEpochs[Index]);
        GetOwner()->EndSubmission();
        return RHI::ERHIResult::InvalidState;
    }

    @autoreleasepool
    {
        id<MTLCommandQueue> Queue = (__bridge id<MTLCommandQueue>)NativeQueue_;
        id<MTLCommandBuffer> Native = [Queue commandBuffer];
        if (!Native)
        {
            for (Core::usize Index = 0; Index < WaitEpochs.size(); ++Index)
                Waits[Index]->CancelSubmissionWait(WaitEpochs[Index]);
            Commands->CompleteSubmission(); GetOwner()->EndSubmission();
            return RHI::ERHIResult::Failed;
        }
        for (Core::usize Index = 0; Index < Waits.size(); ++Index)
            Waits[Index]->EncodeSubmissionWait(
                (__bridge void*)Native, WaitEpochs[Index]);

        for (Core::usize Index = 0; Index < Records.size();)
        {
            Core::usize Consumed = 0;
            RHI::ERHIResult Result = RHI::ERHIResult::InvalidState;
            if (Records[Index].Type ==
                RHI::ERHISymbolicCommandType::BeginRenderPass)
                Result = EncodeMetalRenderCommands(
                    (__bridge void*)Native,
                    std::span(Records).subspan(Index), Consumed);
            else if (Records[Index].Type ==
                RHI::ERHISymbolicCommandType::BindComputePipeline)
                Result = EncodeMetalComputeCommands(
                    (__bridge void*)Native,
                    std::span(Records).subspan(Index), Capabilities_, Consumed);
            else
            {
                Result = EncodeMetalBlitCommand(
                    (__bridge void*)Native, Records[Index]);
                Consumed = 1;
            }
            if (Result != RHI::ERHIResult::Success || Consumed == 0)
            {
                for (Core::usize WaitIndex = 0;
                     WaitIndex < WaitEpochs.size(); ++WaitIndex)
                    Waits[WaitIndex]->CancelSubmissionWait(
                        WaitEpochs[WaitIndex]);
                Commands->CompleteSubmission(); GetOwner()->EndSubmission();
                return Result;
            }
            Index += Consumed;
        }

        Core::TArray<Core::uint64> SignalEpochs;
        try
        {
            SignalEpochs.reserve(Signals.size());
            for (const auto& Signal : Signals)
            {
                const Core::uint64 Epoch = Signal->ReserveSubmissionSignal();
                if (Epoch == 0) throw std::length_error("signal-reservation");
                SignalEpochs.push_back(Epoch);
            }
        }
        catch (const std::bad_alloc&)
        {
            for (Core::usize Index = 0; Index < WaitEpochs.size(); ++Index)
                Waits[Index]->CancelSubmissionWait(WaitEpochs[Index]);
            Commands->CompleteSubmission(); GetOwner()->EndSubmission();
            return RHI::ERHIResult::Failed;
        }
        catch (const std::length_error&)
        {
            for (Core::usize Index = 0; Index < SignalEpochs.size(); ++Index)
                Signals[Index]->CompleteSubmissionSignal(
                    SignalEpochs[Index], false);
            for (Core::usize Index = 0; Index < WaitEpochs.size(); ++Index)
                Waits[Index]->CancelSubmissionWait(WaitEpochs[Index]);
            Commands->CompleteSubmission(); GetOwner()->EndSubmission();
            return RHI::ERHIResult::InvalidState;
        }
        const Core::uint64 FenceEpoch = NativeFence
            ? NativeFence->ReserveSubmissionSignal() : 0;
        if (NativeFence && FenceEpoch == 0)
        {
            for (Core::usize Index = 0; Index < SignalEpochs.size(); ++Index)
                Signals[Index]->CompleteSubmissionSignal(
                    SignalEpochs[Index], false);
            for (Core::usize Index = 0; Index < WaitEpochs.size(); ++Index)
                Waits[Index]->CancelSubmissionWait(WaitEpochs[Index]);
            Commands->CompleteSubmission(); GetOwner()->EndSubmission();
            return RHI::ERHIResult::InvalidState;
        }
        for (Core::usize Index = 0; Index < Signals.size(); ++Index)
            Signals[Index]->EncodeSubmissionSignal(
                (__bridge void*)Native, SignalEpochs[Index]);
        if (NativeFence)
            NativeFence->EncodeSubmissionSignal(
                (__bridge void*)Native, FenceEpoch);

        Core::TSharedPtr<FMetalSubmission> Submission;
        try
        {
            Submission = Core::MakeShared<FMetalSubmission>(
                GetOwner(), Commands, std::move(Records), Waits, Signals,
                SignalEpochs, NativeFence, FenceEpoch);
            std::lock_guard Lock(Mutex_);
            Submissions_.push_back(Submission);
            ++SubmittedCount_;
        }
        catch (const std::bad_alloc&)
        {
            for (Core::usize Index = 0; Index < Signals.size(); ++Index)
                Signals[Index]->CompleteSubmissionSignal(
                    SignalEpochs[Index], false);
            if (NativeFence)
                NativeFence->CompleteSubmissionSignal(FenceEpoch, false);
            for (Core::usize Index = 0; Index < WaitEpochs.size(); ++Index)
                Waits[Index]->CancelSubmissionWait(WaitEpochs[Index]);
            Commands->CompleteSubmission(); GetOwner()->EndSubmission();
            return RHI::ERHIResult::Failed;
        }
        [Native addCompletedHandler:^(id<MTLCommandBuffer> Buffer) {
            const bool bSucceeded = Buffer.status == MTLCommandBufferStatusCompleted &&
                Buffer.error == nil;
            Submission->Complete(bSucceeded);
        }];
        for (Core::usize Index = 0; Index < Waits.size(); ++Index)
            Waits[Index]->CommitSubmissionWait(WaitEpochs[Index]);
        [Native commit];
        PruneCompleted();
        return RHI::ERHIResult::Success;
    }
}

RHI::ERHIResult FMetalQueue::WaitIdle()
{
    Core::TArray<Core::TSharedPtr<FMetalSubmission>> Snapshot;
    try
    {
        std::lock_guard Lock(Mutex_);
        Snapshot = Submissions_;
    }
    catch (const std::bad_alloc&)
    {
        return RHI::ERHIResult::Failed;
    }
    RHI::ERHIResult Result = RHI::ERHIResult::Success;
    for (const auto& Submission : Snapshot)
    {
        const auto WaitResult = Submission->Wait();
        if (WaitResult != RHI::ERHIResult::Success) Result = WaitResult;
    }
    PruneCompleted();
    return Result;
}

void FMetalQueue::PruneCompleted() noexcept
{
    std::lock_guard Lock(Mutex_);
    Submissions_.erase(
        std::remove_if(Submissions_.begin(), Submissions_.end(),
            [](const auto& Submission) {
                return Submission && Submission->IsComplete();
            }),
        Submissions_.end());
}

} // namespace Stoner::Backend::Metal::Private
