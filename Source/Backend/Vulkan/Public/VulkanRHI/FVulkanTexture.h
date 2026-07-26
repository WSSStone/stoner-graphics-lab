#pragma once

#include "VulkanRHI/FVulkanMemoryAllocator.h"
#include "RHI/IRHITexture.h"

#include <memory>

namespace Stoner::Backend::Vulkan
{

class FVulkanDevice;

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

    Stoner::RHI::ERHIResult Invalidate() override;

private:
    friend class FVulkanDevice;

    FVulkanTexture(
        const Stoner::RHI::FRHITextureDesc& InDesc,
        FVulkanResourceAllocation&& InAllocation,
        std::shared_ptr<FVulkanMemoryAllocator> InAllocator);

    Stoner::RHI::FRHITextureDesc Desc;
    FVulkanResourceAllocation Allocation;
    std::shared_ptr<FVulkanMemoryAllocator> Allocator;
    Stoner::RHI::ERHIResourceLifecycleState LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Valid;
};

} // namespace Stoner::Backend::Vulkan
