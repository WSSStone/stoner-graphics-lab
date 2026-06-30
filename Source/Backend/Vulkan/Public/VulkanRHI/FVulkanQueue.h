#pragma once

#include "RHI/RHIMinimal.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanQueue final : public Stoner::RHI::IRHICommandQueue
{
public:
    explicit FVulkanQueue(Stoner::RHI::ERHIQueueType InQueueType) noexcept;

    [[nodiscard]] Stoner::RHI::ERHIQueueType GetQueueType() const noexcept override;
    [[nodiscard]] Stoner::Core::uint32 GetSubmittedCommandBufferCount() const noexcept override;
    [[nodiscard]] bool IsValid() const noexcept;

    Stoner::RHI::ERHIResult Submit(
        const Stoner::Core::TSharedPtr<Stoner::RHI::IRHICommandBuffer>& CommandBuffer,
        const Stoner::Core::TArray<Stoner::Core::TSharedPtr<Stoner::RHI::IRHISemaphore>>& WaitSemaphores = {},
        const Stoner::Core::TArray<Stoner::Core::TSharedPtr<Stoner::RHI::IRHISemaphore>>& SignalSemaphores = {},
        const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIFence>& Fence = nullptr) override;
    Stoner::RHI::ERHIResult WaitIdle() override;
    void Invalidate() noexcept;

private:
    Stoner::RHI::ERHIQueueType QueueType;
    Stoner::Core::uint32 SubmittedCommandBufferCount = 0;
    bool bValid = true;
};

} // namespace Stoner::Backend::Vulkan
