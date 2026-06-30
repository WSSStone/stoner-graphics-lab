#include "VulkanRHI/FVulkanQueue.h"

namespace Stoner::Backend::Vulkan
{

FVulkanQueue::FVulkanQueue(Stoner::RHI::ERHIQueueType InQueueType) noexcept
    : QueueType(InQueueType)
{
}

Stoner::RHI::ERHIQueueType FVulkanQueue::GetQueueType() const noexcept
{
    return QueueType;
}

Stoner::Core::uint32 FVulkanQueue::GetSubmittedCommandBufferCount() const noexcept
{
    return SubmittedCommandBufferCount;
}

bool FVulkanQueue::IsValid() const noexcept
{
    return bValid;
}

Stoner::RHI::ERHIResult FVulkanQueue::Submit(
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHICommandBuffer>& CommandBuffer,
    const Stoner::Core::TArray<Stoner::Core::TSharedPtr<Stoner::RHI::IRHISemaphore>>&,
    const Stoner::Core::TArray<Stoner::Core::TSharedPtr<Stoner::RHI::IRHISemaphore>>&,
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIFence>&)
{
    if (!bValid)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (!CommandBuffer || CommandBuffer->GetState() != Stoner::RHI::ERHICommandBufferState::Completed)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    return Stoner::RHI::ERHIResult::Unsupported;
}

Stoner::RHI::ERHIResult FVulkanQueue::WaitIdle()
{
    return bValid ? Stoner::RHI::ERHIResult::Success : Stoner::RHI::ERHIResult::InvalidState;
}

void FVulkanQueue::Invalidate() noexcept
{
    bValid = false;
}

} // namespace Stoner::Backend::Vulkan
