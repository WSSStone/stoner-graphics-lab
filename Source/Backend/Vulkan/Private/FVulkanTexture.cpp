#include "VulkanRHI/FVulkanTexture.h"
#include "VulkanRHI/FVulkanNativeContext.h"

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

FVulkanTexture::FVulkanTexture(
    const Stoner::RHI::FRHITextureDesc& InDesc,
    FVulkanResourceAllocation&& InAllocation,
    std::shared_ptr<FVulkanMemoryAllocator> InAllocator,
    Stoner::Core::TSharedPtr<FVulkanNativeContext> InNativeContext,
    Stoner::Core::uint64 InNativeToken)
    : Desc(InDesc)
    , Allocation(IsCompatibleAllocation(InDesc, InAllocation, InAllocator)
              ? std::move(InAllocation)
              : FVulkanResourceAllocation{})
    , Allocator(std::move(InAllocator))
    , UploadedMips(InDesc.MipLevels)
    , NativeContext(std::move(InNativeContext))
    , NativeToken(InNativeToken)
{
    if (!Allocation.IsSuccessful() ||
        ((NativeContext != nullptr) != (NativeToken != 0)))
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
bool FVulkanTexture::HasUploadedMip(Stoner::Core::uint32 MipLevel) const noexcept
{
    return MipLevel < UploadedMips.size() && !UploadedMips[MipLevel].empty();
}

std::span<const Stoner::Core::uint8> FVulkanTexture::GetUploadedMipData(
    Stoner::Core::uint32 MipLevel) const noexcept
{
    if (!HasUploadedMip(MipLevel))
    {
        return {};
    }
    return UploadedMips[MipLevel];
}

Stoner::RHI::ERHIResult FVulkanTexture::RecordUploadedMip(
    Stoner::Core::uint32 MipLevel,
    Stoner::Core::TArray<Stoner::Core::uint8> Bytes)
{
    if (LifecycleState !=
            Stoner::RHI::ERHIResourceLifecycleState::Valid ||
        MipLevel >= UploadedMips.size() || Bytes.empty())
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    UploadedMips[MipLevel] = std::move(Bytes);
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanTexture::Invalidate()
{
    if (LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Invalidated)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
    UploadedMips.clear();
    if (NativeContext && NativeToken != 0)
    {
        NativeContext->DestroyOwnedTexture(NativeToken);
        NativeToken = 0;
        NativeContext.reset();
    }
    return Allocator
        ? Allocator->Release(Allocation)
        : Stoner::RHI::ERHIResult::InvalidState;
}

} // namespace Stoner::Backend::Vulkan
