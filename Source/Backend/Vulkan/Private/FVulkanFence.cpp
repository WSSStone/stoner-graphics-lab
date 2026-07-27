#include "VulkanRHI/FVulkanFence.h"

#include "VulkanRHI/FVulkanDeviceOwnerState.h"

namespace Stoner::Backend::Vulkan
{

FVulkanFence::FVulkanFence(
    bool bInitiallySignaled,
    Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> InOwner) noexcept
    : State(bInitiallySignaled ? Stoner::RHI::ERHIFenceState::Signaled : Stoner::RHI::ERHIFenceState::Unsignaled)
    , Owner(std::move(InOwner))
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
    if (!bValid || !Owner || !Owner->bActive)
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
    if (!bValid || !Owner || !Owner->bActive)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    State = Stoner::RHI::ERHIFenceState::Unsignaled;
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanFence::Signal()
{
    if (!bValid || !Owner || !Owner->bActive)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    State = Stoner::RHI::ERHIFenceState::Signaled;
    return Stoner::RHI::ERHIResult::Success;
}

bool FVulkanFence::BelongsTo(
    const Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState>& InOwner) const noexcept
{
    return bValid && Owner && Owner->bActive && InOwner && Owner == InOwner;
}

bool FVulkanFence::CanSignalForSubmission() const noexcept
{
    return bValid && Owner && Owner->bActive && !IsSignaled();
}

void FVulkanFence::CommitSignalForSubmission() noexcept
{
    State = Stoner::RHI::ERHIFenceState::Signaled;
}

void FVulkanFence::Invalidate() noexcept
{
    bValid = false;
    Owner.reset();
}

} // namespace Stoner::Backend::Vulkan
