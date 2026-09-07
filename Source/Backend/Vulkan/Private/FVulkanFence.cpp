#include "VulkanRHI/FVulkanFence.h"

#include "VulkanRHI/FVulkanDeviceOwnerState.h"
#include "VulkanRHI/FVulkanNativeContext.h"

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
    if (bTerminalFailure)
    {
        return Stoner::RHI::ERHIResult::Failed;
    }
    if (NativeContext != nullptr && NativeSubmissionId != 0)
    {
        const Stoner::RHI::ERHIResult Result =
            NativeContext->WaitDeferredSubmission(
                NativeSubmissionId, TimeoutMicroseconds);
        if (Result == Stoner::RHI::ERHIResult::Success)
        {
            NativeSubmissionId = 0;
            NativeContext = nullptr;
            State = Stoner::RHI::ERHIFenceState::Waited;
        }
        return Result;
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

    if (NativeSubmissionId != 0)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    State = Stoner::RHI::ERHIFenceState::Unsignaled;
    bTerminalFailure = false;
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanFence::Signal()
{
    if (!bValid || !Owner || !Owner->bActive)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    if (NativeSubmissionId != 0 || bTerminalFailure ||
        State == Stoner::RHI::ERHIFenceState::Signaled)
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
    return bValid && Owner && Owner->bActive && NativeSubmissionId == 0 &&
        !bTerminalFailure && !IsSignaled();
}

void FVulkanFence::CommitSignalForSubmission() noexcept
{
    State = Stoner::RHI::ERHIFenceState::Signaled;
}

void FVulkanFence::AttachNativeSubmission(
    FVulkanNativeContext* InContext,
    Stoner::Core::uint64 SubmissionId) noexcept
{
    if (!bValid || !Owner || !Owner->bActive || InContext == nullptr ||
        SubmissionId == 0 || NativeSubmissionId != 0)
    {
        return;
    }
    NativeContext = InContext;
    NativeSubmissionId = SubmissionId;
    State = Stoner::RHI::ERHIFenceState::Unsignaled;
}

void FVulkanFence::CompleteNativeSubmission(bool bSucceeded) noexcept
{
    if (NativeSubmissionId == 0)
    {
        return;
    }
    NativeSubmissionId = 0;
    NativeContext = nullptr;
    if (bSucceeded)
    {
        State = Stoner::RHI::ERHIFenceState::Signaled;
        bTerminalFailure = false;
    }
    else
    {
        State = Stoner::RHI::ERHIFenceState::Unsignaled;
        bTerminalFailure = true;
    }
}

void FVulkanFence::Invalidate() noexcept
{
    bValid = false;
    NativeContext = nullptr;
    NativeSubmissionId = 0;
    Owner.reset();
}

} // namespace Stoner::Backend::Vulkan
