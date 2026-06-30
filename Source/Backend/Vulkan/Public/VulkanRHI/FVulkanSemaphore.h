#pragma once

#include "RHI/RHIMinimal.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanSemaphore final : public Stoner::RHI::IRHISemaphore
{
public:
    [[nodiscard]] Stoner::RHI::ERHISemaphoreState GetState() const noexcept override;
    [[nodiscard]] bool IsSignaled() const noexcept override;

    Stoner::RHI::ERHIResult Signal() override;
    Stoner::RHI::ERHIResult Consume() override;
    Stoner::RHI::ERHIResult Reset() override;
    void Invalidate() noexcept;

private:
    Stoner::RHI::ERHISemaphoreState State = Stoner::RHI::ERHISemaphoreState::Unsignaled;
    bool bValid = true;
};

} // namespace Stoner::Backend::Vulkan
