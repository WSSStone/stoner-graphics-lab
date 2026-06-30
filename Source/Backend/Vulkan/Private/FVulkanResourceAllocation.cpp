#include "VulkanRHI/FVulkanResourceAllocation.h"

namespace Stoner::Backend::Vulkan
{

FVulkanResourceAllocation FVulkanResourceAllocation::MakeSuccess(EVulkanResourceKind InKind, EVulkanAllocationMode InMode, Stoner::Core::uint64 InSizeInBytes, Stoner::Core::uint64 InBudgetLimit, Stoner::Core::uint32 InCountLimit, const char* InReason) noexcept
{
    FVulkanResourceAllocation Allocation;
    Allocation.Kind = InKind;
    Allocation.Mode = InMode;
    Allocation.SizeInBytes = InSizeInBytes;
    Allocation.BudgetLimitAtAllocation = InBudgetLimit;
    Allocation.AllocationCountLimitAtAllocation = InCountLimit;
    Allocation.Reason = InReason ? InReason : "";
    return Allocation;
}

FVulkanResourceAllocation FVulkanResourceAllocation::MakeFailure(EVulkanResourceKind InKind, EVulkanAllocationFailure InFailure, Stoner::Core::uint64 InSizeInBytes, const char* InReason) noexcept
{
    FVulkanResourceAllocation Allocation;
    Allocation.Kind = InKind;
    Allocation.Mode = EVulkanAllocationMode::Failed;
    Allocation.Failure = InFailure;
    Allocation.SizeInBytes = InSizeInBytes;
    Allocation.Reason = InReason ? InReason : "";
    return Allocation;
}

bool FVulkanResourceAllocation::IsSuccessful() const noexcept
{
    return Mode != EVulkanAllocationMode::Failed && Failure == EVulkanAllocationFailure::None && !bReleased;
}

bool FVulkanResourceAllocation::IsReleased() const noexcept
{
    return bReleased;
}

Stoner::Core::uint64 FVulkanResourceAllocation::GetByteSize() const noexcept
{
    return SizeInBytes;
}

Stoner::RHI::ERHIResult FVulkanResourceAllocation::Release() noexcept
{
    if (bReleased)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    bReleased = true;
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
