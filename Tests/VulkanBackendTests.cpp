#include "VulkanBackendTests.h"
#include "ShaderTestFixtures.h"

#include "VulkanRHI/VulkanDevice.h"

#include <array>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

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
    ERHIResult RecordDrawIndexed(uint32, uint32 = 1, uint32 = 0) override
    {
        return ERHIResult::Unsupported;
    }
    ERHIResult RecordDispatch(uint32, uint32, uint32) override { return ERHIResult::Unsupported; }
    ERHIResult BindGraphicsPipeline(const TSharedPtr<IRHIGraphicsPipeline>&) override { return ERHIResult::Unsupported; }
    ERHIResult BindComputePipeline(const TSharedPtr<IRHIComputePipeline>&) override { return ERHIResult::Unsupported; }
    ERHIResult RecordBarrier() override { return ERHIResult::Unsupported; }
    ERHIResult RecordBarrier(const FRHIResourceBarrierDesc&) override { return ERHIResult::Unsupported; }
    ERHIResult RecordBufferCopy(const TSharedPtr<IRHIBuffer>&, const TSharedPtr<IRHIBuffer>&, FRHIBufferCopyRange) override { return ERHIResult::Unsupported; }
    ERHIResult RecordTextureCopy(const TSharedPtr<IRHITexture>&, const TSharedPtr<IRHITexture>&, FRHITextureCopyRegion) override { return ERHIResult::Unsupported; }
    ERHIResult RecordLayoutTransition(const FRHIResourceBarrierDesc&) override { return ERHIResult::Unsupported; }
    ERHIResult BeginRenderPass(const TSharedPtr<IRHIRenderPass>&, const TSharedPtr<IRHIFramebuffer>&) override { return ERHIResult::Unsupported; }
    ERHIResult EndRenderPass() override { return ERHIResult::Unsupported; }

private:
    ERHICommandBufferState State = ERHICommandBufferState::Completed;
};

template <typename T>
concept CHasPublicMarkSubmitted = requires(T& Value)
{
    Value.MarkSubmitted();
};

template <typename T>
concept CHasPublicMarkCompletedOrResettable = requires(T& Value)
{
    Value.MarkCompletedOrResettable();
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

[[nodiscard]] FVulkanInstanceDesc MakeDeterministicInstanceDesc()
{
    FVulkanInstanceDesc Desc;
    Desc.RuntimeMode = EVulkanInstanceRuntimeMode::DeterministicFallback;
    return Desc;
}

[[nodiscard]] ERHIResult InitializeDeterministic(FVulkanDevice& Device)
{
    return Device.Initialize(MakeDeterministicInstanceDesc());
}

void TestAdapterSelection(FVulkanBackendTestResult& Result)
{
    TArray<FVulkanAdapterCandidate> Candidates = {
        MakeCandidate("Integrated", EVulkanPhysicalDeviceType::Integrated, true, {true, true, true, false}, false, {true, true}),
        MakeCandidate("Rejected", EVulkanPhysicalDeviceType::Discrete, false, {true, true, true, true}, true, {true, true}),
        MakeCandidate("Discrete", EVulkanPhysicalDeviceType::Discrete, true, {true, true, true, true}, true, {true, true}),
    };

    const FVulkanAdapterSelection Selection = SelectBestAdapter(Candidates);
    Record(Result, Selection.bSucceeded && Selection.Selected.Name.View() == "Discrete", "Vulkan deterministic adapter selection");
    Record(Result, !Selection.Candidates[1].bPassesRequiredGate && Selection.Candidates[1].Score < 0 &&
        !Selection.Candidates[1].RejectionReason.IsEmpty(), "Vulkan rejected adapter diagnostics");

    std::string MutableName = "CallerOwnedName";
    const FVulkanAdapterSelection OwnedSelection = SelectBestAdapter({
        MakeCandidate(MutableName.c_str(), EVulkanPhysicalDeviceType::Discrete, true,
            {true, true, true, false}, false, {true, true}),
    });
    MutableName[0] = 'X';
    Record(Result, OwnedSelection.bSucceeded && OwnedSelection.Selected.Name.View() == "CallerOwnedName",
        "Vulkan selected adapter identity owns its storage");

    const FVulkanAdapterSelection NullSafeSelection = SelectBestAdapter({
        MakeCandidate(nullptr, EVulkanPhysicalDeviceType::Discrete, true,
            {true, true, true, false}, false, {true, true}),
        MakeCandidate("Named", EVulkanPhysicalDeviceType::Discrete, true,
            {true, true, true, false}, false, {true, true}),
    });
    Record(Result, NullSafeSelection.bSucceeded && NullSafeSelection.Selected.Name.View() == "Named" &&
        NullSafeSelection.Candidates[0].Score < 0 &&
        !NullSafeSelection.Candidates[0].RejectionReason.IsEmpty(),
        "Vulkan null adapter identity is rejected without crashing");
}

void TestInitialization(FVulkanBackendTestResult& Result)
{
    FVulkanDevice RealDevice;
    const ERHIResult RealResult = RealDevice.Initialize();
    Record(Result, RealResult == ERHIResult::Unsupported && !RealDevice.IsActive() &&
        !RealDevice.GetDiagnostics().bUsedRuntimeFallback &&
        RealDevice.GetDiagnostics().Availability == EVulkanBackendAvailability::UnsupportedRuntime &&
        RealDevice.GetDiagnostics().RuntimeModeReason[0] != '\0',
        "Vulkan default device requires an integrated real runtime");

    FVulkanDevice Device;
    const ERHIResult InitResult = InitializeDeterministic(Device);
    Record(Result, InitResult == ERHIResult::Success && Device.IsActive() &&
        Device.GetDiagnostics().bUsedRuntimeFallback &&
        Device.GetDiagnostics().Availability == EVulkanBackendAvailability::DeterministicFallback &&
        Device.GetDiagnostics().RuntimeModeReason[0] != '\0',
        "Vulkan explicit deterministic backend initialization");
    Record(Result, Device.GetCapabilities().bSupportsGraphicsQueue && Device.GetCapabilities().bSupportsSynchronization &&
        Device.GetCapabilities().SupportsFormat(ERHIFormat::B8G8R8A8_UNorm) &&
        IsValidRHIDeviceCapabilities(Device.GetCapabilities()) &&
        Device.GetCapabilities().MaxBufferSizeBytes == 128ULL * 1024ULL * 1024ULL &&
        Device.GetCapabilities().MaxTextureDimension2D == 8192 &&
        Device.GetCapabilities().MaxPerStageTextureBindings == 16 &&
        Device.GetCapabilities().MaxConstantRangeBytes == 128 &&
        Device.GetCapabilities().MaxComputeThreadsPerThreadgroup == 128 &&
        Device.GetCapabilities().SupportsSampleCount(ERHISampleCount::Four),
        "Vulkan selected device capabilities publish a valid conservative limit snapshot");
    FRHIBufferDesc OversizedBuffer;
    OversizedBuffer.SizeInBytes =
        Device.GetCapabilities().MaxBufferSizeBytes + 1ULL;
    OversizedBuffer.Usage = ERHIBufferUsage::Storage;
    FRHITextureDesc OversizedTexture;
    OversizedTexture.Width = 1;
    OversizedTexture.Height = 1;
    OversizedTexture.Format = ERHIFormat::R8G8B8A8_UNorm;
    OversizedTexture.Usage = ERHITextureUsage::Sampled;
    OversizedTexture.Width = Device.GetCapabilities().MaxTextureDimension2D + 1U;
    Record(Result,
        Device.CreateBuffer(OversizedBuffer).Result == ERHIResult::Unsupported &&
            Device.CreateTexture(OversizedTexture).Result == ERHIResult::Unsupported,
        "Vulkan factories enforce their published resource and texture limits");

    FVulkanInstanceDesc UnsupportedDesc;
    UnsupportedDesc.bForceUnsupportedRuntime = true;
    FVulkanDevice UnsupportedDevice;
    Record(Result, UnsupportedDevice.Initialize(UnsupportedDesc) == ERHIResult::Unsupported && !UnsupportedDevice.IsActive(), "Vulkan unsupported runtime is explicit");

    FVulkanInstanceDesc NoAdapterDesc = MakeDeterministicInstanceDesc();
    NoAdapterDesc.SyntheticCandidates = {
        MakeCandidate("NoGraphics", EVulkanPhysicalDeviceType::Integrated, true, {false, true, true, false}, false, {true, true}),
    };
    FVulkanDevice NoAdapterDevice;
    Record(Result, NoAdapterDevice.Initialize(NoAdapterDesc) == ERHIResult::Unsupported && !NoAdapterDevice.IsActive(), "Vulkan no-compatible-adapter unsupported result");

    FVulkanInstanceDesc ValidationDesc = MakeDeterministicInstanceDesc();
    ValidationDesc.bForceValidationUnavailable = true;
    FVulkanDevice ValidationDevice;
    Record(Result, ValidationDevice.Initialize(ValidationDesc) == ERHIResult::Success &&
        ValidationDevice.GetDiagnostics().Validation == EVulkanValidationState::RequestedUnavailable, "Vulkan validation unavailable is diagnostic");

    FVulkanInstanceDesc ColorOnlyDesc = MakeDeterministicInstanceDesc();
    ColorOnlyDesc.SyntheticCandidates = {
        MakeCandidate("ColorOnly", EVulkanPhysicalDeviceType::Integrated, true,
            {true, true, true, false}, false, {true, false}),
    };
    FVulkanDevice ColorOnlyDevice;
    FRHITextureDesc DepthTexture;
    DepthTexture.Width = 4;
    DepthTexture.Height = 4;
    DepthTexture.Format = ERHIFormat::D32_Float;
    DepthTexture.Usage = ERHITextureUsage::DepthStencilAttachment;
    Record(Result, ColorOnlyDevice.Initialize(ColorOnlyDesc) == ERHIResult::Success &&
        !ColorOnlyDevice.GetCapabilities().SupportsFormat(ERHIFormat::D32_Float) &&
        ColorOnlyDevice.CreateTexture(DepthTexture).Result == ERHIResult::Unsupported,
        "Vulkan selected adapter formats constrain capabilities and factories");

    (void)Device.Shutdown();
    (void)ValidationDevice.Shutdown();
    (void)ColorOnlyDevice.Shutdown();
}

void TestQueues(FVulkanBackendTestResult& Result)
{
    FVulkanDevice Device;
    Record(Result, InitializeDeterministic(Device) == ERHIResult::Success, "Vulkan queue fixture device initializes");

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
    Record(Result, GraphicsQueue.Object && GraphicsQueue.Object->Submit(CompletedCommand) == ERHIResult::InvalidState, "Vulkan queue rejects foreign executable command buffer");

    FVulkanInstanceDesc LimitedDesc = MakeDeterministicInstanceDesc();
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
    FVulkanDevice ForeignDevice;
    Record(Result,
        InitializeDeterministic(Device) == ERHIResult::Success &&
            InitializeDeterministic(ForeignDevice) == ERHIResult::Success,
        "Vulkan swapchain fixtures initialize");

    FVulkanSurface Surface;
    FPlatformWindow InvalidWindow;
    Record(Result, Device.CreateSurface(InvalidWindow, Surface) == ERHIResult::InvalidState &&
        !Surface.IsValid() && Device.GetDiagnostics().PresentationSkipReason[0] != '\0',
        "Vulkan invalid platform window rejection clears surface output");

    int NativeToken = 7;
    FPlatformWindow Window(&NativeToken);
    Record(Result,
        Device.CreateSurface(Window, Surface) == ERHIResult::Success &&
            Surface.IsValid(),
        "Vulkan surface creation with Core platform window wrapper");

    FVulkanSurface PreservedOutput = Surface;
    FVulkanDevice InactiveDevice;
    Record(Result,
        InactiveDevice.CreateSurface(InvalidWindow, PreservedOutput) ==
                ERHIResult::InvalidState &&
            !PreservedOutput.IsValid(),
        "Vulkan inactive surface creation clears a prior usable output");

    const auto SwapchainResult = Device.CreateSwapchainForSurface(Surface, 2);
    Record(Result,
        SwapchainResult.Succeeded() && SwapchainResult.Object->GetImage(0) &&
            SwapchainResult.Object->GetImage(1) &&
            !SwapchainResult.Object->GetImage(2),
        "Vulkan legacy surface adapter creates surface-backed images");
    Record(Result,
        ForeignDevice.CreateSwapchainForSurface(Surface, 2).Result ==
            ERHIResult::InvalidState,
        "Vulkan legacy surface adapter rejects foreign device provenance");
    Record(Result,
        Device.CreateSwapchainForSurface(Surface, 0).Result ==
                ERHIResult::InvalidState &&
            Device.CreateSwapchain(0).Result == ERHIResult::InvalidState,
        "Vulkan zero-frame swapchain requests are invalid input");

    FVulkanSwapchain InvalidConcreteSwapchain(0);
    uint32 InvalidFrameIndex = 99;
    Record(Result,
        InvalidConcreteSwapchain.GetState() == ERHISwapchainState::Unavailable &&
            InvalidConcreteSwapchain.AcquireNextFrame(InvalidFrameIndex) ==
                ERHIResult::InvalidState &&
            InvalidFrameIndex == 99,
        "Vulkan concrete swapchain rejects a zero-frame constructor");

    uint32 FrameIndex = 99;
    Record(Result, SwapchainResult.Object && SwapchainResult.Object->AcquireNextFrame(FrameIndex) == ERHIResult::Success &&
        FrameIndex == 0, "Vulkan swapchain acquire next frame");
    Record(Result, SwapchainResult.Object && SwapchainResult.Object->AcquireNextFrame(FrameIndex) == ERHIResult::InvalidState, "Vulkan swapchain rejects double acquire");
    Record(Result, SwapchainResult.Object && SwapchainResult.Object->Present(1) == ERHIResult::InvalidState, "Vulkan swapchain rejects stale frame");
    Record(Result, SwapchainResult.Object && SwapchainResult.Object->Present(0) == ERHIResult::Success &&
        SwapchainResult.Object->GetCurrentFrameIndex() == 1, "Vulkan swapchain present advances frame");

    auto ConcreteSwapchain = std::dynamic_pointer_cast<FVulkanSwapchain>(SwapchainResult.Object);
    const Stoner::Core::uint64 FirstGeneration = ConcreteSwapchain->GetGeneration();
    const auto FirstImage = SwapchainResult.Object->GetImage(0);
    ConcreteSwapchain->SimulateResizeRequired();
    Record(Result, SwapchainResult.Object->AcquireNextFrame(FrameIndex) == ERHIResult::ResizeRequired &&
        ConcreteSwapchain->Recreate(3) == ERHIResult::Success &&
        SwapchainResult.Object->GetFrameCount() == 3 &&
        ConcreteSwapchain->GetGeneration() == FirstGeneration + 1 &&
        FirstImage->GetLifecycleState() ==
            ERHIResourceLifecycleState::Invalidated &&
        SwapchainResult.Object->GetImage(2),
        "Vulkan recreation replaces imported images and advances generation");
    Record(Result, SwapchainResult.Object->AcquireNextFrame(FrameIndex) == ERHIResult::Success && FrameIndex == 0 &&
        ConcreteSwapchain->Recreate(2) == ERHIResult::Success &&
        SwapchainResult.Object->Present(FrameIndex) == ERHIResult::InvalidState,
        "Vulkan swapchain invalidates an acquired image from the old generation");
    const Stoner::Core::uint64 StableGeneration = ConcreteSwapchain->GetGeneration();
    const Stoner::Core::uint32 StableFrameCount = ConcreteSwapchain->GetFrameCount();
    Record(Result, ConcreteSwapchain->Recreate(0) == ERHIResult::InvalidState &&
        ConcreteSwapchain->GetGeneration() == StableGeneration &&
        ConcreteSwapchain->GetFrameCount() == StableFrameCount,
        "Vulkan partial recreation failure preserves the active generation");
    Record(Result, SwapchainResult.Object->AcquireNextFrame(FrameIndex) == ERHIResult::Success && FrameIndex == 0 &&
        SwapchainResult.Object->Present(0) == ERHIResult::Success &&
        SwapchainResult.Object->AcquireNextFrame(FrameIndex) == ERHIResult::Success && FrameIndex == 1 &&
        SwapchainResult.Object->Present(1) == ERHIResult::Success,
        "Vulkan swapchain synchronization follows acquired image indices");
    ConcreteSwapchain->SetUnavailable();
    Record(Result, SwapchainResult.Object->Present(0) == ERHIResult::Unavailable, "Vulkan swapchain unavailable state");

    IRHIDevice& BaseDevice = Device;
    FRHIPresentationSurfaceDesc SurfaceDesc;
    SurfaceDesc.Window = Window;
    SurfaceDesc.DebugName = "BackendNeutralSurface";
    const auto NeutralSurfaceResult =
        BaseDevice.CreatePresentationSurface(SurfaceDesc);
    Record(Result,
        NeutralSurfaceResult.Succeeded() &&
            NeutralSurfaceResult.Object->IsValid() &&
            NeutralSurfaceResult.Object->GetDesc().DebugName.View() ==
                "BackendNeutralSurface",
        "Vulkan backend-neutral presentation surface creation succeeds");
    Record(Result,
        BaseDevice.CreatePresentationSurface({}).Result ==
            ERHIResult::InvalidState,
        "Vulkan backend-neutral surface rejects an invalid description");

    FRHISwapchainDesc NeutralDesc;
    NeutralDesc.Width = 64;
    NeutralDesc.Height = 32;
    NeutralDesc.FramesInFlight = 2;
    NeutralDesc.PreferredFormat = ERHIFormat::B8G8R8A8_UNorm;
    const auto NeutralSwapchainResult =
        BaseDevice.CreateSwapchain(NeutralSurfaceResult.Object, NeutralDesc);
    const auto NeutralImage = NeutralSwapchainResult.Object
        ? NeutralSwapchainResult.Object->GetImage(0)
        : nullptr;
    Record(Result,
        NeutralSwapchainResult.Succeeded() && NeutralImage &&
            NeutralImage->GetDesc().Width == NeutralDesc.Width &&
            NeutralImage->GetDesc().Height == NeutralDesc.Height &&
            NeutralImage->GetFormat() == NeutralDesc.PreferredFormat &&
            HasRHIFlag(NeutralImage->GetUsage(), ERHITextureUsage::Present),
        "Vulkan backend-neutral swapchain exposes imported presentation images");

    FRHISwapchainDesc InvalidNeutralDesc = NeutralDesc;
    InvalidNeutralDesc.FramesInFlight = 0;
    FRHISwapchainDesc UnsupportedNeutralDesc = NeutralDesc;
    UnsupportedNeutralDesc.FramesInFlight =
        Device.GetCapabilities().MaxInFlightFrames + 1;
    FRHISwapchainDesc DepthNeutralDesc = NeutralDesc;
    DepthNeutralDesc.PreferredFormat = ERHIFormat::D32_Float;
    Record(Result,
        BaseDevice.CreateSwapchain(
            NeutralSurfaceResult.Object, InvalidNeutralDesc).Result ==
                ERHIResult::InvalidState &&
            BaseDevice.CreateSwapchain(
                NeutralSurfaceResult.Object, UnsupportedNeutralDesc).Result ==
                ERHIResult::Unsupported &&
            BaseDevice.CreateSwapchain(
                NeutralSurfaceResult.Object, DepthNeutralDesc).Result ==
                ERHIResult::InvalidState,
        "Vulkan swapchain distinguishes invalid descriptions from unsupported capability");
    Record(Result,
        ForeignDevice.CreateSwapchain(
            NeutralSurfaceResult.Object, NeutralDesc).Result ==
            ERHIResult::InvalidState,
        "Vulkan backend-neutral swapchain rejects a foreign surface");

    const auto AcquireSignal = Device.CreateSemaphore();
    uint32 NeutralFrameIndex = 99;
    Record(Result,
        AcquireSignal.Succeeded() &&
            NeutralSwapchainResult.Object->AcquireNextFrame(
                NeutralFrameIndex, AcquireSignal.Object) ==
                ERHIResult::Success &&
            NeutralFrameIndex == 0 && AcquireSignal.Object->IsSignaled(),
        "Vulkan synchronized acquire commits image and signal together");
    Record(Result,
        NeutralSwapchainResult.Object->Present(
            NeutralFrameIndex + 1, AcquireSignal.Object) ==
                ERHIResult::InvalidState &&
            AcquireSignal.Object->IsSignaled() &&
            NeutralSwapchainResult.Object->GetState() ==
                ERHISwapchainState::Acquired,
        "Vulkan failed synchronized present preserves frame and signal");
    Record(Result,
        NeutralSwapchainResult.Object->Present(
            NeutralFrameIndex, AcquireSignal.Object) ==
                ERHIResult::Success &&
            AcquireSignal.Object->GetState() ==
                ERHISemaphoreState::Consumed &&
            NeutralSwapchainResult.Object->GetState() ==
                ERHISwapchainState::Ready,
        "Vulkan synchronized present consumes signal and advances atomically");

    const auto AlreadySignaled = Device.CreateSemaphore();
    uint32 PreservedFrameIndex = 99;
    Record(Result,
        AlreadySignaled.Succeeded() &&
            AlreadySignaled.Object->Signal() == ERHIResult::Success &&
            NeutralSwapchainResult.Object->AcquireNextFrame(
                PreservedFrameIndex, AlreadySignaled.Object) ==
                ERHIResult::InvalidState &&
            PreservedFrameIndex == 99 &&
            NeutralSwapchainResult.Object->GetState() ==
                ERHISwapchainState::Ready &&
            AlreadySignaled.Object->IsSignaled(),
        "Vulkan failed synchronized acquire preserves all states");

    auto NeutralSurface =
        std::dynamic_pointer_cast<FVulkanSurface>(
            NeutralSurfaceResult.Object);
    auto NeutralSwapchain =
        std::dynamic_pointer_cast<FVulkanSwapchain>(
            NeutralSwapchainResult.Object);
    Record(Result,
        NeutralSurface && NeutralSwapchain &&
            NeutralSurface->Invalidate() == ERHIResult::Success &&
            NeutralSwapchain->GetState() ==
                ERHISwapchainState::Unavailable &&
            NeutralSwapchain->AcquireNextFrame(PreservedFrameIndex) ==
                ERHIResult::Unavailable &&
            NeutralSwapchain->Recreate(2) == ERHIResult::Unavailable &&
            !NeutralSwapchain->GetImage(0),
        "Vulkan surface loss blocks acquire image access and recreation");

    const auto ShutdownSurface =
        BaseDevice.CreatePresentationSurface(SurfaceDesc);
    const auto ShutdownSwapchain =
        BaseDevice.CreateSwapchain(ShutdownSurface.Object, NeutralDesc);
    const auto ShutdownImage = ShutdownSwapchain.Object
        ? ShutdownSwapchain.Object->GetImage(0)
        : nullptr;
    (void)Device.Shutdown();
    Record(Result,
        Device.CreateSwapchain(2).Result == ERHIResult::InvalidState &&
            SwapchainResult.Object->AcquireNextFrame(FrameIndex) ==
                ERHIResult::InvalidState &&
            ShutdownSurface.Succeeded() &&
            !ShutdownSurface.Object->IsValid() &&
            ShutdownSwapchain.Succeeded() &&
            ShutdownSwapchain.Object->AcquireNextFrame(FrameIndex) ==
                ERHIResult::InvalidState &&
            ShutdownImage &&
            ShutdownImage->GetLifecycleState() ==
                ERHIResourceLifecycleState::Invalidated,
        "Vulkan shutdown invalidates surfaces swapchains and imported images");
    (void)ForeignDevice.Shutdown();
}

void TestSynchronization(FVulkanBackendTestResult& Result)
{
    FVulkanDevice Device;
    Record(Result, InitializeDeterministic(Device) == ERHIResult::Success, "Vulkan sync fixture device initializes");

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

void TestLifecycleAndFactoryState(FVulkanBackendTestResult& Result)
{
    for (int Index = 0; Index < 3; ++Index)
    {
        const auto DeviceResult = CreateVulkanDevice(MakeDeterministicInstanceDesc());
        Record(Result, DeviceResult.Succeeded(), "Vulkan repeated create cycle");
        if (DeviceResult.Object)
        {
            Record(Result, DeviceResult.Object->CreateCommandBuffer(ERHIQueueType::Graphics).Succeeded() &&
                DeviceResult.Object->CreateShaderModule({}).Result == ERHIResult::Unsupported &&
                DeviceResult.Object->CreateGraphicsPipeline({}).Result == ERHIResult::InvalidState, "Vulkan factories reject invalid descriptions explicitly");
            Record(Result, DeviceResult.Object->Shutdown() == ERHIResult::Success &&
                DeviceResult.Object->CreateCommandBuffer(ERHIQueueType::Graphics).Result == ERHIResult::InvalidState, "Vulkan shutdown rejects later factories");
        }
    }
}

[[nodiscard]] FRHIBufferDesc ValidBufferDesc()
{
    return {256, ERHIBufferUsage::Uniform | ERHIBufferUsage::CopyDestination};
}

[[nodiscard]] FRHITextureDesc ValidTextureDesc()
{
    FRHITextureDesc Desc;
    Desc.Width = 8;
    Desc.Height = 8;
    Desc.Format = ERHIFormat::R8G8B8A8_UNorm;
    Desc.Usage = ERHITextureUsage::Sampled | ERHITextureUsage::CopyDestination;
    return Desc;
}

[[nodiscard]] FRHITextureDesc ValidColorAttachmentTextureDesc()
{
    FRHITextureDesc Desc = ValidTextureDesc();
    Desc.Usage = ERHITextureUsage::ColorAttachment | ERHITextureUsage::CopyDestination | ERHITextureUsage::CopySource | ERHITextureUsage::Sampled;
    return Desc;
}

[[nodiscard]] FRHIBufferDesc ValidCopySourceBufferDesc()
{
    return {256, ERHIBufferUsage::CopySource | ERHIBufferUsage::Storage};
}

[[nodiscard]] FRHIRenderPassDesc ValidRenderPassDesc()
{
    FRHIRenderPassDesc Desc;
    Desc.Attachments = {
        {ERHIAttachmentRole::Color, ERHIFormat::R8G8B8A8_UNorm, ERHISampleCount::One, ERHIAttachmentLoadOp::Clear, ERHIAttachmentStoreOp::Store},
    };
    return Desc;
}

[[nodiscard]] FRHIFramebufferDesc ValidFramebufferDesc(const TSharedPtr<IRHIRenderPass>& RenderPass, const TSharedPtr<IRHITexture>& Texture)
{
    FRHIFramebufferDesc Desc;
    Desc.RenderPass = RenderPass;
    Desc.Attachments = {{Texture, 0, 0}};
    Desc.Width = 8;
    Desc.Height = 8;
    return Desc;
}

[[nodiscard]] FRHISamplerDesc ValidSamplerDesc()
{
    return {};
}

[[nodiscard]] FRHIPipelineLayoutDesc ResourceLayoutDesc()
{
    FRHIPipelineLayoutDesc Desc;
    Desc.Bindings = {
        {0, 0, ERHIDescriptorType::UniformBuffer, 1, ERHIShaderStageFlags::Vertex},
        {0, 1, ERHIDescriptorType::SampledTexture, 1, ERHIShaderStageFlags::Fragment},
        {0, 2, ERHIDescriptorType::Sampler, 1, ERHIShaderStageFlags::Fragment},
        {0, 3, ERHIDescriptorType::CombinedTextureSampler, 1, ERHIShaderStageFlags::Fragment},
        {1, 0, ERHIDescriptorType::StorageBuffer, 1, ERHIShaderStageFlags::Compute},
    };
    Desc.ConstantRanges = {{0, 64, ERHIShaderStageFlags::Vertex | ERHIShaderStageFlags::Fragment | ERHIShaderStageFlags::Compute}};
    return Desc;
}

[[nodiscard]] FRHIShaderModuleDesc ShaderDesc(ERHIShaderStage Stage, const char* EntryPoint, const char* Payload)
{
    FRHIShaderModuleDesc Desc;
    Desc.Stage = Stage;
    Desc.EntryPoint = EntryPoint;
    Desc.Payload = Stoner::Tests::MakeMinimalShaderPayload(
        Stage, EntryPoint, Payload);
    const ERHIShaderStageFlags Visibility = ToShaderStageFlag(Stage);
    if (Stage == ERHIShaderStage::Vertex)
    {
        Desc.InterfaceMetadata.Bindings = {{0, 0, ERHIDescriptorType::UniformBuffer, 1, Visibility}};
    }
    else if (Stage == ERHIShaderStage::Fragment)
    {
        Desc.InterfaceMetadata.Bindings = {{0, 1, ERHIDescriptorType::SampledTexture, 1, Visibility}};
    }
    else if (Stage == ERHIShaderStage::Compute)
    {
        Desc.InterfaceMetadata.Bindings = {{1, 0, ERHIDescriptorType::StorageBuffer, 1, Visibility}};
    }
    Desc.InterfaceMetadata.ConstantRanges = {{0, 16, Visibility}};
    Desc.InterfaceMetadata.DebugName = Payload;
    Desc.DebugName = Payload;
    return Desc;
}

[[nodiscard]] FRHIGraphicsPipelineDesc GraphicsPipelineDesc(
    const TSharedPtr<IRHIShaderModule>& Vertex,
    const TSharedPtr<IRHIShaderModule>& Fragment,
    const TSharedPtr<IRHIPipelineLayout>& Layout)
{
    FRHIGraphicsPipelineDesc Desc;
    Desc.PipelineLayout = Layout;
    Desc.ShaderModules = {Vertex, Fragment};
    Desc.VertexInput.Stride = 12;
    Desc.VertexInput.Attributes = {{0, ERHIFormat::R32_Float, 0}};
    Desc.Topology = ERHIPrimitiveTopology::TriangleList;
    Desc.RenderTargets.ColorFormats = {ERHIFormat::R8G8B8A8_UNorm};
    Desc.RenderTargets.SampleCount = ERHISampleCount::One;
    Desc.Multisample.SampleCount = ERHISampleCount::One;
    return Desc;
}

[[nodiscard]] FRHIComputePipelineDesc ComputePipelineDesc(
    const TSharedPtr<IRHIShaderModule>& Compute,
    const TSharedPtr<IRHIPipelineLayout>& Layout)
{
    FRHIComputePipelineDesc Desc;
    Desc.PipelineLayout = Layout;
    Desc.ShaderModules = {Compute};
    return Desc;
}

void TestShaderPipelineAndBinding(FVulkanBackendTestResult& Result)
{
    static_assert(
        !std::is_constructible_v<
            FVulkanShaderModule, FRHIShaderModuleDesc, const char*> &&
        !std::is_constructible_v<
            FVulkanPipelineLayout, const FRHIPipelineLayoutDesc&> &&
        !std::is_constructible_v<
            FVulkanGraphicsPipeline,
            FRHIGraphicsPipelineDesc,
            const char*> &&
        !std::is_constructible_v<
            FVulkanComputePipeline,
            FRHIComputePipelineDesc,
            const char*>,
        "Vulkan shader, layout, and pipeline construction must remain device-owned");

    FVulkanDevice Device;
    Record(Result, InitializeDeterministic(Device) == ERHIResult::Success, "Vulkan shader pipeline fixture device initializes");

    const auto Layout = Device.CreatePipelineLayout(ResourceLayoutDesc());
    const auto Vertex = Device.CreateShaderModule(ShaderDesc(ERHIShaderStage::Vertex, "MainVS", "vs_payload"));
    const auto Fragment = Device.CreateShaderModule(ShaderDesc(ERHIShaderStage::Fragment, "MainPS", "ps_payload"));
    const auto Compute = Device.CreateShaderModule(ShaderDesc(ERHIShaderStage::Compute, "MainCS", "cs_payload"));
    Record(Result, Layout.Succeeded() && Vertex.Succeeded() && Fragment.Succeeded() && Compute.Succeeded() &&
        Device.GetDiagnostics().ShaderModuleReason[0] != '\0', "Vulkan shader modules and layout create with diagnostics");

    auto VulkanShader = std::dynamic_pointer_cast<FVulkanShaderModule>(Vertex.Object);
    Record(Result, VulkanShader && VulkanShader->GetRuntimeMode() == ERHIRuntimeObjectMode::DeterministicFallback &&
        VulkanShader->GetValidationMode() == ERHIShaderBytecodeValidationMode::StructuralFallback &&
        Device.GetDiagnostics().RuntimeModeReason[0] != '\0', "Vulkan shader module fallback runtime mode is explicit");

    FRHIShaderModuleDesc BadBytecode = ShaderDesc(ERHIShaderStage::Vertex, "MainVS", "bad");
    (void)SetRHIShaderSpirvWords(
        BadBytecode.Payload, {1u, 2u, 3u}, "bad");
    FRHIShaderModuleDesc TruncatedHeader =
        ShaderDesc(ERHIShaderStage::Vertex, "MainVS", "truncated");
    (void)SetRHIShaderSpirvWords(
        TruncatedHeader.Payload,
        {0x07230203u, 0x00010000u, 0u, 1u},
        "truncated");
    FRHIShaderModuleDesc WrongStage =
        ShaderDesc(ERHIShaderStage::Fragment, "MainVS", "wrong_stage");
    WrongStage.Payload = Stoner::Tests::MakeMinimalShaderPayload(
        ERHIShaderStage::Vertex, "MainVS", "wrong_stage");
    FRHIShaderModuleDesc BadMetadata = ShaderDesc(ERHIShaderStage::Vertex, "MainVS", "bad_meta");
    BadMetadata.InterfaceMetadata.Bindings[0].Visibility = ERHIShaderStageFlags::Fragment;
    Record(Result, Device.CreateShaderModule(BadBytecode).Result == ERHIResult::InvalidState &&
        Device.CreateShaderModule(TruncatedHeader).Result == ERHIResult::InvalidState &&
        Device.CreateShaderModule(WrongStage).Result == ERHIResult::InvalidState &&
        Device.CreateShaderModule(BadMetadata).Result == ERHIResult::InvalidState &&
        Device.CreateShaderModule(ShaderDesc(ERHIShaderStage::Mesh, "MainMS", "mesh")).Result == ERHIResult::Unsupported, "Vulkan shader module rejects malformed unsupported and metadata-incompatible inputs");
    FRHIPipelineLayoutDesc OverlappingLayout = ResourceLayoutDesc();
    OverlappingLayout.ConstantRanges.push_back({8, 16, ERHIShaderStageFlags::Compute});
    Record(Result, Device.CreatePipelineLayout(OverlappingLayout).Result == ERHIResult::InvalidState,
        "Vulkan pipeline layout rejects incompatible overlapping constant ranges");
    FRHIPipelineLayoutDesc MissingRangeLayoutDesc = ResourceLayoutDesc();
    MissingRangeLayoutDesc.ConstantRanges.clear();
    const auto MissingRangeLayout = Device.CreatePipelineLayout(MissingRangeLayoutDesc);
    Record(Result, MissingRangeLayout.Succeeded() &&
            Device.CreateComputePipeline(ComputePipelineDesc(Compute.Object, MissingRangeLayout.Object)).Result == ERHIResult::InvalidState,
        "Vulkan compute pipeline rejects shader constant ranges absent from its layout");

    const auto GraphicsPipeline = Device.CreateGraphicsPipeline(GraphicsPipelineDesc(Vertex.Object, Fragment.Object, Layout.Object));
    const auto GraphicsPipelineAgain = Device.CreateGraphicsPipeline(GraphicsPipelineDesc(Vertex.Object, Fragment.Object, Layout.Object));
    FRHIGraphicsPipelineDesc CounterClockwiseDesc =
        GraphicsPipelineDesc(Vertex.Object, Fragment.Object, Layout.Object);
    CounterClockwiseDesc.Rasterizer.FrontFace = ERHIFrontFace::CounterClockwise;
    const auto CounterClockwisePipeline = Device.CreateGraphicsPipeline(CounterClockwiseDesc);
    FRHIGraphicsPipelineDesc MirroredDesc =
        GraphicsPipelineDesc(Vertex.Object, Fragment.Object, Layout.Object);
    MirroredDesc.Rasterizer.FrontFace = ResolveRHIFrontFaceForTransform(
        MirroredDesc.Rasterizer.FrontFace, true);
    const auto MirroredPipeline = Device.CreateGraphicsPipeline(MirroredDesc);
    const auto ComputePipeline = Device.CreateComputePipeline(ComputePipelineDesc(Compute.Object, Layout.Object));
    const auto ComputePipelineAgain = Device.CreateComputePipeline(ComputePipelineDesc(Compute.Object, Layout.Object));
    FVulkanDevice ForeignDevice;
    Record(Result,
        InitializeDeterministic(ForeignDevice) == ERHIResult::Success,
        "Vulkan foreign shader fixture device initializes");
    const auto ForeignLayout =
        ForeignDevice.CreatePipelineLayout(ResourceLayoutDesc());
    const auto ForeignVertex = ForeignDevice.CreateShaderModule(
        ShaderDesc(ERHIShaderStage::Vertex, "MainVS", "foreign_vs"));
    const auto ForeignFragment = ForeignDevice.CreateShaderModule(
        ShaderDesc(ERHIShaderStage::Fragment, "MainPS", "foreign_ps"));
    const auto ForeignCompute = ForeignDevice.CreateShaderModule(
        ShaderDesc(ERHIShaderStage::Compute, "MainCS", "foreign_cs"));
    const auto ForeignGraphicsPipeline =
        ForeignDevice.CreateGraphicsPipeline(GraphicsPipelineDesc(
            ForeignVertex.Object,
            ForeignFragment.Object,
            ForeignLayout.Object));
    const auto ForeignComputePipeline =
        ForeignDevice.CreateComputePipeline(ComputePipelineDesc(
            ForeignCompute.Object,
            ForeignLayout.Object));
    Record(Result,
        ForeignLayout.Succeeded() && ForeignVertex.Succeeded() &&
            ForeignFragment.Succeeded() && ForeignCompute.Succeeded() &&
            ForeignGraphicsPipeline.Succeeded() &&
            ForeignComputePipeline.Succeeded() &&
            Device.CreateGraphicsPipeline(GraphicsPipelineDesc(
                ForeignVertex.Object,
                ForeignFragment.Object,
                ForeignLayout.Object)).Result == ERHIResult::InvalidState &&
            Device.CreateGraphicsPipeline(GraphicsPipelineDesc(
                ForeignVertex.Object,
                ForeignFragment.Object,
                Layout.Object)).Result == ERHIResult::InvalidState &&
            Device.CreateDescriptorSet(
                ForeignLayout.Object, 0).Result == ERHIResult::InvalidState,
        "Vulkan pipeline and descriptor factories reject foreign shader and layout provenance");
    auto VulkanGraphics = std::dynamic_pointer_cast<FVulkanGraphicsPipeline>(GraphicsPipeline.Object);
    auto VulkanCompute = std::dynamic_pointer_cast<FVulkanComputePipeline>(ComputePipeline.Object);
    Record(Result, GraphicsPipeline.Succeeded() && ComputePipeline.Succeeded() &&
        VulkanGraphics && VulkanGraphics->GetRuntimeMode() == ERHIRuntimeObjectMode::DeterministicFallback &&
        VulkanCompute && VulkanCompute->GetRuntimeMode() == ERHIRuntimeObjectMode::DeterministicFallback &&
        VulkanGraphics->GetDesc().Rasterizer.FrontFace == ERHIFrontFace::Clockwise &&
        CounterClockwisePipeline.Succeeded() &&
        CounterClockwisePipeline.Object->GetDesc().Rasterizer.FrontFace == ERHIFrontFace::CounterClockwise &&
        MirroredPipeline.Succeeded() &&
        MirroredPipeline.Object->GetDesc().Rasterizer.FrontFace == ERHIFrontFace::CounterClockwise &&
        MirroredPipeline.Object != GraphicsPipeline.Object,
        "Vulkan pipelines preserve clockwise defaults and resolved mirrored front-face overrides");
    FRHIGraphicsPipelineDesc InvalidFixedFunction =
        GraphicsPipelineDesc(Vertex.Object, Fragment.Object, Layout.Object);
    InvalidFixedFunction.Rasterizer.CullMode = static_cast<ERHICullMode>(255);
    Record(Result, Device.CreateGraphicsPipeline(InvalidFixedFunction).Result == ERHIResult::InvalidState,
        "Vulkan graphics pipeline rejects undefined fixed-function state");
    Record(Result, GraphicsPipelineAgain.Succeeded() && GraphicsPipelineAgain.Object == GraphicsPipeline.Object &&
        ComputePipelineAgain.Succeeded() && ComputePipelineAgain.Object == ComputePipeline.Object &&
        Device.GetDiagnostics().PipelineCacheReason[0] != '\0', "Vulkan pipeline cache reuses equivalent successful requests");

    FRHIGraphicsPipelineDesc MissingFragment = GraphicsPipelineDesc(Vertex.Object, Fragment.Object, Layout.Object);
    MissingFragment.ShaderModules = {Vertex.Object};
    FRHIGraphicsPipelineDesc InvalidVertexInput = GraphicsPipelineDesc(Vertex.Object, Fragment.Object, Layout.Object);
    InvalidVertexInput.VertexInput.Stride = 0;
    Record(Result, Device.CreateGraphicsPipeline(MissingFragment).Result == ERHIResult::InvalidState &&
        Device.CreateGraphicsPipeline(InvalidVertexInput).Result == ERHIResult::InvalidState, "Vulkan graphics pipeline rejects missing stages and invalid vertex input");

    FRHIComputePipelineDesc WrongCompute = ComputePipelineDesc(Vertex.Object, Layout.Object);
    Record(Result, Device.CreateComputePipeline(WrongCompute).Result == ERHIResult::Unsupported, "Vulkan compute pipeline rejects wrong-stage shader");

    const auto RenderPass = Device.CreateRenderPass(ValidRenderPassDesc());
    const auto Texture = Device.CreateTexture(ValidColorAttachmentTextureDesc());
    const auto Framebuffer = Device.CreateFramebuffer(ValidFramebufferDesc(RenderPass.Object, Texture.Object));
    const auto GraphicsCommand = Device.CreateCommandBuffer(ERHIQueueType::Graphics);
    Record(Result, GraphicsCommand.Object->Begin() == ERHIResult::Success &&
        GraphicsCommand.Object->BeginRenderPass(RenderPass.Object, Framebuffer.Object) == ERHIResult::Success &&
        GraphicsCommand.Object->BindGraphicsPipeline(
            ForeignGraphicsPipeline.Object) == ERHIResult::InvalidState &&
        GraphicsCommand.Object->BindGraphicsPipeline(GraphicsPipeline.Object) == ERHIResult::Success &&
        GraphicsCommand.Object->RecordDraw(3, 1) == ERHIResult::Success &&
        GraphicsCommand.Object->RecordDrawIndexed(3, 1) == ERHIResult::Success &&
        GraphicsCommand.Object->EndRenderPass() == ERHIResult::Success &&
        GraphicsCommand.Object->End() == ERHIResult::Success &&
        Device.GetDiagnostics().PipelineBindingReason[0] != '\0', "Vulkan command buffer binds graphics pipeline and records draw diagnostics");

    const auto FullArgumentCommand =
        Device.CreateCommandBuffer(ERHIQueueType::Graphics);
    const auto ConcreteFullArgumentCommand =
        std::dynamic_pointer_cast<FVulkanCommandBuffer>(
            FullArgumentCommand.Object);
    const FRHIIndexedDrawArguments FullArguments{36, 4, 12, -7, 9};
    Record(Result,
        FullArgumentCommand.Succeeded() &&
            FullArgumentCommand.Object->Begin() == ERHIResult::Success &&
            FullArgumentCommand.Object->BeginRenderPass(
                RenderPass.Object, Framebuffer.Object) == ERHIResult::Success &&
            FullArgumentCommand.Object->RecordDrawIndexed(FullArguments) ==
                ERHIResult::Success &&
            ConcreteFullArgumentCommand &&
            ConcreteFullArgumentCommand->GetRecordedCommands().back().A == 36 &&
            ConcreteFullArgumentCommand->GetRecordedCommands().back().B == 4 &&
            ConcreteFullArgumentCommand->GetRecordedCommands().back().C == 12 &&
            ConcreteFullArgumentCommand->GetRecordedCommands().back().D == -7 &&
            ConcreteFullArgumentCommand->GetRecordedCommands().back().E == 9 &&
            FullArgumentCommand.Object->RecordDrawIndexed(
                {0, 1, 0, 0, 0}) == ERHIResult::InvalidState,
        "Vulkan indexed draw records all backend-neutral fields without reinterpretation");

    const auto ComputeCommand = Device.CreateCommandBuffer(ERHIQueueType::Compute);
    const auto TransferCommand = Device.CreateCommandBuffer(ERHIQueueType::Transfer);
    Record(Result, ComputeCommand.Object->Begin() == ERHIResult::Success &&
        ComputeCommand.Object->BindComputePipeline(
            ForeignComputePipeline.Object) == ERHIResult::InvalidState &&
        ComputeCommand.Object->BindComputePipeline(ComputePipeline.Object) == ERHIResult::Success &&
        ComputeCommand.Object->RecordDispatch(1, 1, 1) == ERHIResult::Success &&
        TransferCommand.Object->Begin() == ERHIResult::Success &&
        TransferCommand.Object->BindComputePipeline(ComputePipeline.Object) == ERHIResult::Unsupported, "Vulkan command buffer binds compute pipeline and rejects transfer queue binding");

    FVulkanDevice LimitedDevice;
    Record(Result, InitializeDeterministic(LimitedDevice) == ERHIResult::Success, "Vulkan pipeline limit fixture initializes");
    LimitedDevice.ConfigurePipelineCreationLimit(1);
    const auto LimitedLayout = LimitedDevice.CreatePipelineLayout(ResourceLayoutDesc());
    const auto LimitedVS = LimitedDevice.CreateShaderModule(ShaderDesc(ERHIShaderStage::Vertex, "MainVS", "limit_vs"));
    const auto LimitedPS = LimitedDevice.CreateShaderModule(ShaderDesc(ERHIShaderStage::Fragment, "MainPS", "limit_ps"));
    const auto LimitedCS = LimitedDevice.CreateShaderModule(ShaderDesc(ERHIShaderStage::Compute, "MainCS", "limit_cs"));
    Record(Result, LimitedDevice.CreateGraphicsPipeline(GraphicsPipelineDesc(LimitedVS.Object, LimitedPS.Object, LimitedLayout.Object)).Succeeded() &&
        LimitedDevice.CreateComputePipeline(ComputePipelineDesc(LimitedCS.Object, LimitedLayout.Object)).Result == ERHIResult::Unavailable, "Vulkan configured pipeline creation limit is deterministic");

    (void)Device.Shutdown();
    (void)ForeignDevice.Shutdown();
    Record(Result, Vertex.Object->GetLifecycleState() == ERHIResourceLifecycleState::Invalidated &&
        GraphicsPipeline.Object->GetLifecycleState() == ERHIResourceLifecycleState::Invalidated &&
        ComputePipeline.Object->GetLifecycleState() == ERHIResourceLifecycleState::Invalidated &&
        Device.CreateShaderModule(ShaderDesc(ERHIShaderStage::Vertex, "MainVS", "after")).Result == ERHIResult::InvalidState, "Vulkan shutdown invalidates shader and pipeline objects");
    (void)LimitedDevice.Shutdown();
}

void TestDrawDispatchPipelineDiagnostics(FVulkanBackendTestResult& Result)
{
    FVulkanDevice Device;
    Record(Result, InitializeDeterministic(Device) == ERHIResult::Success, "Vulkan draw/dispatch diagnostics fixture device initializes");

    const auto Layout = Device.CreatePipelineLayout(ResourceLayoutDesc());
    const auto Vertex = Device.CreateShaderModule(ShaderDesc(ERHIShaderStage::Vertex, "MainVS", "vs_diag"));
    const auto Fragment = Device.CreateShaderModule(ShaderDesc(ERHIShaderStage::Fragment, "MainPS", "ps_diag"));
    const auto Compute = Device.CreateShaderModule(ShaderDesc(ERHIShaderStage::Compute, "MainCS", "cs_diag"));
    const auto GraphicsPipeline = Device.CreateGraphicsPipeline(GraphicsPipelineDesc(Vertex.Object, Fragment.Object, Layout.Object));
    const auto ComputePipeline = Device.CreateComputePipeline(ComputePipelineDesc(Compute.Object, Layout.Object));

    const auto RenderPass = Device.CreateRenderPass(ValidRenderPassDesc());
    const auto Texture = Device.CreateTexture(ValidColorAttachmentTextureDesc());
    const auto Framebuffer = Device.CreateFramebuffer(ValidFramebufferDesc(RenderPass.Object, Texture.Object));

    const auto HasReason = [&Device](const char* Needle) {
        return std::string_view(Device.GetDiagnostics().CommandRecordingReason).find(Needle) != std::string_view::npos;
    };

    // Draw/indexed draw without a bound graphics pipeline are still recorded, but flag a missing pipeline.
    const auto GraphicsCommand = Device.CreateCommandBuffer(ERHIQueueType::Graphics);
    Record(Result, GraphicsCommand.Object->Begin() == ERHIResult::Success &&
        GraphicsCommand.Object->BeginRenderPass(RenderPass.Object, Framebuffer.Object) == ERHIResult::Success &&
        GraphicsCommand.Object->RecordDraw(3, 1) == ERHIResult::Success && HasReason("missing") &&
        GraphicsCommand.Object->RecordDrawIndexed(3, 1) == ERHIResult::Success && HasReason("missing"),
        "Vulkan draw without bound graphics pipeline reports missing-pipeline diagnostics");

    // Draw/indexed draw with a compatible bound graphics pipeline report compatible binding.
    Record(Result, GraphicsCommand.Object->BindGraphicsPipeline(GraphicsPipeline.Object) == ERHIResult::Success &&
        GraphicsCommand.Object->RecordDraw(3, 1) == ERHIResult::Success && HasReason("compatible") &&
        GraphicsCommand.Object->RecordDrawIndexed(3, 1) == ERHIResult::Success && HasReason("compatible"),
        "Vulkan draw with bound graphics pipeline reports compatible-pipeline diagnostics");

    // The graphics and compute binding slots are independent: a dispatch here still lacks a compute pipeline.
    Record(Result, GraphicsCommand.Object->RecordDispatch(1, 1, 1) == ERHIResult::Success && HasReason("missing"),
        "Vulkan dispatch without bound compute pipeline reports missing-pipeline diagnostics on graphics queue");
    (void)GraphicsCommand.Object->EndRenderPass();
    (void)GraphicsCommand.Object->End();

    // Dispatch transitions from missing to compatible once a compute pipeline is bound.
    const auto ComputeCommand = Device.CreateCommandBuffer(ERHIQueueType::Compute);
    Record(Result, ComputeCommand.Object->Begin() == ERHIResult::Success &&
        ComputeCommand.Object->RecordDispatch(1, 1, 1) == ERHIResult::Success && HasReason("missing") &&
        ComputeCommand.Object->BindComputePipeline(ComputePipeline.Object) == ERHIResult::Success &&
        ComputeCommand.Object->RecordDispatch(1, 1, 1) == ERHIResult::Success && HasReason("compatible"),
        "Vulkan dispatch reports missing then compatible compute pipeline diagnostics");
    (void)ComputeCommand.Object->End();

    // An invalidated pipeline cannot be bound, leaving unrelated state untouched.
    const auto FreshCompute = Device.CreateCommandBuffer(ERHIQueueType::Compute);
    Record(Result, ComputePipeline.Object->Invalidate() == ERHIResult::Success &&
        FreshCompute.Object->Begin() == ERHIResult::Success &&
        FreshCompute.Object->BindComputePipeline(ComputePipeline.Object) == ERHIResult::InvalidState,
        "Vulkan binding an invalidated compute pipeline is rejected");
    (void)Device.Shutdown();
}

void TestPipelineCacheKeyAndStateValidation(FVulkanBackendTestResult& Result)
{
    FVulkanDevice Device;
    Record(Result, InitializeDeterministic(Device) == ERHIResult::Success, "Vulkan pipeline cache/state fixture device initializes");

    const auto Layout = Device.CreatePipelineLayout(ResourceLayoutDesc());
    const auto Vertex = Device.CreateShaderModule(ShaderDesc(ERHIShaderStage::Vertex, "MainVS", "vs_cache"));
    const auto Fragment = Device.CreateShaderModule(ShaderDesc(ERHIShaderStage::Fragment, "MainPS", "ps_cache"));

    // Pipelines differing only in fixed-function state must not collide on the same cache key.
    const FRHIGraphicsPipelineDesc Base = GraphicsPipelineDesc(Vertex.Object, Fragment.Object, Layout.Object);
    FRHIGraphicsPipelineDesc BlendVariant = Base;
    BlendVariant.Blend.bEnabled = true;
    FRHIGraphicsPipelineDesc CullVariant = Base;
    CullVariant.Rasterizer.CullMode = ERHICullMode::None;

    const auto P1 = Device.CreateGraphicsPipeline(Base);
    const auto P2 = Device.CreateGraphicsPipeline(BlendVariant);
    const auto P3 = Device.CreateGraphicsPipeline(CullVariant);
    const auto P1Again = Device.CreateGraphicsPipeline(Base);
    Record(Result, P1.Succeeded() && P2.Succeeded() && P3.Succeeded() &&
        P1.Object != P2.Object && P1.Object != P3.Object && P2.Object != P3.Object,
        "Vulkan pipeline cache key distinguishes fixed-function state variants");
    Record(Result, P1Again.Succeeded() && P1Again.Object == P1.Object,
        "Vulkan pipeline cache still reuses identical graphics pipeline descriptions");

    FRHIShaderModuleDesc InterfaceVariantDesc =
        ShaderDesc(ERHIShaderStage::Vertex, "MainVS", "vs_cache");
    InterfaceVariantDesc.InterfaceMetadata.Bindings.clear();
    InterfaceVariantDesc.InterfaceMetadata.ConstantRanges.clear();
    const auto InterfaceVariantShader =
        Device.CreateShaderModule(InterfaceVariantDesc);
    const auto InterfaceVariantPipeline =
        Device.CreateGraphicsPipeline(GraphicsPipelineDesc(
            InterfaceVariantShader.Object,
            Fragment.Object,
            Layout.Object));
    Record(Result,
        InterfaceVariantShader.Succeeded() &&
            InterfaceVariantPipeline.Succeeded() &&
            InterfaceVariantPipeline.Object != P1.Object,
        "Vulkan pipeline cache key includes complete shader interface metadata");

    const auto DelimiterVertexA = Device.CreateShaderModule(
        ShaderDesc(ERHIShaderStage::Vertex, "c", "a:b"));
    const auto DelimiterVertexB = Device.CreateShaderModule(
        ShaderDesc(ERHIShaderStage::Vertex, "b:c", "a"));
    const auto DelimiterPipelineA =
        Device.CreateGraphicsPipeline(GraphicsPipelineDesc(
            DelimiterVertexA.Object, Fragment.Object, Layout.Object));
    const auto DelimiterPipelineB =
        Device.CreateGraphicsPipeline(GraphicsPipelineDesc(
            DelimiterVertexB.Object, Fragment.Object, Layout.Object));
    Record(Result,
        DelimiterPipelineA.Succeeded() &&
            DelimiterPipelineB.Succeeded() &&
            DelimiterPipelineA.Object != DelimiterPipelineB.Object,
        "Vulkan pipeline cache key length-prefixes shader identities and entry points");

    // Depth test/write requires a depth-stencil attachment; the same state with a depth format is accepted.
    FRHIGraphicsPipelineDesc DepthNoFormat = Base;
    DepthNoFormat.DepthStencil.bDepthTestEnabled = true;
    FRHIGraphicsPipelineDesc DepthWithFormat = DepthNoFormat;
    DepthWithFormat.RenderTargets.DepthStencilFormat = ERHIFormat::D24_UNorm_S8_UInt;
    Record(Result, Device.CreateGraphicsPipeline(DepthNoFormat).Result == ERHIResult::InvalidState,
        "Vulkan graphics pipeline rejects depth test without a depth-stencil attachment");
    Record(Result, Device.CreateGraphicsPipeline(DepthWithFormat).Succeeded(),
        "Vulkan graphics pipeline accepts depth test with a compatible depth-stencil attachment");

    const FRHIShaderModuleDesc ReplacementVertexDesc = Vertex.Object->GetDesc();
    Record(Result,
        Vertex.Object->Invalidate() == ERHIResult::Success,
        "Vulkan pipeline cache stale-dependency fixture invalidates the original shader");
    const auto ReplacementVertex =
        Device.CreateShaderModule(ReplacementVertexDesc);
    const auto ReplacementPipeline =
        Device.CreateGraphicsPipeline(GraphicsPipelineDesc(
            ReplacementVertex.Object,
            Fragment.Object,
            Layout.Object));
    const auto ConcreteP1 =
        std::dynamic_pointer_cast<FVulkanGraphicsPipeline>(P1.Object);
    Record(Result,
        ReplacementVertex.Succeeded() &&
            ReplacementPipeline.Succeeded() &&
            ReplacementPipeline.Object != P1.Object &&
            ConcreteP1 && !ConcreteP1->HasValidDependencies(),
        "Vulkan pipeline cache rejects entries with invalidated retained dependencies");
    (void)Device.Shutdown();
}

void TestPipelineFormatCapabilities(FVulkanBackendTestResult& Result)
{
    FVulkanInstanceDesc ColorOnlyDesc = MakeDeterministicInstanceDesc();
    ColorOnlyDesc.SyntheticCandidates = {
        MakeCandidate(
            "PipelineColorOnly",
            EVulkanPhysicalDeviceType::Integrated,
            true,
            {true, true, true, false},
            false,
            {true, false}),
    };
    FVulkanDevice Device;
    Record(Result,
        Device.Initialize(ColorOnlyDesc) == ERHIResult::Success,
        "Vulkan pipeline format-capability fixture initializes");
    const auto Layout = Device.CreatePipelineLayout(ResourceLayoutDesc());
    const auto Vertex = Device.CreateShaderModule(
        ShaderDesc(ERHIShaderStage::Vertex, "MainVS", "format_vs"));
    const auto Fragment = Device.CreateShaderModule(
        ShaderDesc(ERHIShaderStage::Fragment, "MainPS", "format_ps"));
    FRHIGraphicsPipelineDesc UnsupportedDepth =
        GraphicsPipelineDesc(Vertex.Object, Fragment.Object, Layout.Object);
    UnsupportedDepth.RenderTargets.DepthStencilFormat =
        ERHIFormat::D32_Float;
    UnsupportedDepth.DepthStencil.bDepthTestEnabled = true;
    Record(Result,
        Device.CreateGraphicsPipeline(UnsupportedDepth).Result ==
            ERHIResult::Unsupported,
        "Vulkan graphics pipeline rejects attachment formats absent from device capabilities");
    (void)Device.Shutdown();
}

void TestResourceCreationAndAllocation(FVulkanBackendTestResult& Result)
{
    FVulkanDevice Device;
    Record(Result, InitializeDeterministic(Device) == ERHIResult::Success, "Vulkan resource fixture device initializes");

    const auto Buffer = Device.CreateBuffer(ValidBufferDesc());
    const auto Texture = Device.CreateTexture(ValidTextureDesc());
    Record(Result, Buffer.Succeeded() && Buffer.Object->GetSizeInBytes() == 256 &&
        Buffer.Object->GetLifecycleState() == ERHIResourceLifecycleState::Valid, "Vulkan buffer creation preserves description and lifecycle");
    Record(Result, Texture.Succeeded() && Texture.Object->GetFormat() == ERHIFormat::R8G8B8A8_UNorm &&
        Texture.Object->GetLifecycleState() == ERHIResourceLifecycleState::Valid, "Vulkan texture creation preserves description and lifecycle");

    auto VulkanBuffer = std::dynamic_pointer_cast<FVulkanBuffer>(Buffer.Object);
    auto VulkanTexture = std::dynamic_pointer_cast<FVulkanTexture>(Texture.Object);
    Record(Result, VulkanBuffer && VulkanBuffer->GetAllocation().IsSuccessful() &&
        VulkanTexture && VulkanTexture->GetAllocation().IsSuccessful() &&
        Device.GetDiagnostics().ResourceAllocationReason[0] != '\0', "Vulkan resources expose real-or-fallback allocation diagnostics");

    FRHIBufferDesc InvalidBuffer = ValidBufferDesc();
    InvalidBuffer.SizeInBytes = 0;
    FRHITextureDesc InvalidTexture = ValidTextureDesc();
    InvalidTexture.Width = 0;
    Record(Result, Device.CreateBuffer(InvalidBuffer).Result == ERHIResult::Unsupported &&
        Device.CreateTexture(InvalidTexture).Result == ERHIResult::Unsupported, "Vulkan invalid buffer and texture descriptions are rejected");

    FVulkanDevice LimitedDevice;
    Record(Result, InitializeDeterministic(LimitedDevice) == ERHIResult::Success, "Vulkan allocation-limit fixture initializes");
    LimitedDevice.ConfigureAllocationBudget(16);
    Record(Result, LimitedDevice.CreateBuffer(ValidBufferDesc()).Result == ERHIResult::Unavailable &&
        LimitedDevice.GetAllocationSnapshot().LastFailure == EVulkanAllocationFailure::BudgetExceeded, "Vulkan allocation budget failure is deterministic");
    LimitedDevice.ResetResourceConfiguration();
    LimitedDevice.ConfigureAllocationCountLimit(1);
    Record(Result, LimitedDevice.CreateBuffer(ValidBufferDesc()).Succeeded() &&
        LimitedDevice.CreateTexture(ValidTextureDesc()).Result == ERHIResult::Unavailable &&
        LimitedDevice.GetAllocationSnapshot().LastFailure == EVulkanAllocationFailure::AllocationCountExceeded, "Vulkan allocation-count failure is deterministic");

    Record(Result, Buffer.Object && Buffer.Object->Invalidate() == ERHIResult::Success &&
        Buffer.Object->Invalidate() == ERHIResult::InvalidState, "Vulkan buffer invalidation releases allocation once");
    (void)Device.Shutdown();
    Record(Result, Texture.Object && Texture.Object->GetLifecycleState() == ERHIResourceLifecycleState::Invalidated &&
        Device.CreateBuffer(ValidBufferDesc()).Result == ERHIResult::InvalidState, "Vulkan shutdown invalidates resource objects and rejects creation");
    (void)LimitedDevice.Shutdown();
}

void TestAllocationOwnershipAndFootprints(
    FVulkanBackendTestResult& Result)
{
    static_assert(
        !std::is_copy_constructible_v<FVulkanResourceAllocation> &&
        !std::is_copy_assignable_v<FVulkanResourceAllocation>,
        "allocation ownership records must not be duplicated");
    static_assert(
        !std::is_copy_constructible_v<FVulkanMemoryAllocator> &&
        !std::is_move_constructible_v<FVulkanMemoryAllocator>,
        "allocator identity must remain stable for its full lifetime");
    static_assert(!std::is_constructible_v<FVulkanBuffer,
        const FRHIBufferDesc&, FVulkanResourceAllocation&&,
        std::shared_ptr<FVulkanMemoryAllocator>>,
        "buffers must be created through FVulkanDevice");
    static_assert(!std::is_constructible_v<FVulkanTexture,
        const FRHITextureDesc&, FVulkanResourceAllocation&&,
        std::shared_ptr<FVulkanMemoryAllocator>>,
        "textures must be created through FVulkanDevice");

    FVulkanMemoryAllocator Owner;
    FVulkanMemoryAllocator ForeignOwner;
    const FRHIBufferDesc ThirtyTwoBytes = {
        32, ERHIBufferUsage::Storage};
    auto Owned = Owner.AllocateBuffer(ThirtyTwoBytes, true);
    Record(Result, Owned.IsSuccessful() &&
        ForeignOwner.Release(Owned) == ERHIResult::InvalidState &&
        Owner.GetSnapshot().AllocatedBytes == 32 &&
        Owner.GetSnapshot().LiveAllocationCount == 1,
        "Vulkan allocation release rejects foreign allocator ownership");
    Record(Result, Owner.Release(Owned) == ERHIResult::Success &&
        Owner.Release(Owned) == ERHIResult::InvalidState &&
        Owner.GetSnapshot().AllocatedBytes == 0 &&
        Owner.GetSnapshot().LiveAllocationCount == 0,
        "Vulkan move-only allocation ownership releases accounting once");

    auto Stale = Owner.AllocateBuffer(ThirtyTwoBytes, true);
    Owner.Reset();
    Record(Result, Owner.Release(Stale) == ERHIResult::InvalidState &&
        Owner.GetSnapshot().AllocatedBytes == 0 &&
        Owner.GetSnapshot().LiveAllocationCount == 0,
        "Vulkan allocator reset rejects stale ownership epochs");

    FRHITextureDesc Multisampled = ValidTextureDesc();
    Multisampled.Width = 4;
    Multisampled.Height = 4;
    Multisampled.SampleCount = ERHISampleCount::Four;
    FRHITextureDesc WideFormat = Multisampled;
    WideFormat.SampleCount = ERHISampleCount::One;
    WideFormat.Format = ERHIFormat::R32G32B32_Float;
    FRHITextureDesc MipChain = ValidTextureDesc();
    MipChain.MipLevels = 4;

    uint64 MultisampledBytes = 0;
    uint64 WideFormatBytes = 0;
    uint64 MipChainBytes = 0;
    Record(Result,
        FVulkanMemoryAllocator::TryEstimateTextureBytes(
            Multisampled, MultisampledBytes) &&
        FVulkanMemoryAllocator::TryEstimateTextureBytes(
            WideFormat, WideFormatBytes) &&
        FVulkanMemoryAllocator::TryEstimateTextureBytes(
            MipChain, MipChainBytes) &&
        MultisampledBytes == 256 && WideFormatBytes == 192 &&
        MipChainBytes == 340,
        "Vulkan texture footprints include format samples and mip extents");

    FVulkanMemoryAllocator TextureBudget;
    TextureBudget.ConfigureBudgetLimit(255);
    auto RejectedMultisample =
        TextureBudget.AllocateTexture(Multisampled, true);
    TextureBudget.ConfigureBudgetLimit(256);
    auto AcceptedMultisample =
        TextureBudget.AllocateTexture(Multisampled, true);
    Record(Result,
        !RejectedMultisample.IsSuccessful() &&
        RejectedMultisample.GetFailure() ==
            EVulkanAllocationFailure::BudgetExceeded &&
        AcceptedMultisample.IsSuccessful() &&
        TextureBudget.GetSnapshot().AllocatedBytes == 256,
        "Vulkan texture budget uses the checked physical footprint");
    (void)TextureBudget.Release(AcceptedMultisample);

    FRHITextureDesc Unrepresentable = ValidTextureDesc();
    Unrepresentable.Dimension = ERHITextureDimension::Texture2DArray;
    Unrepresentable.Width = std::numeric_limits<uint32>::max();
    Unrepresentable.Height = std::numeric_limits<uint32>::max();
    Unrepresentable.ArrayLayers = 2;
    Unrepresentable.Format = ERHIFormat::R32G32B32_Float;
    uint64 UnrepresentableBytes = 1;
    auto RejectedTexture =
        TextureBudget.AllocateTexture(Unrepresentable, true);
    Record(Result,
        !FVulkanMemoryAllocator::TryEstimateTextureBytes(
            Unrepresentable, UnrepresentableBytes) &&
        UnrepresentableBytes == 0 &&
        !RejectedTexture.IsSuccessful() &&
        RejectedTexture.GetFailure() ==
            EVulkanAllocationFailure::ArithmeticOverflow,
        "Vulkan texture footprint overflow is explicit and non-mutating");

    FVulkanMemoryAllocator OverflowAllocator;
    FRHIBufferDesc HugeBuffer = ValidBufferDesc();
    HugeBuffer.SizeInBytes = std::numeric_limits<uint64>::max();
    HugeBuffer.MemoryAccess = ERHIMemoryAccess::HostVisible;
    auto HugeAllocation = OverflowAllocator.AllocateBuffer(HugeBuffer, true);
    FRHIBufferDesc TwoBytes = ValidBufferDesc();
    TwoBytes.SizeInBytes = 2;
    auto Overflowed = OverflowAllocator.AllocateBuffer(TwoBytes, true);
    Record(Result, HugeAllocation.IsSuccessful() &&
        !Overflowed.IsSuccessful() &&
        Overflowed.GetFailure() == EVulkanAllocationFailure::ArithmeticOverflow &&
        OverflowAllocator.GetSnapshot().LastFailure ==
            EVulkanAllocationFailure::ArithmeticOverflow &&
        OverflowAllocator.GetSnapshot().AllocatedBytes ==
            std::numeric_limits<uint64>::max() &&
        OverflowAllocator.GetSnapshot().LiveAllocationCount == 1,
        "Vulkan allocation accounting rejects overflow without mutation");

    OverflowAllocator.ConfigureBudgetLimit(2);
    FRHIBufferDesc OneByte = ValidBufferDesc();
    OneByte.SizeInBytes = 1;
    auto BudgetRejected = OverflowAllocator.AllocateBuffer(OneByte, true);
    Record(Result,
        !BudgetRejected.IsSuccessful() &&
        BudgetRejected.GetFailure() == EVulkanAllocationFailure::BudgetExceeded &&
        OverflowAllocator.GetSnapshot().LastFailure ==
            EVulkanAllocationFailure::BudgetExceeded &&
        OverflowAllocator.GetSnapshot().AllocatedBytes ==
            std::numeric_limits<uint64>::max(),
        "Vulkan post-overflow budget checks cannot be bypassed");

    FVulkanDevice OverflowDevice;
    Record(Result,
        InitializeDeterministic(OverflowDevice) == ERHIResult::Success,
        "Vulkan allocation-overflow fixture initializes");
    const auto Huge = OverflowDevice.CreateBuffer(HugeBuffer);
    FRHIBufferDesc SmallHostBuffer = ValidBufferDesc();
    SmallHostBuffer.SizeInBytes = 1;
    SmallHostBuffer.MemoryAccess = ERHIMemoryAccess::HostVisible;
    const auto Small = OverflowDevice.CreateBuffer(SmallHostBuffer);
    auto SmallVulkanBuffer =
        std::dynamic_pointer_cast<FVulkanBuffer>(Small.Object);
    const uint8 Byte = 0x5a;
    Record(Result, Huge.Result == ERHIResult::Unsupported &&
        OverflowDevice.GetAllocationSnapshot().LiveAllocationCount == 1 &&
        SmallVulkanBuffer &&
        SmallVulkanBuffer->Upload(&Byte, 1, 0) == ERHIResult::Success &&
        SmallVulkanBuffer->Upload(&Byte, 1, 1) == ERHIResult::InvalidState,
        "Vulkan capability gates oversized buffers and host uploads reject invalid ranges without growth");
    (void)OverflowAllocator.Release(HugeAllocation);
    (void)OverflowDevice.Shutdown();
    Record(Result,
        OverflowDevice.GetAllocationSnapshot().AllocatedBytes == 0 &&
        OverflowDevice.GetAllocationSnapshot().LiveAllocationCount == 0,
        "Vulkan shutdown releases extreme-size allocation accounting");
}

void TestCommandBuffersRecordingAndSubmission(FVulkanBackendTestResult& Result)
{
    static_assert(
        !std::is_constructible_v<FVulkanCommandPool,
            ERHIQueueType, uint32> &&
        !std::is_constructible_v<FVulkanCommandBuffer,
            ERHIQueueType, FVulkanDiagnostics*> &&
        !std::is_constructible_v<FVulkanCommandSubmission,
            TSharedPtr<FVulkanCommandBuffer>,
            EVulkanSubmissionMode,
            FVulkanCompletionInjectionConfig> &&
        !std::is_constructible_v<FVulkanQueue, ERHIQueueType> &&
        !std::is_constructible_v<FVulkanFence, bool> &&
        !std::is_default_constructible_v<FVulkanSemaphore> &&
        !CHasPublicMarkSubmitted<FVulkanCommandBuffer> &&
        !CHasPublicMarkCompletedOrResettable<FVulkanCommandBuffer>,
        "Vulkan command, queue, and synchronization objects must be owner-only");

    TSharedPtr<IRHICommandBuffer> RetainedCommand;
    {
        auto OwnedDevice = std::make_unique<FVulkanDevice>();
        Record(Result,
            InitializeDeterministic(*OwnedDevice) == ERHIResult::Success,
            "Vulkan retained-command fixture initializes");
        RetainedCommand =
            OwnedDevice->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
    }
    Record(Result, RetainedCommand &&
        RetainedCommand->Begin() == ERHIResult::InvalidState,
        "Vulkan device destruction invalidates retained command buffers");

    FVulkanDevice ZeroCapacityDevice;
    Record(Result,
        InitializeDeterministic(ZeroCapacityDevice) == ERHIResult::Success,
        "Vulkan zero-capacity command fixture initializes");
    ZeroCapacityDevice.ConfigureCommandBufferCapacity(0);
    Record(Result,
        ZeroCapacityDevice.CreateCommandBuffer(ERHIQueueType::Graphics).Result ==
            ERHIResult::Unavailable,
        "Vulkan zero command-buffer capacity rejects allocation");
    (void)ZeroCapacityDevice.Shutdown();

    FVulkanDevice Device;
    Record(Result, InitializeDeterministic(Device) == ERHIResult::Success, "Vulkan command fixture device initializes");

    Device.ConfigureCommandBufferCapacity(2);
    const auto FirstCommand = Device.CreateCommandBuffer(ERHIQueueType::Graphics);
    const auto SecondCommand = Device.CreateCommandBuffer(ERHIQueueType::Graphics);
    const auto ExhaustedCommand = Device.CreateCommandBuffer(ERHIQueueType::Graphics);
    Record(Result, FirstCommand.Succeeded() && FirstCommand.Object->GetCompatibleQueueType() == ERHIQueueType::Graphics &&
        FirstCommand.Object->GetRecordedCommandCount() == 0, "Vulkan command buffer allocation succeeds with metadata");
    Record(Result, SecondCommand.Succeeded() && ExhaustedCommand.Result == ERHIResult::Unavailable &&
        Device.GetDiagnostics().CommandAllocationReason[0] != '\0', "Vulkan command buffer capacity exhaustion is explicit");

    Record(Result, FirstCommand.Object->Begin() == ERHIResult::Success &&
        FirstCommand.Object->Begin() == ERHIResult::InvalidState &&
        FirstCommand.Object->RecordDraw(3) == ERHIResult::InvalidState &&
        FirstCommand.Object->RecordDispatch(1, 1, 1) == ERHIResult::Success &&
        FirstCommand.Object->RecordBarrier() == ERHIResult::Success &&
        FirstCommand.Object->End() == ERHIResult::Success, "Vulkan command lifecycle and compute/barrier recording");
    Record(Result, FirstCommand.Object->RecordDispatch(1, 1, 1) == ERHIResult::InvalidState &&
        FirstCommand.Object->Reset() == ERHIResult::Success &&
        FirstCommand.Object->GetRecordedCommandCount() == 0, "Vulkan reset clears stale recorded commands");

    const auto Queue = Device.CreateCommandQueue(ERHIQueueType::Graphics);
    Record(Result, Queue.Succeeded(), "Vulkan command submission queue creates");
    (void)FirstCommand.Object->Begin();
    (void)FirstCommand.Object->RecordDispatch(1, 1, 1);
    (void)FirstCommand.Object->End();
    const auto WaitSemaphore = Device.CreateSemaphore();
    const auto SignalSemaphore = Device.CreateSemaphore();
    const auto Fence = Device.CreateFence(false);
    (void)WaitSemaphore.Object->Signal();
    Record(Result, Queue.Object->Submit(FirstCommand.Object, {WaitSemaphore.Object}, {SignalSemaphore.Object}, Fence.Object) == ERHIResult::Success &&
        Queue.Object->GetSubmittedCommandBufferCount() == 1 &&
        WaitSemaphore.Object->GetState() == ERHISemaphoreState::Consumed &&
        SignalSemaphore.Object->IsSignaled() &&
        Fence.Object->IsSignaled() &&
        FirstCommand.Object->GetState() == ERHICommandBufferState::Submitted &&
        Device.GetDiagnostics().SubmissionReason[0] != '\0', "Vulkan fallback queue submission consumes/signals sync and marks submitted");
    Record(Result, FirstCommand.Object->Reset() == ERHIResult::InvalidState &&
        Queue.Object->WaitIdle() == ERHIResult::Success &&
        FirstCommand.Object->GetState() == ERHICommandBufferState::Resettable &&
        FirstCommand.Object->Reset() == ERHIResult::Success, "Vulkan queue wait idle makes submitted command buffer resettable");

    const auto FirstAtomicWait = Device.CreateSemaphore();
    const auto SecondAtomicWait = Device.CreateSemaphore();
    (void)FirstAtomicWait.Object->Signal();
    (void)SecondCommand.Object->Begin();
    (void)SecondCommand.Object->RecordDispatch(1, 1, 1);
    (void)SecondCommand.Object->End();
    Record(Result,
        Queue.Object->Submit(
            SecondCommand.Object,
            {FirstAtomicWait.Object, SecondAtomicWait.Object}) ==
                ERHIResult::NotReady &&
            FirstAtomicWait.Object->IsSignaled() &&
            SecondAtomicWait.Object->GetState() ==
                ERHISemaphoreState::Unsignaled &&
            SecondCommand.Object->GetState() ==
                ERHICommandBufferState::Completed &&
            Queue.Object->GetSubmittedCommandBufferCount() == 1,
        "Vulkan queue wait preflight preserves all state on failure");

    const auto AlreadySignaledOutput = Device.CreateSemaphore();
    const auto AtomicFailureFence = Device.CreateFence(false);
    (void)AlreadySignaledOutput.Object->Signal();
    Record(Result,
        Queue.Object->Submit(
            SecondCommand.Object,
            {},
            {AlreadySignaledOutput.Object},
            AtomicFailureFence.Object) == ERHIResult::InvalidState &&
            AlreadySignaledOutput.Object->IsSignaled() &&
            !AtomicFailureFence.Object->IsSignaled() &&
            SecondCommand.Object->GetState() ==
                ERHICommandBufferState::Completed &&
            Queue.Object->GetSubmittedCommandBufferCount() == 1,
        "Vulkan queue signal preflight preserves all state on failure");

    const auto DuplicateWait = Device.CreateSemaphore();
    (void)DuplicateWait.Object->Signal();
    Record(Result,
        Queue.Object->Submit(
            SecondCommand.Object,
            {DuplicateWait.Object, DuplicateWait.Object}) ==
                ERHIResult::InvalidState &&
            DuplicateWait.Object->IsSignaled() &&
            Queue.Object->Submit(
                SecondCommand.Object,
                {DuplicateWait.Object},
                {DuplicateWait.Object}) == ERHIResult::InvalidState &&
            DuplicateWait.Object->IsSignaled() &&
            SecondCommand.Object->GetState() ==
                ERHICommandBufferState::Completed &&
            Queue.Object->GetSubmittedCommandBufferCount() == 1,
        "Vulkan queue rejects duplicate and overlapping semaphores atomically");

    FVulkanDevice ForeignDevice;
    Record(Result,
        InitializeDeterministic(ForeignDevice) == ERHIResult::Success,
        "Vulkan foreign-owner fixture initializes");
    const auto ForeignQueue =
        ForeignDevice.CreateCommandQueue(ERHIQueueType::Graphics);
    const auto ForeignCommand =
        ForeignDevice.CreateCommandBuffer(ERHIQueueType::Graphics);
    (void)ForeignCommand.Object->Begin();
    (void)ForeignCommand.Object->RecordDispatch(1, 1, 1);
    (void)ForeignCommand.Object->End();
    Record(Result,
        ForeignQueue.Object->Submit(SecondCommand.Object) ==
                ERHIResult::InvalidState &&
            ForeignQueue.Object->Submit(
                ForeignCommand.Object, {DuplicateWait.Object}) ==
                ERHIResult::InvalidState &&
            ForeignCommand.Object->GetState() ==
                ERHICommandBufferState::Completed &&
            DuplicateWait.Object->IsSignaled() &&
            ForeignQueue.Object->GetSubmittedCommandBufferCount() == 0,
        "Vulkan queue rejects foreign command and synchronization ownership");

    auto VulkanQueue = std::dynamic_pointer_cast<FVulkanQueue>(Queue.Object);
    Record(Result,
        Queue.Object->Submit(SecondCommand.Object) == ERHIResult::Success &&
            VulkanQueue &&
            VulkanQueue->ObserveLastSubmissionCompletion(1) ==
                ERHIResult::Success &&
            SecondCommand.Object->GetState() ==
                ERHICommandBufferState::Resettable &&
            SecondCommand.Object->Reset() == ERHIResult::Success,
        "Vulkan nonzero completion timeout makes command resettable");

    const auto ComputeCommand = Device.CreateCommandBuffer(ERHIQueueType::Compute);
    (void)ComputeCommand.Object->Begin();
    (void)ComputeCommand.Object->RecordDispatch(1, 1, 1);
    (void)ComputeCommand.Object->End();
    Record(Result, Queue.Object->Submit(ComputeCommand.Object) == ERHIResult::Unsupported, "Vulkan queue rejects incompatible command buffer submission");

    FVulkanDevice InjectionDevice;
    Record(Result, InitializeDeterministic(InjectionDevice) == ERHIResult::Success, "Vulkan fallback injection fixture initializes");
    InjectionDevice.ConfigureFallbackCompletionInjection({true, false});
    const auto InjectedQueue = InjectionDevice.CreateCommandQueue(ERHIQueueType::Graphics);
    const auto InjectedCommand = InjectionDevice.CreateCommandBuffer(ERHIQueueType::Graphics);
    (void)InjectedCommand.Object->Begin();
    (void)InjectedCommand.Object->RecordDispatch(1, 1, 1);
    (void)InjectedCommand.Object->End();
    (void)InjectedQueue.Object->Submit(InjectedCommand.Object);
    auto InjectedVulkanQueue =
        std::dynamic_pointer_cast<FVulkanQueue>(InjectedQueue.Object);
    Record(Result,
        InjectedVulkanQueue &&
            InjectedVulkanQueue->ObserveLastSubmissionCompletion() ==
                ERHIResult::NotReady &&
            InjectedCommand.Object->GetState() ==
                ERHICommandBufferState::Submitted &&
            InjectedQueue.Object->WaitIdle() == ERHIResult::Success &&
            InjectedCommand.Object->GetState() ==
                ERHICommandBufferState::Resettable &&
            InjectedVulkanQueue->ObserveLastSubmissionCompletion() ==
                ERHIResult::Success,
        "Vulkan fallback not-ready completion remains recoverable by wait idle");
    InjectionDevice.ConfigureFallbackCompletionInjection({false, true});
    const auto TimeoutCommand = InjectionDevice.CreateCommandBuffer(ERHIQueueType::Graphics);
    (void)TimeoutCommand.Object->Begin();
    (void)TimeoutCommand.Object->RecordDispatch(1, 1, 1);
    (void)TimeoutCommand.Object->End();
    (void)InjectedQueue.Object->Submit(TimeoutCommand.Object);
    Record(Result,
        InjectedVulkanQueue &&
            InjectedVulkanQueue->ObserveLastSubmissionCompletion() ==
                ERHIResult::Timeout &&
            TimeoutCommand.Object->GetState() ==
                ERHICommandBufferState::Submitted &&
            InjectedQueue.Object->WaitIdle() == ERHIResult::Success &&
            TimeoutCommand.Object->GetState() ==
                ERHICommandBufferState::Resettable &&
            InjectedVulkanQueue->ObserveLastSubmissionCompletion() ==
                ERHIResult::Success,
        "Vulkan fallback timeout completion remains recoverable by wait idle");

    (void)Device.Shutdown();
    Record(Result, FirstCommand.Object->Begin() == ERHIResult::InvalidState &&
        Queue.Object->WaitIdle() == ERHIResult::InvalidState, "Vulkan command and queue invalidation on shutdown");
    (void)ForeignDevice.Shutdown();
    (void)InjectionDevice.Shutdown();
}

void TestRenderPassFramebufferRecordingAndUploads(FVulkanBackendTestResult& Result)
{
    static_assert(
        !std::is_constructible_v<FVulkanRenderPass,
            FRHIRenderPassDesc> &&
        !std::is_constructible_v<FVulkanFramebuffer,
            FRHIFramebufferDesc>,
        "Vulkan render-pass and framebuffer wrappers must be device-only");

    FVulkanDevice Device;
    Record(Result, InitializeDeterministic(Device) == ERHIResult::Success, "Vulkan render pass command fixture initializes");

    const auto RenderPass = Device.CreateRenderPass(ValidRenderPassDesc());
    const auto Texture = Device.CreateTexture(ValidColorAttachmentTextureDesc());
    const auto Framebuffer = Device.CreateFramebuffer(ValidFramebufferDesc(RenderPass.Object, Texture.Object));
    Record(Result, RenderPass.Succeeded() && RenderPass.Object->GetAttachmentCount() == 1 &&
        Framebuffer.Succeeded() && Framebuffer.Object->GetAttachmentCount() == 1, "Vulkan minimal render pass and framebuffer creation succeeds");

    FRHIRenderPassDesc EmptyPass;
    Record(Result, Device.CreateRenderPass(EmptyPass).Result == ERHIResult::Unsupported, "Vulkan minimal render pass rejects invalid description");
    FRHIRenderPassDesc InvalidStatePass = ValidRenderPassDesc();
    InvalidStatePass.Attachments[0].Role = static_cast<ERHIAttachmentRole>(255);
    InvalidStatePass.Attachments[0].SampleCount = static_cast<ERHISampleCount>(3);
    InvalidStatePass.Attachments[0].LoadOp = static_cast<ERHIAttachmentLoadOp>(255);
    InvalidStatePass.Attachments[0].StoreOp = static_cast<ERHIAttachmentStoreOp>(255);
    Record(Result, Device.CreateRenderPass(InvalidStatePass).Result == ERHIResult::Unsupported,
        "Vulkan minimal render pass rejects undefined attachment state");
    FRHIFramebufferDesc BadFramebuffer = ValidFramebufferDesc(RenderPass.Object, Texture.Object);
    BadFramebuffer.Width = 64;
    Record(Result, Device.CreateFramebuffer(BadFramebuffer).Result == ERHIResult::Unsupported, "Vulkan framebuffer rejects incompatible attachment dimensions");

    FRHITextureDesc ArrayTextureDesc = ValidColorAttachmentTextureDesc();
    ArrayTextureDesc.Dimension = ERHITextureDimension::Texture2DArray;
    ArrayTextureDesc.ArrayLayers = 2;
    ArrayTextureDesc.MipLevels = 2;
    const auto ArrayTexture = Device.CreateTexture(ArrayTextureDesc);
    FRHIFramebufferDesc MipFramebuffer =
        ValidFramebufferDesc(RenderPass.Object, ArrayTexture.Object);
    MipFramebuffer.Attachments[0] = {ArrayTexture.Object, 1, 1};
    MipFramebuffer.Width = 4;
    MipFramebuffer.Height = 4;
    Record(Result, ArrayTexture.Succeeded() &&
            Device.CreateFramebuffer(MipFramebuffer).Succeeded(),
        "Vulkan framebuffer accepts valid nonzero mip and array-layer extent");
    FRHIFramebufferDesc InvalidSubresource = MipFramebuffer;
    InvalidSubresource.Attachments[0].ArrayLayer = 2;
    Record(Result, Device.CreateFramebuffer(InvalidSubresource).Result == ERHIResult::Unsupported,
        "Vulkan framebuffer rejects array layer at the layer count");
    InvalidSubresource = MipFramebuffer;
    InvalidSubresource.Attachments[0].MipLevel = 2;
    Record(Result, Device.CreateFramebuffer(InvalidSubresource).Result == ERHIResult::Unsupported,
        "Vulkan framebuffer rejects mip level at the mip count");

    const auto Command = Device.CreateCommandBuffer(ERHIQueueType::Graphics);
    Record(Result, Command.Succeeded() &&
        Command.Object->Begin() == ERHIResult::Success &&
        Command.Object->BeginRenderPass(RenderPass.Object, Framebuffer.Object) == ERHIResult::Success &&
        Command.Object->BeginRenderPass(RenderPass.Object, Framebuffer.Object) == ERHIResult::InvalidState &&
        Command.Object->RecordDraw(3, 1) == ERHIResult::Success &&
        Command.Object->RecordDrawIndexed(3, 1) == ERHIResult::Success &&
        Command.Object->EndRenderPass() == ERHIResult::Success &&
        Command.Object->EndRenderPass() == ERHIResult::InvalidState &&
        Command.Object->End() == ERHIResult::Success &&
        Command.Object->GetRecordedCommandCount() == 4 &&
        Device.GetDiagnostics().CommandRecordingReason[0] != '\0', "Vulkan graphics render pass scope records draw placeholders and rejects invalid ordering");

    const auto IndexBuffer = Device.CreateBuffer({64, ERHIBufferUsage::Index});
    const auto DescriptorLayout = Device.CreatePipelineLayout(ResourceLayoutDesc());
    const auto DescriptorSet = Device.CreateDescriptorSet(DescriptorLayout.Object, 0);
    const auto UniformBuffer = Device.CreateBuffer(ValidBufferDesc());
    (void)DescriptorSet.Object->UpdateBuffer(0, 0, UniformBuffer.Object);
    const auto DeferredCommand = Device.CreateCommandBuffer(ERHIQueueType::Graphics);
    FRHIRenderPassClearValues ExplicitClears;
    ExplicitClears.Colors = {{0.0f, 0.0f, 0.0f, 0.0f}};
    ExplicitClears.Depth = 0.0f;
    Record(Result, DeferredCommand.Object->Begin() == ERHIResult::Success &&
        DeferredCommand.Object->BeginRenderPass(RenderPass.Object, Framebuffer.Object, ExplicitClears) == ERHIResult::Success &&
        DeferredCommand.Object->BindDescriptorSet(DescriptorSet.Object) == ERHIResult::Success &&
        DeferredCommand.Object->BindIndexBuffer(IndexBuffer.Object, ERHIIndexType::UInt16, 0) == ERHIResult::Success &&
        DeferredCommand.Object->BindIndexBuffer(IndexBuffer.Object, ERHIIndexType::UInt32, 2) == ERHIResult::InvalidState &&
        DeferredCommand.Object->EndRenderPass() == ERHIResult::Success &&
        DeferredCommand.Object->End() == ERHIResult::Success,
        "Vulkan command buffer validates explicit clears descriptor sets and aligned index binding");
    FRHIRenderPassClearValues MissingColorClear;
    const auto InvalidClearCommand = Device.CreateCommandBuffer(ERHIQueueType::Graphics);
    (void)InvalidClearCommand.Object->Begin();
    Record(Result, InvalidClearCommand.Object->BeginRenderPass(RenderPass.Object, Framebuffer.Object, MissingColorClear) == ERHIResult::InvalidState,
        "Vulkan command buffer rejects incompatible explicit render-pass clear values");

    const auto TransferCommand = Device.CreateCommandBuffer(ERHIQueueType::Transfer);
    const auto SourceBuffer = Device.CreateBuffer(ValidCopySourceBufferDesc());
    const auto DestinationBuffer = Device.CreateBuffer(ValidBufferDesc());
    const auto SourceTexture = Device.CreateTexture(ValidColorAttachmentTextureDesc());
    const auto DestinationTexture = Device.CreateTexture(ValidColorAttachmentTextureDesc());
    Record(Result, TransferCommand.Succeeded() && SourceBuffer.Succeeded() && DestinationBuffer.Succeeded() &&
        SourceTexture.Succeeded() && DestinationTexture.Succeeded(), "Vulkan transfer recording fixture creates resources");
    (void)TransferCommand.Object->Begin();
    Record(Result, TransferCommand.Object->RecordBufferCopy(SourceBuffer.Object, DestinationBuffer.Object, {0, 0, 16}) == ERHIResult::Success &&
        TransferCommand.Object->RecordTextureCopy(SourceTexture.Object, DestinationTexture.Object, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 1}) == ERHIResult::Success &&
        TransferCommand.Object->RecordTextureToBufferCopy(SourceTexture.Object, DestinationBuffer.Object,
            {0, 0, 0, 0, 0, 4, 4, 1, 0, 0, 0}) == ERHIResult::Success &&
        TransferCommand.Object->RecordLayoutTransition({nullptr, DestinationTexture.Object, ERHIBufferUsage::None, ERHITextureUsage::CopyDestination, ERHIResourceLayout::Undefined, ERHIResourceLayout::CopyDestination}) == ERHIResult::Success, "Vulkan transfer command records copy and declarative layout intent");
    Record(Result, TransferCommand.Object->RecordDispatch(1, 1, 1) == ERHIResult::Unsupported &&
        TransferCommand.Object->RecordBufferCopy(SourceBuffer.Object, DestinationBuffer.Object, {900, 0, 16}) == ERHIResult::InvalidState &&
        TransferCommand.Object->RecordTextureToBufferCopy(SourceTexture.Object, DestinationBuffer.Object,
            {0, 0, 7, 0, 0, 4, 4, 1, 0, 0, 0}) == ERHIResult::InvalidState,
        "Vulkan transfer command rejects incompatible compute and invalid copy ranges");

    FRHITextureDesc MipTransferDesc = ValidColorAttachmentTextureDesc();
    MipTransferDesc.MipLevels = 2;
    const auto MipSource = Device.CreateTexture(MipTransferDesc);
    const auto MipDestination = Device.CreateTexture(MipTransferDesc);
    FRHITextureCopyRegion OversizedMipCopy;
    OversizedMipCopy.SourceMipLevel = 1;
    OversizedMipCopy.DestinationMipLevel = 1;
    OversizedMipCopy.Width = 8;
    OversizedMipCopy.Height = 8;
    FRHITextureCopyRegion ValidMipCopy = OversizedMipCopy;
    ValidMipCopy.Width = 4;
    ValidMipCopy.Height = 4;
    Record(Result, MipSource.Succeeded() && MipDestination.Succeeded() &&
        TransferCommand.Object->RecordTextureCopy(
            MipSource.Object, MipDestination.Object, OversizedMipCopy) ==
            ERHIResult::InvalidState &&
        TransferCommand.Object->RecordTextureCopy(
            MipSource.Object, MipDestination.Object, ValidMipCopy) ==
            ERHIResult::Success &&
        TransferCommand.Object->RecordTextureToBufferCopy(
            MipSource.Object, DestinationBuffer.Object,
            {1, 0, 0, 0, 0, 8, 8, 1, 0, 0, 0}) ==
            ERHIResult::InvalidState,
        "Vulkan transfer validation uses selected mip extents");

    FRHITextureDesc DifferentFormatDesc = MipTransferDesc;
    DifferentFormatDesc.Format = ERHIFormat::B8G8R8A8_UNorm;
    const auto DifferentFormatDestination =
        Device.CreateTexture(DifferentFormatDesc);
    Record(Result, DifferentFormatDestination.Succeeded() &&
        TransferCommand.Object->RecordTextureCopy(
            MipSource.Object, DifferentFormatDestination.Object,
            ValidMipCopy) == ERHIResult::Unsupported,
        "Vulkan texture copy rejects incompatible formats");

    FRHITextureBufferCopyRegion PaddedReadback;
    PaddedReadback.Width = 2;
    PaddedReadback.Height = 2;
    PaddedReadback.Depth = 2;
    PaddedReadback.DestinationRowLengthTexels = 4;
    PaddedReadback.DestinationImageHeightTexels = 3;
    uint64 PaddedBytes = 0;
    FRHITextureBufferCopyRegion OverflowedReadback;
    OverflowedReadback.Width = 1;
    OverflowedReadback.Height = std::numeric_limits<uint32>::max();
    OverflowedReadback.Depth = std::numeric_limits<uint32>::max();
    OverflowedReadback.DestinationRowLengthTexels =
        std::numeric_limits<uint32>::max();
    OverflowedReadback.DestinationImageHeightTexels =
        std::numeric_limits<uint32>::max();
    uint64 OverflowedBytes = 1;
    Record(Result,
        TryGetRHITextureBufferCopyByteSize(
            PaddedReadback, ERHIFormat::R8G8B8A8_UNorm, PaddedBytes) &&
        PaddedBytes == 72 &&
        !TryGetRHITextureBufferCopyByteSize(
            OverflowedReadback,
            ERHIFormat::R8G8B8A8_UNorm,
            OverflowedBytes) &&
        OverflowedBytes == 0,
        "RHI texture readback footprint is exact and overflow-safe");

    const unsigned char Data[16] = {};
    const unsigned char TextureData[64] = {};
    const auto BufferUpload = Device.StageBufferUpload(DestinationBuffer.Object, Data, sizeof(Data), {0, sizeof(Data)});
    const auto TextureUpload = Device.StageTextureUpload(DestinationTexture.Object, TextureData, sizeof(TextureData), {0, 0, 0, 0, 0, 4, 4, 1});
    auto VulkanTransfer = std::dynamic_pointer_cast<FVulkanCommandBuffer>(TransferCommand.Object);
    Record(Result, VulkanTransfer && BufferUpload.Succeeded() && TextureUpload.Succeeded() &&
        VulkanTransfer->ScheduleBufferUpload(BufferUpload.Object) == ERHIResult::Success &&
        VulkanTransfer->ScheduleTextureUpload(TextureUpload.Object) == ERHIResult::Success &&
        BufferUpload.Object->GetLifecycle() == EVulkanUploadLifecycle::Scheduled &&
        TextureUpload.Object->GetLifecycle() == EVulkanUploadLifecycle::Scheduled &&
        !BufferUpload.Object->ClaimsExecution() &&
        Device.GetDiagnostics().UploadSchedulingReason[0] != '\0', "Vulkan upload scheduling records pending uploads without claiming execution");
    Record(Result, VulkanTransfer && VulkanTransfer->ScheduleBufferUpload(BufferUpload.Object) == ERHIResult::InvalidState, "Vulkan upload scheduling rejects already scheduled uploads");

    (void)Device.Shutdown();
    Record(Result, RenderPass.Object->GetLifecycleState() == ERHIResourceLifecycleState::Invalidated &&
        Framebuffer.Object->GetLifecycleState() == ERHIResourceLifecycleState::Invalidated &&
        BufferUpload.Object->GetLifecycle() == EVulkanUploadLifecycle::Invalidated, "Vulkan shutdown invalidates render pass framebuffer and upload scheduling records");
}

void TestSamplersDescriptorsAndUploads(FVulkanBackendTestResult& Result)
{
    static_assert(
        !std::is_constructible_v<FVulkanDescriptorPool, uint32> &&
        !std::is_constructible_v<FVulkanDescriptorSet,
            const TSharedPtr<IRHIPipelineLayout>&,
            uint32,
            FVulkanDescriptorReservation&&> &&
        !std::is_constructible_v<FVulkanSampler,
            const FRHISamplerDesc&> &&
        !std::is_default_constructible_v<FVulkanUploadRequest> &&
        !std::is_copy_constructible_v<FVulkanDescriptorReservation>,
        "Vulkan descriptor, sampler, and upload invariants must be factory-only");

    Record(Result,
        GetRHIFormatByteSize(ERHIFormat::Unknown) == 0 &&
        GetRHIFormatByteSize(ERHIFormat::R8_UNorm) == 1 &&
        GetRHIFormatByteSize(ERHIFormat::R8G8_UNorm) == 2 &&
        GetRHIFormatByteSize(ERHIFormat::R8G8B8A8_UNorm) == 4 &&
        GetRHIFormatByteSize(ERHIFormat::R8G8B8A8_sRGB) == 4 &&
        GetRHIFormatByteSize(ERHIFormat::B8G8R8A8_UNorm) == 4 &&
        GetRHIFormatByteSize(ERHIFormat::R16G16B16A16_Float) == 8 &&
        GetRHIFormatByteSize(ERHIFormat::R32_Float) == 4 &&
        GetRHIFormatByteSize(ERHIFormat::R32G32_Float) == 8 &&
        GetRHIFormatByteSize(ERHIFormat::R32G32B32_Float) == 12 &&
        GetRHIFormatByteSize(ERHIFormat::R32G32B32A32_Float) == 16 &&
        GetRHIFormatByteSize(ERHIFormat::D24_UNorm_S8_UInt) == 4 &&
        GetRHIFormatByteSize(ERHIFormat::D32_Float) == 4 &&
        GetRHIFormatByteSize(ERHIFormat::S8_UInt) == 1,
        "RHI format byte widths cover every declared format exactly");

    FVulkanDevice Device;
    Record(Result, InitializeDeterministic(Device) == ERHIResult::Success, "Vulkan descriptor fixture device initializes");

    const auto Buffer = Device.CreateBuffer(ValidBufferDesc());
    const auto Texture = Device.CreateTexture(ValidTextureDesc());
    const auto Sampler = Device.CreateSampler(ValidSamplerDesc());
    Record(Result, Buffer.Succeeded() && Texture.Succeeded() && Sampler.Succeeded() &&
        Sampler.Object->GetLifecycleState() == ERHIResourceLifecycleState::Valid, "Vulkan sampler creation succeeds and preserves lifecycle");

    FRHISamplerDesc InvalidSampler = ValidSamplerDesc();
    InvalidSampler.CompareMode = ERHISamplerCompareMode::Less;
    InvalidSampler.MipFilter = ERHISamplerMipFilter::None;
    Record(Result, Device.CreateSampler(InvalidSampler).Result == ERHIResult::Unsupported, "Vulkan unsupported sampler modes are rejected");

    Device.ConfigureDescriptorPoolCapacity(1);
    const auto Layout = Device.CreatePipelineLayout(ResourceLayoutDesc());
    const auto DescriptorSet = Device.CreateDescriptorSet(Layout.Object, 0);
    Record(Result, Layout.Succeeded() && DescriptorSet.Succeeded() &&
        Device.GetDescriptorPoolAllocatedCount() == 1, "Vulkan descriptor set allocates from fixed-capacity pool");
    Record(Result, Device.CreateDescriptorSet(Layout.Object, 0).Result == ERHIResult::Unavailable &&
        Device.GetDiagnostics().DescriptorPoolReason[0] != '\0', "Vulkan descriptor pool exhaustion is explicit");

    Record(Result, DescriptorSet.Object->UpdateBuffer(0, 0, Buffer.Object) == ERHIResult::Success &&
        DescriptorSet.Object->UpdateTexture(1, 0, Texture.Object) == ERHIResult::Success &&
        DescriptorSet.Object->UpdateSampler(2, 0, Sampler.Object) == ERHIResult::Success &&
        DescriptorSet.Object->UpdateCombinedTextureSampler(3, 0, Texture.Object, Sampler.Object) == ERHIResult::Success &&
        DescriptorSet.Object->GetBoundResourceCount() == 4, "Vulkan descriptor updates retain buffer texture sampler and combined records");
    Record(Result, DescriptorSet.Object->UpdateTexture(0, 0, Texture.Object) == ERHIResult::Unsupported &&
        DescriptorSet.Object->UpdateBuffer(99, 0, Buffer.Object) == ERHIResult::InvalidState, "Vulkan descriptor update rejects wrong type and missing binding");

    auto ConcreteSet = std::dynamic_pointer_cast<FVulkanDescriptorSet>(DescriptorSet.Object);
    (void)Texture.Object->Invalidate();
    Record(Result, DescriptorSet.Object->GetBoundResourceKind(1, 0) == ERHIDescriptorResourceKind::Texture &&
        ConcreteSet && !ConcreteSet->IsBoundResourceValid(1, 0) &&
        DescriptorSet.Object->UpdateTexture(1, 0, Texture.Object) == ERHIResult::InvalidState, "Vulkan descriptor retained binding reports invalidated resources");

    Record(Result,
        DescriptorSet.Object->Invalidate() == ERHIResult::Success &&
        Device.GetDescriptorPoolAllocatedCount() == 0,
        "Vulkan descriptor invalidation returns exactly one pool reservation");
    const auto ReplacementSet = Device.CreateDescriptorSet(Layout.Object, 0);
    Record(Result,
        ReplacementSet.Succeeded() &&
        Device.GetDescriptorPoolAllocatedCount() == 1,
        "Vulkan descriptor capacity is reusable after reservation release");

    const unsigned char Data[16] = {};
    const auto BufferUpload = Device.StageBufferUpload(Buffer.Object, Data, sizeof(Data), {0, sizeof(Data)});
    Record(Result, BufferUpload.Succeeded() && BufferUpload.Object->GetLifecycle() == EVulkanUploadLifecycle::Pending &&
        BufferUpload.Object->GetStagingData().size() == sizeof(Data) && !BufferUpload.Object->ClaimsExecution(), "Vulkan buffer upload staging preserves CPU-visible data without execution");
    Record(Result, Device.StageBufferUpload(Buffer.Object, nullptr, sizeof(Data), {0, sizeof(Data)}).Result == ERHIResult::InvalidState &&
        Device.StageBufferUpload(Buffer.Object, Data, sizeof(Data), {512, sizeof(Data)}).Result == ERHIResult::InvalidState &&
        Device.StageBufferUpload(Buffer.Object, Data, sizeof(Data), {0, 8}).Result == ERHIResult::InvalidState,
        "Vulkan buffer upload staging rejects missing, mismatched, and out-of-bounds ranges");
    const auto NonCopyBuffer = Device.CreateBuffer(
        {64, ERHIBufferUsage::Uniform});
    Record(Result,
        NonCopyBuffer.Succeeded() &&
        Device.StageBufferUpload(NonCopyBuffer.Object, Data, sizeof(Data),
            {0, sizeof(Data)}).Result == ERHIResult::Unsupported,
        "Vulkan buffer upload staging rejects destinations without copy usage");

    const auto FreshTexture = Device.CreateTexture(ValidTextureDesc());
    const unsigned char TextureData[64] = {};
    const auto TextureUpload = Device.StageTextureUpload(FreshTexture.Object,
        TextureData, sizeof(TextureData), {0, 0, 0, 0, 0, 4, 4, 1});
    Record(Result, TextureUpload.Succeeded() && TextureUpload.Object->GetKind() == EVulkanUploadKind::Texture &&
        TextureUpload.Object->GetTextureRegion().Width == 4, "Vulkan texture upload staging preserves destination region");

    FRHITextureDesc SynchronousTextureDesc = ValidTextureDesc();
    SynchronousTextureDesc.Width = 4;
    SynchronousTextureDesc.Height = 4;
    const auto SynchronousTexture =
        Device.CreateTexture(SynchronousTextureDesc);
    auto ConcreteSynchronousTexture =
        std::dynamic_pointer_cast<FVulkanTexture>(
            SynchronousTexture.Object);
    const unsigned char PaddedTextureData[80] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 99, 99, 99, 99,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 99, 99, 99, 99,
        33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 99, 99, 99, 99,
        49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 99, 99, 99, 99};
    FRHITextureUploadDesc SynchronousUpload;
    SynchronousUpload.Width = 4;
    SynchronousUpload.Height = 4;
    SynchronousUpload.RowPitchBytes = 20;
    SynchronousUpload.Data = PaddedTextureData;
    SynchronousUpload.DataSizeBytes = sizeof(PaddedTextureData);
    const ERHIResult SynchronousResult =
        Device.UploadTexture(
            SynchronousTexture.Object, SynchronousUpload);
    const auto UploadedBase = ConcreteSynchronousTexture
        ? ConcreteSynchronousTexture->GetUploadedMipData(0)
        : std::span<const unsigned char>{};
    Record(Result,
        SynchronousResult == ERHIResult::Success &&
            ConcreteSynchronousTexture &&
            ConcreteSynchronousTexture->HasUploadedMip(0) &&
            UploadedBase.size() == 64 &&
            UploadedBase.front() == 1 &&
            UploadedBase[16] == 17 &&
            UploadedBase.back() == 64,
        "Vulkan synchronous upload strips row padding and records a sample-ready mip");

    FRHITextureUploadDesc InvalidSynchronousUpload =
        SynchronousUpload;
    InvalidSynchronousUpload.RowPitchBytes = 15;
    Record(Result,
        Device.UploadTexture(
            SynchronousTexture.Object,
            InvalidSynchronousUpload) == ERHIResult::InvalidState,
        "Vulkan synchronous upload rejects an invalid row pitch without replacing the mip");
    Record(Result,
        Device.StageTextureUpload(FreshTexture.Object, TextureData,
            sizeof(TextureData), {0, 0, 7, 0, 0, 4, 4, 1}).Result ==
                ERHIResult::InvalidState &&
        Device.StageTextureUpload(FreshTexture.Object, Data, sizeof(Data),
            {0, 0, 0, 0, 0, 4, 4, 1}).Result ==
                ERHIResult::InvalidState,
        "Vulkan texture upload staging rejects invalid regions and byte footprints");

    FRHITextureDesc MippedDesc = ValidTextureDesc();
    MippedDesc.MipLevels = 2;
    const auto MippedTexture = Device.CreateTexture(MippedDesc);
    const unsigned char OversizedMipData[256] = {};
    Record(Result,
        MippedTexture.Succeeded() &&
        Device.StageTextureUpload(MippedTexture.Object, TextureData,
            sizeof(TextureData), {1, 0, 0, 0, 0, 4, 4, 1}).Succeeded() &&
        Device.StageTextureUpload(MippedTexture.Object, OversizedMipData,
            sizeof(OversizedMipData), {1, 0, 0, 0, 0, 8, 8, 1}).Result ==
                ERHIResult::InvalidState,
        "Vulkan texture upload validation uses the selected mip extent");

    const unsigned char OnePixelMip[4] = {7, 8, 9, 10};
    FRHITextureUploadDesc OnePixelUpload;
    OnePixelUpload.MipLevel = 1;
    OnePixelUpload.Width = 2;
    OnePixelUpload.Height = 2;
    OnePixelUpload.RowPitchBytes = 8;
    OnePixelUpload.Data = OnePixelMip;
    OnePixelUpload.DataSizeBytes = sizeof(OnePixelMip);
    Record(Result,
        Device.UploadTexture(MippedTexture.Object, OnePixelUpload) ==
            ERHIResult::InvalidState,
        "Vulkan synchronous upload rejects insufficient selected-mip data");

    FRHITextureDesc NonCopyTextureDesc = ValidTextureDesc();
    NonCopyTextureDesc.Usage = ERHITextureUsage::Sampled;
    const auto NonCopyTexture = Device.CreateTexture(NonCopyTextureDesc);
    FRHITextureDesc MultisampledDesc = ValidTextureDesc();
    MultisampledDesc.Width = 4;
    MultisampledDesc.Height = 4;
    MultisampledDesc.SampleCount = ERHISampleCount::Four;
    const auto MultisampledTexture = Device.CreateTexture(MultisampledDesc);
    Record(Result,
        NonCopyTexture.Succeeded() && MultisampledTexture.Succeeded() &&
        Device.StageTextureUpload(NonCopyTexture.Object, TextureData,
            sizeof(TextureData), {0, 0, 0, 0, 0, 4, 4, 1}).Result ==
                ERHIResult::Unsupported &&
        Device.StageTextureUpload(MultisampledTexture.Object,
            OversizedMipData, sizeof(OversizedMipData),
            {0, 0, 0, 0, 0, 4, 4, 1}).Result ==
                ERHIResult::Unsupported,
        "Vulkan texture upload staging rejects unsupported transfer paths");

    Record(Result,
        Device.UploadTexture(NonCopyTexture.Object, SynchronousUpload) ==
                ERHIResult::Unsupported &&
            Device.UploadTexture(
                MultisampledTexture.Object,
                SynchronousUpload) == ERHIResult::Unsupported,
        "Vulkan synchronous upload rejects missing copy usage and multisampling");

    FRHIBufferDesc HostVisibleBufferDesc{64,
        ERHIBufferUsage::Vertex | ERHIBufferUsage::CopyDestination,
        ERHIMemoryAccess::HostVisible};
    FRHIBufferDesc DeviceLocalBufferDesc = HostVisibleBufferDesc;
    DeviceLocalBufferDesc.MemoryAccess = ERHIMemoryAccess::DeviceLocal;
    const auto HostVisibleBuffer = Device.CreateBuffer(HostVisibleBufferDesc);
    const auto DeviceLocalBuffer = Device.CreateBuffer(DeviceLocalBufferDesc);
    const unsigned char BufferBytes[4] = {9, 8, 7, 6};
    const auto ConcreteHostBuffer =
        std::dynamic_pointer_cast<FVulkanBuffer>(HostVisibleBuffer.Object);
    const Stoner::Core::uint32 UploadCountBefore =
        Device.GetTrackedUploadRequestCount();
    Record(Result,
        HostVisibleBuffer.Succeeded() && DeviceLocalBuffer.Succeeded() &&
            Device.UploadBuffer(HostVisibleBuffer.Object,
                {4, BufferBytes, sizeof(BufferBytes)}) ==
                ERHIResult::Success &&
            ConcreteHostBuffer &&
            ConcreteHostBuffer->GetUploadedBytes().size() == 8 &&
            ConcreteHostBuffer->GetUploadedBytes()[4] == 9 &&
            Device.UploadBuffer(DeviceLocalBuffer.Object,
                {0, BufferBytes, sizeof(BufferBytes)}) ==
                ERHIResult::Success &&
            Device.GetTrackedUploadRequestCount() == UploadCountBefore + 1,
        "Vulkan buffer upload uses direct host writes and tracked device-local staging");
    const auto DeviceLocalNonCopyBuffer = Device.CreateBuffer({64,
        ERHIBufferUsage::Vertex, ERHIMemoryAccess::DeviceLocal});
    (void)DeviceLocalBuffer.Object->Invalidate();
    Record(Result,
        DeviceLocalNonCopyBuffer.Succeeded() &&
            Device.UploadBuffer(DeviceLocalNonCopyBuffer.Object,
                {0, BufferBytes, sizeof(BufferBytes)}) ==
                ERHIResult::Unsupported &&
            Device.UploadBuffer(DeviceLocalBuffer.Object,
                {0, BufferBytes, sizeof(BufferBytes)}) ==
                ERHIResult::InvalidState,
        "Vulkan buffer upload rejects missing transfer usage and invalidated buffers");

    (void)Device.Shutdown();
    Record(Result, DescriptorSet.Object->UpdateBuffer(0, 0, Buffer.Object) == ERHIResult::InvalidState &&
        BufferUpload.Object->GetLifecycle() == EVulkanUploadLifecycle::Invalidated &&
        ReplacementSet.Object->GetLifecycleState() == ERHIResourceLifecycleState::Invalidated &&
        Device.CreateSampler(ValidSamplerDesc()).Result == ERHIResult::InvalidState, "Vulkan shutdown invalidates descriptors uploads and sampler creation");
}

void TestCompressedTextureFormatsAndUploads(
    FVulkanBackendTestResult& Result)
{
    FVulkanDevice Device;
    Record(
        Result,
        InitializeDeterministic(Device) == ERHIResult::Success,
        "Vulkan compressed texture fixture initializes");

    constexpr auto Required =
        ERHIFormatCapability::SampledImage |
        ERHIFormatCapability::CopySource |
        ERHIFormatCapability::CopyDestination;
    Record(
        Result,
        Device.GetCapabilities().SupportsFormatUsage(
            ERHIFormat::BC7_RGBA_UNorm, Required) &&
            Device.GetCapabilities().SupportsFormatUsage(
                ERHIFormat::ETC2_RGBA8_UNorm, Required) &&
            Device.GetCapabilities().SupportsFormatUsage(
                ERHIFormat::ASTC_4x4_RGBA_UNorm, Required) &&
            !Device.GetCapabilities().SupportsFormatUsage(
                ERHIFormat::BC7_RGBA_UNorm,
                ERHIFormatCapability::ColorAttachment),
        "Vulkan synthetic capabilities expose compressed sampled transfer usage only");

    FRHITextureDesc Desc;
    Desc.Width = 7;
    Desc.Height = 5;
    Desc.MipLevels = 3;
    Desc.Format = ERHIFormat::BC7_RGBA_UNorm;
    Desc.Usage =
        ERHITextureUsage::Sampled |
        ERHITextureUsage::CopySource |
        ERHITextureUsage::CopyDestination;
    const auto Texture = Device.CreateTexture(Desc);
    const auto AfterCreate = Device.GetAllocationSnapshot();
    Record(
        Result,
        Texture.Succeeded() &&
            AfterCreate.LiveAllocationCount == 1 &&
            AfterCreate.AllocatedBytes == 96,
        "Vulkan compressed allocation sums rounded blocks and terminal mips");

    std::array<unsigned char, 80> PaddedBase{};
    for (std::size_t Index = 0; Index < PaddedBase.size(); ++Index)
    {
        PaddedBase[Index] =
            static_cast<unsigned char>(Index + 1);
    }
    FRHITextureUploadDesc BaseUpload;
    BaseUpload.Width = 7;
    BaseUpload.Height = 5;
    BaseUpload.RowPitchBytes = 48;
    BaseUpload.Data = PaddedBase.data();
    BaseUpload.DataSizeBytes = PaddedBase.size();
    const ERHIResult BaseResult =
        Device.UploadTexture(Texture.Object, BaseUpload);
    Stoner::Core::TArray<Stoner::Core::uint8> BaseReadback;
    const ERHIResult ReadbackResult =
        Device.ReadbackTextureForTesting(
            Texture.Object, 0, BaseReadback);
    bool bBaseMatches =
        BaseReadback.size() == 64;
    if (bBaseMatches)
    {
        bBaseMatches =
            std::equal(
                BaseReadback.begin(),
                BaseReadback.begin() + 32,
                PaddedBase.begin()) &&
            std::equal(
                BaseReadback.begin() + 32,
                BaseReadback.end(),
                PaddedBase.begin() + 48);
    }
    Record(
        Result,
        BaseResult == ERHIResult::Success &&
            ReadbackResult == ERHIResult::Success &&
            bBaseMatches,
        "Vulkan compressed upload strips block-row padding to the exact footprint");

    const std::array<unsigned char, 16> TerminalBlock = {
        0, 1, 2, 3, 4, 5, 6, 7,
        8, 9, 10, 11, 12, 13, 14, 15};
    FRHITextureUploadDesc TerminalUpload;
    TerminalUpload.MipLevel = 2;
    TerminalUpload.Width = 1;
    TerminalUpload.Height = 1;
    TerminalUpload.RowPitchBytes = TerminalBlock.size();
    TerminalUpload.Data = TerminalBlock.data();
    TerminalUpload.DataSizeBytes = TerminalBlock.size();
    Record(
        Result,
        Device.UploadTexture(
            Texture.Object, TerminalUpload) ==
                ERHIResult::Success,
        "Vulkan compressed terminal mip uploads one complete block");

    FRHITextureUploadDesc InvalidUpload = TerminalUpload;
    InvalidUpload.DataSizeBytes = TerminalBlock.size() - 1;
    Record(
        Result,
        Device.UploadTexture(Texture.Object, InvalidUpload) ==
                ERHIResult::InvalidState &&
            Device.GetAllocationSnapshot().LiveAllocationCount == 1,
        "Vulkan compressed upload failure preserves the prior resource");

    FVulkanDevice LimitedDevice;
    const bool bLimitedInitialized =
        InitializeDeterministic(LimitedDevice) ==
        ERHIResult::Success;
    LimitedDevice.ConfigureAllocationBudget(95);
    const auto Rejected = LimitedDevice.CreateTexture(Desc);
    const auto RejectedSnapshot =
        LimitedDevice.GetAllocationSnapshot();
    Record(
        Result,
        bLimitedInitialized &&
            Rejected.Result == ERHIResult::Unavailable &&
            RejectedSnapshot.LiveAllocationCount == 0 &&
            RejectedSnapshot.AllocatedBytes == 0,
        "Vulkan compressed allocation failure publishes no resource or accounting");

    (void)Device.Shutdown();
    (void)LimitedDevice.Shutdown();
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
    TestLifecycleAndFactoryState(Result);
    TestShaderPipelineAndBinding(Result);
    TestDrawDispatchPipelineDiagnostics(Result);
    TestPipelineCacheKeyAndStateValidation(Result);
    TestPipelineFormatCapabilities(Result);
    TestResourceCreationAndAllocation(Result);
    TestAllocationOwnershipAndFootprints(Result);
    TestCommandBuffersRecordingAndSubmission(Result);
    TestRenderPassFramebufferRecordingAndUploads(Result);
    TestSamplersDescriptorsAndUploads(Result);
    TestCompressedTextureFormatsAndUploads(Result);
    return Result;
}
