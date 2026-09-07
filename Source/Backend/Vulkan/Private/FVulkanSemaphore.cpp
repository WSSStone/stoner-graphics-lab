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
    if (!bValid || !Owner || !Owner->bActive || bLabBound)
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
    if (!bValid || !Owner || !Owner->bActive || bLabBound)
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
        State == Stoner::RHI::ERHISemaphoreState::Signaled &&
        (!bLabBound || NativeHandleValue != 0);
}

bool FVulkanSemaphore::CanSignalForSubmission() const noexcept
{
    return bValid && Owner && Owner->bActive &&
        State != Stoner::RHI::ERHISemaphoreState::Signaled && !bLabBound;
}

void FVulkanSemaphore::CommitConsumeForSubmission() noexcept
{
    State = Stoner::RHI::ERHISemaphoreState::Consumed;
}

void FVulkanSemaphore::CommitSignalForSubmission() noexcept
{
    State = Stoner::RHI::ERHISemaphoreState::Signaled;
}

bool FVulkanSemaphore::BindLabAcquireSemaphore(
    Stoner::Core::uint64 AcquisitionToken,
    Stoner::Core::uint64 NativeHandleValueIn) noexcept
{
    if (!bValid || !Owner || !Owner->bActive || AcquisitionToken == 0 ||
        NativeHandleValueIn == 0 || State !=
            Stoner::RHI::ERHISemaphoreState::Unsignaled)
    {
        return false;
    }
    if (bLabBound)
    {
        return bLabAcquireSemaphore && LabBindingToken == AcquisitionToken &&
            NativeHandleValue == NativeHandleValueIn;
    }
    bLabBound = true;
    bLabAcquireSemaphore = true;
    LabBindingToken = AcquisitionToken;
    NativeHandleValue = NativeHandleValueIn;
    return true;
}

bool FVulkanSemaphore::BindLabRenderSemaphore(
    Stoner::Core::uint64 AcquisitionToken,
    Stoner::Core::uint64 NativeHandleValueIn) noexcept
{
    if (!bValid || !Owner || !Owner->bActive || AcquisitionToken == 0 ||
        NativeHandleValueIn == 0 || State !=
            Stoner::RHI::ERHISemaphoreState::Unsignaled)
    {
        return false;
    }
    if (bLabBound)
    {
        return !bLabAcquireSemaphore && LabBindingToken == AcquisitionToken &&
            NativeHandleValue == NativeHandleValueIn;
    }
    bLabBound = true;
    bLabAcquireSemaphore = false;
    LabBindingToken = AcquisitionToken;
    NativeHandleValue = NativeHandleValueIn;
    return true;
}

void FVulkanSemaphore::MarkLabAcquireReady() noexcept
{
    if (bValid && bLabBound && bLabAcquireSemaphore)
    {
        State = Stoner::RHI::ERHISemaphoreState::Signaled;
    }
}

void FVulkanSemaphore::RetireLabBinding() noexcept
{
    if (!bLabBound)
    {
        return;
    }
    bLabBound = false;
    bLabAcquireSemaphore = false;
    LabBindingToken = 0;
    NativeHandleValue = 0;
    State = Stoner::RHI::ERHISemaphoreState::Consumed;
}

void FVulkanSemaphore::AdoptOwner(
    Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> InOwner) noexcept
{
    if (!bValid || !InOwner || !InOwner->bActive)
    {
        return;
    }
    if (!Owner)
    {
        Owner = std::move(InOwner);
    }
}

void FVulkanSemaphore::Invalidate() noexcept
{
    bValid = false;
    Owner.reset();
    NativeHandleValue = 0;
    LabBindingToken = 0;
    bLabBound = false;
    bLabAcquireSemaphore = false;
}

} // namespace Stoner::Backend::Vulkan
