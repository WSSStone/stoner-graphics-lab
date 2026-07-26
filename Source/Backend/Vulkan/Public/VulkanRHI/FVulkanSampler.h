#pragma once

#include "RHI/IRHISampler.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanDevice;

class FVulkanSampler final : public Stoner::RHI::IRHISampler
{
public:
    ~FVulkanSampler() override = default;
    FVulkanSampler(const FVulkanSampler&) = delete;
    FVulkanSampler& operator=(const FVulkanSampler&) = delete;

    [[nodiscard]] const Stoner::RHI::FRHISamplerDesc& GetDesc() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIResourceLifecycleState GetLifecycleState() const noexcept override;

    Stoner::RHI::ERHIResult Invalidate() override;

private:
    friend class FVulkanDevice;

    explicit FVulkanSampler(
        const Stoner::RHI::FRHISamplerDesc& InDesc) noexcept;

    Stoner::RHI::FRHISamplerDesc Desc;
    Stoner::RHI::ERHIResourceLifecycleState LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Valid;
};

} // namespace Stoner::Backend::Vulkan
