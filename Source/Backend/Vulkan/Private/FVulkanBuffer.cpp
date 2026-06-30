#include "VulkanRHI/FVulkanBuffer.h"

namespace Stoner::Backend::Vulkan
{

FVulkanBuffer::FVulkanBuffer(const Stoner::RHI::FRHIBufferDesc& InDesc, const FVulkanResourceAllocation& InAllocation, std::shared_ptr<FVulkanMemoryAllocator> InAllocator)
    : Desc(InDesc)
    , Allocation(InAllocation)
    , Allocator(std::move(InAllocator))
{
}

const Stoner::RHI::FRHIBufferDesc& FVulkanBuffer::GetDesc() const noexcept { return Desc; }
Stoner::Core::uint64 FVulkanBuffer::GetSizeInBytes() const noexcept { return Desc.SizeInBytes; }
Stoner::RHI::ERHIBufferUsage FVulkanBuffer::GetUsage() const noexcept { return Desc.Usage; }
Stoner::RHI::ERHIResourceLifecycleState FVulkanBuffer::GetLifecycleState() const noexcept { return LifecycleState; }
const FVulkanResourceAllocation& FVulkanBuffer::GetAllocation() const noexcept { return Allocation; }

Stoner::RHI::ERHIResult FVulkanBuffer::Invalidate()
{
    if (LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Invalidated)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
    return Allocator ? Allocator->Release(Allocation) : Allocation.Release();
}

} // namespace Stoner::Backend::Vulkan
