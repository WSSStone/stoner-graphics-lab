#pragma once

#include "VulkanRHI/FVulkanMemoryAllocator.h"
#include "RHI/IRHIBuffer.h"

#include <memory>

namespace Stoner::Backend::Vulkan
{

class FVulkanBuffer final : public Stoner::RHI::IRHIBuffer
{
public:
    FVulkanBuffer(const Stoner::RHI::FRHIBufferDesc& InDesc, const FVulkanResourceAllocation& InAllocation, std::shared_ptr<FVulkanMemoryAllocator> InAllocator);

    [[nodiscard]] const Stoner::RHI::FRHIBufferDesc& GetDesc() const noexcept override;
    [[nodiscard]] Stoner::Core::uint64 GetSizeInBytes() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIBufferUsage GetUsage() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIResourceLifecycleState GetLifecycleState() const noexcept override;
    [[nodiscard]] const FVulkanResourceAllocation& GetAllocation() const noexcept;

    Stoner::RHI::ERHIResult Invalidate() override;

private:
    Stoner::RHI::FRHIBufferDesc Desc;
    FVulkanResourceAllocation Allocation;
    std::shared_ptr<FVulkanMemoryAllocator> Allocator;
    Stoner::RHI::ERHIResourceLifecycleState LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Valid;
};

} // namespace Stoner::Backend::Vulkan
