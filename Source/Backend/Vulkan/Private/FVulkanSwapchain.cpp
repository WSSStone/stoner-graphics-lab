#include "VulkanRHI/FVulkanSwapchain.h"

namespace Stoner::Backend::Vulkan
{

FVulkanSwapchain::FVulkanSwapchain(Stoner::Core::uint32 InFrameCount) noexcept
    : FrameCount(InFrameCount > 0 ? InFrameCount : 1)
{
}

Stoner::RHI::ERHISwapchainState FVulkanSwapchain::GetState() const noexcept
{
    return State;
}

Stoner::Core::uint32 FVulkanSwapchain::GetFrameCount() const noexcept
{
    return FrameCount;
}

Stoner::Core::uint32 FVulkanSwapchain::GetCurrentFrameIndex() const noexcept
{
    return CurrentFrameIndex;
}

Stoner::RHI::ERHIResult FVulkanSwapchain::AcquireNextFrame(Stoner::Core::uint32& OutFrameIndex)
{
    if (!bValid)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (State == Stoner::RHI::ERHISwapchainState::ResizeRequired)
    {
        return Stoner::RHI::ERHIResult::ResizeRequired;
    }
    if (State == Stoner::RHI::ERHISwapchainState::Unavailable)
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    if (State == Stoner::RHI::ERHISwapchainState::Acquired)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    OutFrameIndex = CurrentFrameIndex;
    AcquiredGeneration = Generation;
    State = Stoner::RHI::ERHISwapchainState::Acquired;
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanSwapchain::Present(Stoner::Core::uint32 FrameIndex)
{
    if (!bValid)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (State == Stoner::RHI::ERHISwapchainState::ResizeRequired)
    {
        return Stoner::RHI::ERHIResult::ResizeRequired;
    }
    if (State == Stoner::RHI::ERHISwapchainState::Unavailable)
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    if (State != Stoner::RHI::ERHISwapchainState::Acquired || FrameIndex != CurrentFrameIndex || AcquiredGeneration != Generation)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    CurrentFrameIndex = (CurrentFrameIndex + 1) % FrameCount;
    AcquiredGeneration = 0;
    State = Stoner::RHI::ERHISwapchainState::Ready;
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanSwapchain::Recreate(Stoner::Core::uint32 NewFrameCount) noexcept
{
    if (!bValid)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (NewFrameCount == 0)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    FrameCount = NewFrameCount;
    CurrentFrameIndex = 0;
    AcquiredGeneration = 0;
    ++Generation;
    State = Stoner::RHI::ERHISwapchainState::Ready;
    return Stoner::RHI::ERHIResult::Success;
}

void FVulkanSwapchain::SimulateResizeRequired() noexcept
{
    if (bValid)
    {
        State = Stoner::RHI::ERHISwapchainState::ResizeRequired;
    }
}

void FVulkanSwapchain::SetUnavailable() noexcept
{
    if (bValid)
    {
        State = Stoner::RHI::ERHISwapchainState::Unavailable;
    }
}

void FVulkanSwapchain::Invalidate() noexcept
{
    bValid = false;
    State = Stoner::RHI::ERHISwapchainState::Unavailable;
}

} // namespace Stoner::Backend::Vulkan
