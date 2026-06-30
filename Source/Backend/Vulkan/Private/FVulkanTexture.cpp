#include "VulkanRHI/FVulkanTexture.h"

namespace Stoner::Backend::Vulkan
{

FVulkanTexture::FVulkanTexture(const Stoner::RHI::FRHITextureDesc& InDesc, const FVulkanResourceAllocation& InAllocation, std::shared_ptr<FVulkanMemoryAllocator> InAllocator)
    : Desc(InDesc)
    , Allocation(InAllocation)
    , Allocator(std::move(InAllocator))
{
}

const Stoner::RHI::FRHITextureDesc& FVulkanTexture::GetDesc() const noexcept { return Desc; }
Stoner::RHI::ERHITextureDimension FVulkanTexture::GetDimension() const noexcept { return Desc.Dimension; }
Stoner::RHI::ERHIFormat FVulkanTexture::GetFormat() const noexcept { return Desc.Format; }
Stoner::RHI::ERHITextureUsage FVulkanTexture::GetUsage() const noexcept { return Desc.Usage; }
Stoner::RHI::ERHIResourceLifecycleState FVulkanTexture::GetLifecycleState() const noexcept { return LifecycleState; }
const FVulkanResourceAllocation& FVulkanTexture::GetAllocation() const noexcept { return Allocation; }

Stoner::RHI::ERHIResult FVulkanTexture::Invalidate()
{
    if (LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Invalidated)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
    return Allocator ? Allocator->Release(Allocation) : Allocation.Release();
}

} // namespace Stoner::Backend::Vulkan
