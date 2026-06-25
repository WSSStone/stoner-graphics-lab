#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIQueueType.h"
#include "RHI/ERHIResult.h"
#include "RHI/FRHIDeviceCapabilities.h"

namespace Stoner::RHI
{

class IRHICommandBuffer;
class IRHICommandQueue;
class IRHIFence;
class IRHISemaphore;
class IRHISwapchain;

enum class ERHIDeviceState
{
    Uninitialized,
    Active,
    Shutdown
};

template <typename T>
struct TRHIObjectResult
{
    ERHIResult Result = ERHIResult::Failed;
    Stoner::Core::TSharedPtr<T> Object;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return Result == ERHIResult::Success && Object != nullptr;
    }
};

class IRHIDevice
{
public:
    virtual ~IRHIDevice() = default;

    [[nodiscard]] virtual ERHIDeviceState GetState() const noexcept = 0;
    [[nodiscard]] virtual const FRHIDeviceCapabilities& GetCapabilities() const noexcept = 0;
    [[nodiscard]] virtual bool IsActive() const noexcept = 0;

    virtual ERHIResult Shutdown() = 0;

    virtual TRHIObjectResult<IRHICommandQueue> CreateCommandQueue(ERHIQueueType QueueType) = 0;
    virtual TRHIObjectResult<IRHICommandBuffer> CreateCommandBuffer(ERHIQueueType CompatibleQueueType) = 0;
    virtual TRHIObjectResult<IRHIFence> CreateFence(bool bInitiallySignaled = false) = 0;
    virtual TRHIObjectResult<IRHISemaphore> CreateSemaphore() = 0;
    virtual TRHIObjectResult<IRHISwapchain> CreateSwapchain(Stoner::Core::uint32 FrameCount) = 0;
};

} // namespace Stoner::RHI
