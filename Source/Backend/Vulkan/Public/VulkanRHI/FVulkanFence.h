#pragma once

#include "RHI/RHIMinimal.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanFence final : public Stoner::RHI::IRHIFence
{
public:
    explicit FVulkanFence(bool bInitiallySignaled = false) noexcept;

    [[nodiscard]] Stoner::RHI::ERHIFenceState GetState() const noexcept override;
    [[nodiscard]] bool IsSignaled() const noexcept override;

    Stoner::RHI::ERHIResult Wait(Stoner::Core::uint64 TimeoutMicroseconds = 0) override;
    Stoner::RHI::ERHIResult Reset() override;
    Stoner::RHI::ERHIResult Signal() override;
    void Invalidate() noexcept;

private:
    Stoner::RHI::ERHIFenceState State;
    bool bValid = true;
};

} // namespace Stoner::Backend::Vulkan
