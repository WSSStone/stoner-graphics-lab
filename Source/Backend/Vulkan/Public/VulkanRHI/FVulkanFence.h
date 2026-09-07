#pragma once

#include "RHI/RHIMinimal.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanDevice;
class FVulkanQueue;
class FVulkanNativeContext;
class FDeferredNativeSubmission;
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
    friend class FVulkanNativeContext;
    friend class FDeferredNativeSubmission;

    FVulkanFence(bool bInitiallySignaled,
        Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> InOwner) noexcept;
    [[nodiscard]] bool BelongsTo(
        const Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState>& InOwner) const noexcept;
    [[nodiscard]] bool CanSignalForSubmission() const noexcept;
    void CommitSignalForSubmission() noexcept;
    void AttachNativeSubmission(
        FVulkanNativeContext* InContext,
        Stoner::Core::uint64 SubmissionId) noexcept;
    void CompleteNativeSubmission(bool bSucceeded) noexcept;
    void Invalidate() noexcept;

    Stoner::RHI::ERHIFenceState State;
    Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> Owner;
    FVulkanNativeContext* NativeContext = nullptr;
    Stoner::Core::uint64 NativeSubmissionId = 0;
    bool bTerminalFailure = false;
    bool bValid = true;
};

} // namespace Stoner::Backend::Vulkan
