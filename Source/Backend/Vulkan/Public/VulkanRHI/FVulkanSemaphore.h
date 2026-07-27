#pragma once

#include "RHI/RHIMinimal.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanDevice;
class FVulkanQueue;
struct FVulkanDeviceOwnerState;

class FVulkanSemaphore final : public Stoner::RHI::IRHISemaphore
{
public:
    ~FVulkanSemaphore() override = default;

    [[nodiscard]] Stoner::RHI::ERHISemaphoreState GetState() const noexcept override;
    [[nodiscard]] bool IsSignaled() const noexcept override;

    Stoner::RHI::ERHIResult Signal() override;
    Stoner::RHI::ERHIResult Consume() override;
    Stoner::RHI::ERHIResult Reset() override;
private:
    friend class FVulkanDevice;
    friend class FVulkanQueue;

    explicit FVulkanSemaphore(
        Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> InOwner) noexcept;
    [[nodiscard]] bool BelongsTo(
        const Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState>& InOwner) const noexcept;
    [[nodiscard]] bool CanConsumeForSubmission() const noexcept;
    [[nodiscard]] bool CanSignalForSubmission() const noexcept;
    void CommitConsumeForSubmission() noexcept;
    void CommitSignalForSubmission() noexcept;
    void Invalidate() noexcept;

    Stoner::RHI::ERHISemaphoreState State = Stoner::RHI::ERHISemaphoreState::Unsignaled;
    Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> Owner;
    bool bValid = true;
};

} // namespace Stoner::Backend::Vulkan
