#include "VulkanRHI/FVulkanTexture.h"

namespace Stoner::Backend::Vulkan
{

namespace
{

[[nodiscard]] bool IsCompatibleAllocation(
    const Stoner::RHI::FRHITextureDesc& Desc,
    const FVulkanResourceAllocation& Allocation,
    const std::shared_ptr<FVulkanMemoryAllocator>& Allocator) noexcept
{
    Stoner::Core::uint64 ExpectedByteSize = 0;
    return Allocator &&
        FVulkanMemoryAllocator::TryEstimateTextureBytes(
            Desc, ExpectedByteSize) &&
        Allocation.IsSuccessful() &&
        Allocation.GetKind() == EVulkanResourceKind::Texture &&
        Allocation.GetByteSize() == ExpectedByteSize;
}

} // namespace

FVulkanTexture::FVulkanTexture(const Stoner::RHI::FRHITextureDesc& InDesc, FVulkanResourceAllocation&& InAllocation, std::shared_ptr<FVulkanMemoryAllocator> InAllocator)
    : Desc(InDesc)
    , Allocation(IsCompatibleAllocation(InDesc, InAllocation, InAllocator)
              ? std::move(InAllocation)
              : FVulkanResourceAllocation{})
    , Allocator(std::move(InAllocator))
{
    if (!Allocation.IsSuccessful())
    {
        LifecycleState =
            Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
    }
}

FVulkanTexture::~FVulkanTexture()
{
    if (LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Valid)
    {
        (void)Invalidate();
    }
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
    return Allocator
        ? Allocator->Release(Allocation)
        : Stoner::RHI::ERHIResult::InvalidState;
}

} // namespace Stoner::Backend::Vulkan
