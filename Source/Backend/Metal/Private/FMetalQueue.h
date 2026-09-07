#pragma once

#include "FMetalNativeObject.h"
#include "FMetalSubmission.h"
#include "RHI/FRHIDeviceCapabilities.h"
#include "RHI/IRHICommandQueue.h"

#include <mutex>

namespace Stoner::Backend::Metal::Private
{

struct FMetalQueueInspection
{
    Core::uint64 AcceptedCount = 0;
    // Includes admission reservations that have not yet become submissions.
    Core::uint64 PendingCount = 0;
    // Successful render completions only; failures are reported by the owner.
    Core::uint64 RenderCompletedCount = 0;
};

class FMetalQueue final
    : public RHI::IRHICommandQueue,
      public FMetalNativeObject
{
public:
    FMetalQueue(
        Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
        RHI::ERHIQueueType QueueType,
        void* RetainedNativeQueue,
        RHI::FRHIDeviceCapabilities Capabilities) noexcept;
    ~FMetalQueue() override;

    [[nodiscard]] RHI::ERHIQueueType GetQueueType() const noexcept override;
    [[nodiscard]] Core::uint32 GetSubmittedCommandBufferCount()
        const noexcept override;
    [[nodiscard]] FMetalQueueInspection Inspect() noexcept;
    RHI::ERHIResult Submit(
        const Core::TSharedPtr<RHI::IRHICommandBuffer>& CommandBuffer,
        const Core::TArray<Core::TSharedPtr<RHI::IRHISemaphore>>& WaitSemaphores,
        const Core::TArray<Core::TSharedPtr<RHI::IRHISemaphore>>& SignalSemaphores,
        const Core::TSharedPtr<RHI::IRHIFence>& Fence) override;
    RHI::ERHIResult SubmitDeferred(
        const Core::TSharedPtr<RHI::IRHICommandBuffer>& CommandBuffer,
        const Core::TArray<Core::TSharedPtr<RHI::IRHISemaphore>>& WaitSemaphores,
        const Core::TArray<Core::TSharedPtr<RHI::IRHISemaphore>>& SignalSemaphores,
        const Core::TSharedPtr<RHI::IRHIFence>& CompletionFence) override;
    RHI::ERHIResult WaitIdle() override;

private:
    friend struct FMetalQueueDeferredTestAccess;

    RHI::ERHIResult SubmitInternal(
        const Core::TSharedPtr<RHI::IRHICommandBuffer>& CommandBuffer,
        const Core::TArray<Core::TSharedPtr<RHI::IRHISemaphore>>& WaitSemaphores,
        const Core::TArray<Core::TSharedPtr<RHI::IRHISemaphore>>& SignalSemaphores,
        const Core::TSharedPtr<RHI::IRHIFence>& Fence,
        bool bDeferred);
    void PruneCompleted() noexcept;
    void PruneCompletedLocked() noexcept;
    [[nodiscard]] bool TryReserveDeferredSlot() noexcept;
    void ReleaseDeferredSlot() noexcept;

    mutable std::mutex Mutex_;
    RHI::ERHIQueueType QueueType_;
    void* NativeQueue_ = nullptr;
    RHI::FRHIDeviceCapabilities Capabilities_;
    Core::uint32 SubmittedCount_ = 0;
    Core::uint32 DeferredPendingReservations_ = 0;
    Core::uint64 RenderCompletedCount_ = 0;
    Core::TArray<Core::TSharedPtr<FMetalSubmission>> Submissions_;
    Core::TArray<Core::TSharedPtr<FMetalSubmission>> DeferredSubmissions_;
};

} // namespace Stoner::Backend::Metal::Private
