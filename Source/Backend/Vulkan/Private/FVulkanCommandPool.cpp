#include "VulkanRHI/FVulkanCommandPool.h"

#include "VulkanRHI/FVulkanCommandBuffer.h"
#include "VulkanRHI/FVulkanDiagnostics.h"

#include <new>
#include <stdexcept>

namespace Stoner::Backend::Vulkan
{

FVulkanCommandPool::FVulkanCommandPool(Stoner::RHI::ERHIQueueType InQueueType, Stoner::Core::uint32 InCapacity) noexcept
    : QueueType(InQueueType)
    , Capacity(InCapacity)
{
}

Stoner::RHI::ERHIQueueType FVulkanCommandPool::GetQueueType() const noexcept { return QueueType; }
Stoner::Core::uint32 FVulkanCommandPool::GetCapacity() const noexcept { return Capacity; }
Stoner::Core::uint32 FVulkanCommandPool::GetAllocatedCount() const noexcept { return static_cast<Stoner::Core::uint32>(CommandBuffers.size()); }
bool FVulkanCommandPool::IsValid() const noexcept { return bValid; }

Stoner::RHI::TRHIObjectResult<FVulkanCommandBuffer> FVulkanCommandPool::Allocate(FVulkanDiagnostics& Diagnostics)
{
    if (!bValid)
    {
        MarkCommandAllocation(Diagnostics, "command pool is invalidated");
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (CommandBuffers.size() >= Capacity)
    {
        MarkCommandAllocation(Diagnostics, "command buffer capacity exhausted");
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }

    Stoner::Core::TSharedPtr<FVulkanCommandBuffer> CommandBuffer;
    try
    {
        CommandBuffer.reset(new FVulkanCommandBuffer(QueueType, &Diagnostics));
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
    for (const auto& CommandBuffer : CommandBuffers)
    {
        if (CommandBuffer)
        {
            CommandBuffer->Invalidate();
        }
    }
}

} // namespace Stoner::Backend::Vulkan
