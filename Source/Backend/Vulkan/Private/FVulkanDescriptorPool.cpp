#include "VulkanRHI/FVulkanDescriptorPool.h"

namespace Stoner::Backend::Vulkan
{

FVulkanDescriptorPool::FVulkanDescriptorPool(Stoner::Core::uint32 InCapacity) noexcept
    : Capacity(InCapacity)
{
}

Stoner::Core::uint32 FVulkanDescriptorPool::GetCapacity() const noexcept { return Capacity; }
Stoner::Core::uint32 FVulkanDescriptorPool::GetAllocatedCount() const noexcept { return AllocatedCount; }
bool FVulkanDescriptorPool::IsExhausted() const noexcept { return AllocatedCount >= Capacity; }
Stoner::RHI::ERHIResourceLifecycleState FVulkanDescriptorPool::GetLifecycleState() const noexcept { return LifecycleState; }

Stoner::RHI::ERHIResult FVulkanDescriptorPool::Allocate() noexcept
{
    if (LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Invalidated)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (IsExhausted())
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    ++AllocatedCount;
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanDescriptorPool::Release() noexcept
{
    if (AllocatedCount == 0)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    --AllocatedCount;
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanDescriptorPool::Invalidate() noexcept
{
    if (LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Invalidated)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
