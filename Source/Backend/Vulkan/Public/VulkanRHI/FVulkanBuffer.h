#pragma once

#include "VulkanRHI/FVulkanMemoryAllocator.h"
#include "RHI/IRHIBuffer.h"

#include <memory>

namespace Stoner::Backend::Vulkan
{

class FVulkanDevice;

class FVulkanBuffer final : public Stoner::RHI::IRHIBuffer
{
public:
    ~FVulkanBuffer() override;
    FVulkanBuffer(const FVulkanBuffer&) = delete;
    FVulkanBuffer& operator=(const FVulkanBuffer&) = delete;

    [[nodiscard]] const Stoner::RHI::FRHIBufferDesc& GetDesc() const noexcept override;
    [[nodiscard]] Stoner::Core::uint64 GetSizeInBytes() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIBufferUsage GetUsage() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIResourceLifecycleState GetLifecycleState() const noexcept override;
    [[nodiscard]] const FVulkanResourceAllocation& GetAllocation() const noexcept;

    Stoner::RHI::ERHIResult Invalidate() override;
    Stoner::RHI::ERHIResult Upload(const void* Data, Stoner::Core::uint64 SizeBytes, Stoner::Core::uint64 OffsetBytes = 0) override;
    [[nodiscard]] const Stoner::Core::TArray<Stoner::Core::uint8>& GetUploadedBytes() const noexcept { return UploadedBytes; }

private:
    friend class FVulkanDevice;

    FVulkanBuffer(
        const Stoner::RHI::FRHIBufferDesc& InDesc,
        FVulkanResourceAllocation&& InAllocation,
        std::shared_ptr<FVulkanMemoryAllocator> InAllocator);

    Stoner::RHI::FRHIBufferDesc Desc;
    FVulkanResourceAllocation Allocation;
    std::shared_ptr<FVulkanMemoryAllocator> Allocator;
    Stoner::RHI::ERHIResourceLifecycleState LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Valid;
    Stoner::Core::TArray<Stoner::Core::uint8> UploadedBytes;
};

} // namespace Stoner::Backend::Vulkan
