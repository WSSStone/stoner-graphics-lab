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
    BudgetExceeded,
    AllocationCountExceeded,
    DeviceInactive
};

struct FVulkanResourceAllocation
{
    EVulkanResourceKind Kind = EVulkanResourceKind::Unknown;
    EVulkanAllocationMode Mode = EVulkanAllocationMode::Failed;
    EVulkanAllocationFailure Failure = EVulkanAllocationFailure::None;
    Stoner::Core::uint64 SizeInBytes = 0;
    Stoner::Core::uint64 BudgetLimitAtAllocation = 0;
    Stoner::Core::uint32 AllocationCountLimitAtAllocation = 0;
    const char* Reason = "";
    bool bReleased = false;

    [[nodiscard]] static FVulkanResourceAllocation MakeSuccess(EVulkanResourceKind Kind, EVulkanAllocationMode Mode, Stoner::Core::uint64 SizeInBytes, Stoner::Core::uint64 BudgetLimit, Stoner::Core::uint32 CountLimit, const char* Reason) noexcept;
    [[nodiscard]] static FVulkanResourceAllocation MakeFailure(EVulkanResourceKind Kind, EVulkanAllocationFailure Failure, Stoner::Core::uint64 SizeInBytes, const char* Reason) noexcept;

    [[nodiscard]] bool IsSuccessful() const noexcept;
    [[nodiscard]] bool IsReleased() const noexcept;
    [[nodiscard]] Stoner::Core::uint64 GetByteSize() const noexcept;
    Stoner::RHI::ERHIResult Release() noexcept;
};

} // namespace Stoner::Backend::Vulkan
