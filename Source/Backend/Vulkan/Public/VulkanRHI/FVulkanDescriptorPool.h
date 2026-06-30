#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIResult.h"
#include "RHI/ERHIPipelineState.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanDescriptorPool
{
public:
    explicit FVulkanDescriptorPool(Stoner::Core::uint32 InCapacity = 16) noexcept;

    [[nodiscard]] Stoner::Core::uint32 GetCapacity() const noexcept;
    [[nodiscard]] Stoner::Core::uint32 GetAllocatedCount() const noexcept;
    [[nodiscard]] bool IsExhausted() const noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResourceLifecycleState GetLifecycleState() const noexcept;

    Stoner::RHI::ERHIResult Allocate() noexcept;
    Stoner::RHI::ERHIResult Release() noexcept;
    Stoner::RHI::ERHIResult Invalidate() noexcept;

private:
    Stoner::Core::uint32 Capacity = 0;
    Stoner::Core::uint32 AllocatedCount = 0;
    Stoner::RHI::ERHIResourceLifecycleState LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Valid;
};

} // namespace Stoner::Backend::Vulkan
