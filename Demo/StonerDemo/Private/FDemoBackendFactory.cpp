#include "FDemoBackendFactory.h"

#include "FProductionPresentationPixels.h"

#include "Asset/FAssetDigest.h"
#include "Core/SGPlatform.h"
#include "RHI/RHIMinimal.h"
#include "VulkanRHI/FVulkanDevice.h"
#include "VulkanRHI/FVulkanNativeContext.h"

#if SG_PLATFORM_MAC
#include "MetalRHI/FMetalDeviceFactory.h"
#endif

#include <array>
#include <iostream>
#include <utility>

namespace Stoner::Demo
{
namespace
{

#if SG_PLATFORM_MAC
bool NormalizePresentationPixels(
    std::span<const Core::uint8> Native,
    RHI::ERHIFormat Format,
    Core::TArray<Core::uint8>& OutRgba8)
{
    OutRgba8.clear();
    const bool bBgra = Format == RHI::ERHIFormat::B8G8R8A8_UNorm;
    const bool bRgba = Format == RHI::ERHIFormat::R8G8B8A8_UNorm ||
        Format == RHI::ERHIFormat::R8G8B8A8_sRGB;
    if ((!bBgra && !bRgba) || Native.empty() || Native.size() % 4u != 0)
        return false;
    OutRgba8.resize(Native.size());
    for (Core::usize Offset = 0; Offset < Native.size(); Offset += 4u)
    {
        OutRgba8[Offset] = Native[Offset + (bBgra ? 2u : 0u)];
        OutRgba8[Offset + 1u] = Native[Offset + 1u];
        OutRgba8[Offset + 2u] = Native[Offset + (bBgra ? 0u : 2u)];
        OutRgba8[Offset + 3u] = Native[Offset + 3u];
    }
    return true;
}
#endif

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
        Device_ = Core::MakeShared<Backend::Vulkan::FVulkanDevice>();
        // The bounded production corpus can realize hundreds of material
        // descriptor sets before the shared Deferred lighting sets. Keep the
        // explicit fixed-capacity RHI contract, but size the demo runtime for
        // the admitted Sponza upper bound rather than the triangle default.
        Device_->ConfigureDescriptorPoolCapacity(4096);
        Backend::Vulkan::FVulkanInstanceDesc DeviceDesc;
        DeviceDesc.RuntimeMode =
            Backend::Vulkan::EVulkanInstanceRuntimeMode::DeterministicFallback;
        DeviceDesc.bRequestValidation = false;
        RHI::ERHIResult DeviceResult = Device_->Initialize(DeviceDesc);
        if (DeviceResult == RHI::ERHIResult::Success)
            DeviceResult = Device_->EnableNativeShaderRuntime();
        if (DeviceResult != RHI::ERHIResult::Success)
        {
            if (Device_) (void)Device_->Shutdown();
            Device_.reset();
            return DeviceResult;
        }
        if (Mode == EDemoRunMode::NativeHeadless)
            return RHI::ERHIResult::Success;
        const RHI::ERHIResult ContextResult = Context_.Initialize(RuntimeMode, Window);
        if (ContextResult != RHI::ERHIResult::Success)
        {
            (void)Device_->Shutdown();
            Device_.reset();
        }
        return ContextResult;
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
        if (bProductionPresentation_)
        {
            const RHI::ERHIResult Result =
                Context_.PrepareVisibleImage(Width, Height);
            if (Result == RHI::ERHIResult::Success)
            {
                ProductionPresentationWidth_ = Width;
                ProductionPresentationHeight_ = Height;
            }
            return Result;
        }
        return Context_.RecreateVisiblePresentation(Width, Height);
    }

    RHI::ERHIResult PrepareProductionPresentation(
        Core::uint32 Width,
        Core::uint32 Height) override
    {
        const RHI::ERHIResult Result =
            Context_.PrepareVisibleImage(Width, Height);
        bProductionPresentation_ = Result == RHI::ERHIResult::Success;
        if (bProductionPresentation_)
        {
            ProductionPresentationWidth_ = Width;
            ProductionPresentationHeight_ = Height;
        }
        return Result;
    }

    RHI::ERHIResult PresentProductionImage(
        std::span<const Core::uint8> Rgba8,
        Core::uint32 Width,
        Core::uint32 Height,
        Core::uint32 RowPitchBytes,
        FDemoProductionPresentationResult& OutResult) override
    {
        OutResult.Rgba8.clear();
        OutResult.Width = 0;
        OutResult.Height = 0;
        OutResult.RowPitchBytes = 0;
        OutResult.bPresented = false;
        if (!BuildAspectFitPresentationPixels(
                Rgba8, Width, Height, RowPitchBytes,
                ProductionPresentationWidth_, ProductionPresentationHeight_,
                RHI::ERHIFormat::R8G8B8A8_UNorm,
                ProductionPresentationPixels_))
            return RHI::ERHIResult::InvalidState;
        const RHI::ERHIResult Result = Context_.PresentVisibleRgba8(
            ProductionPresentationPixels_, ProductionPresentationWidth_,
            ProductionPresentationHeight_,
            ProductionPresentationWidth_ * 4u, OutResult.Rgba8,
            OutResult.Width, OutResult.Height);
        if (Result == RHI::ERHIResult::Success)
        {
            OutResult.RowPitchBytes = OutResult.Width * 4u;
            OutResult.bPresented = true;
        }
        return Result;
    }

    RHI::ERHIResult ExecuteOffscreenTriangle(
        const Renderer::FForwardFramePlan&,
        const RHI::FRHIShaderModuleDesc& VertexShader,
        const RHI::FRHIShaderModuleDesc& FragmentShader) override
    {
        if (!Context_.IsAvailable() &&
            Context_.Initialize(RHI::ERHIRuntimeMode::NativeHeadless) !=
                RHI::ERHIResult::Success)
            return RHI::ERHIResult::Unavailable;
        return Context_.ExecuteOffscreenTriangle(VertexShader, FragmentShader);
    }

    RHI::FRHIRuntimeSnapshot GetSnapshot() const noexcept override
    {
        return Device_ ? Device_->GetRuntimeSnapshot() : Context_.GetSnapshot();
    }

    Core::TSharedPtr<RHI::IRHIDevice> GetDevice() const noexcept override
    {
        return Device_;
    }

    RHI::ERHIResult Shutdown() override
    {
        LastNativeFrame_ = {};
        RHI::ERHIResult DeviceResult = RHI::ERHIResult::Success;
        if (Device_)
        {
            DeviceResult = Device_->Shutdown();
            Device_.reset();
        }
        const RHI::ERHIResult ContextResult = Context_.Shutdown();
        return DeviceResult != RHI::ERHIResult::Success ? DeviceResult : ContextResult;
    }

private:
    EDemoRunMode Mode_ = EDemoRunMode::InteractiveNative;
    Backend::Vulkan::FVulkanNativeContext Context_;
    Core::TSharedPtr<Backend::Vulkan::FVulkanDevice> Device_;
    Backend::Vulkan::FVulkanNativeFrameBindings LastNativeFrame_;
    bool bProductionPresentation_ = false;
    Core::uint32 ProductionPresentationWidth_ = 0;
    Core::uint32 ProductionPresentationHeight_ = 0;
    Core::TArray<Core::uint8> ProductionPresentationPixels_;
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

    RHI::ERHIResult PrepareProductionPresentation(
        Core::uint32 Width,
        Core::uint32 Height) override
    {
        if (!Device_ || !Swapchain_ || Width == 0 || Height == 0)
            return RHI::ERHIResult::InvalidState;
        if (!ProductionReadbackFence_)
        {
            auto Fence = Device_->CreateFence();
            if (!Fence.Succeeded()) return Fence.Result;
            ProductionReadbackFence_ = std::move(Fence.Object);
        }
        return RHI::ERHIResult::Success;
    }

    RHI::ERHIResult PresentProductionImage(
        std::span<const Core::uint8> Rgba8,
        Core::uint32 Width,
        Core::uint32 Height,
        Core::uint32 RowPitchBytes,
        FDemoProductionPresentationResult& OutResult) override
    {
        OutResult.Rgba8.clear();
        OutResult.Width = 0;
        OutResult.Height = 0;
        OutResult.RowPitchBytes = 0;
        OutResult.bPresented = false;
        if (!Device_ || !Queue_ || !Swapchain_)
            return RHI::ERHIResult::InvalidState;
        Core::uint32 FrameIndex = 0;
        const RHI::ERHIResult Acquire =
            Swapchain_->AcquireNextFrame(FrameIndex);
        if (Acquire != RHI::ERHIResult::Success) return Acquire;
        const auto Image = Swapchain_->GetImage(FrameIndex);
        const auto ReleaseAcquire = [this, FrameIndex]() {
            (void)Swapchain_->Present(FrameIndex);
        };
        if (!Image)
        {
            ReleaseAcquire();
            return RHI::ERHIResult::Failed;
        }
        const auto& Desc = Image->GetDesc();
        Core::TArray<Core::uint8> NativePixels;
        if (!BuildAspectFitPresentationPixels(
                Rgba8, Width, Height, RowPitchBytes,
                Desc.Width, Desc.Height, Desc.Format, NativePixels))
        {
            ReleaseAcquire();
            return RHI::ERHIResult::InvalidState;
        }
        RHI::FRHITextureUploadDesc Upload;
        Upload.Width = Desc.Width;
        Upload.Height = Desc.Height;
        Upload.RowPitchBytes = static_cast<Core::uint64>(Desc.Width) * 4u;
        Upload.Data = NativePixels.data();
        Upload.DataSizeBytes = NativePixels.size();
        if (Device_->UploadTexture(Image, Upload) != RHI::ERHIResult::Success)
        {
            ReleaseAcquire();
            return RHI::ERHIResult::Failed;
        }

        RHI::FRHIBufferDesc ReadbackDesc;
        ReadbackDesc.SizeInBytes = NativePixels.size();
        ReadbackDesc.Usage = RHI::ERHIBufferUsage::CopyDestination;
        ReadbackDesc.MemoryAccess = RHI::ERHIMemoryAccess::HostVisible;
        auto Readback = Device_->CreateBuffer(ReadbackDesc);
        auto Commands = Device_->CreateCommandBuffer(
            RHI::ERHIQueueType::Graphics);
        if (!Readback.Succeeded() || !Commands.Succeeded() ||
            !ProductionReadbackFence_)
        {
            ReleaseAcquire();
            return RHI::ERHIResult::Failed;
        }
        RHI::FRHITextureBufferCopyRegion Region;
        Region.Width = Desc.Width;
        Region.Height = Desc.Height;
        Region.DestinationRowLengthTexels = Desc.Width;
        Region.DestinationImageHeightTexels = Desc.Height;
        const bool bRecorded =
            Commands.Object->Begin() == RHI::ERHIResult::Success &&
            Commands.Object->RecordTextureToBufferCopy(
                Image, Readback.Object, Region) ==
                RHI::ERHIResult::Success &&
            Commands.Object->End() == RHI::ERHIResult::Success;
        if (!bRecorded || Queue_->Submit(
                Commands.Object, {}, {}, ProductionReadbackFence_) !=
                RHI::ERHIResult::Success ||
            ProductionReadbackFence_->Wait(30'000'000) !=
                RHI::ERHIResult::Success ||
            ProductionReadbackFence_->Reset() != RHI::ERHIResult::Success)
        {
            ReleaseAcquire();
            return RHI::ERHIResult::Failed;
        }
        Core::TArray<Core::uint8> NativeReadback;
        if (Backend::Metal::ReadMetalBufferForValidation(
                Device_, Readback.Object, 0, NativePixels.size(),
                NativeReadback) != RHI::ERHIResult::Success ||
            !NormalizePresentationPixels(
                NativeReadback, Desc.Format, OutResult.Rgba8))
        {
            ReleaseAcquire();
            return RHI::ERHIResult::Failed;
        }
        const RHI::ERHIResult Presented = Swapchain_->Present(FrameIndex);
        if (Presented != RHI::ERHIResult::Success)
        {
            OutResult.Rgba8.clear();
            OutResult.Width = 0;
            OutResult.Height = 0;
            OutResult.RowPitchBytes = 0;
            OutResult.bPresented = false;
            return Presented;
        }
        OutResult.Width = Desc.Width;
        OutResult.Height = Desc.Height;
        OutResult.RowPitchBytes = Desc.Width * 4u;
        OutResult.bPresented = true;
        return RHI::ERHIResult::Success;
    }

    RHI::ERHIResult ExecuteOffscreenTriangle(
        const Renderer::FForwardFramePlan& Plan,
        const RHI::FRHIShaderModuleDesc& VertexShader,
        const RHI::FRHIShaderModuleDesc& FragmentShader) override
    {
        constexpr Core::uint32 Extent = 64;
        constexpr Core::uint64 ReadbackSize = Extent * Extent * 4u;
        if (!Device_ || !Plan.IsValid() ||
            Plan.OutputTarget.Extent.Width != Extent ||
            Plan.OutputTarget.Extent.Height != Extent)
            return RHI::ERHIResult::InvalidState;
        if (PrepareTriangle(
                VertexShader, FragmentShader, Extent, Extent) !=
            RHI::ERHIResult::Success)
            return RHI::ERHIResult::Failed;

        const auto Fail = [this]() {
            ResetTriangleResources();
            return RHI::ERHIResult::Failed;
        };
        RHI::FRHITextureDesc OutputDesc;
        OutputDesc.Width = Extent;
        OutputDesc.Height = Extent;
        OutputDesc.Format = RHI::ERHIFormat::B8G8R8A8_UNorm;
        OutputDesc.Usage = RHI::ERHITextureUsage::ColorAttachment |
            RHI::ERHITextureUsage::CopySource;
        auto Output = Device_->CreateTexture(OutputDesc);
        if (!Output.Succeeded()) return Fail();

        RHI::FRHIFramebufferDesc FramebufferDesc;
        FramebufferDesc.RenderPass = RenderPass_;
        FramebufferDesc.Attachments.push_back({Output.Object, 0, 0});
        FramebufferDesc.Width = Extent;
        FramebufferDesc.Height = Extent;
        auto Framebuffer = Device_->CreateFramebuffer(FramebufferDesc);
        auto RenderCommands = Device_->CreateCommandBuffer(
            RHI::ERHIQueueType::Graphics);
        auto Queue = Device_->CreateCommandQueue(RHI::ERHIQueueType::Graphics);
        auto RenderFence = Device_->CreateFence();
        if (!Framebuffer.Succeeded() || !RenderCommands.Succeeded() ||
            !Queue.Succeeded() || !RenderFence.Succeeded())
            return Fail();

        Renderer::FForwardFrameExecutionBindings Bindings;
        Bindings.OutputTexture = Output.Object;
        Bindings.VertexBuffer = VertexBuffer_;
        Bindings.GraphicsPipeline = GraphicsPipeline_;
        Bindings.RenderPass = RenderPass_;
        Bindings.Framebuffer = Framebuffer.Object;
        Bindings.CommandBuffer = RenderCommands.Object;
        const auto Execution = Renderer::FForwardFrameExecutor().Execute(
            Plan, Bindings);
        if (!Execution.Succeeded() ||
            Queue.Object->Submit(
                RenderCommands.Object, {}, {}, RenderFence.Object) !=
                RHI::ERHIResult::Success ||
            RenderFence.Object->Wait(5'000'000) != RHI::ERHIResult::Success)
            return Fail();

        RHI::FRHIBufferDesc ReadbackDesc;
        ReadbackDesc.SizeInBytes = ReadbackSize;
        ReadbackDesc.Usage = RHI::ERHIBufferUsage::CopyDestination;
        ReadbackDesc.MemoryAccess = RHI::ERHIMemoryAccess::HostVisible;
        auto Readback = Device_->CreateBuffer(ReadbackDesc);
        auto CopyCommands = Device_->CreateCommandBuffer(
            RHI::ERHIQueueType::Graphics);
        auto CopyFence = Device_->CreateFence();
        if (!Readback.Succeeded() || !CopyCommands.Succeeded() ||
            !CopyFence.Succeeded())
            return Fail();

        RHI::FRHIResourceBarrierDesc ToCopy;
        ToCopy.Texture = Output.Object;
        ToCopy.RequiredTextureUsage = RHI::ERHITextureUsage::CopySource;
        ToCopy.Before = RHI::ERHIResourceLayout::Present;
        ToCopy.After = RHI::ERHIResourceLayout::CopySource;
        RHI::FRHITextureBufferCopyRegion Region;
        Region.Width = Extent;
        Region.Height = Extent;
        const bool bCopyRecorded =
            CopyCommands.Object->Begin() == RHI::ERHIResult::Success &&
            CopyCommands.Object->RecordLayoutTransition(ToCopy) ==
                RHI::ERHIResult::Success &&
            CopyCommands.Object->RecordTextureToBufferCopy(
                Output.Object, Readback.Object, Region) ==
                RHI::ERHIResult::Success &&
            CopyCommands.Object->End() == RHI::ERHIResult::Success;
        if (!bCopyRecorded ||
            Queue.Object->Submit(
                CopyCommands.Object, {}, {}, CopyFence.Object) !=
                RHI::ERHIResult::Success ||
            CopyFence.Object->Wait(5'000'000) != RHI::ERHIResult::Success)
            return Fail();

        Core::TArray<Core::uint8> Bytes;
        if (Backend::Metal::ReadMetalBufferForValidation(
                Device_, Readback.Object, 0, ReadbackSize, Bytes) !=
                RHI::ERHIResult::Success || Bytes.size() != ReadbackSize)
            return Fail();
        bool bObservedDrawnPixel = false;
        for (Core::usize Pixel = 0; Pixel < Bytes.size() / 4; ++Pixel)
        {
            const Core::usize Byte = Pixel * 4;
            if (Bytes[Byte] > 16u || Bytes[Byte + 1] > 16u ||
                Bytes[Byte + 2] > 16u)
            {
                bObservedDrawnPixel = true;
                break;
            }
        }
        if (!bObservedDrawnPixel) return Fail();

        const Asset::FAssetDigest Digest = Asset::FAssetDigest::FromBytes(Bytes);
        std::cout << "[EVIDENCE] metal-native-triangle status=passed readback="
                  << Digest.ToLowerHex().CStr() << '\n';
        ResetTriangleResources();
        return RHI::ERHIResult::Success;
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
        ProductionReadbackFence_.reset();
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
    Core::TSharedPtr<RHI::IRHIFence> ProductionReadbackFence_;
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
