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
    ERHIResult RecordDrawIndexed(uint32, uint32 = 1) override { return ERHIResult::Unsupported; }
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
    Record(Result, GraphicsQueue.Object && GraphicsQueue.Object->Submit(CompletedCommand) == ERHIResult::InvalidState, "Vulkan queue rejects foreign executable command buffer");

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

void TestLifecycleAndFactoryState(FVulkanBackendTestResult& Result)
{
    for (int Index = 0; Index < 3; ++Index)
    {
        const auto DeviceResult = CreateVulkanDevice();
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
    Desc.PayloadIdentity = Payload;
    Desc.Bytecode.Words = {0x07230203u, 0u, 1u, static_cast<uint32>(Stage)};
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
    FVulkanDevice Device;
    Record(Result, Device.Initialize() == ERHIResult::Success, "Vulkan shader pipeline fixture device initializes");

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
    BadBytecode.Bytecode.Words = {1u, 2u, 3u};
    FRHIShaderModuleDesc BadMetadata = ShaderDesc(ERHIShaderStage::Vertex, "MainVS", "bad_meta");
    BadMetadata.InterfaceMetadata.Bindings[0].Visibility = ERHIShaderStageFlags::Fragment;
    Record(Result, Device.CreateShaderModule(BadBytecode).Result == ERHIResult::InvalidState &&
        Device.CreateShaderModule(BadMetadata).Result == ERHIResult::InvalidState &&
        Device.CreateShaderModule(ShaderDesc(ERHIShaderStage::Mesh, "MainMS", "mesh")).Result == ERHIResult::Unsupported, "Vulkan shader module rejects malformed unsupported and metadata-incompatible inputs");

    const auto GraphicsPipeline = Device.CreateGraphicsPipeline(GraphicsPipelineDesc(Vertex.Object, Fragment.Object, Layout.Object));
    const auto GraphicsPipelineAgain = Device.CreateGraphicsPipeline(GraphicsPipelineDesc(Vertex.Object, Fragment.Object, Layout.Object));
    const auto ComputePipeline = Device.CreateComputePipeline(ComputePipelineDesc(Compute.Object, Layout.Object));
    const auto ComputePipelineAgain = Device.CreateComputePipeline(ComputePipelineDesc(Compute.Object, Layout.Object));
    auto VulkanGraphics = std::dynamic_pointer_cast<FVulkanGraphicsPipeline>(GraphicsPipeline.Object);
    auto VulkanCompute = std::dynamic_pointer_cast<FVulkanComputePipeline>(ComputePipeline.Object);
    Record(Result, GraphicsPipeline.Succeeded() && ComputePipeline.Succeeded() &&
        VulkanGraphics && VulkanGraphics->GetRuntimeMode() == ERHIRuntimeObjectMode::DeterministicFallback &&
        VulkanCompute && VulkanCompute->GetRuntimeMode() == ERHIRuntimeObjectMode::DeterministicFallback, "Vulkan graphics and compute pipelines create deterministic fallback objects");
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
        GraphicsCommand.Object->BindGraphicsPipeline(GraphicsPipeline.Object) == ERHIResult::Success &&
        GraphicsCommand.Object->RecordDraw(3, 1) == ERHIResult::Success &&
        GraphicsCommand.Object->RecordDrawIndexed(3, 1) == ERHIResult::Success &&
        GraphicsCommand.Object->EndRenderPass() == ERHIResult::Success &&
        GraphicsCommand.Object->End() == ERHIResult::Success &&
        Device.GetDiagnostics().PipelineBindingReason[0] != '\0', "Vulkan command buffer binds graphics pipeline and records draw diagnostics");

    const auto ComputeCommand = Device.CreateCommandBuffer(ERHIQueueType::Compute);
    const auto TransferCommand = Device.CreateCommandBuffer(ERHIQueueType::Transfer);
    Record(Result, ComputeCommand.Object->Begin() == ERHIResult::Success &&
        ComputeCommand.Object->BindComputePipeline(ComputePipeline.Object) == ERHIResult::Success &&
        ComputeCommand.Object->RecordDispatch(1, 1, 1) == ERHIResult::Success &&
        TransferCommand.Object->Begin() == ERHIResult::Success &&
        TransferCommand.Object->BindComputePipeline(ComputePipeline.Object) == ERHIResult::Unsupported, "Vulkan command buffer binds compute pipeline and rejects transfer queue binding");

    FVulkanDevice LimitedDevice;
    Record(Result, LimitedDevice.Initialize() == ERHIResult::Success, "Vulkan pipeline limit fixture initializes");
    LimitedDevice.ConfigurePipelineCreationLimit(1);
    const auto LimitedLayout = LimitedDevice.CreatePipelineLayout(ResourceLayoutDesc());
    const auto LimitedVS = LimitedDevice.CreateShaderModule(ShaderDesc(ERHIShaderStage::Vertex, "MainVS", "limit_vs"));
    const auto LimitedPS = LimitedDevice.CreateShaderModule(ShaderDesc(ERHIShaderStage::Fragment, "MainPS", "limit_ps"));
    const auto LimitedCS = LimitedDevice.CreateShaderModule(ShaderDesc(ERHIShaderStage::Compute, "MainCS", "limit_cs"));
    Record(Result, LimitedDevice.CreateGraphicsPipeline(GraphicsPipelineDesc(LimitedVS.Object, LimitedPS.Object, LimitedLayout.Object)).Succeeded() &&
        LimitedDevice.CreateComputePipeline(ComputePipelineDesc(LimitedCS.Object, LimitedLayout.Object)).Result == ERHIResult::Unavailable, "Vulkan configured pipeline creation limit is deterministic");

    (void)Device.Shutdown();
    Record(Result, Vertex.Object->GetLifecycleState() == ERHIResourceLifecycleState::Invalidated &&
        GraphicsPipeline.Object->GetLifecycleState() == ERHIResourceLifecycleState::Invalidated &&
        ComputePipeline.Object->GetLifecycleState() == ERHIResourceLifecycleState::Invalidated &&
        Device.CreateShaderModule(ShaderDesc(ERHIShaderStage::Vertex, "MainVS", "after")).Result == ERHIResult::InvalidState, "Vulkan shutdown invalidates shader and pipeline objects");
    (void)LimitedDevice.Shutdown();
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

void TestCommandBuffersRecordingAndSubmission(FVulkanBackendTestResult& Result)
{
    FVulkanDevice Device;
    Record(Result, Device.Initialize() == ERHIResult::Success, "Vulkan command fixture device initializes");

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

    const auto ComputeCommand = Device.CreateCommandBuffer(ERHIQueueType::Compute);
    (void)ComputeCommand.Object->Begin();
    (void)ComputeCommand.Object->RecordDispatch(1, 1, 1);
    (void)ComputeCommand.Object->End();
    Record(Result, Queue.Object->Submit(ComputeCommand.Object) == ERHIResult::Unsupported, "Vulkan queue rejects incompatible command buffer submission");

    FVulkanDevice InjectionDevice;
    Record(Result, InjectionDevice.Initialize() == ERHIResult::Success, "Vulkan fallback injection fixture initializes");
    InjectionDevice.ConfigureFallbackCompletionInjection({true, false});
    const auto InjectedQueue = InjectionDevice.CreateCommandQueue(ERHIQueueType::Graphics);
    const auto InjectedCommand = InjectionDevice.CreateCommandBuffer(ERHIQueueType::Graphics);
    (void)InjectedCommand.Object->Begin();
    (void)InjectedCommand.Object->RecordDispatch(1, 1, 1);
    (void)InjectedCommand.Object->End();
    (void)InjectedQueue.Object->Submit(InjectedCommand.Object);
    auto VulkanQueue = std::dynamic_pointer_cast<FVulkanQueue>(InjectedQueue.Object);
    Record(Result, VulkanQueue && VulkanQueue->ObserveLastSubmissionCompletion() == ERHIResult::NotReady &&
        InjectedCommand.Object->GetState() == ERHICommandBufferState::Submitted, "Vulkan fallback completion can inject not-ready");
    InjectionDevice.ConfigureFallbackCompletionInjection({false, true});
    const auto TimeoutCommand = InjectionDevice.CreateCommandBuffer(ERHIQueueType::Graphics);
    (void)TimeoutCommand.Object->Begin();
    (void)TimeoutCommand.Object->RecordDispatch(1, 1, 1);
    (void)TimeoutCommand.Object->End();
    (void)InjectedQueue.Object->Submit(TimeoutCommand.Object);
    Record(Result, VulkanQueue && VulkanQueue->ObserveLastSubmissionCompletion() == ERHIResult::Timeout, "Vulkan fallback completion can inject timeout");

    (void)Device.Shutdown();
    Record(Result, FirstCommand.Object->Begin() == ERHIResult::InvalidState &&
        Queue.Object->WaitIdle() == ERHIResult::InvalidState, "Vulkan command and queue invalidation on shutdown");
    (void)InjectionDevice.Shutdown();
}

void TestRenderPassFramebufferRecordingAndUploads(FVulkanBackendTestResult& Result)
{
    FVulkanDevice Device;
    Record(Result, Device.Initialize() == ERHIResult::Success, "Vulkan render pass command fixture initializes");

    const auto RenderPass = Device.CreateRenderPass(ValidRenderPassDesc());
    const auto Texture = Device.CreateTexture(ValidColorAttachmentTextureDesc());
    const auto Framebuffer = Device.CreateFramebuffer(ValidFramebufferDesc(RenderPass.Object, Texture.Object));
    Record(Result, RenderPass.Succeeded() && RenderPass.Object->GetAttachmentCount() == 1 &&
        Framebuffer.Succeeded() && Framebuffer.Object->GetAttachmentCount() == 1, "Vulkan minimal render pass and framebuffer creation succeeds");

    FRHIRenderPassDesc EmptyPass;
    Record(Result, Device.CreateRenderPass(EmptyPass).Result == ERHIResult::Unsupported, "Vulkan minimal render pass rejects invalid description");
    FRHIFramebufferDesc BadFramebuffer = ValidFramebufferDesc(RenderPass.Object, Texture.Object);
    BadFramebuffer.Width = 64;
    Record(Result, Device.CreateFramebuffer(BadFramebuffer).Result == ERHIResult::Unsupported, "Vulkan framebuffer rejects incompatible attachment dimensions");

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
        TransferCommand.Object->RecordLayoutTransition({nullptr, DestinationTexture.Object, ERHIBufferUsage::None, ERHITextureUsage::CopyDestination, ERHIResourceLayout::Undefined, ERHIResourceLayout::CopyDestination}) == ERHIResult::Success, "Vulkan transfer command records copy and declarative layout intent");
    Record(Result, TransferCommand.Object->RecordDispatch(1, 1, 1) == ERHIResult::Unsupported &&
        TransferCommand.Object->RecordBufferCopy(SourceBuffer.Object, DestinationBuffer.Object, {900, 0, 16}) == ERHIResult::InvalidState, "Vulkan transfer command rejects incompatible compute and invalid ranges");

    const unsigned char Data[16] = {};
    const auto BufferUpload = Device.StageBufferUpload(DestinationBuffer.Object, Data, sizeof(Data), {0, sizeof(Data)});
    const auto TextureUpload = Device.StageTextureUpload(DestinationTexture.Object, Data, sizeof(Data), {0, 0, 0, 0, 0, 4, 4, 1});
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
    TestLifecycleAndFactoryState(Result);
    TestShaderPipelineAndBinding(Result);
    TestResourceCreationAndAllocation(Result);
    TestCommandBuffersRecordingAndSubmission(Result);
    TestRenderPassFramebufferRecordingAndUploads(Result);
    TestSamplersDescriptorsAndUploads(Result);
    return Result;
}
