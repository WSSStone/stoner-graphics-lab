#pragma once

#include "FMetalNativeObject.h"
#include "RHI/IRHIFence.h"
#include "RHI/IRHISemaphore.h"

#include <condition_variable>
#include <mutex>

namespace Stoner::Backend::Metal::Private
{

class FMetalFence final : public RHI::IRHIFence, public FMetalNativeObject
{
public:
    FMetalFence(
        Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
        void* RetainedEvent,
        bool bInitiallySignaled) noexcept;
    ~FMetalFence() override;

    [[nodiscard]] RHI::ERHIFenceState GetState() const noexcept override;
    [[nodiscard]] bool IsSignaled() const noexcept override;
    RHI::ERHIResult Wait(Core::uint64 TimeoutMicroseconds = 0) override;
    RHI::ERHIResult Reset() override;
    RHI::ERHIResult Signal() override;

    [[nodiscard]] bool CanSignalForSubmission(
        const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner) const noexcept;
    [[nodiscard]] Core::uint64 ReserveSubmissionSignal() noexcept;
    void EncodeSubmissionSignal(
        void* CommandBuffer,
        Core::uint64 Epoch) noexcept;
    void CompleteSubmissionSignal(Core::uint64 Epoch, bool bSucceeded) noexcept;

private:
    mutable std::mutex Mutex_;
    std::condition_variable Condition_;
    void* Event_ = nullptr;
    RHI::ERHIFenceState State_ = RHI::ERHIFenceState::Unsignaled;
    Core::uint64 Epoch_ = 0;
    Core::uint64 PendingEpoch_ = 0;
};

class FMetalSemaphore final
    : public RHI::IRHISemaphore,
      public FMetalNativeObject
{
public:
    FMetalSemaphore(
        Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
        void* RetainedEvent) noexcept;
    ~FMetalSemaphore() override;

    [[nodiscard]] RHI::ERHISemaphoreState GetState() const noexcept override;
    [[nodiscard]] bool IsSignaled() const noexcept override;
    RHI::ERHIResult Signal() override;
    RHI::ERHIResult Consume() override;
    RHI::ERHIResult Reset() override;

    [[nodiscard]] bool CanWaitForSubmission(
        const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner) const noexcept;
    [[nodiscard]] bool CanSignalForSubmission(
        const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner) const noexcept;
    [[nodiscard]] Core::uint64 GetWaitEpoch() const noexcept;
    [[nodiscard]] Core::uint64 ReserveSubmissionWait() noexcept;
    void CancelSubmissionWait(Core::uint64 Epoch) noexcept;
    [[nodiscard]] Core::uint64 ReserveSubmissionSignal() noexcept;
    void EncodeSubmissionWait(
        void* CommandBuffer,
        Core::uint64 Epoch) noexcept;
    void EncodeSubmissionSignal(
        void* CommandBuffer,
        Core::uint64 Epoch) noexcept;
    void CommitSubmissionWait(Core::uint64 Epoch) noexcept;
    void CompleteSubmissionSignal(Core::uint64 Epoch, bool bSucceeded) noexcept;

private:
    mutable std::mutex Mutex_;
    void* Event_ = nullptr;
    RHI::ERHISemaphoreState State_ = RHI::ERHISemaphoreState::Unsignaled;
    Core::uint64 Epoch_ = 0;
    Core::uint64 PendingEpoch_ = 0;
    Core::uint64 PendingWaitEpoch_ = 0;
    Core::uint64 ConsumedEpoch_ = 0;
};

} // namespace Stoner::Backend::Metal::Private
