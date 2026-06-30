#include "VulkanBackendTests.h"

#include "VulkanRHI/VulkanDevice.h"

#include <iostream>
#include <memory>
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
                DeviceResult.Object->CreateShaderModule({}).Result == ERHIResult::Unsupported &&
                DeviceResult.Object->CreateGraphicsPipeline({}).Result == ERHIResult::Unsupported, "Vulkan out-of-scope factories return unsupported");
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
    };
    return Desc;
}

void TestResourceCreationAndAllocation(FVulkanBackendTestResult& Result)
{
    FVulkanDevice Device;
    Record(Result, Device.Initialize() == ERHIResult::Success, "Vulkan resource fixture device initializes");

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
    Record(Result, LimitedDevice.Initialize() == ERHIResult::Success, "Vulkan allocation-limit fixture initializes");
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

void TestSamplersDescriptorsAndUploads(FVulkanBackendTestResult& Result)
{
    FVulkanDevice Device;
    Record(Result, Device.Initialize() == ERHIResult::Success, "Vulkan descriptor fixture device initializes");

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

    const unsigned char Data[16] = {};
    const auto BufferUpload = Device.StageBufferUpload(Buffer.Object, Data, sizeof(Data), {0, sizeof(Data)});
    Record(Result, BufferUpload.Succeeded() && BufferUpload.Object->GetLifecycle() == EVulkanUploadLifecycle::Pending &&
        BufferUpload.Object->GetStagingData().size() == sizeof(Data) && !BufferUpload.Object->ClaimsExecution(), "Vulkan buffer upload staging preserves CPU-visible data without execution");
    Record(Result, Device.StageBufferUpload(Buffer.Object, nullptr, sizeof(Data), {0, sizeof(Data)}).Result == ERHIResult::InvalidState &&
        Device.StageBufferUpload(Buffer.Object, Data, sizeof(Data), {512, sizeof(Data)}).Result == ERHIResult::InvalidState, "Vulkan buffer upload staging rejects missing data and out-of-bounds ranges");

    const auto FreshTexture = Device.CreateTexture(ValidTextureDesc());
    const auto TextureUpload = Device.StageTextureUpload(FreshTexture.Object, Data, sizeof(Data), {0, 0, 0, 0, 0, 4, 4, 1});
    Record(Result, TextureUpload.Succeeded() && TextureUpload.Object->GetKind() == EVulkanUploadKind::Texture &&
        TextureUpload.Object->GetTextureRegion().Width == 4, "Vulkan texture upload staging preserves destination region");
    Record(Result, Device.StageTextureUpload(FreshTexture.Object, Data, sizeof(Data), {0, 0, 7, 0, 0, 4, 4, 1}).Result == ERHIResult::InvalidState, "Vulkan texture upload staging rejects invalid regions");

    (void)Device.Shutdown();
    Record(Result, DescriptorSet.Object->UpdateBuffer(0, 0, Buffer.Object) == ERHIResult::InvalidState &&
        BufferUpload.Object->GetLifecycle() == EVulkanUploadLifecycle::Invalidated &&
        Device.CreateSampler(ValidSamplerDesc()).Result == ERHIResult::InvalidState, "Vulkan shutdown invalidates descriptors uploads and sampler creation");
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
    TestResourceCreationAndAllocation(Result);
    TestSamplersDescriptorsAndUploads(Result);
    return Result;
}
