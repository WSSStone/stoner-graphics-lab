#include "VulkanRHI/FVulkanSemaphore.h"

#include "VulkanRHI/FVulkanDeviceOwnerState.h"

namespace Stoner::Backend::Vulkan
{

FVulkanSemaphore::FVulkanSemaphore(
    Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> InOwner) noexcept
    : Owner(std::move(InOwner))
{
}

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
    if (!bValid || !Owner || !Owner->bActive)
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
    if (!bValid || !Owner || !Owner->bActive)
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
    if (!bValid || !Owner || !Owner->bActive)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    State = Stoner::RHI::ERHISemaphoreState::Unsignaled;
    return Stoner::RHI::ERHIResult::Success;
}

bool FVulkanSemaphore::BelongsTo(
    const Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState>& InOwner) const noexcept
{
    return bValid && Owner && Owner->bActive && InOwner && Owner == InOwner;
}

bool FVulkanSemaphore::CanConsumeForSubmission() const noexcept
{
    return bValid && Owner && Owner->bActive &&
        State == Stoner::RHI::ERHISemaphoreState::Signaled;
}

bool FVulkanSemaphore::CanSignalForSubmission() const noexcept
{
    return bValid && Owner && Owner->bActive &&
        State != Stoner::RHI::ERHISemaphoreState::Signaled;
}

void FVulkanSemaphore::CommitConsumeForSubmission() noexcept
{
    State = Stoner::RHI::ERHISemaphoreState::Consumed;
}

void FVulkanSemaphore::CommitSignalForSubmission() noexcept
{
    State = Stoner::RHI::ERHISemaphoreState::Signaled;
}

void FVulkanSemaphore::Invalidate() noexcept
{
    bValid = false;
    Owner.reset();
}

} // namespace Stoner::Backend::Vulkan
