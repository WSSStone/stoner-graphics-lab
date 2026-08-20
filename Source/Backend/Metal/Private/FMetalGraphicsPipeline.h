#pragma once

#include "FMetalNativeObject.h"
#include "RHI/IRHIDevice.h"
#include "RHI/IRHIGraphicsPipeline.h"

#import <Metal/Metal.h>

namespace Stoner::Backend::Metal::Private
{

class FMetalGraphicsPipeline final
    : public RHI::IRHIGraphicsPipeline,
      public FMetalNativeObject
{
public:
    FMetalGraphicsPipeline(
        Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
        RHI::FRHIGraphicsPipelineDesc Desc,
        id<MTLRenderPipelineState> Pipeline,
        id<MTLDepthStencilState> DepthStencil) noexcept;
    ~FMetalGraphicsPipeline() override;

    [[nodiscard]] const RHI::FRHIGraphicsPipelineDesc& GetDesc()
        const noexcept override;
    [[nodiscard]] Core::TSharedPtr<RHI::IRHIPipelineLayout>
    GetPipelineLayout() const noexcept override;
    [[nodiscard]] RHI::ERHIResourceLifecycleState GetLifecycleState()
        const noexcept override;
    RHI::ERHIResult Invalidate() override;

    [[nodiscard]] id<MTLRenderPipelineState> GetNativePipeline()
        const noexcept;
    [[nodiscard]] id<MTLDepthStencilState> GetNativeDepthStencil()
        const noexcept;
    void MarkReused() noexcept;

private:
    RHI::FRHIGraphicsPipelineDesc Desc_;
    __strong id<MTLRenderPipelineState> Pipeline_;
    __strong id<MTLDepthStencilState> DepthStencil_;
};

[[nodiscard]] Core::FString BuildMetalGraphicsPipelineKey(
    const RHI::FRHIGraphicsPipelineDesc& Desc);
[[nodiscard]] RHI::TRHIObjectResult<RHI::IRHIGraphicsPipeline>
CreateMetalGraphicsPipeline(
    const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner,
    void* NativeDevice,
    const RHI::FRHIDeviceCapabilities& Capabilities,
    const RHI::FRHIGraphicsPipelineDesc& Desc) noexcept;

} // namespace Stoner::Backend::Metal::Private
