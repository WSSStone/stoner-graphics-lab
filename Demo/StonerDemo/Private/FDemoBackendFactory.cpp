#include "FDemoBackendFactory.h"

#include "Core/SGPlatform.h"
#include "RHI/RHIMinimal.h"
#include "VulkanRHI/FVulkanNativeContext.h"

#if SG_PLATFORM_MAC
#include "MetalRHI/FMetalDeviceFactory.h"
#endif

#include <array>
#include <utility>

namespace Stoner::Demo
{
namespace
{

class FVulkanDemoBackendRuntime final : public IDemoBackendRuntime
{
public:
    EDemoGraphicsBackend GetBackend() const noexcept override
    {
        return EDemoGraphicsBackend::Vulkan;
    }

    RHI::ERHIResult Initialize(
        EDemoRunMode Mode,
        const Core::FPlatformWindow& Window,
        Core::uint32,
        bool) override
    {
        Mode_ = Mode;
        const RHI::ERHIRuntimeMode RuntimeMode =
            Mode == EDemoRunMode::NativeHeadless
                ? RHI::ERHIRuntimeMode::NativeHeadless
                : RHI::ERHIRuntimeMode::Native;
        return Context_.Initialize(RuntimeMode, Window);
    }

    RHI::ERHIResult PrepareTriangle(
        const RHI::FRHIShaderModuleDesc& VertexShader,
        const RHI::FRHIShaderModuleDesc& FragmentShader,
        Core::uint32 Width,
        Core::uint32 Height) override
    {
        return Context_.PrepareVisibleTriangle(
            VertexShader, FragmentShader, Width, Height);
    }

    RHI::ERHIResult AcquireFrame(FDemoBackendFrame& OutFrame) override
    {
        Backend::Vulkan::FVulkanNativeFrameBindings Native;
        const RHI::ERHIResult Result = Context_.AcquireVisibleFrame(Native);
        if (Result != RHI::ERHIResult::Success) return Result;
        OutFrame = {};
        OutFrame.FrameIndex = Native.ImageIndex;
        OutFrame.ExecutionBindings.OutputTexture = Native.OutputTexture;
        OutFrame.ExecutionBindings.VertexBuffer = Native.VertexBuffer;
        OutFrame.ExecutionBindings.GraphicsPipeline = Native.GraphicsPipeline;
        OutFrame.ExecutionBindings.RenderPass = Native.RenderPass;
        OutFrame.ExecutionBindings.Framebuffer = Native.Framebuffer;
        OutFrame.ExecutionBindings.CommandBuffer = Native.CommandBuffer;
        LastNativeFrame_ = std::move(Native);
        return RHI::ERHIResult::Success;
    }

    RHI::ERHIResult SubmitFrame(const FDemoBackendFrame&) override
    {
        const RHI::ERHIResult Result =
            Context_.SubmitAndPresentVisibleFrame(LastNativeFrame_);
        LastNativeFrame_ = {};
        return Result;
    }

    RHI::ERHIResult RecreatePresentation(
        Core::uint32 Width,
        Core::uint32 Height) override
    {
        return Context_.RecreateVisiblePresentation(Width, Height);
    }

    RHI::ERHIResult ExecuteOffscreenTriangle(
        const RHI::FRHIShaderModuleDesc& VertexShader,
        const RHI::FRHIShaderModuleDesc& FragmentShader) override
    {
        return Context_.ExecuteOffscreenTriangle(VertexShader, FragmentShader);
    }

    RHI::FRHIRuntimeSnapshot GetSnapshot() const noexcept override
    {
        return Context_.GetSnapshot();
    }

    Core::TSharedPtr<RHI::IRHIDevice> GetDevice() const noexcept override
    {
        return nullptr;
    }

    RHI::ERHIResult Shutdown() override
    {
        LastNativeFrame_ = {};
        return Context_.Shutdown();
    }

private:
    EDemoRunMode Mode_ = EDemoRunMode::InteractiveNative;
    Backend::Vulkan::FVulkanNativeContext Context_;
    Backend::Vulkan::FVulkanNativeFrameBindings LastNativeFrame_;
};

#if SG_PLATFORM_MAC
class FMetalDemoBackendRuntime final : public IDemoBackendRuntime
{
public:
    EDemoGraphicsBackend GetBackend() const noexcept override
    {
        return EDemoGraphicsBackend::Metal;
    }

    RHI::ERHIResult Initialize(
        EDemoRunMode Mode,
        const Core::FPlatformWindow& Window,
        Core::uint32 FramesInFlight,
        bool bEnableValidation) override
    {
        Backend::Metal::FMetalBackendConfig Config;
        Config.bRequirePresentation = Window.IsValid();
        Config.bEnableValidation = bEnableValidation;
        auto Created = Backend::Metal::CreateMetalDevice(Config);
        if (!Created.Succeeded()) return Created.Result;
        Device_ = std::move(Created.Device);
        Mode_ = Mode;
        FramesInFlight_ = FramesInFlight;
        if (!Window.IsValid()) return RHI::ERHIResult::Success;

        RHI::FRHIPresentationSurfaceDesc SurfaceDesc;
        SurfaceDesc.Window = Window;
        auto Surface = Device_->CreatePresentationSurface(SurfaceDesc);
        if (!Surface.Succeeded()) return Surface.Result;
        Surface_ = std::move(Surface.Object);
        RHI::FRHISwapchainDesc SwapchainDesc;
        SwapchainDesc.Width = 1;
        SwapchainDesc.Height = 1;
        SwapchainDesc.FramesInFlight = FramesInFlight;
        auto Swapchain = Device_->CreateSwapchain(Surface_, SwapchainDesc);
        if (!Swapchain.Succeeded()) return Swapchain.Result;
        Swapchain_ = std::move(Swapchain.Object);
        auto Queue = Device_->CreateCommandQueue(RHI::ERHIQueueType::Graphics);
        if (!Queue.Succeeded()) return Queue.Result;
        Queue_ = std::move(Queue.Object);
        return RHI::ERHIResult::Success;
    }

    RHI::ERHIResult PrepareTriangle(
        const RHI::FRHIShaderModuleDesc& VertexShader,
        const RHI::FRHIShaderModuleDesc& FragmentShader,
        Core::uint32 Width,
        Core::uint32 Height) override
    {
        if (!Device_ || Width == 0 || Height == 0 ||
            VertexShader.Stage != RHI::ERHIShaderStage::Vertex ||
            FragmentShader.Stage != RHI::ERHIShaderStage::Fragment)
            return RHI::ERHIResult::InvalidState;
        if (VertexShader.Payload.Format !=
                RHI::ERHIShaderPayloadFormat::MetalLibrary ||
            FragmentShader.Payload.Format !=
                RHI::ERHIShaderPayloadFormat::MetalLibrary)
            return RHI::ERHIResult::InvalidState;

        ResetTriangleResources();
        constexpr std::array<float, 15> Vertices = {
             0.0f, -0.6f, 1.0f, 0.0f, 0.0f,
             0.6f,  0.6f, 0.0f, 1.0f, 0.0f,
            -0.6f,  0.6f, 0.0f, 0.0f, 1.0f,
        };
        RHI::FRHIBufferDesc VertexBufferDesc;
        VertexBufferDesc.SizeInBytes = sizeof(Vertices);
        VertexBufferDesc.Usage = RHI::ERHIBufferUsage::Vertex |
            RHI::ERHIBufferUsage::CopyDestination;
        VertexBufferDesc.MemoryAccess = RHI::ERHIMemoryAccess::HostVisible;
        auto VertexBuffer = Device_->CreateBuffer(VertexBufferDesc);
        if (!VertexBuffer.Succeeded()) return VertexBuffer.Result;
        const RHI::FRHIBufferUploadDesc Upload{
            0, Vertices.data(), sizeof(Vertices)};
        if (Device_->UploadBuffer(VertexBuffer.Object, Upload) !=
            RHI::ERHIResult::Success)
            return RHI::ERHIResult::Failed;

        auto VertexModule = Device_->CreateShaderModule(VertexShader);
        auto FragmentModule = Device_->CreateShaderModule(FragmentShader);
        if (!VertexModule.Succeeded()) return VertexModule.Result;
        if (!FragmentModule.Succeeded()) return FragmentModule.Result;

        // The current RHI layout contract requires one declared set even for
        // resource-free shaders. This inert entry is never bound or exposed.
        RHI::FRHIPipelineLayoutDesc LayoutDesc;
        LayoutDesc.Bindings.push_back({
            0, 0, RHI::ERHIDescriptorType::UniformBuffer, 1,
            RHI::ERHIShaderStageFlags::Vertex |
                RHI::ERHIShaderStageFlags::Fragment});
        auto Layout = Device_->CreatePipelineLayout(LayoutDesc);
        if (!Layout.Succeeded()) return Layout.Result;

        RHI::FRHIRenderPassDesc RenderPassDesc;
        RenderPassDesc.Attachments.push_back({
            RHI::ERHIAttachmentRole::Color,
            RHI::ERHIFormat::B8G8R8A8_UNorm,
            RHI::ERHISampleCount::One,
            RHI::ERHIAttachmentLoadOp::Clear,
            RHI::ERHIAttachmentStoreOp::Store});
        auto RenderPass = Device_->CreateRenderPass(RenderPassDesc);
        if (!RenderPass.Succeeded()) return RenderPass.Result;

        RHI::FRHIGraphicsPipelineDesc PipelineDesc;
        PipelineDesc.ShaderModules = {
            VertexModule.Object, FragmentModule.Object};
        PipelineDesc.PipelineLayout = Layout.Object;
        PipelineDesc.VertexInput.Stride = sizeof(float) * 5;
        PipelineDesc.VertexInput.Attributes = {
            {0, RHI::ERHIFormat::R32G32_Float, 0},
            {1, RHI::ERHIFormat::R32G32B32_Float, sizeof(float) * 2}};
        PipelineDesc.Rasterizer.CullMode = RHI::ERHICullMode::None;
        PipelineDesc.RenderTargets.ColorFormats = {
            RHI::ERHIFormat::B8G8R8A8_UNorm};
        PipelineDesc.RuntimeMode = RHI::ERHIRuntimeObjectMode::RealRuntime;
        auto Pipeline = Device_->CreateGraphicsPipeline(PipelineDesc);
        if (!Pipeline.Succeeded()) return Pipeline.Result;

        VertexBuffer_ = std::move(VertexBuffer.Object);
        VertexShader_ = std::move(VertexModule.Object);
        FragmentShader_ = std::move(FragmentModule.Object);
        PipelineLayout_ = std::move(Layout.Object);
        RenderPass_ = std::move(RenderPass.Object);
        GraphicsPipeline_ = std::move(Pipeline.Object);
        Width_ = Width;
        Height_ = Height;
        return RHI::ERHIResult::Success;
    }

    RHI::ERHIResult AcquireFrame(FDemoBackendFrame& OutFrame) override
    {
        if (!Swapchain_ || !GraphicsPipeline_ || PendingCommand_)
            return RHI::ERHIResult::InvalidState;
        Core::uint32 FrameIndex = 0;
        const RHI::ERHIResult Acquire =
            Swapchain_->AcquireNextFrame(FrameIndex);
        if (Acquire != RHI::ERHIResult::Success) return Acquire;
        const auto Image = Swapchain_->GetImage(FrameIndex);
        if (!Image) return RHI::ERHIResult::Failed;

        RHI::FRHIFramebufferDesc FramebufferDesc;
        FramebufferDesc.RenderPass = RenderPass_;
        FramebufferDesc.Attachments.push_back({Image, 0, 0});
        FramebufferDesc.Width = Image->GetDesc().Width;
        FramebufferDesc.Height = Image->GetDesc().Height;
        auto Framebuffer = Device_->CreateFramebuffer(FramebufferDesc);
        auto Commands = Device_->CreateCommandBuffer(RHI::ERHIQueueType::Graphics);
        auto RenderComplete = Device_->CreateSemaphore();
        if (!Framebuffer.Succeeded()) return Framebuffer.Result;
        if (!Commands.Succeeded()) return Commands.Result;
        if (!RenderComplete.Succeeded()) return RenderComplete.Result;

        OutFrame = {};
        OutFrame.FrameIndex = FrameIndex;
        OutFrame.Width = FramebufferDesc.Width;
        OutFrame.Height = FramebufferDesc.Height;
        OutFrame.ExecutionBindings.OutputTexture = Image;
        OutFrame.ExecutionBindings.VertexBuffer = VertexBuffer_;
        OutFrame.ExecutionBindings.GraphicsPipeline = GraphicsPipeline_;
        OutFrame.ExecutionBindings.RenderPass = RenderPass_;
        OutFrame.ExecutionBindings.Framebuffer = Framebuffer.Object;
        OutFrame.ExecutionBindings.CommandBuffer = Commands.Object;
        PendingCommand_ = std::move(Commands.Object);
        PendingFramebuffer_ = std::move(Framebuffer.Object);
        PendingRenderComplete_ = std::move(RenderComplete.Object);
        PendingFrameIndex_ = FrameIndex;
        return RHI::ERHIResult::Success;
    }

    RHI::ERHIResult SubmitFrame(const FDemoBackendFrame& Frame) override
    {
        if (!Queue_ || !Swapchain_ || !PendingCommand_ ||
            !PendingRenderComplete_ || Frame.FrameIndex != PendingFrameIndex_ ||
            Frame.ExecutionBindings.CommandBuffer != PendingCommand_)
            return RHI::ERHIResult::InvalidState;
        const RHI::ERHIResult Submit = Queue_->Submit(
            PendingCommand_, {}, {PendingRenderComplete_}, nullptr);
        if (Submit != RHI::ERHIResult::Success)
        {
            ClearPendingFrame();
            return Submit;
        }
        const RHI::ERHIResult Present = Swapchain_->Present(
            PendingFrameIndex_, PendingRenderComplete_);
        ClearPendingFrame();
        return Present;
    }

    RHI::ERHIResult RecreatePresentation(Core::uint32, Core::uint32) override
    {
        return Swapchain_ ? RHI::ERHIResult::Success : RHI::ERHIResult::InvalidState;
    }

    RHI::ERHIResult ExecuteOffscreenTriangle(
        const RHI::FRHIShaderModuleDesc&,
        const RHI::FRHIShaderModuleDesc&) override
    {
        return RHI::ERHIResult::Unsupported;
    }

    RHI::FRHIRuntimeSnapshot GetSnapshot() const noexcept override
    {
        return Device_ ? Device_->GetRuntimeSnapshot() : RHI::FRHIRuntimeSnapshot{};
    }

    Core::TSharedPtr<RHI::IRHIDevice> GetDevice() const noexcept override
    {
        return Device_;
    }

    RHI::ERHIResult Shutdown() override
    {
        ClearPendingFrame();
        ResetTriangleResources();
        Queue_.reset();
        Swapchain_.reset();
        if (Surface_)
        {
            (void)Surface_->Invalidate();
            Surface_.reset();
        }
        if (!Device_) return RHI::ERHIResult::Success;
        const RHI::ERHIResult Result = Device_->Shutdown();
        Device_.reset();
        return Result;
    }

private:
    void ClearPendingFrame() noexcept
    {
        PendingCommand_.reset();
        PendingFramebuffer_.reset();
        PendingRenderComplete_.reset();
        PendingFrameIndex_ = 0;
    }

    void ResetTriangleResources() noexcept
    {
        ClearPendingFrame();
        GraphicsPipeline_.reset();
        RenderPass_.reset();
        PipelineLayout_.reset();
        FragmentShader_.reset();
        VertexShader_.reset();
        VertexBuffer_.reset();
        Width_ = 0;
        Height_ = 0;
    }

    EDemoRunMode Mode_ = EDemoRunMode::InteractiveNative;
    Core::uint32 FramesInFlight_ = 2;
    Core::uint32 Width_ = 0;
    Core::uint32 Height_ = 0;
    Core::uint32 PendingFrameIndex_ = 0;
    Core::TSharedPtr<RHI::IRHIDevice> Device_;
    Core::TSharedPtr<RHI::IRHIPresentationSurface> Surface_;
    Core::TSharedPtr<RHI::IRHISwapchain> Swapchain_;
    Core::TSharedPtr<RHI::IRHICommandQueue> Queue_;
    Core::TSharedPtr<RHI::IRHIBuffer> VertexBuffer_;
    Core::TSharedPtr<RHI::IRHIShaderModule> VertexShader_;
    Core::TSharedPtr<RHI::IRHIShaderModule> FragmentShader_;
    Core::TSharedPtr<RHI::IRHIPipelineLayout> PipelineLayout_;
    Core::TSharedPtr<RHI::IRHIRenderPass> RenderPass_;
    Core::TSharedPtr<RHI::IRHIGraphicsPipeline> GraphicsPipeline_;
    Core::TSharedPtr<RHI::IRHICommandBuffer> PendingCommand_;
    Core::TSharedPtr<RHI::IRHIFramebuffer> PendingFramebuffer_;
    Core::TSharedPtr<RHI::IRHISemaphore> PendingRenderComplete_;
};
#endif

} // namespace

FDemoBackendCreateResult FDemoBackendFactory::Create(
    EDemoGraphicsBackend Backend) const
{
    FDemoBackendCreateResult Result;
    Result.RequestedBackend = Backend;
    Result.SelectedBackend = Backend;
    try
    {
        if (Backend == EDemoGraphicsBackend::Vulkan)
        {
            Result.Runtime = Core::MakeUnique<FVulkanDemoBackendRuntime>();
            Result.Result = RHI::ERHIResult::Success;
            return Result;
        }
#if SG_PLATFORM_MAC
        Result.Runtime = Core::MakeUnique<FMetalDemoBackendRuntime>();
        Result.Result = RHI::ERHIResult::Success;
#else
        Result.Result = RHI::ERHIResult::Unavailable;
        Result.FailureReason = "Metal backend is unavailable on this host";
#endif
    }
    catch (const std::bad_alloc&)
    {
        Result.Result = RHI::ERHIResult::Failed;
        Result.FailureReason = "backend runtime allocation failed";
    }
    return Result;
}

} // namespace Stoner::Demo
