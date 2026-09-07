#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIQueueType.h"
#include "RHI/ERHIResult.h"

namespace Stoner::RHI
{

class IRHICommandBuffer;
class IRHIFence;
class IRHISemaphore;

class IRHICommandQueue
{
public:
    virtual ~IRHICommandQueue() = default;

    [[nodiscard]] virtual ERHIQueueType GetQueueType() const noexcept = 0;
    [[nodiscard]] virtual Stoner::Core::uint32 GetSubmittedCommandBufferCount() const noexcept = 0;

    virtual ERHIResult Submit(
        const Stoner::Core::TSharedPtr<IRHICommandBuffer>& CommandBuffer,
        const Stoner::Core::TArray<Stoner::Core::TSharedPtr<IRHISemaphore>>& WaitSemaphores = {},
        const Stoner::Core::TArray<Stoner::Core::TSharedPtr<IRHISemaphore>>& SignalSemaphores = {},
        const Stoner::Core::TSharedPtr<IRHIFence>& Fence = nullptr) = 0;

    // Interactive preview submission is explicitly asynchronous. The
    // completion fence is mandatory so acceptance can never be confused with
    // render completion. Backends opt in by overriding this seam; legacy
    // queues remain available and fail closed without forwarding to Submit.
    virtual ERHIResult SubmitDeferred(
        const Stoner::Core::TSharedPtr<IRHICommandBuffer>& CommandBuffer,
        const Stoner::Core::TArray<Stoner::Core::TSharedPtr<IRHISemaphore>>& WaitSemaphores,
        const Stoner::Core::TArray<Stoner::Core::TSharedPtr<IRHISemaphore>>& SignalSemaphores,
        const Stoner::Core::TSharedPtr<IRHIFence>& CompletionFence)
    {
        (void)CommandBuffer;
        (void)WaitSemaphores;
        (void)SignalSemaphores;
        if (!CompletionFence)
        {
            return ERHIResult::InvalidState;
        }
        return ERHIResult::Unsupported;
    }

    virtual ERHIResult WaitIdle() = 0;
};

} // namespace Stoner::RHI
