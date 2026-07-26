#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIResult.h"

namespace Stoner::Backend::Vulkan
{

enum class EVulkanResourceKind
{
    Unknown,
    Buffer,
    Texture
};

enum class EVulkanAllocationMode
{
    RealRuntime,
    DeterministicFallback,
    Failed
};

enum class EVulkanAllocationFailure
{
    None,
    InvalidRequest,
    ArithmeticOverflow,
    BudgetExceeded,
    AllocationCountExceeded,
    DeviceInactive
};

class FVulkanMemoryAllocator;

class FVulkanResourceAllocation
{
public:
    FVulkanResourceAllocation() = default;
    FVulkanResourceAllocation(const FVulkanResourceAllocation&) = delete;
    FVulkanResourceAllocation& operator=(
        const FVulkanResourceAllocation&) = delete;
    FVulkanResourceAllocation(FVulkanResourceAllocation&& Other) noexcept;
    FVulkanResourceAllocation& operator=(
        FVulkanResourceAllocation&& Other) noexcept = delete;

    [[nodiscard]] EVulkanResourceKind GetKind() const noexcept;
    [[nodiscard]] EVulkanAllocationMode GetMode() const noexcept;
    [[nodiscard]] EVulkanAllocationFailure GetFailure() const noexcept;
    [[nodiscard]] Stoner::Core::uint64 GetByteSize() const noexcept;
    [[nodiscard]] Stoner::Core::uint64 GetBudgetLimitAtAllocation() const noexcept;
    [[nodiscard]] Stoner::Core::uint32 GetAllocationCountLimitAtAllocation() const noexcept;
    [[nodiscard]] const char* GetReason() const noexcept;

    [[nodiscard]] bool IsSuccessful() const noexcept;
    [[nodiscard]] bool IsReleased() const noexcept;

private:
    friend class FVulkanMemoryAllocator;

    [[nodiscard]] static FVulkanResourceAllocation MakeSuccess(
        EVulkanResourceKind Kind,
        EVulkanAllocationMode Mode,
        Stoner::Core::uint64 SizeInBytes,
        Stoner::Core::uint64 BudgetLimit,
        Stoner::Core::uint32 CountLimit,
        const char* Reason,
        const FVulkanMemoryAllocator* Owner,
        Stoner::Core::uint64 OwnerIdentity,
        Stoner::Core::uint64 OwnerEpoch,
        Stoner::Core::uint64 AllocationId) noexcept;
    [[nodiscard]] static FVulkanResourceAllocation MakeFailure(
        EVulkanResourceKind Kind,
        EVulkanAllocationFailure Failure,
        Stoner::Core::uint64 SizeInBytes,
        const char* Reason) noexcept;
    [[nodiscard]] bool BelongsTo(const FVulkanMemoryAllocator* Owner,
        Stoner::Core::uint64 OwnerIdentity,
        Stoner::Core::uint64 OwnerEpoch) const noexcept;
    Stoner::RHI::ERHIResult MarkReleased() noexcept;

    EVulkanResourceKind Kind = EVulkanResourceKind::Unknown;
    EVulkanAllocationMode Mode = EVulkanAllocationMode::Failed;
    EVulkanAllocationFailure Failure = EVulkanAllocationFailure::None;
    Stoner::Core::uint64 SizeInBytes = 0;
    Stoner::Core::uint64 BudgetLimitAtAllocation = 0;
    Stoner::Core::uint32 AllocationCountLimitAtAllocation = 0;
    const char* Reason = "";
    const FVulkanMemoryAllocator* Owner = nullptr;
    Stoner::Core::uint64 OwnerIdentity = 0;
    Stoner::Core::uint64 OwnerEpoch = 0;
    Stoner::Core::uint64 AllocationId = 0;
    bool bReleased = false;
};

} // namespace Stoner::Backend::Vulkan
