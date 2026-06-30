#include "VulkanRHI/FVulkanMemoryAllocator.h"

#include <limits>

namespace Stoner::Backend::Vulkan
{

void FVulkanMemoryAllocator::Reset() noexcept
{
    AllocatedBytes = 0;
    LiveAllocationCount = 0;
    LastFailure = EVulkanAllocationFailure::None;
    LastMode = EVulkanAllocationMode::Failed;
    LastReason = "";
}

void FVulkanMemoryAllocator::SetRuntimeAvailable(bool bInRuntimeAvailable) noexcept
{
    bRuntimeAvailable = bInRuntimeAvailable;
}

void FVulkanMemoryAllocator::ConfigureBudgetLimit(Stoner::Core::uint64 MaxBytes) noexcept
{
    Limits.BudgetBytes = MaxBytes;
}

void FVulkanMemoryAllocator::ConfigureAllocationCountLimit(Stoner::Core::uint32 MaxAllocations) noexcept
{
    Limits.AllocationCount = MaxAllocations;
}

void FVulkanMemoryAllocator::ClearLimits() noexcept
{
    Limits = {};
}

FVulkanResourceAllocation FVulkanMemoryAllocator::AllocateBuffer(const Stoner::RHI::FRHIBufferDesc& Desc, bool bDeviceActive) noexcept
{
    return Allocate(EVulkanResourceKind::Buffer, EstimateBufferBytes(Desc), bDeviceActive);
}

FVulkanResourceAllocation FVulkanMemoryAllocator::AllocateTexture(const Stoner::RHI::FRHITextureDesc& Desc, bool bDeviceActive) noexcept
{
    return Allocate(EVulkanResourceKind::Texture, EstimateTextureBytes(Desc), bDeviceActive);
}

Stoner::RHI::ERHIResult FVulkanMemoryAllocator::Release(FVulkanResourceAllocation& Allocation) noexcept
{
    if (!Allocation.IsSuccessful())
    {
        return Allocation.IsReleased() ? Stoner::RHI::ERHIResult::InvalidState : Stoner::RHI::ERHIResult::Failed;
    }

    if (AllocatedBytes >= Allocation.SizeInBytes)
    {
        AllocatedBytes -= Allocation.SizeInBytes;
    }
    else
    {
        AllocatedBytes = 0;
    }
    if (LiveAllocationCount > 0)
    {
        --LiveAllocationCount;
    }
    return Allocation.Release();
}

FVulkanAllocationSnapshot FVulkanMemoryAllocator::GetSnapshot() const noexcept
{
    return {AllocatedBytes, LiveAllocationCount, Limits, LastFailure, LastMode, LastReason};
}

Stoner::Core::uint64 FVulkanMemoryAllocator::EstimateBufferBytes(const Stoner::RHI::FRHIBufferDesc& Desc) noexcept
{
    return Desc.SizeInBytes;
}

Stoner::Core::uint64 FVulkanMemoryAllocator::EstimateTextureBytes(const Stoner::RHI::FRHITextureDesc& Desc) noexcept
{
    Stoner::Core::uint64 TexelSize = 4;
    switch (Desc.Format)
    {
    case Stoner::RHI::ERHIFormat::R8_UNorm: TexelSize = 1; break;
    case Stoner::RHI::ERHIFormat::R16G16B16A16_Float: TexelSize = 8; break;
    case Stoner::RHI::ERHIFormat::R32_Float: TexelSize = 4; break;
    default: TexelSize = 4; break;
    }

    const Stoner::Core::uint64 Width = Desc.Width ? Desc.Width : 1;
    const Stoner::Core::uint64 Height = Desc.Height ? Desc.Height : 1;
    const Stoner::Core::uint64 Depth = Desc.Depth ? Desc.Depth : 1;
    const Stoner::Core::uint64 Layers = Desc.ArrayLayers ? Desc.ArrayLayers : 1;
    const Stoner::Core::uint64 Mips = Desc.MipLevels ? Desc.MipLevels : 1;
    return Width * Height * Depth * Layers * Mips * TexelSize;
}

FVulkanResourceAllocation FVulkanMemoryAllocator::Allocate(EVulkanResourceKind Kind, Stoner::Core::uint64 ByteSize, bool bDeviceActive) noexcept
{
    if (!bDeviceActive)
    {
        LastFailure = EVulkanAllocationFailure::DeviceInactive;
        LastMode = EVulkanAllocationMode::Failed;
        LastReason = "device is inactive";
        return FVulkanResourceAllocation::MakeFailure(Kind, LastFailure, ByteSize, LastReason);
    }
    if (ByteSize == 0)
    {
        LastFailure = EVulkanAllocationFailure::InvalidRequest;
        LastMode = EVulkanAllocationMode::Failed;
        LastReason = "allocation size is zero";
        return FVulkanResourceAllocation::MakeFailure(Kind, LastFailure, ByteSize, LastReason);
    }
    if (Limits.AllocationCount > 0 && LiveAllocationCount >= Limits.AllocationCount)
    {
        LastFailure = EVulkanAllocationFailure::AllocationCountExceeded;
        LastMode = EVulkanAllocationMode::Failed;
        LastReason = "allocation count limit exceeded";
        return FVulkanResourceAllocation::MakeFailure(Kind, LastFailure, ByteSize, LastReason);
    }
    if (Limits.BudgetBytes > 0 && (ByteSize > Limits.BudgetBytes || AllocatedBytes > Limits.BudgetBytes - ByteSize))
    {
        LastFailure = EVulkanAllocationFailure::BudgetExceeded;
        LastMode = EVulkanAllocationMode::Failed;
        LastReason = "allocation budget exceeded";
        return FVulkanResourceAllocation::MakeFailure(Kind, LastFailure, ByteSize, LastReason);
    }

    AllocatedBytes += ByteSize;
    ++LiveAllocationCount;
    LastFailure = EVulkanAllocationFailure::None;
    LastMode = bRuntimeAvailable ? EVulkanAllocationMode::RealRuntime : EVulkanAllocationMode::DeterministicFallback;
    LastReason = bRuntimeAvailable ? "runtime allocation path selected" : "runtime unavailable; deterministic fallback allocation selected";
    return FVulkanResourceAllocation::MakeSuccess(Kind, LastMode, ByteSize, Limits.BudgetBytes, Limits.AllocationCount, LastReason);
}

} // namespace Stoner::Backend::Vulkan
