#pragma once

#include "RHI/IRHISampler.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanSampler final : public Stoner::RHI::IRHISampler
{
public:
    explicit FVulkanSampler(const Stoner::RHI::FRHISamplerDesc& InDesc);

    [[nodiscard]] const Stoner::RHI::FRHISamplerDesc& GetDesc() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIResourceLifecycleState GetLifecycleState() const noexcept override;

    Stoner::RHI::ERHIResult Invalidate() override;

private:
    Stoner::RHI::FRHISamplerDesc Desc;
    Stoner::RHI::ERHIResourceLifecycleState LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Valid;
};

} // namespace Stoner::Backend::Vulkan
