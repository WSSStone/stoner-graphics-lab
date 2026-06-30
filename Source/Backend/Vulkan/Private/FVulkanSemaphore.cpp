#include "VulkanRHI/FVulkanSemaphore.h"

namespace Stoner::Backend::Vulkan
{

Stoner::RHI::ERHISemaphoreState FVulkanSemaphore::GetState() const noexcept
{
    return State;
}

bool FVulkanSemaphore::IsSignaled() const noexcept
{
    return State == Stoner::RHI::ERHISemaphoreState::Signaled;
}

Stoner::RHI::ERHIResult FVulkanSemaphore::Signal()
{
    if (!bValid)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (State == Stoner::RHI::ERHISemaphoreState::Signaled)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    State = Stoner::RHI::ERHISemaphoreState::Signaled;
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanSemaphore::Consume()
{
    if (!bValid)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (State != Stoner::RHI::ERHISemaphoreState::Signaled)
    {
        return Stoner::RHI::ERHIResult::NotReady;
    }

    State = Stoner::RHI::ERHISemaphoreState::Consumed;
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanSemaphore::Reset()
{
    if (!bValid)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    State = Stoner::RHI::ERHISemaphoreState::Unsignaled;
    return Stoner::RHI::ERHIResult::Success;
}

void FVulkanSemaphore::Invalidate() noexcept
{
    bValid = false;
}

} // namespace Stoner::Backend::Vulkan
