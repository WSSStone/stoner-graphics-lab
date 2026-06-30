#pragma once

#include "VulkanRHI/FVulkanResourceAllocation.h"
#include "RHI/FRHIBufferDesc.h"
#include "RHI/FRHITextureDesc.h"

namespace Stoner::Backend::Vulkan
{

struct FVulkanAllocationLimits
{
    Stoner::Core::uint64 BudgetBytes = 0;
    Stoner::Core::uint32 AllocationCount = 0;
};

struct FVulkanAllocationSnapshot
{
    Stoner::Core::uint64 AllocatedBytes = 0;
    Stoner::Core::uint32 LiveAllocationCount = 0;
    FVulkanAllocationLimits Limits;
    EVulkanAllocationFailure LastFailure = EVulkanAllocationFailure::None;
    EVulkanAllocationMode LastMode = EVulkanAllocationMode::Failed;
    const char* LastReason = "";
};

class FVulkanMemoryAllocator
{
public:
    void Reset() noexcept;
    void SetRuntimeAvailable(bool bInRuntimeAvailable) noexcept;
    void ConfigureBudgetLimit(Stoner::Core::uint64 MaxBytes) noexcept;
    void ConfigureAllocationCountLimit(Stoner::Core::uint32 MaxAllocations) noexcept;
    void ClearLimits() noexcept;

    [[nodiscard]] FVulkanResourceAllocation AllocateBuffer(const Stoner::RHI::FRHIBufferDesc& Desc, bool bDeviceActive) noexcept;
    [[nodiscard]] FVulkanResourceAllocation AllocateTexture(const Stoner::RHI::FRHITextureDesc& Desc, bool bDeviceActive) noexcept;
    Stoner::RHI::ERHIResult Release(FVulkanResourceAllocation& Allocation) noexcept;

    [[nodiscard]] FVulkanAllocationSnapshot GetSnapshot() const noexcept;
    [[nodiscard]] static Stoner::Core::uint64 EstimateBufferBytes(const Stoner::RHI::FRHIBufferDesc& Desc) noexcept;
    [[nodiscard]] static Stoner::Core::uint64 EstimateTextureBytes(const Stoner::RHI::FRHITextureDesc& Desc) noexcept;

private:
    [[nodiscard]] FVulkanResourceAllocation Allocate(EVulkanResourceKind Kind, Stoner::Core::uint64 ByteSize, bool bDeviceActive) noexcept;

    bool bRuntimeAvailable = false;
    Stoner::Core::uint64 AllocatedBytes = 0;
    Stoner::Core::uint32 LiveAllocationCount = 0;
    FVulkanAllocationLimits Limits;
    EVulkanAllocationFailure LastFailure = EVulkanAllocationFailure::None;
    EVulkanAllocationMode LastMode = EVulkanAllocationMode::Failed;
    const char* LastReason = "";
};

} // namespace Stoner::Backend::Vulkan
