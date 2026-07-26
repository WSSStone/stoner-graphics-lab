#include "Tests/RHICoreTests.cpp"

#include <iostream>

int main()
{
    bool bPassed = true;
    FMockDevice Device;

    IRHIDevice& BaseDevice = Device;
    FRHISwapchainDesc InvalidDesc;
    const auto SurfaceResult = BaseDevice.CreateSwapchain(nullptr, InvalidDesc);
    const bool bSurfaceFailedClosed =
        SurfaceResult.Result == ERHIResult::Unsupported && !SurfaceResult.Object;
    bPassed = bPassed && bSurfaceFailedClosed;

    FLegacyCommandBuffer LegacyCommands;
    IRHICommandBuffer& BaseCommands = LegacyCommands;
    FRHIRenderPassClearValues Clears;
    Clears.Colors.push_back({1.0f, 0.0f, 0.0f, 1.0f});
    const bool bClearsFailedClosed =
        BaseCommands.BeginRenderPass(nullptr, nullptr, Clears) == ERHIResult::Unsupported &&
        !LegacyCommands.WasLegacyBeginCalled();
    bPassed = bPassed && bClearsFailedClosed;

    auto LegacySwapchain = MakeShared<FLegacySwapchain>();
    TSharedPtr<IRHISwapchain> BaseSwapchain = LegacySwapchain;
    auto LegacySignal = Device.CreateSemaphore().Object;
    uint32 LegacyIndex = 99;
    const bool bSwapchainFailedClosed =
        BaseSwapchain->AcquireNextFrame(LegacyIndex, LegacySignal) == ERHIResult::Unsupported &&
        LegacyIndex == 99 &&
        !LegacySwapchain->WasLegacyAcquireCalled() &&
        !LegacySignal->IsSignaled();
    bPassed = bPassed && bSwapchainFailedClosed;

    auto AtomicSwapchain = Device.CreateSwapchain(2).Object;
    auto AlreadySignaled = Device.CreateSemaphore().Object;
    AlreadySignaled->Signal();
    uint32 AtomicIndex = 99;
    const bool bAcquireAtomic =
        AtomicSwapchain->AcquireNextFrame(AtomicIndex, AlreadySignaled) == ERHIResult::InvalidState &&
        AtomicIndex == 99 &&
        AtomicSwapchain->GetState() == ERHISwapchainState::Ready &&
        AlreadySignaled->IsSignaled();
    bPassed = bPassed && bAcquireAtomic;

    auto PartialWaitQueue = Device.CreateCommandQueue(ERHIQueueType::Graphics).Object;
    auto PartialWaitBuffer = Device.CreateCommandBuffer(ERHIQueueType::Graphics).Object;
    PartialWaitBuffer->Begin();
    PartialWaitBuffer->RecordDraw(3);
    PartialWaitBuffer->End();
    auto FirstWait = Device.CreateSemaphore().Object;
    auto SecondWait = Device.CreateSemaphore().Object;
    FirstWait->Signal();
    const bool bWaitAtomic =
        PartialWaitQueue->Submit(PartialWaitBuffer, {FirstWait, SecondWait}) == ERHIResult::NotReady &&
        FirstWait->IsSignaled() &&
        PartialWaitBuffer->GetState() == ERHICommandBufferState::Completed &&
        PartialWaitQueue->GetSubmittedCommandBufferCount() == 0;
    bPassed = bPassed && bWaitAtomic;

    auto SignalQueue = Device.CreateCommandQueue(ERHIQueueType::Graphics).Object;
    auto SignalBuffer = Device.CreateCommandBuffer(ERHIQueueType::Graphics).Object;
    SignalBuffer->Begin();
    SignalBuffer->RecordDraw(3);
    SignalBuffer->End();
    auto InvalidOutput = Device.CreateSemaphore().Object;
    auto Fence = Device.CreateFence().Object;
    InvalidOutput->Signal();
    const bool bSignalAtomic =
        SignalQueue->Submit(SignalBuffer, {}, {InvalidOutput}, Fence) == ERHIResult::InvalidState &&
        SignalBuffer->GetState() == ERHICommandBufferState::Completed &&
        SignalQueue->GetSubmittedCommandBufferCount() == 0 &&
        !Fence->IsSignaled();
    bPassed = bPassed && bSignalAtomic;

    std::cout << "surface_fail_closed=" << bSurfaceFailedClosed << '\n'
              << "clear_fail_closed=" << bClearsFailedClosed << '\n'
              << "swapchain_fail_closed=" << bSwapchainFailedClosed << '\n'
              << "acquire_atomic=" << bAcquireAtomic << '\n'
              << "queue_wait_atomic=" << bWaitAtomic << '\n'
              << "queue_signal_atomic=" << bSignalAtomic << '\n'
              << "classification=" << (bPassed ? "fixed" : "failed") << '\n';
    return bPassed ? 0 : 3;
}
