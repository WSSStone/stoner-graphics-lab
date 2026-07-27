#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIQueueType.h"
#include "RHI/ERHIResult.h"
#include "RHI/IRHIDevice.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanCommandBuffer;
class FVulkanDevice;
struct FVulkanDiagnostics;

class FVulkanCommandPool final
{
public:
    ~FVulkanCommandPool() = default;
    FVulkanCommandPool(const FVulkanCommandPool&) = delete;
    FVulkanCommandPool& operator=(const FVulkanCommandPool&) = delete;

    [[nodiscard]] Stoner::RHI::ERHIQueueType GetQueueType() const noexcept;
    [[nodiscard]] Stoner::Core::uint32 GetCapacity() const noexcept;
    [[nodiscard]] Stoner::Core::uint32 GetAllocatedCount() const noexcept;
    [[nodiscard]] bool IsValid() const noexcept;

private:
    friend class FVulkanDevice;

    FVulkanCommandPool(Stoner::RHI::ERHIQueueType InQueueType,
        Stoner::Core::uint32 InCapacity) noexcept;
    Stoner::RHI::TRHIObjectResult<FVulkanCommandBuffer> Allocate(
        FVulkanDiagnostics& Diagnostics);
    void Invalidate() noexcept;

    Stoner::RHI::ERHIQueueType QueueType = Stoner::RHI::ERHIQueueType::Graphics;
    Stoner::Core::uint32 Capacity = 0;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<FVulkanCommandBuffer>> CommandBuffers;
    bool bValid = true;
};

} // namespace Stoner::Backend::Vulkan
