#pragma once

#include "FMetalDeviceOwnerState.h"
#include "MetalRHI/FMetalBackendConfig.h"
#include "MetalRHI/FMetalBackendDiagnostics.h"
#include "RHI/IRHIDevice.h"

namespace Stoner::Backend::Metal::Private
{

class FMetalDevice final : public RHI::IRHIDevice
{
public:
    FMetalDevice(
        void* RetainedNativeDevice,
        Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
        FMetalAdapterSummary Adapter,
        RHI::FRHIDeviceCapabilities Capabilities);
    ~FMetalDevice() override;

    [[nodiscard]] bool Initialize(
        EMetalInitializationFailurePoint FailurePoint) noexcept;
    [[nodiscard]] RHI::ERHIDeviceState GetState() const noexcept override;
    [[nodiscard]] const RHI::FRHIDeviceCapabilities& GetCapabilities()
        const noexcept override;
    [[nodiscard]] bool IsActive() const noexcept override;
    [[nodiscard]] RHI::ERHIRuntimeMode GetRuntimeMode() const noexcept override;
    [[nodiscard]] RHI::FRHIRuntimeSnapshot GetRuntimeSnapshot()
        const noexcept override;
    RHI::ERHIResult Shutdown() override;

    RHI::TRHIObjectResult<RHI::IRHICommandQueue> CreateCommandQueue(
        RHI::ERHIQueueType QueueType) override;
    RHI::TRHIObjectResult<RHI::IRHICommandBuffer> CreateCommandBuffer(
        RHI::ERHIQueueType CompatibleQueueType) override;
    RHI::TRHIObjectResult<RHI::IRHIFence> CreateFence(
        bool bInitiallySignaled = false) override;
    RHI::TRHIObjectResult<RHI::IRHISemaphore> CreateSemaphore() override;
    RHI::TRHIObjectResult<RHI::IRHISwapchain> CreateSwapchain(
        Core::uint32 FrameCount) override;
    RHI::TRHIObjectResult<RHI::IRHIBuffer> CreateBuffer(
        const RHI::FRHIBufferDesc& Desc) override;
    RHI::ERHIResult UploadBuffer(
        const Core::TSharedPtr<RHI::IRHIBuffer>& Buffer,
        const RHI::FRHIBufferUploadDesc& Upload) override;
    RHI::TRHIObjectResult<RHI::IRHITexture> CreateTexture(
        const RHI::FRHITextureDesc& Desc) override;
    RHI::ERHIResult UploadTexture(
        const Core::TSharedPtr<RHI::IRHITexture>& Texture,
        const RHI::FRHITextureUploadDesc& Upload) override;
    RHI::TRHIObjectResult<RHI::IRHISampler> CreateSampler(
        const RHI::FRHISamplerDesc& Desc) override;
    RHI::TRHIObjectResult<RHI::IRHIShaderModule> CreateShaderModule(
        const RHI::FRHIShaderModuleDesc& Desc) override;
    RHI::TRHIObjectResult<RHI::IRHIPipelineLayout> CreatePipelineLayout(
        const RHI::FRHIPipelineLayoutDesc& Desc) override;
    RHI::TRHIObjectResult<RHI::IRHIDescriptorSet> CreateDescriptorSet(
        const Core::TSharedPtr<RHI::IRHIPipelineLayout>& Layout,
        Core::uint32 SetIndex) override;
    RHI::TRHIObjectResult<RHI::IRHIGraphicsPipeline> CreateGraphicsPipeline(
        const RHI::FRHIGraphicsPipelineDesc& Desc) override;
    RHI::TRHIObjectResult<RHI::IRHIComputePipeline> CreateComputePipeline(
        const RHI::FRHIComputePipelineDesc& Desc) override;
    RHI::TRHIObjectResult<RHI::IRHIRenderPass> CreateRenderPass(
        const RHI::FRHIRenderPassDesc& Desc) override;
    RHI::TRHIObjectResult<RHI::IRHIFramebuffer> CreateFramebuffer(
        const RHI::FRHIFramebufferDesc& Desc) override;
    RHI::TRHIObjectResult<RHI::IRHIPresentationSurface>
    CreatePresentationSurface(
        const RHI::FRHIPresentationSurfaceDesc& Desc) override;
    RHI::TRHIObjectResult<RHI::IRHISwapchain> CreateSwapchain(
        const Core::TSharedPtr<RHI::IRHIPresentationSurface>& Surface,
        const RHI::FRHISwapchainDesc& Desc) override;

    [[nodiscard]] FMetalBackendInspection Inspect() const noexcept;
    [[nodiscard]] FMetalBackendDiagnostics InspectDiagnostics() const;
    [[nodiscard]] const Core::TSharedPtr<FMetalDeviceOwnerState>&
    GetOwner() const noexcept;
    [[nodiscard]] void* GetNativeDevice() const noexcept;
    [[nodiscard]] void* GetNativeQueue() const noexcept;
    [[nodiscard]] RHI::ERHIResult ReadbackTextureForTesting(
        const Core::TSharedPtr<RHI::IRHITexture>& Texture,
        Core::TArray<Core::uint8>& OutBytes) const noexcept;

private:
    struct FImpl;
    Core::TUniquePtr<FImpl> Impl_;
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner_;
    FMetalAdapterSummary Adapter_;
    RHI::FRHIDeviceCapabilities Capabilities_;
    RHI::ERHIDeviceState State_ = RHI::ERHIDeviceState::Uninitialized;
};

} // namespace Stoner::Backend::Metal::Private
