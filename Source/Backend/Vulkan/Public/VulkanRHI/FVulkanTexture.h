#pragma once

#include "VulkanRHI/FVulkanMemoryAllocator.h"
#include "RHI/IRHITexture.h"

#include <memory>
#include <span>

namespace Stoner::Backend::Vulkan
{

class FVulkanDevice;
class FVulkanNativeContext;

class FVulkanTexture final : public Stoner::RHI::IRHITexture
{
public:
    ~FVulkanTexture() override;
    FVulkanTexture(const FVulkanTexture&) = delete;
    FVulkanTexture& operator=(const FVulkanTexture&) = delete;

    [[nodiscard]] const Stoner::RHI::FRHITextureDesc& GetDesc() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHITextureDimension GetDimension() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIFormat GetFormat() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHITextureUsage GetUsage() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIResourceLifecycleState GetLifecycleState() const noexcept override;
    [[nodiscard]] const FVulkanResourceAllocation& GetAllocation() const noexcept;
    [[nodiscard]] bool HasUploadedMip(Stoner::Core::uint32 MipLevel) const noexcept;
    [[nodiscard]] std::span<const Stoner::Core::uint8> GetUploadedMipData(
        Stoner::Core::uint32 MipLevel) const noexcept;

    Stoner::RHI::ERHIResult Invalidate() override;

private:
    friend class FVulkanDevice;

    FVulkanTexture(
        const Stoner::RHI::FRHITextureDesc& InDesc,
        FVulkanResourceAllocation&& InAllocation,
        std::shared_ptr<FVulkanMemoryAllocator> InAllocator,
        Stoner::Core::TSharedPtr<FVulkanNativeContext> InNativeContext,
        Stoner::Core::uint64 InNativeToken);
    [[nodiscard]] Stoner::RHI::ERHIResult RecordUploadedMip(
        Stoner::Core::uint32 MipLevel,
        Stoner::Core::TArray<Stoner::Core::uint8> Bytes);

    Stoner::RHI::FRHITextureDesc Desc;
    FVulkanResourceAllocation Allocation;
    std::shared_ptr<FVulkanMemoryAllocator> Allocator;
    Stoner::Core::TArray<Stoner::Core::TArray<Stoner::Core::uint8>>
        UploadedMips;
    Stoner::Core::TSharedPtr<FVulkanNativeContext> NativeContext;
    Stoner::Core::uint64 NativeToken = 0;
    Stoner::RHI::ERHIResourceLifecycleState LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Valid;
};

} // namespace Stoner::Backend::Vulkan
