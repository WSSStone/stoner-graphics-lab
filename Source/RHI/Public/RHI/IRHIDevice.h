#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIQueueType.h"
#include "RHI/ERHIResult.h"
#include "RHI/FRHIDeviceCapabilities.h"
#include "RHI/FRHIRuntimeSnapshot.h"
#include "RHI/FRHIPresentationSurfaceDesc.h"
#include "RHI/FRHISwapchainDesc.h"

namespace Stoner::RHI
{

class IRHICommandBuffer;
class IRHICommandQueue;
class IRHIBuffer;
class IRHIComputePipeline;
class IRHIDescriptorSet;
class IRHIFence;
class IRHIFramebuffer;
class IRHIGraphicsPipeline;
class IRHIPipelineLayout;
class IRHIPresentationSurface;
class IRHIRenderPass;
class IRHISampler;
class IRHISemaphore;
class IRHIShaderModule;
class IRHISwapchain;
class IRHITexture;

struct FRHIBufferDesc;
struct FRHIComputePipelineDesc;
struct FRHIFramebufferDesc;
struct FRHIGraphicsPipelineDesc;
struct FRHIPipelineLayoutDesc;
struct FRHIRenderPassDesc;
struct FRHISamplerDesc;
struct FRHIShaderModuleDesc;
struct FRHITextureDesc;

enum class ERHIDeviceState
{
    Uninitialized,
    Active,
    Shutdown
};

template <typename T>
struct TRHIObjectResult
{
    ERHIResult Result = ERHIResult::Failed;
    Stoner::Core::TSharedPtr<T> Object;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return Result == ERHIResult::Success && Object != nullptr;
    }
};

class IRHIDevice
{
public:
    virtual ~IRHIDevice() = default;

    [[nodiscard]] virtual ERHIDeviceState GetState() const noexcept = 0;
    [[nodiscard]] virtual const FRHIDeviceCapabilities& GetCapabilities() const noexcept = 0;
    [[nodiscard]] virtual bool IsActive() const noexcept = 0;
    [[nodiscard]] virtual ERHIRuntimeMode GetRuntimeMode() const noexcept { return ERHIRuntimeMode::Deterministic; }
    [[nodiscard]] virtual FRHIRuntimeSnapshot GetRuntimeSnapshot() const noexcept { return {}; }

    virtual ERHIResult Shutdown() = 0;

    virtual TRHIObjectResult<IRHICommandQueue> CreateCommandQueue(ERHIQueueType QueueType) = 0;
    virtual TRHIObjectResult<IRHICommandBuffer> CreateCommandBuffer(ERHIQueueType CompatibleQueueType) = 0;
    virtual TRHIObjectResult<IRHIFence> CreateFence(bool bInitiallySignaled = false) = 0;
    virtual TRHIObjectResult<IRHISemaphore> CreateSemaphore() = 0;
    virtual TRHIObjectResult<IRHISwapchain> CreateSwapchain(Stoner::Core::uint32 FrameCount) = 0;
    virtual TRHIObjectResult<IRHIBuffer> CreateBuffer(const FRHIBufferDesc& Desc) = 0;
    virtual TRHIObjectResult<IRHITexture> CreateTexture(const FRHITextureDesc& Desc) = 0;
    virtual TRHIObjectResult<IRHISampler> CreateSampler(const FRHISamplerDesc& Desc) = 0;
    virtual TRHIObjectResult<IRHIShaderModule> CreateShaderModule(const FRHIShaderModuleDesc& Desc) = 0;
    virtual TRHIObjectResult<IRHIPipelineLayout> CreatePipelineLayout(const FRHIPipelineLayoutDesc& Desc) = 0;
    virtual TRHIObjectResult<IRHIDescriptorSet> CreateDescriptorSet(const Stoner::Core::TSharedPtr<IRHIPipelineLayout>& Layout, Stoner::Core::uint32 SetIndex) = 0;
    virtual TRHIObjectResult<IRHIGraphicsPipeline> CreateGraphicsPipeline(const FRHIGraphicsPipelineDesc& Desc) = 0;
    virtual TRHIObjectResult<IRHIComputePipeline> CreateComputePipeline(const FRHIComputePipelineDesc& Desc) = 0;
    virtual TRHIObjectResult<IRHIRenderPass> CreateRenderPass(const FRHIRenderPassDesc& Desc) = 0;
    virtual TRHIObjectResult<IRHIFramebuffer> CreateFramebuffer(const FRHIFramebufferDesc& Desc) = 0;
    virtual TRHIObjectResult<IRHIPresentationSurface> CreatePresentationSurface(const FRHIPresentationSurfaceDesc&)
    {
        return {ERHIResult::Unsupported, nullptr};
    }
    virtual TRHIObjectResult<IRHISwapchain> CreateSwapchain(
        const Stoner::Core::TSharedPtr<IRHIPresentationSurface>&,
        const FRHISwapchainDesc& Desc)
    {
        return CreateSwapchain(Desc.FramesInFlight);
    }
};

} // namespace Stoner::RHI
