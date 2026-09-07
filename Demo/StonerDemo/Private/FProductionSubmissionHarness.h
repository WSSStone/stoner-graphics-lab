#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIResult.h"

namespace Stoner::RHI
{
class IRHICommandBuffer;
class IRHICommandQueue;
class IRHIDevice;
class IRHIFence;
class IRHISemaphore;
}

namespace Stoner::Demo
{

struct FProductionDeferredSubmissionResult
{
    RHI::ERHIResult Result = RHI::ERHIResult::InvalidState;
    bool bSubmissionAccepted = false;
    bool bCompletionObserved = false;
    bool bRetired = false;
};

class FProductionSubmissionHarness
{
public:
    [[nodiscard]] RHI::ERHIResult Initialize(
        const Core::TSharedPtr<RHI::IRHIDevice>& Device);
    [[nodiscard]] RHI::ERHIResult SubmitAndWait(
        const Core::TSharedPtr<RHI::IRHICommandBuffer>& Commands,
        Core::uint64 TimeoutMicroseconds);
    [[nodiscard]] FProductionDeferredSubmissionResult SubmitDeferred(
        const Core::TSharedPtr<RHI::IRHICommandBuffer>& Commands,
        const Core::TArray<Core::TSharedPtr<RHI::IRHISemaphore>>&
            WaitSemaphores,
        const Core::TArray<Core::TSharedPtr<RHI::IRHISemaphore>>&
            SignalSemaphores,
        const Core::TSharedPtr<RHI::IRHIFence>& CompletionFence);
    [[nodiscard]] FProductionDeferredSubmissionResult PollDeferred(
        const Core::TSharedPtr<RHI::IRHIFence>& CompletionFence) noexcept;
    [[nodiscard]] FProductionDeferredSubmissionResult RetireDeferred(
        const Core::TSharedPtr<RHI::IRHIFence>& CompletionFence) noexcept;
    // Returns NotReady while deferred owners remain in flight. The owning
    // session must retain this harness until each frame is retired or device
    // teardown proves completion; destruction is not completion evidence.
    // Existing formal callers may ignore the result; preview shutdown must
    // inspect it.
    RHI::ERHIResult Release() noexcept;

    FProductionSubmissionHarness() = default;
    FProductionSubmissionHarness(const FProductionSubmissionHarness&) = delete;
    FProductionSubmissionHarness& operator=(
        const FProductionSubmissionHarness&) = delete;
    FProductionSubmissionHarness(FProductionSubmissionHarness&&) = delete;
    FProductionSubmissionHarness& operator=(
        FProductionSubmissionHarness&&) = delete;

private:
    struct FPendingDeferredSubmission
    {
        Core::TSharedPtr<RHI::IRHICommandBuffer> CommandBuffer;
        Core::TSharedPtr<RHI::IRHIFence> CompletionFence;
        Core::TArray<Core::TSharedPtr<RHI::IRHISemaphore>> WaitSemaphores;
        Core::TArray<Core::TSharedPtr<RHI::IRHISemaphore>> SignalSemaphores;
        RHI::ERHIResult FirstFailure = RHI::ERHIResult::Success;
        bool bSubmissionAccepted = false;
        bool bCompletionObserved = false;
    };
    [[nodiscard]] FPendingDeferredSubmission* FindPending(
        const Core::TSharedPtr<RHI::IRHIFence>& CompletionFence) noexcept;

    Core::TSharedPtr<RHI::IRHICommandQueue> Queue;
    Core::TSharedPtr<RHI::IRHIFence> Fence;
    Core::TArray<FPendingDeferredSubmission> PendingDeferredSubmissions;
};

} // namespace Stoner::Demo
