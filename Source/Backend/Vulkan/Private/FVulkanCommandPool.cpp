#include "VulkanRHI/FVulkanCommandPool.h"

#include "VulkanRHI/FVulkanCommandBuffer.h"
#include "VulkanRHI/FVulkanDeviceOwnerState.h"
#include "VulkanRHI/FVulkanDiagnostics.h"

#include <algorithm>
#include <new>
#include <stdexcept>

namespace Stoner::Backend::Vulkan
{

FVulkanCommandPool::FVulkanCommandPool(
    Stoner::RHI::ERHIQueueType InQueueType,
    Stoner::Core::uint32 InCapacity,
    Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> InOwner) noexcept
    : QueueType(InQueueType)
    , Capacity(InCapacity)
    , Owner(std::move(InOwner))
{
}

Stoner::RHI::ERHIQueueType FVulkanCommandPool::GetQueueType() const noexcept { return QueueType; }
Stoner::Core::uint32 FVulkanCommandPool::GetCapacity() const noexcept { return Capacity; }
Stoner::Core::uint32 FVulkanCommandPool::GetAllocatedCount() const noexcept
{
    return static_cast<Stoner::Core::uint32>(std::count_if(
        CommandBuffers.begin(), CommandBuffers.end(),
        [](const auto& Candidate) { return !Candidate.expired(); }));
}
bool FVulkanCommandPool::IsValid() const noexcept
{
    return bValid && Owner && Owner->bActive;
}

Stoner::RHI::TRHIObjectResult<FVulkanCommandBuffer> FVulkanCommandPool::Allocate(FVulkanDiagnostics& Diagnostics)
{
    if (!IsValid())
    {
        MarkCommandAllocation(Diagnostics, "command pool is invalidated");
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    CommandBuffers.erase(
        std::remove_if(
            CommandBuffers.begin(), CommandBuffers.end(),
            [](const auto& Candidate) { return Candidate.expired(); }),
        CommandBuffers.end());
    if (CommandBuffers.size() >= Capacity)
    {
        MarkCommandAllocation(Diagnostics, "command buffer capacity exhausted");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }

    Stoner::Core::TSharedPtr<FVulkanCommandBuffer> CommandBuffer;
    try
    {
        CommandBuffer.reset(new FVulkanCommandBuffer(
            QueueType, &Diagnostics, Owner));
    }
    catch (const std::bad_alloc&)
    {
        MarkCommandAllocation(Diagnostics, "command buffer wrapper allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    try
    {
        CommandBuffers.push_back(CommandBuffer);
    }
    catch (const std::bad_alloc&)
    {
        CommandBuffer->Invalidate();
        MarkCommandAllocation(Diagnostics, "command pool tracking allocation failed");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        CommandBuffer->Invalidate();
        MarkCommandAllocation(Diagnostics, "command pool tracking capacity exceeded");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    MarkCommandAllocation(Diagnostics, "command buffer allocated");
    return {Stoner::RHI::ERHIResult::Success, CommandBuffer};
}

void FVulkanCommandPool::Invalidate() noexcept
{
    if (!bValid)
    {
        return;
    }
    bValid = false;
    for (const auto& WeakCommandBuffer : CommandBuffers)
    {
        if (const auto CommandBuffer = WeakCommandBuffer.lock())
        {
            CommandBuffer->Invalidate();
        }
    }
}

} // namespace Stoner::Backend::Vulkan
