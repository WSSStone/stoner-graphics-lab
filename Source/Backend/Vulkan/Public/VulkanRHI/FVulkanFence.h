#pragma once

#include "RHI/RHIMinimal.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanDevice;
class FVulkanQueue;
struct FVulkanDeviceOwnerState;

class FVulkanFence final : public Stoner::RHI::IRHIFence
{
public:
    ~FVulkanFence() override = default;

    [[nodiscard]] Stoner::RHI::ERHIFenceState GetState() const noexcept override;
    [[nodiscard]] bool IsSignaled() const noexcept override;

    Stoner::RHI::ERHIResult Wait(Stoner::Core::uint64 TimeoutMicroseconds = 0) override;
    Stoner::RHI::ERHIResult Reset() override;
    Stoner::RHI::ERHIResult Signal() override;
private:
    friend class FVulkanDevice;
    friend class FVulkanQueue;

    FVulkanFence(bool bInitiallySignaled,
        Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> InOwner) noexcept;
    [[nodiscard]] bool BelongsTo(
        const Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState>& InOwner) const noexcept;
    [[nodiscard]] bool CanSignalForSubmission() const noexcept;
    void CommitSignalForSubmission() noexcept;
    void Invalidate() noexcept;

    Stoner::RHI::ERHIFenceState State;
    Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> Owner;
    bool bValid = true;
};

} // namespace Stoner::Backend::Vulkan
