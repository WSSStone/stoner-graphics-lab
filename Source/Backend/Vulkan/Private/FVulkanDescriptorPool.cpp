#include "VulkanRHI/FVulkanDescriptorPool.h"

namespace Stoner::Backend::Vulkan
{

FVulkanDescriptorReservation::FVulkanDescriptorReservation(
    std::shared_ptr<FVulkanDescriptorPool> InPool) noexcept
    : Pool(std::move(InPool))
{
}

FVulkanDescriptorReservation::~FVulkanDescriptorReservation()
{
    Reset();
}

FVulkanDescriptorReservation::FVulkanDescriptorReservation(
    FVulkanDescriptorReservation&& Other) noexcept
    : Pool(std::move(Other.Pool))
{
}

FVulkanDescriptorReservation& FVulkanDescriptorReservation::operator=(
    FVulkanDescriptorReservation&& Other) noexcept
{
    if (this != &Other)
    {
        Reset();
        Pool = std::move(Other.Pool);
    }
    return *this;
}

bool FVulkanDescriptorReservation::IsActive() const noexcept
{
    return Pool != nullptr;
}

void FVulkanDescriptorReservation::Reset() noexcept
{
    if (Pool)
    {
        const std::shared_ptr<FVulkanDescriptorPool> Owner =
            std::move(Pool);
        (void)Owner->ReleaseReservation();
    }
}

FVulkanDescriptorPool::FVulkanDescriptorPool(Stoner::Core::uint32 InCapacity) noexcept
    : Capacity(InCapacity)
{
}

Stoner::Core::uint32 FVulkanDescriptorPool::GetCapacity() const noexcept { return Capacity; }
Stoner::Core::uint32 FVulkanDescriptorPool::GetAllocatedCount() const noexcept { return AllocatedCount; }
bool FVulkanDescriptorPool::IsExhausted() const noexcept { return AllocatedCount >= Capacity; }
Stoner::RHI::ERHIResourceLifecycleState FVulkanDescriptorPool::GetLifecycleState() const noexcept { return LifecycleState; }

Stoner::RHI::ERHIResult FVulkanDescriptorPool::Acquire(
    const std::shared_ptr<FVulkanDescriptorPool>& Owner,
    FVulkanDescriptorReservation& OutReservation) noexcept
{
    if (LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Invalidated)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (!Owner || Owner.get() != this || OutReservation.IsActive())
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (IsExhausted())
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    OutReservation = FVulkanDescriptorReservation(Owner);
    ++AllocatedCount;
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanDescriptorPool::ReleaseReservation() noexcept
{
    if (AllocatedCount == 0)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    --AllocatedCount;
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanDescriptorPool::Invalidate() noexcept
{
    if (LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Invalidated)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
