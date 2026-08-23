#include "FMetalSubmission.h"

#include <chrono>

namespace Stoner::Backend::Metal::Private
{

FMetalSubmission::FMetalSubmission(
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
    Core::TSharedPtr<FMetalCommandBuffer> CommandBuffer,
    Core::TArray<FMetalCommandRecord> Records,
    Core::TArray<Core::TSharedPtr<FMetalSemaphore>> WaitSemaphores,
    Core::TArray<Core::TSharedPtr<FMetalSemaphore>> SignalSemaphores,
    Core::TArray<Core::uint64> SignalEpochs,
    Core::TSharedPtr<FMetalFence> Fence,
    Core::uint64 FenceEpoch) noexcept
    : Owner_(std::move(Owner)), CommandBuffer_(std::move(CommandBuffer)),
      Records_(std::move(Records)),
      WaitSemaphores_(std::move(WaitSemaphores)),
      SignalSemaphores_(std::move(SignalSemaphores)),
      SignalEpochs_(std::move(SignalEpochs)), Fence_(std::move(Fence)),
      FenceEpoch_(FenceEpoch)
{
}

void FMetalSubmission::Complete(bool bSucceeded) noexcept
{
    std::lock_guard Lock(Mutex_);
    if (bComplete_) return;
    bComplete_ = true;
    bSucceeded_ = bSucceeded;
    for (Core::usize Index = 0; Index < SignalSemaphores_.size(); ++Index)
        SignalSemaphores_[Index]->CompleteSubmissionSignal(
            SignalEpochs_[Index], bSucceeded);
    if (Fence_) Fence_->CompleteSubmissionSignal(FenceEpoch_, bSucceeded);
    if (CommandBuffer_) CommandBuffer_->CompleteSubmission();
    if (Owner_)
    {
        if (!bSucceeded_)
            Owner_->RecordTerminalFailure(Core::FString("metal-command-buffer-failed"));
        Owner_->EndSubmission();
    }
    Records_.clear();
    WaitSemaphores_.clear();
    SignalSemaphores_.clear();
    SignalEpochs_.clear();
    CommandBuffer_.reset();
    Fence_.reset();
    FenceEpoch_ = 0;
    Condition_.notify_all();
}

RHI::ERHIResult FMetalSubmission::Wait(Core::uint64 TimeoutMicroseconds) noexcept
{
    std::unique_lock Lock(Mutex_);
    if (!bComplete_)
    {
        if (TimeoutMicroseconds == 0)
            Condition_.wait(Lock, [this] { return bComplete_; });
        else if (!Condition_.wait_for(
            Lock, std::chrono::microseconds(TimeoutMicroseconds),
            [this] { return bComplete_; }))
            return RHI::ERHIResult::Timeout;
    }
    return bSucceeded_ ? RHI::ERHIResult::Success : RHI::ERHIResult::Failed;
}

bool FMetalSubmission::IsComplete() const noexcept
{
    std::lock_guard Lock(Mutex_);
    return bComplete_;
}

} // namespace Stoner::Backend::Metal::Private
