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
    [[nodiscard]] bool IsBorrowedPresentation() const noexcept
    {
        return bBorrowedPresentation;
    }
    [[nodiscard]] Stoner::Core::uint64 GetBorrowedContextToken() const noexcept
    {
        return bBorrowedPresentation ? BorrowedContextIdentity : 0;
    }
    [[nodiscard]] Stoner::Core::uint64 GetBorrowedGeneration() const noexcept
    {
        return bBorrowedPresentation ? BorrowedGeneration : 0;
    }
    [[nodiscard]] Stoner::Core::uint32 GetBorrowedImageIndex() const noexcept
    {
        return bBorrowedPresentation ? BorrowedImageIndex : 0;
    }
    [[nodiscard]] Stoner::Core::uint64 GetBorrowedAcquisitionToken() const noexcept
    {
        return bBorrowedPresentation ? BorrowedAcquisitionToken : 0;
    }

    Stoner::RHI::ERHIResult Invalidate() override;

private:
    friend class FVulkanDevice;
    friend class FVulkanNativeContext;

    FVulkanTexture(
        const Stoner::RHI::FRHITextureDesc& InDesc,
        FVulkanResourceAllocation&& InAllocation,
        std::shared_ptr<FVulkanMemoryAllocator> InAllocator,
        Stoner::Core::TSharedPtr<FVulkanNativeContext> InNativeContext,
        Stoner::Core::uint64 InNativeToken);
    FVulkanTexture(
        const Stoner::RHI::FRHITextureDesc& InDesc,
        Stoner::Core::uint64 InNativeToken,
        Stoner::Core::uint64 InBorrowedContextIdentity,
        Stoner::Core::uint64 InBorrowedGeneration,
        Stoner::Core::uint32 InBorrowedImageIndex,
        Stoner::Core::uint64 InBorrowedAcquisitionToken) noexcept;
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
    Stoner::Core::uint64 BorrowedContextIdentity = 0;
    Stoner::Core::uint64 BorrowedGeneration = 0;
    Stoner::Core::uint32 BorrowedImageIndex = 0;
    Stoner::Core::uint64 BorrowedAcquisitionToken = 0;
    bool bBorrowedPresentation = false;
    Stoner::RHI::ERHIResourceLifecycleState LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Valid;
};

} // namespace Stoner::Backend::Vulkan
