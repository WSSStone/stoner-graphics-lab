#include "VulkanRHI/FVulkanResourceAllocation.h"

namespace Stoner::Backend::Vulkan
{

FVulkanResourceAllocation::FVulkanResourceAllocation(
    FVulkanResourceAllocation&& Other) noexcept
    : Kind(Other.Kind)
    , Mode(Other.Mode)
    , Failure(Other.Failure)
    , SizeInBytes(Other.SizeInBytes)
    , BudgetLimitAtAllocation(Other.BudgetLimitAtAllocation)
    , AllocationCountLimitAtAllocation(
          Other.AllocationCountLimitAtAllocation)
    , Reason(Other.Reason)
    , Owner(Other.Owner)
    , OwnerIdentity(Other.OwnerIdentity)
    , OwnerEpoch(Other.OwnerEpoch)
    , AllocationId(Other.AllocationId)
    , bReleased(Other.bReleased)
{
    Other.Kind = EVulkanResourceKind::Unknown;
    Other.Mode = EVulkanAllocationMode::Failed;
    Other.Failure = EVulkanAllocationFailure::None;
    Other.SizeInBytes = 0;
    Other.BudgetLimitAtAllocation = 0;
    Other.AllocationCountLimitAtAllocation = 0;
    Other.Reason = "";
    Other.Owner = nullptr;
    Other.OwnerIdentity = 0;
    Other.OwnerEpoch = 0;
    Other.AllocationId = 0;
    Other.bReleased = true;
}

FVulkanResourceAllocation FVulkanResourceAllocation::MakeSuccess(
    EVulkanResourceKind InKind,
    EVulkanAllocationMode InMode,
    Stoner::Core::uint64 InSizeInBytes,
    Stoner::Core::uint64 InBudgetLimit,
    Stoner::Core::uint32 InCountLimit,
    const char* InReason,
    const FVulkanMemoryAllocator* InOwner,
    Stoner::Core::uint64 InOwnerIdentity,
    Stoner::Core::uint64 InOwnerEpoch,
    Stoner::Core::uint64 InAllocationId) noexcept
{
    FVulkanResourceAllocation Allocation;
    Allocation.Kind = InKind;
    Allocation.Mode = InMode;
    Allocation.SizeInBytes = InSizeInBytes;
    Allocation.BudgetLimitAtAllocation = InBudgetLimit;
    Allocation.AllocationCountLimitAtAllocation = InCountLimit;
    Allocation.Reason = InReason ? InReason : "";
    Allocation.Owner = InOwner;
    Allocation.OwnerIdentity = InOwnerIdentity;
    Allocation.OwnerEpoch = InOwnerEpoch;
    Allocation.AllocationId = InAllocationId;
    return Allocation;
}

FVulkanResourceAllocation FVulkanResourceAllocation::MakeFailure(
    EVulkanResourceKind InKind,
    EVulkanAllocationFailure InFailure,
    Stoner::Core::uint64 InSizeInBytes,
    const char* InReason) noexcept
{
    FVulkanResourceAllocation Allocation;
    Allocation.Kind = InKind;
    Allocation.Mode = EVulkanAllocationMode::Failed;
    Allocation.Failure = InFailure;
    Allocation.SizeInBytes = InSizeInBytes;
    Allocation.Reason = InReason ? InReason : "";
    return Allocation;
}

EVulkanResourceKind FVulkanResourceAllocation::GetKind() const noexcept
{
    return Kind;
}

EVulkanAllocationMode FVulkanResourceAllocation::GetMode() const noexcept
{
    return Mode;
}

EVulkanAllocationFailure
FVulkanResourceAllocation::GetFailure() const noexcept
{
    return Failure;
}

Stoner::Core::uint64
FVulkanResourceAllocation::GetByteSize() const noexcept
{
    return SizeInBytes;
}

Stoner::Core::uint64
FVulkanResourceAllocation::GetBudgetLimitAtAllocation() const noexcept
{
    return BudgetLimitAtAllocation;
}

Stoner::Core::uint32
FVulkanResourceAllocation::GetAllocationCountLimitAtAllocation() const noexcept
{
    return AllocationCountLimitAtAllocation;
}

const char* FVulkanResourceAllocation::GetReason() const noexcept
{
    return Reason;
}

bool FVulkanResourceAllocation::IsSuccessful() const noexcept
{
    return Mode != EVulkanAllocationMode::Failed &&
        Failure == EVulkanAllocationFailure::None && !bReleased && Owner &&
        OwnerIdentity != 0 && OwnerEpoch != 0 && AllocationId != 0;
}

bool FVulkanResourceAllocation::IsReleased() const noexcept
{
    return bReleased;
}

bool FVulkanResourceAllocation::BelongsTo(
    const FVulkanMemoryAllocator* InOwner,
    Stoner::Core::uint64 InOwnerIdentity,
    Stoner::Core::uint64 InOwnerEpoch) const noexcept
{
    return InOwner && Owner == InOwner &&
        OwnerIdentity == InOwnerIdentity && OwnerEpoch == InOwnerEpoch;
}

Stoner::RHI::ERHIResult
FVulkanResourceAllocation::MarkReleased() noexcept
{
    if (bReleased)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    bReleased = true;
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
