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
    virtual ERHIResult WaitIdle() = 0;
};

} // namespace Stoner::RHI
