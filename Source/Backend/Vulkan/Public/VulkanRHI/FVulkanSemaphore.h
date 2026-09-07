#pragma once

#include "RHI/RHIMinimal.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanDevice;
class FVulkanQueue;
class FVulkanNativeContext;
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
    friend class FVulkanNativeContext;

    explicit FVulkanSemaphore(
        Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> InOwner) noexcept;
    [[nodiscard]] bool BelongsTo(
        const Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState>& InOwner) const noexcept;
    [[nodiscard]] bool CanConsumeForSubmission() const noexcept;
    [[nodiscard]] bool CanSignalForSubmission() const noexcept;
    void CommitConsumeForSubmission() noexcept;
    void CommitSignalForSubmission() noexcept;
    [[nodiscard]] Stoner::Core::uint64 GetNativeHandleValue() const noexcept
    {
        return NativeHandleValue;
    }
    [[nodiscard]] Stoner::Core::uint64 GetLabBindingToken() const noexcept
    {
        return bLabBound ? LabBindingToken : 0;
    }
    [[nodiscard]] bool IsLabAcquireSemaphore() const noexcept
    {
        return bLabBound && bLabAcquireSemaphore;
    }
    [[nodiscard]] bool BindLabAcquireSemaphore(
        Stoner::Core::uint64 AcquisitionToken,
        Stoner::Core::uint64 NativeHandleValueIn) noexcept;
    [[nodiscard]] bool BindLabRenderSemaphore(
        Stoner::Core::uint64 AcquisitionToken,
        Stoner::Core::uint64 NativeHandleValueIn) noexcept;
    [[nodiscard]] bool IsLabBoundTo(
        Stoner::Core::uint64 AcquisitionToken) const noexcept
    {
        return bLabBound && LabBindingToken == AcquisitionToken;
    }
    void MarkLabAcquireReady() noexcept;
    void RetireLabBinding() noexcept;
    void AdoptOwner(
        Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> InOwner) noexcept;
    void Invalidate() noexcept;

    Stoner::RHI::ERHISemaphoreState State = Stoner::RHI::ERHISemaphoreState::Unsignaled;
    Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> Owner;
    Stoner::Core::uint64 NativeHandleValue = 0;
    Stoner::Core::uint64 LabBindingToken = 0;
    bool bLabBound = false;
    bool bLabAcquireSemaphore = false;
    bool bValid = true;
};

} // namespace Stoner::Backend::Vulkan
