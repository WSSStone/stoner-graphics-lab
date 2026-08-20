#pragma once

#include "FMetalNativeObject.h"
#include "FMetalSubmission.h"
#include "RHI/FRHIDeviceCapabilities.h"
#include "RHI/IRHICommandQueue.h"

#include <mutex>

namespace Stoner::Backend::Metal::Private
{

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
    RHI::ERHIResult Submit(
        const Core::TSharedPtr<RHI::IRHICommandBuffer>& CommandBuffer,
        const Core::TArray<Core::TSharedPtr<RHI::IRHISemaphore>>& WaitSemaphores,
        const Core::TArray<Core::TSharedPtr<RHI::IRHISemaphore>>& SignalSemaphores,
        const Core::TSharedPtr<RHI::IRHIFence>& Fence) override;
    RHI::ERHIResult WaitIdle() override;

private:
    void PruneCompleted() noexcept;

    mutable std::mutex Mutex_;
    RHI::ERHIQueueType QueueType_;
    void* NativeQueue_ = nullptr;
    RHI::FRHIDeviceCapabilities Capabilities_;
    Core::uint32 SubmittedCount_ = 0;
    Core::TArray<Core::TSharedPtr<FMetalSubmission>> Submissions_;
};

} // namespace Stoner::Backend::Metal::Private
