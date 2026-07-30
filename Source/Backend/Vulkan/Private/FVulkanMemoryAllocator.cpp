#include "VulkanRHI/FVulkanMemoryAllocator.h"

#include "RHI/FRHIFormatInfo.h"

#include <atomic>
#include <limits>

namespace Stoner::Backend::Vulkan
{

namespace
{

std::atomic<Stoner::Core::uint64> GNextAllocatorIdentity{1};

[[nodiscard]] Stoner::Core::uint64 NextAllocatorIdentity() noexcept
{
    Stoner::Core::uint64 Identity =
        GNextAllocatorIdentity.fetch_add(1, std::memory_order_relaxed);
    if (Identity == 0)
    {
        Identity =
            GNextAllocatorIdentity.fetch_add(1, std::memory_order_relaxed);
    }
    return Identity;
}

[[nodiscard]] bool TryMultiply(
    Stoner::Core::uint64 Left,
    Stoner::Core::uint64 Right,
    Stoner::Core::uint64& OutValue) noexcept
{
    if (Left != 0 &&
        Right > std::numeric_limits<Stoner::Core::uint64>::max() / Left)
    {
        return false;
    }
    OutValue = Left * Right;
    return true;
}

[[nodiscard]] bool TryAdd(
    Stoner::Core::uint64 Left,
    Stoner::Core::uint64 Right,
    Stoner::Core::uint64& OutValue) noexcept
{
    if (Right > std::numeric_limits<Stoner::Core::uint64>::max() - Left)
    {
        return false;
    }
    OutValue = Left + Right;
    return true;
}

} // namespace

FVulkanMemoryAllocator::FVulkanMemoryAllocator() noexcept
    : OwnerIdentity(NextAllocatorIdentity())
{
}

void FVulkanMemoryAllocator::Reset() noexcept
{
    AllocatedBytes = 0;
    LiveAllocationCount = 0;
    LastFailure = EVulkanAllocationFailure::None;
    LastMode = EVulkanAllocationMode::Failed;
    LastReason = "";
    OwnerEpoch = OwnerEpoch ==
            std::numeric_limits<Stoner::Core::uint64>::max()
        ? 1
        : OwnerEpoch + 1;
    NextAllocationId = 1;
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
    if (!Stoner::RHI::IsValidRHIBufferDesc(Desc))
    {
        return Fail(EVulkanResourceKind::Buffer,
            EVulkanAllocationFailure::InvalidRequest,
            Desc.SizeInBytes,
            "invalid buffer allocation request");
    }
    return Allocate(EVulkanResourceKind::Buffer, EstimateBufferBytes(Desc), bDeviceActive);
}

FVulkanResourceAllocation FVulkanMemoryAllocator::AllocateTexture(const Stoner::RHI::FRHITextureDesc& Desc, bool bDeviceActive) noexcept
{
    if (!Stoner::RHI::IsValidRHITextureDesc(Desc))
    {
        return Fail(EVulkanResourceKind::Texture,
            EVulkanAllocationFailure::InvalidRequest,
            0,
            "invalid texture allocation request");
    }

    Stoner::Core::uint64 ByteSize = 0;
    if (!TryEstimateTextureBytes(Desc, ByteSize))
    {
        return Fail(EVulkanResourceKind::Texture,
            EVulkanAllocationFailure::ArithmeticOverflow,
            0,
            "texture allocation footprint is not representable");
    }
    return Allocate(EVulkanResourceKind::Texture, ByteSize, bDeviceActive);
}

Stoner::RHI::ERHIResult FVulkanMemoryAllocator::Release(FVulkanResourceAllocation& Allocation) noexcept
{
    if (Allocation.IsReleased())
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (!Allocation.BelongsTo(this, OwnerIdentity, OwnerEpoch))
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (!Allocation.IsSuccessful() || LiveAllocationCount == 0 ||
        AllocatedBytes < Allocation.GetByteSize())
    {
        return Stoner::RHI::ERHIResult::Failed;
    }

    const Stoner::RHI::ERHIResult ReleaseResult = Allocation.MarkReleased();
    if (ReleaseResult != Stoner::RHI::ERHIResult::Success)
    {
        return ReleaseResult;
    }
    AllocatedBytes -= Allocation.GetByteSize();
    --LiveAllocationCount;
    return Stoner::RHI::ERHIResult::Success;
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
    Stoner::Core::uint64 ByteSize = 0;
    return TryEstimateTextureBytes(Desc, ByteSize) ? ByteSize : 0;
}

bool FVulkanMemoryAllocator::TryEstimateTextureBytes(
    const Stoner::RHI::FRHITextureDesc& Desc,
    Stoner::Core::uint64& OutByteSize) noexcept
{
    OutByteSize = 0;
    if (!Stoner::RHI::IsValidRHITextureDesc(Desc))
    {
        return false;
    }

    const Stoner::Core::uint64 Samples =
        static_cast<Stoner::Core::uint64>(Desc.SampleCount);
    if (Samples == 0)
    {
        return false;
    }

    Stoner::Core::uint64 TotalBytes = 0;
    for (Stoner::Core::uint32 Mip = 0; Mip < Desc.MipLevels; ++Mip)
    {
        Stoner::RHI::FRHITextureFootprint Footprint;
        if (!Stoner::RHI::TryGetRHITextureFootprint(
                Desc.Format,
                Stoner::RHI::GetRHIMipExtent(Desc.Width, Mip),
                Stoner::RHI::GetRHIMipExtent(Desc.Height, Mip),
                Stoner::RHI::GetRHIMipExtent(Desc.Depth, Mip),
                Footprint))
        {
            return false;
        }
        Stoner::Core::uint64 MipBytes = Footprint.TotalBytes;
        if (!TryMultiply(MipBytes, Desc.ArrayLayers, MipBytes) ||
            !TryMultiply(MipBytes, Samples, MipBytes) ||
            !TryAdd(TotalBytes, MipBytes, TotalBytes))
        {
            OutByteSize = 0;
            return false;
        }
    }

    OutByteSize = TotalBytes;
    return TotalBytes > 0;
}

FVulkanResourceAllocation FVulkanMemoryAllocator::Fail(
    EVulkanResourceKind Kind,
    EVulkanAllocationFailure Failure,
    Stoner::Core::uint64 ByteSize,
    const char* Reason) noexcept
{
    LastFailure = Failure;
    LastMode = EVulkanAllocationMode::Failed;
    LastReason = Reason;
    return FVulkanResourceAllocation::MakeFailure(
        Kind, Failure, ByteSize, Reason);
}

FVulkanResourceAllocation FVulkanMemoryAllocator::Allocate(EVulkanResourceKind Kind, Stoner::Core::uint64 ByteSize, bool bDeviceActive) noexcept
{
    if (!bDeviceActive)
    {
        return Fail(Kind, EVulkanAllocationFailure::DeviceInactive,
            ByteSize, "device is inactive");
    }
    if (ByteSize == 0)
    {
        return Fail(Kind, EVulkanAllocationFailure::InvalidRequest,
            ByteSize, "allocation size is zero");
    }
    if (Limits.AllocationCount > 0 && LiveAllocationCount >= Limits.AllocationCount)
    {
        return Fail(Kind, EVulkanAllocationFailure::AllocationCountExceeded,
            ByteSize, "allocation count limit exceeded");
    }
    if (Limits.BudgetBytes > 0 && (ByteSize > Limits.BudgetBytes || AllocatedBytes > Limits.BudgetBytes - ByteSize))
    {
        return Fail(Kind, EVulkanAllocationFailure::BudgetExceeded,
            ByteSize, "allocation budget exceeded");
    }
    if (ByteSize > std::numeric_limits<Stoner::Core::uint64>::max() -
            AllocatedBytes ||
        LiveAllocationCount ==
            std::numeric_limits<Stoner::Core::uint32>::max() ||
        NextAllocationId ==
            std::numeric_limits<Stoner::Core::uint64>::max())
    {
        return Fail(Kind, EVulkanAllocationFailure::ArithmeticOverflow,
            ByteSize, "allocation accounting is not representable");
    }

    AllocatedBytes += ByteSize;
    ++LiveAllocationCount;
    const Stoner::Core::uint64 AllocationId = NextAllocationId++;
    LastFailure = EVulkanAllocationFailure::None;
    LastMode = bRuntimeAvailable ? EVulkanAllocationMode::RealRuntime : EVulkanAllocationMode::DeterministicFallback;
    LastReason = bRuntimeAvailable ? "runtime allocation path selected" : "runtime unavailable; deterministic fallback allocation selected";
    return FVulkanResourceAllocation::MakeSuccess(Kind, LastMode, ByteSize,
        Limits.BudgetBytes, Limits.AllocationCount, LastReason,
        this, OwnerIdentity, OwnerEpoch, AllocationId);
}

} // namespace Stoner::Backend::Vulkan
