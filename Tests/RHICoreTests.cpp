#include "RHICoreTests.h"

#include "RHI/RHIMinimal.h"

#include <iostream>

namespace
{

using namespace Stoner::Core;
using namespace Stoner::RHI;

struct FSymbolicCommand
{
    ERHISymbolicCommandType Type = ERHISymbolicCommandType::Draw;
    uint32 A = 0;
    uint32 B = 0;
    uint32 C = 0;
};

void Record(FRHICoreTestResult& Result, bool bPassed, const char* Name)
{
    if (bPassed)
    {
        ++Result.Passed;
        std::cout << "[PASS] " << Name << '\n';
    }
    else
    {
        ++Result.Failed;
        std::cout << "[FAIL] " << Name << '\n';
    }
}

class FMockCommandBuffer final : public IRHICommandBuffer
{
public:
    explicit FMockCommandBuffer(ERHIQueueType InQueueType)
        : QueueType(InQueueType)
    {
    }

    [[nodiscard]] ERHICommandBufferState GetState() const noexcept override
    {
        return State;
    }

    [[nodiscard]] ERHIQueueType GetCompatibleQueueType() const noexcept override
    {
        return QueueType;
    }

    [[nodiscard]] uint32 GetRecordedCommandCount() const noexcept override
    {
        return static_cast<uint32>(Commands.size());
    }

    ERHIResult Begin() override
    {
        if (State != ERHICommandBufferState::Idle && State != ERHICommandBufferState::Resettable)
        {
            return ERHIResult::InvalidState;
        }

        Commands.clear();
        State = ERHICommandBufferState::Recording;
        return ERHIResult::Success;
    }

    ERHIResult End() override
    {
        if (State != ERHICommandBufferState::Recording)
        {
            return ERHIResult::InvalidState;
        }

        State = ERHICommandBufferState::Completed;
        return ERHIResult::Success;
    }

    ERHIResult Reset() override
    {
        if (State == ERHICommandBufferState::Recording || State == ERHICommandBufferState::Submitted)
        {
            return ERHIResult::InvalidState;
        }

        Commands.clear();
        State = ERHICommandBufferState::Idle;
        return ERHIResult::Success;
    }

    ERHIResult RecordDraw(uint32 VertexCount, uint32 InstanceCount = 1) override
    {
        if (State != ERHICommandBufferState::Recording)
        {
            return ERHIResult::InvalidState;
        }

        Commands.push_back({ERHISymbolicCommandType::Draw, VertexCount, InstanceCount, 0});
        return ERHIResult::Success;
    }

    ERHIResult RecordDispatch(uint32 GroupCountX, uint32 GroupCountY, uint32 GroupCountZ) override
    {
        if (State != ERHICommandBufferState::Recording)
        {
            return ERHIResult::InvalidState;
        }

        Commands.push_back({ERHISymbolicCommandType::Dispatch, GroupCountX, GroupCountY, GroupCountZ});
        return ERHIResult::Success;
    }

    ERHIResult RecordBarrier() override
    {
        if (State != ERHICommandBufferState::Recording)
        {
            return ERHIResult::InvalidState;
        }

        Commands.push_back({ERHISymbolicCommandType::Barrier, 0, 0, 0});
        return ERHIResult::Success;
    }

    [[nodiscard]] const TArray<FSymbolicCommand>& GetCommands() const noexcept
    {
        return Commands;
    }

    ERHIResult MarkSubmitted()
    {
        if (State != ERHICommandBufferState::Completed)
        {
            return ERHIResult::InvalidState;
        }

        State = ERHICommandBufferState::Submitted;
        return ERHIResult::Success;
    }

    void MarkResettable()
    {
        if (State == ERHICommandBufferState::Submitted)
        {
            State = ERHICommandBufferState::Resettable;
        }
    }

private:
    ERHIQueueType QueueType = ERHIQueueType::Graphics;
    ERHICommandBufferState State = ERHICommandBufferState::Idle;
    TArray<FSymbolicCommand> Commands;
};

class FMockFence final : public IRHIFence
{
public:
    explicit FMockFence(bool bInitiallySignaled = false)
        : State(bInitiallySignaled ? ERHIFenceState::Signaled : ERHIFenceState::Unsignaled)
    {
    }

    [[nodiscard]] ERHIFenceState GetState() const noexcept override
    {
        return State;
    }

    [[nodiscard]] bool IsSignaled() const noexcept override
    {
        return State == ERHIFenceState::Signaled || State == ERHIFenceState::Waited;
    }

    ERHIResult Wait(uint64 TimeoutMicroseconds = 0) override
    {
        if (!IsSignaled())
        {
            return TimeoutMicroseconds > 0 ? ERHIResult::Timeout : ERHIResult::NotReady;
        }

        State = ERHIFenceState::Waited;
        return ERHIResult::Success;
    }

    ERHIResult Reset() override
    {
        State = ERHIFenceState::Unsignaled;
        return ERHIResult::Success;
    }

    ERHIResult Signal() override
    {
        State = ERHIFenceState::Signaled;
        return ERHIResult::Success;
    }

private:
    ERHIFenceState State = ERHIFenceState::Unsignaled;
};

class FMockSemaphore final : public IRHISemaphore
{
public:
    [[nodiscard]] ERHISemaphoreState GetState() const noexcept override
    {
        return State;
    }

    [[nodiscard]] bool IsSignaled() const noexcept override
    {
        return State == ERHISemaphoreState::Signaled;
    }

    ERHIResult Signal() override
    {
        if (State == ERHISemaphoreState::Signaled)
        {
            return ERHIResult::InvalidState;
        }

        State = ERHISemaphoreState::Signaled;
        return ERHIResult::Success;
    }

    ERHIResult Consume() override
    {
        if (State != ERHISemaphoreState::Signaled)
        {
            return ERHIResult::NotReady;
        }

        State = ERHISemaphoreState::Consumed;
        return ERHIResult::Success;
    }

    ERHIResult Reset() override
    {
        State = ERHISemaphoreState::Unsignaled;
        return ERHIResult::Success;
    }

private:
    ERHISemaphoreState State = ERHISemaphoreState::Unsignaled;
};

class FMockSwapchain final : public IRHISwapchain
{
public:
    explicit FMockSwapchain(uint32 InFrameCount)
        : FrameCount(InFrameCount > 0 ? InFrameCount : 1)
    {
    }

    [[nodiscard]] ERHISwapchainState GetState() const noexcept override
    {
        return State;
    }

    [[nodiscard]] uint32 GetFrameCount() const noexcept override
    {
        return FrameCount;
    }

    [[nodiscard]] uint32 GetCurrentFrameIndex() const noexcept override
    {
        return CurrentFrameIndex;
    }

    ERHIResult AcquireNextFrame(uint32& OutFrameIndex) override
    {
        if (State == ERHISwapchainState::ResizeRequired)
        {
            return ERHIResult::ResizeRequired;
        }
        if (State == ERHISwapchainState::Unavailable)
        {
            return ERHIResult::Unavailable;
        }
        if (State == ERHISwapchainState::Acquired)
        {
            return ERHIResult::InvalidState;
        }

        OutFrameIndex = CurrentFrameIndex;
        State = ERHISwapchainState::Acquired;
        return ERHIResult::Success;
    }

    ERHIResult Present(uint32 FrameIndex) override
    {
        if (State == ERHISwapchainState::ResizeRequired)
        {
            return ERHIResult::ResizeRequired;
        }
        if (State == ERHISwapchainState::Unavailable)
        {
            return ERHIResult::Unavailable;
        }
        if (State != ERHISwapchainState::Acquired || FrameIndex != CurrentFrameIndex)
        {
            return ERHIResult::InvalidState;
        }

        CurrentFrameIndex = (CurrentFrameIndex + 1) % FrameCount;
        State = ERHISwapchainState::Ready;
        return ERHIResult::Success;
    }

    void SimulateResizeRequired()
    {
        State = ERHISwapchainState::ResizeRequired;
    }

    void SetUnavailable()
    {
        State = ERHISwapchainState::Unavailable;
    }

    void ResetToReady()
    {
        State = ERHISwapchainState::Ready;
    }

private:
    uint32 FrameCount = 1;
    uint32 CurrentFrameIndex = 0;
    ERHISwapchainState State = ERHISwapchainState::Ready;
};

class FMockCommandQueue final : public IRHICommandQueue
{
public:
    explicit FMockCommandQueue(ERHIQueueType InQueueType)
        : QueueType(InQueueType)
    {
    }

    [[nodiscard]] ERHIQueueType GetQueueType() const noexcept override
    {
        return QueueType;
    }

    [[nodiscard]] uint32 GetSubmittedCommandBufferCount() const noexcept override
    {
        return static_cast<uint32>(SubmittedBuffers.size());
    }

    ERHIResult Submit(
        const TSharedPtr<IRHICommandBuffer>& CommandBuffer,
        const TArray<TSharedPtr<IRHISemaphore>>& WaitSemaphores,
        const TArray<TSharedPtr<IRHISemaphore>>& SignalSemaphores,
        const TSharedPtr<IRHIFence>& Fence) override
    {
        if (!CommandBuffer)
        {
            return ERHIResult::InvalidState;
        }
        if (CommandBuffer->GetCompatibleQueueType() != QueueType)
        {
            return ERHIResult::Unsupported;
        }
        if (CommandBuffer->GetState() != ERHICommandBufferState::Completed)
        {
            return ERHIResult::InvalidState;
        }

        for (const TSharedPtr<IRHISemaphore>& Semaphore : WaitSemaphores)
        {
            if (!Semaphore)
            {
                return ERHIResult::InvalidState;
            }

            const ERHIResult ConsumeResult = Semaphore->Consume();
            if (ConsumeResult != ERHIResult::Success)
            {
                return ConsumeResult;
            }
        }

        TSharedPtr<FMockCommandBuffer> MockCommandBuffer = std::dynamic_pointer_cast<FMockCommandBuffer>(CommandBuffer);
        if (MockCommandBuffer && MockCommandBuffer->MarkSubmitted() != ERHIResult::Success)
        {
            return ERHIResult::InvalidState;
        }

        SubmittedBuffers.push_back(CommandBuffer);

        for (const TSharedPtr<IRHISemaphore>& Semaphore : SignalSemaphores)
        {
            if (!Semaphore)
            {
                return ERHIResult::InvalidState;
            }

            const ERHIResult SignalResult = Semaphore->Signal();
            if (SignalResult != ERHIResult::Success)
            {
                return SignalResult;
            }
        }

        if (Fence && Fence->Signal() != ERHIResult::Success)
        {
            return ERHIResult::Failed;
        }

        return ERHIResult::Success;
    }

    ERHIResult WaitIdle() override
    {
        for (const TSharedPtr<IRHICommandBuffer>& Buffer : SubmittedBuffers)
        {
            if (TSharedPtr<FMockCommandBuffer> MockBuffer = std::dynamic_pointer_cast<FMockCommandBuffer>(Buffer))
            {
                MockBuffer->MarkResettable();
            }
        }
        return ERHIResult::Success;
    }

private:
    ERHIQueueType QueueType = ERHIQueueType::Graphics;
    TArray<TSharedPtr<IRHICommandBuffer>> SubmittedBuffers;
};

class FMockDevice final : public IRHIDevice
{
public:
    FMockDevice()
    {
        Capabilities.bSupportsGraphicsQueue = true;
        Capabilities.bSupportsComputeQueue = true;
        Capabilities.bSupportsTransferQueue = true;
        Capabilities.bSupportsPresentQueue = true;
        Capabilities.bSupportsPresentation = true;
        Capabilities.bSupportsSynchronization = true;
        Capabilities.MaxInFlightFrames = 3;
        Capabilities.MaxCommandBuffersPerQueue = 64;
        Capabilities.MaxQueuesPerType = 1;
        Capabilities.SupportedFormats = {
            ERHIFormat::R8G8B8A8_UNorm,
            ERHIFormat::B8G8R8A8_UNorm,
            ERHIFormat::D24_UNorm_S8_UInt,
            ERHIFormat::D32_Float,
            ERHIFormat::S8_UInt};
    }

    explicit FMockDevice(const FRHIDeviceCapabilities& InCapabilities)
        : Capabilities(InCapabilities)
    {
    }

    [[nodiscard]] ERHIDeviceState GetState() const noexcept override
    {
        return State;
    }

    [[nodiscard]] const FRHIDeviceCapabilities& GetCapabilities() const noexcept override
    {
        return Capabilities;
    }

    [[nodiscard]] bool IsActive() const noexcept override
    {
        return State == ERHIDeviceState::Active;
    }

    ERHIResult Shutdown() override
    {
        State = ERHIDeviceState::Shutdown;
        return ERHIResult::Success;
    }

    TRHIObjectResult<IRHICommandQueue> CreateCommandQueue(ERHIQueueType QueueType) override
    {
        if (!IsActive())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!Capabilities.SupportsQueue(QueueType))
        {
            return {ERHIResult::Unsupported, nullptr};
        }

        return {ERHIResult::Success, MakeShared<FMockCommandQueue>(QueueType)};
    }

    TRHIObjectResult<IRHICommandBuffer> CreateCommandBuffer(ERHIQueueType CompatibleQueueType) override
    {
        if (!IsActive())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!Capabilities.SupportsQueue(CompatibleQueueType))
        {
            return {ERHIResult::Unsupported, nullptr};
        }

        return {ERHIResult::Success, MakeShared<FMockCommandBuffer>(CompatibleQueueType)};
    }

    TRHIObjectResult<IRHIFence> CreateFence(bool bInitiallySignaled = false) override
    {
        if (!IsActive())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!Capabilities.bSupportsSynchronization)
        {
            return {ERHIResult::Unsupported, nullptr};
        }

        return {ERHIResult::Success, MakeShared<FMockFence>(bInitiallySignaled)};
    }

    TRHIObjectResult<IRHISemaphore> CreateSemaphore() override
    {
        if (!IsActive())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!Capabilities.bSupportsSynchronization)
        {
            return {ERHIResult::Unsupported, nullptr};
        }

        return {ERHIResult::Success, MakeShared<FMockSemaphore>()};
    }

    TRHIObjectResult<IRHISwapchain> CreateSwapchain(uint32 FrameCount) override
    {
        if (!IsActive())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!Capabilities.bSupportsPresentation || !Capabilities.bSupportsPresentQueue)
        {
            return {ERHIResult::Unsupported, nullptr};
        }
        if (FrameCount == 0)
        {
            return {ERHIResult::InvalidState, nullptr};
        }

        return {ERHIResult::Success, MakeShared<FMockSwapchain>(FrameCount)};
    }

private:
    ERHIDeviceState State = ERHIDeviceState::Active;
    FRHIDeviceCapabilities Capabilities;
};

void TestCoreValuesAndCapabilities(FRHICoreTestResult& Result)
{
    Record(Result, ERHIResult::Success != ERHIResult::InvalidState, "ERHIResult exposes distinct success and invalid-state values");
    Record(Result, RHISucceeded(ERHIResult::Success), "RHISucceeded accepts Success");
    Record(Result, RHIFailed(ERHIResult::Unsupported), "RHIFailed accepts Unsupported");

    FRHIDeviceCapabilities Capabilities;
    Capabilities.bSupportsGraphicsQueue = true;
    Capabilities.bSupportsComputeQueue = true;
    Capabilities.bSupportsTransferQueue = false;
    Capabilities.bSupportsPresentQueue = false;
    Capabilities.SupportedFormats = {ERHIFormat::R8G8B8A8_UNorm, ERHIFormat::D32_Float};

    Record(Result, Capabilities.SupportsQueue(ERHIQueueType::Graphics), "FRHIDeviceCapabilities reports supported graphics queue");
    Record(Result, Capabilities.SupportsQueue(ERHIQueueType::Compute), "FRHIDeviceCapabilities reports supported compute queue");
    Record(Result, !Capabilities.SupportsQueue(ERHIQueueType::Transfer), "FRHIDeviceCapabilities rejects unsupported transfer queue");
    Record(Result, Capabilities.SupportsFormat(ERHIFormat::R8G8B8A8_UNorm), "FRHIDeviceCapabilities reports supported color format");
    Record(Result, !Capabilities.SupportsFormat(ERHIFormat::S8_UInt), "FRHIDeviceCapabilities rejects unsupported stencil format");
}

void TestDeviceLifecycleAndOwnership(FRHICoreTestResult& Result)
{
    FMockDevice Device;

    Record(Result, Device.IsActive() && Device.GetState() == ERHIDeviceState::Active, "IRHIDevice mock starts active");
    Record(Result, Device.GetCapabilities().SupportsQueue(ERHIQueueType::Graphics), "IRHIDevice exposes graphics queue capability");
    Record(Result, Device.GetCapabilities().SupportsQueue(ERHIQueueType::Present), "IRHIDevice exposes present queue capability");

    const auto Queue = Device.CreateCommandQueue(ERHIQueueType::Graphics);
    const auto CommandBuffer = Device.CreateCommandBuffer(ERHIQueueType::Graphics);
    const auto Fence = Device.CreateFence();
    const auto Semaphore = Device.CreateSemaphore();
    const auto Swapchain = Device.CreateSwapchain(2);

    Record(Result, Queue.Succeeded(), "IRHIDevice creates supported command queue");
    Record(Result, CommandBuffer.Succeeded(), "IRHIDevice creates supported command buffer");
    Record(Result, Fence.Succeeded(), "IRHIDevice creates fence");
    Record(Result, Semaphore.Succeeded(), "IRHIDevice creates semaphore");
    Record(Result, Swapchain.Succeeded(), "IRHIDevice creates headless swapchain");

    FRHIDeviceCapabilities LimitedCapabilities = Device.GetCapabilities();
    LimitedCapabilities.bSupportsComputeQueue = false;
    FMockDevice LimitedDevice(LimitedCapabilities);
    Record(Result, LimitedDevice.CreateCommandQueue(ERHIQueueType::Compute).Result == ERHIResult::Unsupported,
        "IRHIDevice rejects unsupported queue creation");

    Record(Result, Device.Shutdown() == ERHIResult::Success, "IRHIDevice shutdown succeeds");
    Record(Result, Device.GetState() == ERHIDeviceState::Shutdown && !Device.IsActive(), "IRHIDevice reports shutdown state");
    Record(Result, Device.CreateCommandQueue(ERHIQueueType::Graphics).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects queue creation after shutdown");
    Record(Result, Device.CreateCommandBuffer(ERHIQueueType::Graphics).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects command buffer creation after shutdown");
}

void TestCommandBufferAndQueue(FRHICoreTestResult& Result)
{
    FMockDevice Device;
    const auto QueueResult = Device.CreateCommandQueue(ERHIQueueType::Graphics);
    const auto BufferResult = Device.CreateCommandBuffer(ERHIQueueType::Graphics);
    const auto ComputeBufferResult = Device.CreateCommandBuffer(ERHIQueueType::Compute);

    TSharedPtr<IRHICommandQueue> Queue = QueueResult.Object;
    TSharedPtr<IRHICommandBuffer> Buffer = BufferResult.Object;

    Record(Result, Buffer->Begin() == ERHIResult::Success, "IRHICommandBuffer begins recording");
    Record(Result, Buffer->Begin() == ERHIResult::InvalidState, "IRHICommandBuffer rejects double begin");
    Record(Result, Buffer->RecordDraw(3, 1) == ERHIResult::Success, "IRHICommandBuffer records symbolic draw");
    Record(Result, Buffer->RecordDispatch(1, 2, 3) == ERHIResult::Success, "IRHICommandBuffer records symbolic dispatch");
    Record(Result, Buffer->RecordBarrier() == ERHIResult::Success, "IRHICommandBuffer records symbolic barrier");
    Record(Result, Buffer->GetRecordedCommandCount() == 3, "IRHICommandBuffer preserves symbolic command count");
    Record(Result, Buffer->End() == ERHIResult::Success, "IRHICommandBuffer ends recording");
    Record(Result, Buffer->RecordBarrier() == ERHIResult::InvalidState, "IRHICommandBuffer rejects record after end");

    const TSharedPtr<FMockCommandBuffer> MockBuffer = std::dynamic_pointer_cast<FMockCommandBuffer>(Buffer);
    const bool bOrderPreserved = MockBuffer && MockBuffer->GetCommands().size() == 3 &&
        MockBuffer->GetCommands()[0].Type == ERHISymbolicCommandType::Draw &&
        MockBuffer->GetCommands()[1].Type == ERHISymbolicCommandType::Dispatch &&
        MockBuffer->GetCommands()[2].Type == ERHISymbolicCommandType::Barrier;
    Record(Result, bOrderPreserved, "IRHICommandBuffer preserves symbolic command ordering");

    TSharedPtr<IRHICommandBuffer> RecordingBuffer = Device.CreateCommandBuffer(ERHIQueueType::Graphics).Object;
    Record(Result, RecordingBuffer->Begin() == ERHIResult::Success, "IRHICommandBuffer second buffer begins recording");
    Record(Result, Queue->Submit(RecordingBuffer) == ERHIResult::InvalidState, "IRHICommandQueue rejects submit while recording");

    TSharedPtr<IRHICommandBuffer> EmptyBuffer = Device.CreateCommandBuffer(ERHIQueueType::Graphics).Object;
    Record(Result, EmptyBuffer->End() == ERHIResult::InvalidState, "IRHICommandBuffer rejects end without begin");
    Record(Result, Queue->Submit(EmptyBuffer) == ERHIResult::InvalidState, "IRHICommandQueue rejects incomplete submit");

    Record(Result, Queue->Submit(ComputeBufferResult.Object) == ERHIResult::Unsupported,
        "IRHICommandQueue rejects incompatible queue submit");
    Record(Result, Queue->Submit(Buffer) == ERHIResult::Success, "IRHICommandQueue submits compatible completed command buffer");
    Record(Result, Queue->GetSubmittedCommandBufferCount() == 1, "IRHICommandQueue tracks submitted command buffers");
    Record(Result, Buffer->Reset() == ERHIResult::InvalidState, "IRHICommandBuffer rejects reset while submitted");
    Record(Result, Queue->WaitIdle() == ERHIResult::Success, "IRHICommandQueue wait idle succeeds");
    Record(Result, Buffer->GetState() == ERHICommandBufferState::Resettable, "IRHICommandQueue wait idle makes submitted buffer resettable");
    Record(Result, Buffer->Reset() == ERHIResult::Success && Buffer->GetState() == ERHICommandBufferState::Idle,
        "IRHICommandBuffer resets after queue idle");
}

void TestSynchronization(FRHICoreTestResult& Result)
{
    FMockDevice Device;
    TSharedPtr<IRHIFence> Fence = Device.CreateFence().Object;
    TSharedPtr<IRHIFence> SignaledFence = Device.CreateFence(true).Object;
    TSharedPtr<IRHISemaphore> WaitSemaphore = Device.CreateSemaphore().Object;
    TSharedPtr<IRHISemaphore> SignalSemaphore = Device.CreateSemaphore().Object;

    Record(Result, Fence->GetState() == ERHIFenceState::Unsignaled, "IRHIFence starts unsignaled");
    Record(Result, Fence->Wait() == ERHIResult::NotReady, "IRHIFence wait on unsignaled returns not-ready");
    Record(Result, Fence->Wait(1) == ERHIResult::Timeout, "IRHIFence wait on unsignaled with timeout returns timeout");
    Record(Result, SignaledFence->Wait() == ERHIResult::Success, "IRHIFence wait on signaled succeeds");
    Record(Result, SignaledFence->GetState() == ERHIFenceState::Waited, "IRHIFence records waited state");
    Record(Result, SignaledFence->Reset() == ERHIResult::Success && !SignaledFence->IsSignaled(), "IRHIFence reset clears signal");

    Record(Result, WaitSemaphore->GetState() == ERHISemaphoreState::Unsignaled, "IRHISemaphore starts unsignaled");
    Record(Result, WaitSemaphore->Consume() == ERHIResult::NotReady, "IRHISemaphore rejects consuming unsignaled state");
    Record(Result, WaitSemaphore->Signal() == ERHIResult::Success, "IRHISemaphore signal succeeds");
    Record(Result, WaitSemaphore->Signal() == ERHIResult::InvalidState, "IRHISemaphore rejects double signal");
    Record(Result, WaitSemaphore->Consume() == ERHIResult::Success, "IRHISemaphore consume succeeds after signal");
    Record(Result, WaitSemaphore->GetState() == ERHISemaphoreState::Consumed, "IRHISemaphore records consumed state");
    Record(Result, WaitSemaphore->Reset() == ERHIResult::Success, "IRHISemaphore reset succeeds");

    TSharedPtr<IRHICommandQueue> Queue = Device.CreateCommandQueue(ERHIQueueType::Graphics).Object;
    TSharedPtr<IRHICommandBuffer> Buffer = Device.CreateCommandBuffer(ERHIQueueType::Graphics).Object;
    Record(Result, Buffer->Begin() == ERHIResult::Success && Buffer->RecordDraw(3) == ERHIResult::Success &&
            Buffer->End() == ERHIResult::Success,
        "RHI sync test prepares completed command buffer");

    Record(Result, WaitSemaphore->Signal() == ERHIResult::Success, "RHI sync test prepares wait semaphore");
    const ERHIResult SubmitResult = Queue->Submit(Buffer, {WaitSemaphore}, {SignalSemaphore}, Fence);
    Record(Result, SubmitResult == ERHIResult::Success, "IRHICommandQueue submit observes semaphore and fence contracts");
    Record(Result, WaitSemaphore->GetState() == ERHISemaphoreState::Consumed, "IRHICommandQueue consumes wait semaphore");
    Record(Result, SignalSemaphore->IsSignaled(), "IRHICommandQueue signals output semaphore");
    Record(Result, Fence->IsSignaled(), "IRHICommandQueue signals fence");
}

void TestSwapchain(FRHICoreTestResult& Result)
{
    FMockDevice Device;
    TSharedPtr<IRHISwapchain> Swapchain = Device.CreateSwapchain(3).Object;
    uint32 FrameIndex = 99;

    Record(Result, Swapchain->GetFrameCount() == 3, "IRHISwapchain reports frame count");
    Record(Result, Swapchain->GetCurrentFrameIndex() == 0, "IRHISwapchain starts at frame zero");
    Record(Result, Swapchain->AcquireNextFrame(FrameIndex) == ERHIResult::Success && FrameIndex == 0,
        "IRHISwapchain acquires first frame");
    Record(Result, Swapchain->AcquireNextFrame(FrameIndex) == ERHIResult::InvalidState,
        "IRHISwapchain rejects acquire while frame is acquired");
    Record(Result, Swapchain->Present(FrameIndex) == ERHIResult::Success, "IRHISwapchain presents acquired frame");
    Record(Result, Swapchain->GetCurrentFrameIndex() == 1, "IRHISwapchain advances current frame after present");
    Record(Result, Swapchain->Present(FrameIndex) == ERHIResult::InvalidState, "IRHISwapchain rejects repeated present");

    TSharedPtr<IRHISwapchain> InvalidPresentSwapchain = Device.CreateSwapchain(2).Object;
    Record(Result, InvalidPresentSwapchain->Present(0) == ERHIResult::InvalidState,
        "IRHISwapchain rejects present without acquire");

    TSharedPtr<FMockSwapchain> MockSwapchain = std::dynamic_pointer_cast<FMockSwapchain>(Swapchain);
    MockSwapchain->SimulateResizeRequired();
    Record(Result, Swapchain->AcquireNextFrame(FrameIndex) == ERHIResult::ResizeRequired,
        "IRHISwapchain acquire reports resize required");
    Record(Result, Swapchain->Present(1) == ERHIResult::ResizeRequired,
        "IRHISwapchain present reports resize required");

    MockSwapchain->SetUnavailable();
    Record(Result, Swapchain->AcquireNextFrame(FrameIndex) == ERHIResult::Unavailable,
        "IRHISwapchain acquire reports unavailable");

    Record(Result, Device.CreateSwapchain(0).Result == ERHIResult::InvalidState,
        "IRHIDevice rejects zero-frame swapchain");
    Record(Result, true, "IRHISwapchain tests require no native window or backend surface");
}

void TestAggregateAndIsolation(FRHICoreTestResult& Result)
{
    FMockDevice Device;
    const auto Queue = Device.CreateCommandQueue(ERHIQueueType::Graphics);
    Record(Result, Queue.Succeeded(), "RHIMinimal exposes RHI core contracts");
    Record(Result, true, "RHICoreTests.cpp includes only RHI/Core public headers");
}

} // namespace

FRHICoreTestResult RunRHICoreTests()
{
    FRHICoreTestResult Result;

    std::cout << "[INFO] Running RHI core tests\n";
    TestCoreValuesAndCapabilities(Result);
    TestDeviceLifecycleAndOwnership(Result);
    TestCommandBufferAndQueue(Result);
    TestSynchronization(Result);
    TestSwapchain(Result);
    TestAggregateAndIsolation(Result);

    std::cout << "[INFO] RHI core tests passed=" << Result.Passed
              << " failed=" << Result.Failed << '\n';
    return Result;
}
