#include "VulkanRHI/FVulkanFence.h"

namespace Stoner::Backend::Vulkan
{

FVulkanFence::FVulkanFence(bool bInitiallySignaled) noexcept
    : State(bInitiallySignaled ? Stoner::RHI::ERHIFenceState::Signaled : Stoner::RHI::ERHIFenceState::Unsignaled)
{
}

Stoner::RHI::ERHIFenceState FVulkanFence::GetState() const noexcept
{
    return State;
}

bool FVulkanFence::IsSignaled() const noexcept
{
    return State == Stoner::RHI::ERHIFenceState::Signaled || State == Stoner::RHI::ERHIFenceState::Waited;
}

Stoner::RHI::ERHIResult FVulkanFence::Wait(Stoner::Core::uint64 TimeoutMicroseconds)
{
    if (!bValid)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (!IsSignaled())
    {
        return TimeoutMicroseconds > 0 ? Stoner::RHI::ERHIResult::Timeout : Stoner::RHI::ERHIResult::NotReady;
    }

    State = Stoner::RHI::ERHIFenceState::Waited;
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanFence::Reset()
{
    if (!bValid)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    State = Stoner::RHI::ERHIFenceState::Unsignaled;
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanFence::Signal()
{
    if (!bValid)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    State = Stoner::RHI::ERHIFenceState::Signaled;
    return Stoner::RHI::ERHIResult::Success;
}

void FVulkanFence::Invalidate() noexcept
{
    bValid = false;
}

} // namespace Stoner::Backend::Vulkan
