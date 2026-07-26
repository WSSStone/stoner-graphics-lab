#include "Tests/RHICoreTests.cpp"

#include <cstring>
#include <iostream>

namespace
{

class FVerifierLegacyCommandBuffer final : public IRHICommandBuffer
{
public:
    [[nodiscard]] ERHICommandBufferState GetState() const noexcept override
    {
        return ERHICommandBufferState::Recording;
    }
    [[nodiscard]] ERHIQueueType GetCompatibleQueueType() const noexcept override
    {
        return ERHIQueueType::Graphics;
    }
    [[nodiscard]] uint32 GetRecordedCommandCount() const noexcept override { return 0; }
    ERHIResult Begin() override { return ERHIResult::Success; }
    ERHIResult End() override { return ERHIResult::Success; }
    ERHIResult Reset() override { return ERHIResult::Success; }
    ERHIResult RecordDraw(uint32, uint32) override { return ERHIResult::Unsupported; }
    ERHIResult RecordDrawIndexed(uint32, uint32, uint32) override { return ERHIResult::Unsupported; }
    ERHIResult RecordDispatch(uint32, uint32, uint32) override { return ERHIResult::Unsupported; }
    ERHIResult BindGraphicsPipeline(const TSharedPtr<IRHIGraphicsPipeline>&) override
    {
        return ERHIResult::Unsupported;
    }
    ERHIResult BindComputePipeline(const TSharedPtr<IRHIComputePipeline>&) override
    {
        return ERHIResult::Unsupported;
    }
    ERHIResult RecordBarrier() override { return ERHIResult::Unsupported; }
    ERHIResult RecordBarrier(const FRHIResourceBarrierDesc&) override
    {
        return ERHIResult::Unsupported;
    }
    ERHIResult RecordBufferCopy(
        const TSharedPtr<IRHIBuffer>&,
        const TSharedPtr<IRHIBuffer>&,
        FRHIBufferCopyRange) override
    {
        return ERHIResult::Unsupported;
    }
    ERHIResult RecordTextureCopy(
        const TSharedPtr<IRHITexture>&,
        const TSharedPtr<IRHITexture>&,
        FRHITextureCopyRegion) override
    {
        return ERHIResult::Unsupported;
    }
    ERHIResult RecordLayoutTransition(const FRHIResourceBarrierDesc&) override
    {
        return ERHIResult::Unsupported;
    }
    ERHIResult BeginRenderPass(
        const TSharedPtr<IRHIRenderPass>&,
        const TSharedPtr<IRHIFramebuffer>&) override
    {
        bLegacyBeginCalled = true;
        return ERHIResult::Success;
    }
    ERHIResult EndRenderPass() override { return ERHIResult::Success; }

    [[nodiscard]] bool WasLegacyBeginCalled() const noexcept
    {
        return bLegacyBeginCalled;
    }

private:
    bool bLegacyBeginCalled = false;
};

class FVerifierLegacySwapchain final : public IRHISwapchain
{
public:
    [[nodiscard]] ERHISwapchainState GetState() const noexcept override { return State; }
    [[nodiscard]] uint32 GetFrameCount() const noexcept override { return 2; }
    [[nodiscard]] uint32 GetCurrentFrameIndex() const noexcept override { return 0; }

    ERHIResult AcquireNextFrame(uint32& OutFrameIndex) override
    {
        bAcquireCalled = true;
        OutFrameIndex = 0;
        State = ERHISwapchainState::Acquired;
        return ERHIResult::Success;
    }
    ERHIResult Present(uint32) override
    {
        bPresentCalled = true;
        State = ERHISwapchainState::Ready;
        return ERHIResult::Success;
    }

    [[nodiscard]] bool WasAcquireCalled() const noexcept { return bAcquireCalled; }
    [[nodiscard]] bool WasPresentCalled() const noexcept { return bPresentCalled; }

private:
    ERHISwapchainState State = ERHISwapchainState::Ready;
    bool bAcquireCalled = false;
    bool bPresentCalled = false;
};

bool PrepareCompleted(
    const TSharedPtr<IRHICommandBuffer>& CommandBuffer)
{
    return CommandBuffer->Begin() == ERHIResult::Success &&
        CommandBuffer->RecordDraw(3) == ERHIResult::Success &&
        CommandBuffer->End() == ERHIResult::Success;
}

} // namespace

int main(int Argc, char** Argv)
{
    if (Argc != 2 ||
        (std::strcmp(Argv[1], "parent") != 0 &&
            std::strcmp(Argv[1], "current") != 0))
    {
        std::cerr << "usage: probe parent|current\n";
        return 2;
    }

    FMockDevice Device;

    IRHIDevice& BaseDevice = Device;
    FRHISwapchainDesc InvalidDesc;
    const auto SurfaceResult = BaseDevice.CreateSwapchain(nullptr, InvalidDesc);
    const bool bSurfaceFixed =
        SurfaceResult.Result == ERHIResult::Unsupported && !SurfaceResult.Object;

    FVerifierLegacyCommandBuffer LegacyCommands;
    IRHICommandBuffer& BaseCommands = LegacyCommands;
    FRHIRenderPassClearValues Clears;
    Clears.Colors.push_back({1.0f, 0.0f, 0.0f, 1.0f});
    const bool bClearFixed =
        BaseCommands.BeginRenderPass(nullptr, nullptr, Clears) == ERHIResult::Unsupported &&
        !LegacyCommands.WasLegacyBeginCalled();

    auto LegacySwapchain = MakeShared<FVerifierLegacySwapchain>();
    TSharedPtr<IRHISwapchain> BaseLegacySwapchain = LegacySwapchain;
    auto LegacySignal = Device.CreateSemaphore().Object;
    uint32 LegacyIndex = 99;
    const bool bSwapchainFallbackFixed =
        BaseLegacySwapchain->AcquireNextFrame(LegacyIndex, LegacySignal) ==
            ERHIResult::Unsupported &&
        LegacyIndex == 99 &&
        !LegacySwapchain->WasAcquireCalled() &&
        LegacySwapchain->GetState() == ERHISwapchainState::Ready &&
        !LegacySignal->IsSignaled();

    auto AcquireSwapchain = Device.CreateSwapchain(2).Object;
    auto AcquireSignal = Device.CreateSemaphore().Object;
    AcquireSignal->Signal();
    uint32 AcquireIndex = 99;
    const bool bAcquireAtomic =
        AcquireSwapchain->AcquireNextFrame(AcquireIndex, AcquireSignal) ==
            ERHIResult::InvalidState &&
        AcquireIndex == 99 &&
        AcquireSwapchain->GetState() == ERHISwapchainState::Ready &&
        AcquireSignal->IsSignaled();

    auto PresentSwapchain = Device.CreateSwapchain(2).Object;
    auto PresentWait = Device.CreateSemaphore().Object;
    uint32 PresentIndex = 99;
    PresentSwapchain->AcquireNextFrame(PresentIndex);
    PresentWait->Signal();
    const bool bPresentAtomic =
        PresentSwapchain->Present(PresentIndex + 1, PresentWait) ==
            ERHIResult::InvalidState &&
        PresentSwapchain->GetState() == ERHISwapchainState::Acquired &&
        PresentWait->IsSignaled();

    auto WaitQueue = Device.CreateCommandQueue(ERHIQueueType::Graphics).Object;
    auto WaitBuffer = Device.CreateCommandBuffer(ERHIQueueType::Graphics).Object;
    auto FirstWait = Device.CreateSemaphore().Object;
    auto SecondWait = Device.CreateSemaphore().Object;
    const bool bWaitPrepared =
        PrepareCompleted(WaitBuffer) &&
        FirstWait->Signal() == ERHIResult::Success;
    const bool bWaitAtomic =
        bWaitPrepared &&
        WaitQueue->Submit(WaitBuffer, {FirstWait, SecondWait}) == ERHIResult::NotReady &&
        FirstWait->IsSignaled() &&
        WaitBuffer->GetState() == ERHICommandBufferState::Completed &&
        WaitQueue->GetSubmittedCommandBufferCount() == 0;

    auto SignalQueue = Device.CreateCommandQueue(ERHIQueueType::Graphics).Object;
    auto SignalBuffer = Device.CreateCommandBuffer(ERHIQueueType::Graphics).Object;
    auto InvalidOutput = Device.CreateSemaphore().Object;
    auto Fence = Device.CreateFence().Object;
    const bool bSignalPrepared =
        PrepareCompleted(SignalBuffer) &&
        InvalidOutput->Signal() == ERHIResult::Success;
    const bool bSignalAtomic =
        bSignalPrepared &&
        SignalQueue->Submit(SignalBuffer, {}, {InvalidOutput}, Fence) ==
            ERHIResult::InvalidState &&
        SignalBuffer->GetState() == ERHICommandBufferState::Completed &&
        SignalQueue->GetSubmittedCommandBufferCount() == 0 &&
        !Fence->IsSignaled();

    const bool Results[] = {
        bSurfaceFixed,
        bClearFixed,
        bSwapchainFallbackFixed,
        bAcquireAtomic,
        bPresentAtomic,
        bWaitAtomic,
        bSignalAtomic};
    uint32 FixedCount = 0;
    for (bool bResult : Results)
    {
        FixedCount += bResult ? 1u : 0u;
    }

    std::cout << "surface_fixed=" << bSurfaceFixed << '\n'
              << "clear_fixed=" << bClearFixed << '\n'
              << "swapchain_fallback_fixed=" << bSwapchainFallbackFixed << '\n'
              << "acquire_atomic=" << bAcquireAtomic << '\n'
              << "present_atomic=" << bPresentAtomic << '\n'
              << "queue_wait_atomic=" << bWaitAtomic << '\n'
              << "queue_signal_atomic=" << bSignalAtomic << '\n'
              << "fixed_count=" << FixedCount << '\n';

    const bool bParent = std::strcmp(Argv[1], "parent") == 0;
    const bool bExpected = bParent ? FixedCount == 0 : FixedCount == 7;
    std::cout << "classification="
              << (bExpected ? (bParent ? "parent-defects" : "current-fixed")
                            : "unexpected")
              << '\n';
    return bExpected ? 0 : 3;
}
