#include "VulkanBackendTests.h"

#include "VulkanRHI/VulkanDevice.h"

#include <iostream>
#include <string_view>

namespace
{

using namespace Stoner::Backend::Vulkan;
using namespace Stoner::Core;
using namespace Stoner::RHI;

class FCompletedCommandBuffer final : public IRHICommandBuffer
{
public:
    explicit FCompletedCommandBuffer(ERHICommandBufferState InState = ERHICommandBufferState::Completed)
        : State(InState)
    {
    }

    [[nodiscard]] ERHICommandBufferState GetState() const noexcept override { return State; }
    [[nodiscard]] ERHIQueueType GetCompatibleQueueType() const noexcept override { return ERHIQueueType::Graphics; }
    [[nodiscard]] uint32 GetRecordedCommandCount() const noexcept override { return State == ERHICommandBufferState::Completed ? 1 : 0; }
    ERHIResult Begin() override { return ERHIResult::Unsupported; }
    ERHIResult End() override { return ERHIResult::Unsupported; }
    ERHIResult Reset() override { return ERHIResult::Unsupported; }
    ERHIResult RecordDraw(uint32, uint32 = 1) override { return ERHIResult::Unsupported; }
    ERHIResult RecordDispatch(uint32, uint32, uint32) override { return ERHIResult::Unsupported; }
    ERHIResult RecordBarrier() override { return ERHIResult::Unsupported; }

private:
    ERHICommandBufferState State = ERHICommandBufferState::Completed;
};

void Record(FVulkanBackendTestResult& Result, bool bPassed, const char* Name)
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

[[nodiscard]] FVulkanAdapterCandidate MakeCandidate(
    const char* Name,
    EVulkanPhysicalDeviceType Type,
    bool bGate,
    FVulkanQueueSupport Queues,
    bool bPresentation,
    FVulkanFormatSupport Formats)
{
    return {Name, Type, bGate, Queues, bPresentation, Formats, 0, ""};
}

void TestAdapterSelection(FVulkanBackendTestResult& Result)
{
    TArray<FVulkanAdapterCandidate> Candidates = {
        MakeCandidate("Integrated", EVulkanPhysicalDeviceType::Integrated, true, {true, true, true, false}, false, {true, true}),
        MakeCandidate("Rejected", EVulkanPhysicalDeviceType::Discrete, false, {true, true, true, true}, true, {true, true}),
        MakeCandidate("Discrete", EVulkanPhysicalDeviceType::Discrete, true, {true, true, true, true}, true, {true, true}),
    };

    const FVulkanAdapterSelection Selection = SelectBestAdapter(Candidates);
    Record(Result, Selection.bSucceeded && std::string_view(Selection.Selected.Name) == "Discrete", "Vulkan deterministic adapter selection");
    Record(Result, !Selection.Candidates[1].bPassesRequiredGate && Selection.Candidates[1].Score < 0 && Selection.Candidates[1].RejectionReason[0] != '\0', "Vulkan rejected adapter diagnostics");
}

void TestInitialization(FVulkanBackendTestResult& Result)
{
    FVulkanDevice Device;
    const ERHIResult InitResult = Device.Initialize();
    Record(Result, InitResult == ERHIResult::Success && Device.IsActive(), "Vulkan headless backend initialization");
    Record(Result, Device.GetCapabilities().bSupportsGraphicsQueue && Device.GetCapabilities().bSupportsSynchronization &&
        Device.GetCapabilities().SupportsFormat(ERHIFormat::B8G8R8A8_UNorm), "Vulkan selected device capabilities");

    FVulkanInstanceDesc UnsupportedDesc;
    UnsupportedDesc.bForceUnsupportedRuntime = true;
    FVulkanDevice UnsupportedDevice;
    Record(Result, UnsupportedDevice.Initialize(UnsupportedDesc) == ERHIResult::Unsupported && !UnsupportedDevice.IsActive(), "Vulkan unsupported runtime is explicit");

    FVulkanInstanceDesc NoAdapterDesc;
    NoAdapterDesc.SyntheticCandidates = {
        MakeCandidate("NoGraphics", EVulkanPhysicalDeviceType::Integrated, true, {false, true, true, false}, false, {true, true}),
    };
    FVulkanDevice NoAdapterDevice;
    Record(Result, NoAdapterDevice.Initialize(NoAdapterDesc) == ERHIResult::Unsupported && !NoAdapterDevice.IsActive(), "Vulkan no-compatible-adapter unsupported result");

    FVulkanInstanceDesc ValidationDesc;
    ValidationDesc.bForceValidationUnavailable = true;
    FVulkanDevice ValidationDevice;
    Record(Result, ValidationDevice.Initialize(ValidationDesc) == ERHIResult::Success &&
        ValidationDevice.GetDiagnostics().Validation == EVulkanValidationState::RequestedUnavailable, "Vulkan validation unavailable is diagnostic");

    (void)Device.Shutdown();
    (void)ValidationDevice.Shutdown();
}

void TestQueues(FVulkanBackendTestResult& Result)
{
    FVulkanDevice Device;
    Record(Result, Device.Initialize() == ERHIResult::Success, "Vulkan queue fixture device initializes");

    const auto GraphicsQueue = Device.CreateCommandQueue(ERHIQueueType::Graphics);
    const auto ComputeQueue = Device.CreateCommandQueue(ERHIQueueType::Compute);
    const auto TransferQueue = Device.CreateCommandQueue(ERHIQueueType::Transfer);
    const auto PresentQueue = Device.CreateCommandQueue(ERHIQueueType::Present);
    Record(Result, GraphicsQueue.Succeeded() && ComputeQueue.Succeeded() && TransferQueue.Succeeded() && PresentQueue.Succeeded(), "Vulkan queue creation success paths");
    Record(Result, GraphicsQueue.Object && GraphicsQueue.Object->GetQueueType() == ERHIQueueType::Graphics &&
        GraphicsQueue.Object->GetSubmittedCommandBufferCount() == 0, "Vulkan queue metadata");
    Record(Result, GraphicsQueue.Object && GraphicsQueue.Object->WaitIdle() == ERHIResult::Success, "Vulkan queue wait idle");
    Record(Result, GraphicsQueue.Object && GraphicsQueue.Object->Submit(nullptr) == ERHIResult::InvalidState, "Vulkan queue rejects missing command buffer");

    auto CompletedCommand = MakeShared<FCompletedCommandBuffer>();
    Record(Result, GraphicsQueue.Object && GraphicsQueue.Object->Submit(CompletedCommand) == ERHIResult::Unsupported, "Vulkan queue rejects executable command until recording phase");

    FVulkanInstanceDesc LimitedDesc;
    LimitedDesc.SyntheticCandidates = {
        MakeCandidate("Limited", EVulkanPhysicalDeviceType::Integrated, true, {true, false, true, false}, false, {true, true}),
    };
    FVulkanDevice LimitedDevice;
    Record(Result, LimitedDevice.Initialize(LimitedDesc) == ERHIResult::Success &&
        LimitedDevice.CreateCommandQueue(ERHIQueueType::Compute).Result == ERHIResult::Unsupported, "Vulkan unsupported queue request");

    (void)Device.Shutdown();
    Record(Result, Device.CreateCommandQueue(ERHIQueueType::Graphics).Result == ERHIResult::InvalidState, "Vulkan post-shutdown queue creation rejection");
    (void)LimitedDevice.Shutdown();
}

void TestSurfaceSwapchain(FVulkanBackendTestResult& Result)
{
    FVulkanDevice Device;
    Record(Result, Device.Initialize() == ERHIResult::Success, "Vulkan swapchain fixture device initializes");

    FVulkanSurface Surface;
    FPlatformWindow InvalidWindow;
    Record(Result, Device.CreateSurface(InvalidWindow, Surface) == ERHIResult::InvalidState &&
        Device.GetDiagnostics().PresentationSkipReason[0] != '\0', "Vulkan invalid platform window rejection and presentation skip");

    int NativeToken = 7;
    FPlatformWindow Window(&NativeToken);
    Record(Result, Device.CreateSurface(Window, Surface) == ERHIResult::Success && Surface.IsValid(), "Vulkan surface creation with Core platform window wrapper");

    const auto SwapchainResult = Device.CreateSwapchainForSurface(Surface, 2);
    Record(Result, SwapchainResult.Succeeded(), "Vulkan swapchain creation success");

    uint32 FrameIndex = 99;
    Record(Result, SwapchainResult.Object && SwapchainResult.Object->AcquireNextFrame(FrameIndex) == ERHIResult::Success &&
        FrameIndex == 0, "Vulkan swapchain acquire next frame");
    Record(Result, SwapchainResult.Object && SwapchainResult.Object->AcquireNextFrame(FrameIndex) == ERHIResult::InvalidState, "Vulkan swapchain rejects double acquire");
    Record(Result, SwapchainResult.Object && SwapchainResult.Object->Present(1) == ERHIResult::InvalidState, "Vulkan swapchain rejects stale frame");
    Record(Result, SwapchainResult.Object && SwapchainResult.Object->Present(0) == ERHIResult::Success &&
        SwapchainResult.Object->GetCurrentFrameIndex() == 1, "Vulkan swapchain present advances frame");

    auto ConcreteSwapchain = std::dynamic_pointer_cast<FVulkanSwapchain>(SwapchainResult.Object);
    ConcreteSwapchain->SimulateResizeRequired();
    Record(Result, SwapchainResult.Object->AcquireNextFrame(FrameIndex) == ERHIResult::ResizeRequired &&
        ConcreteSwapchain->Recreate(3) == ERHIResult::Success &&
        SwapchainResult.Object->GetFrameCount() == 3, "Vulkan swapchain resize-required recreate");
    ConcreteSwapchain->SetUnavailable();
    Record(Result, SwapchainResult.Object->Present(0) == ERHIResult::Unavailable, "Vulkan swapchain unavailable state");

    (void)Device.Shutdown();
    Record(Result, Device.CreateSwapchain(2).Result == ERHIResult::InvalidState &&
        SwapchainResult.Object->AcquireNextFrame(FrameIndex) == ERHIResult::InvalidState, "Vulkan post-shutdown swapchain invalidation");
}

void TestSynchronization(FVulkanBackendTestResult& Result)
{
    FVulkanDevice Device;
    Record(Result, Device.Initialize() == ERHIResult::Success, "Vulkan sync fixture device initializes");

    const auto UnsignaledFence = Device.CreateFence(false);
    const auto SignaledFence = Device.CreateFence(true);
    Record(Result, UnsignaledFence.Succeeded() && !UnsignaledFence.Object->IsSignaled() &&
        SignaledFence.Succeeded() && SignaledFence.Object->IsSignaled(), "Vulkan fence initial states");
    Record(Result, UnsignaledFence.Object->Wait() == ERHIResult::NotReady &&
        UnsignaledFence.Object->Wait(1) == ERHIResult::Timeout &&
        UnsignaledFence.Object->Signal() == ERHIResult::Success &&
        UnsignaledFence.Object->Wait() == ERHIResult::Success &&
        UnsignaledFence.Object->Reset() == ERHIResult::Success, "Vulkan fence wait signal reset transitions");

    const auto Semaphore = Device.CreateSemaphore();
    Record(Result, Semaphore.Succeeded() && !Semaphore.Object->IsSignaled() &&
        Semaphore.Object->Consume() == ERHIResult::NotReady &&
        Semaphore.Object->Signal() == ERHIResult::Success &&
        Semaphore.Object->Signal() == ERHIResult::InvalidState &&
        Semaphore.Object->Consume() == ERHIResult::Success &&
        Semaphore.Object->Reset() == ERHIResult::Success, "Vulkan semaphore transitions");

    (void)Device.Shutdown();
    Record(Result, Device.CreateFence().Result == ERHIResult::InvalidState &&
        Device.CreateSemaphore().Result == ERHIResult::InvalidState &&
        UnsignaledFence.Object->Signal() == ERHIResult::InvalidState &&
        Semaphore.Object->Signal() == ERHIResult::InvalidState, "Vulkan post-shutdown sync rejection");
}

void TestLifecycleAndUnsupportedFactories(FVulkanBackendTestResult& Result)
{
    for (int Index = 0; Index < 3; ++Index)
    {
        const auto DeviceResult = CreateVulkanDevice();
        Record(Result, DeviceResult.Succeeded(), "Vulkan repeated create cycle");
        if (DeviceResult.Object)
        {
            Record(Result, DeviceResult.Object->CreateCommandBuffer(ERHIQueueType::Graphics).Result == ERHIResult::Unsupported &&
                DeviceResult.Object->CreateBuffer({}).Result == ERHIResult::Unsupported &&
                DeviceResult.Object->CreateTexture({}).Result == ERHIResult::Unsupported, "Vulkan out-of-scope factories return unsupported");
            Record(Result, DeviceResult.Object->Shutdown() == ERHIResult::Success &&
                DeviceResult.Object->CreateCommandBuffer(ERHIQueueType::Graphics).Result == ERHIResult::InvalidState, "Vulkan shutdown rejects later factories");
        }
    }
}

} // namespace

FVulkanBackendTestResult RunVulkanBackendTests()
{
    FVulkanBackendTestResult Result;
    TestAdapterSelection(Result);
    TestInitialization(Result);
    TestQueues(Result);
    TestSurfaceSwapchain(Result);
    TestSynchronization(Result);
    TestLifecycleAndUnsupportedFactories(Result);
    return Result;
}
