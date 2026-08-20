#pragma once

#include "FMetalCommandBuffer.h"
#include "FMetalSynchronization.h"

#include <condition_variable>
#include <mutex>

namespace Stoner::Backend::Metal::Private
{

class FMetalSubmission
{
public:
    FMetalSubmission(
        Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
        Core::TSharedPtr<FMetalCommandBuffer> CommandBuffer,
        Core::TArray<FMetalCommandRecord> Records,
        Core::TArray<Core::TSharedPtr<FMetalSemaphore>> WaitSemaphores,
        Core::TArray<Core::TSharedPtr<FMetalSemaphore>> SignalSemaphores,
        Core::TArray<Core::uint64> SignalEpochs,
        Core::TSharedPtr<FMetalFence> Fence,
        Core::uint64 FenceEpoch) noexcept;

    void Complete(bool bSucceeded) noexcept;
    [[nodiscard]] RHI::ERHIResult Wait(
        Core::uint64 TimeoutMicroseconds = 0) noexcept;
    [[nodiscard]] bool IsComplete() const noexcept;

private:
    mutable std::mutex Mutex_;
    std::condition_variable Condition_;
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner_;
    Core::TSharedPtr<FMetalCommandBuffer> CommandBuffer_;
    Core::TArray<FMetalCommandRecord> Records_;
    Core::TArray<Core::TSharedPtr<FMetalSemaphore>> WaitSemaphores_;
    Core::TArray<Core::TSharedPtr<FMetalSemaphore>> SignalSemaphores_;
    Core::TArray<Core::uint64> SignalEpochs_;
    Core::TSharedPtr<FMetalFence> Fence_;
    Core::uint64 FenceEpoch_ = 0;
    bool bComplete_ = false;
    bool bSucceeded_ = false;
};

} // namespace Stoner::Backend::Metal::Private
