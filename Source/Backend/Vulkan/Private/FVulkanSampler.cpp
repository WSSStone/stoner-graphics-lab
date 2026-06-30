#include "VulkanRHI/FVulkanSampler.h"

namespace Stoner::Backend::Vulkan
{

FVulkanSampler::FVulkanSampler(const Stoner::RHI::FRHISamplerDesc& InDesc)
    : Desc(InDesc)
{
}

const Stoner::RHI::FRHISamplerDesc& FVulkanSampler::GetDesc() const noexcept { return Desc; }
Stoner::RHI::ERHIResourceLifecycleState FVulkanSampler::GetLifecycleState() const noexcept { return LifecycleState; }

Stoner::RHI::ERHIResult FVulkanSampler::Invalidate()
{
    if (LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Invalidated)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
