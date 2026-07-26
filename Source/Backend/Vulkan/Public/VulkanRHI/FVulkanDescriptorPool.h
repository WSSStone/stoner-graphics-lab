#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIResult.h"
#include "RHI/ERHIPipelineState.h"

#include <memory>

namespace Stoner::Backend::Vulkan
{

class FVulkanDescriptorPool;
class FVulkanDescriptorSet;
class FVulkanDevice;

class FVulkanDescriptorReservation
{
public:
    FVulkanDescriptorReservation() noexcept = default;
    ~FVulkanDescriptorReservation();
    FVulkanDescriptorReservation(
        const FVulkanDescriptorReservation&) = delete;
    FVulkanDescriptorReservation& operator=(
        const FVulkanDescriptorReservation&) = delete;
    FVulkanDescriptorReservation(
        FVulkanDescriptorReservation&& Other) noexcept;
    FVulkanDescriptorReservation& operator=(
        FVulkanDescriptorReservation&& Other) noexcept;

    [[nodiscard]] bool IsActive() const noexcept;

private:
    friend class FVulkanDescriptorPool;
    friend class FVulkanDescriptorSet;

    explicit FVulkanDescriptorReservation(
        std::shared_ptr<FVulkanDescriptorPool> InPool) noexcept;
    void Reset() noexcept;

    std::shared_ptr<FVulkanDescriptorPool> Pool;
};

class FVulkanDescriptorPool
{
public:
    [[nodiscard]] Stoner::Core::uint32 GetCapacity() const noexcept;
    [[nodiscard]] Stoner::Core::uint32 GetAllocatedCount() const noexcept;
    [[nodiscard]] bool IsExhausted() const noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResourceLifecycleState GetLifecycleState() const noexcept;

    Stoner::RHI::ERHIResult Invalidate() noexcept;

private:
    friend class FVulkanDescriptorReservation;
    friend class FVulkanDevice;

    explicit FVulkanDescriptorPool(
        Stoner::Core::uint32 InCapacity) noexcept;
    Stoner::RHI::ERHIResult Acquire(
        const std::shared_ptr<FVulkanDescriptorPool>& Owner,
        FVulkanDescriptorReservation& OutReservation) noexcept;
    Stoner::RHI::ERHIResult ReleaseReservation() noexcept;

    Stoner::Core::uint32 Capacity = 0;
    Stoner::Core::uint32 AllocatedCount = 0;
    Stoner::RHI::ERHIResourceLifecycleState LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Valid;
};

} // namespace Stoner::Backend::Vulkan
