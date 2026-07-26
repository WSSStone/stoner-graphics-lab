#include "Tests/RHICoreTests.cpp"

#include <iostream>

namespace
{

class FLegacyCommandBuffer final : public IRHICommandBuffer
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

    [[nodiscard]] uint32 GetRecordedCommandCount() const noexcept override
    {
        return 0;
    }

    ERHIResult Begin() override { return ERHIResult::Success; }
    ERHIResult End() override { return ERHIResult::Success; }
    ERHIResult Reset() override { return ERHIResult::Success; }
    ERHIResult RecordDraw(uint32, uint32) override { return ERHIResult::Unsupported; }
    ERHIResult RecordDrawIndexed(uint32, uint32, uint32) override { return ERHIResult::Unsupported; }
    ERHIResult RecordDispatch(uint32, uint32, uint32) override { return ERHIResult::Unsupported; }
    ERHIResult BindGraphicsPipeline(const TSharedPtr<IRHIGraphicsPipeline>&) override { return ERHIResult::Unsupported; }
    ERHIResult BindComputePipeline(const TSharedPtr<IRHIComputePipeline>&) override { return ERHIResult::Unsupported; }
    ERHIResult RecordBarrier() override { return ERHIResult::Unsupported; }
    ERHIResult RecordBarrier(const FRHIResourceBarrierDesc&) override { return ERHIResult::Unsupported; }
    ERHIResult RecordBufferCopy(const TSharedPtr<IRHIBuffer>&, const TSharedPtr<IRHIBuffer>&, FRHIBufferCopyRange) override
    {
        return ERHIResult::Unsupported;
    }
    ERHIResult RecordTextureCopy(const TSharedPtr<IRHITexture>&, const TSharedPtr<IRHITexture>&, FRHITextureCopyRegion) override
    {
        return ERHIResult::Unsupported;
    }
    ERHIResult RecordLayoutTransition(const FRHIResourceBarrierDesc&) override { return ERHIResult::Unsupported; }

    ERHIResult BeginRenderPass(const TSharedPtr<IRHIRenderPass>&, const TSharedPtr<IRHIFramebuffer>&) override
    {
        bLegacyBeginCalled = true;
        return ERHIResult::Success;
    }

    ERHIResult EndRenderPass() override { return ERHIResult::Success; }

    bool bLegacyBeginCalled = false;
};

} // namespace

int main()
{
    FMockDevice Device;
    IRHIDevice& BaseDevice = Device;

    FRHISwapchainDesc InvalidDesc;
    const auto SurfaceAware = BaseDevice.CreateSwapchain(nullptr, InvalidDesc);
    std::cout << "surface_invalid_desc_result=" << static_cast<int>(SurfaceAware.Result)
              << " object=" << (SurfaceAware.Object != nullptr) << '\n';

    auto AcquireSwapchain = Device.CreateSwapchain(2).Object;
    auto AlreadySignaled = Device.CreateSemaphore().Object;
    AlreadySignaled->Signal();
    uint32 ImageIndex = 99;
    const ERHIResult AcquireResult = AcquireSwapchain->AcquireNextFrame(ImageIndex, AlreadySignaled);
    std::cout << "acquire_result=" << static_cast<int>(AcquireResult)
              << " swapchain_state=" << static_cast<int>(AcquireSwapchain->GetState())
              << " image_index=" << ImageIndex << '\n';

    auto PresentSwapchain = Device.CreateSwapchain(2).Object;
    auto PresentWait = Device.CreateSemaphore().Object;
    uint32 PresentIndex = 99;
    PresentSwapchain->AcquireNextFrame(PresentIndex);
    PresentWait->Signal();
    const ERHIResult PresentResult = PresentSwapchain->Present(PresentIndex + 1, PresentWait);
    std::cout << "present_result=" << static_cast<int>(PresentResult)
              << " semaphore_state=" << static_cast<int>(PresentWait->GetState())
              << " swapchain_state=" << static_cast<int>(PresentSwapchain->GetState()) << '\n';

    FLegacyCommandBuffer LegacyCommands;
    IRHICommandBuffer& BaseCommands = LegacyCommands;
    FRHIRenderPassClearValues ClearValues;
    ClearValues.Colors.push_back({1.0f, 0.0f, 0.0f, 1.0f});
    const ERHIResult ClearResult = BaseCommands.BeginRenderPass(nullptr, nullptr, ClearValues);
    std::cout << "clear_overload_result=" << static_cast<int>(ClearResult)
              << " legacy_called=" << LegacyCommands.bLegacyBeginCalled << '\n';

    auto PartialWaitQueue = Device.CreateCommandQueue(ERHIQueueType::Graphics).Object;
    auto PartialWaitBuffer = Device.CreateCommandBuffer(ERHIQueueType::Graphics).Object;
    PartialWaitBuffer->Begin();
    PartialWaitBuffer->RecordDraw(3);
    PartialWaitBuffer->End();
    auto FirstWait = Device.CreateSemaphore().Object;
    auto SecondWait = Device.CreateSemaphore().Object;
    FirstWait->Signal();
    const ERHIResult PartialWaitResult =
        PartialWaitQueue->Submit(PartialWaitBuffer, {FirstWait, SecondWait});
    std::cout << "partial_wait_result=" << static_cast<int>(PartialWaitResult)
              << " first_wait_state=" << static_cast<int>(FirstWait->GetState())
              << " command_state=" << static_cast<int>(PartialWaitBuffer->GetState())
              << " submitted_count=" << PartialWaitQueue->GetSubmittedCommandBufferCount() << '\n';

    auto FailedSignalQueue = Device.CreateCommandQueue(ERHIQueueType::Graphics).Object;
    auto FailedSignalBuffer = Device.CreateCommandBuffer(ERHIQueueType::Graphics).Object;
    FailedSignalBuffer->Begin();
    FailedSignalBuffer->RecordDraw(3);
    FailedSignalBuffer->End();
    auto AlreadySignaledOutput = Device.CreateSemaphore().Object;
    auto CompletionFence = Device.CreateFence().Object;
    AlreadySignaledOutput->Signal();
    const ERHIResult FailedSignalResult =
        FailedSignalQueue->Submit(FailedSignalBuffer, {}, {AlreadySignaledOutput}, CompletionFence);
    std::cout << "failed_signal_result=" << static_cast<int>(FailedSignalResult)
              << " command_state=" << static_cast<int>(FailedSignalBuffer->GetState())
              << " submitted_count=" << FailedSignalQueue->GetSubmittedCommandBufferCount()
              << " fence_signaled=" << CompletionFence->IsSignaled() << '\n';

    return 0;
}
