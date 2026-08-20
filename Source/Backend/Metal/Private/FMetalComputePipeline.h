#pragma once

#include "FMetalNativeObject.h"
#include "RHI/IRHIComputePipeline.h"
#include "RHI/IRHIDevice.h"

#import <Metal/Metal.h>

namespace Stoner::Backend::Metal::Private
{

class FMetalComputePipeline final
    : public RHI::IRHIComputePipeline,
      public FMetalNativeObject
{
public:
    FMetalComputePipeline(
        Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
        RHI::FRHIComputePipelineDesc Desc,
        id<MTLComputePipelineState> Pipeline) noexcept;
    ~FMetalComputePipeline() override;

    [[nodiscard]] const RHI::FRHIComputePipelineDesc& GetDesc()
        const noexcept override;
    [[nodiscard]] Core::TSharedPtr<RHI::IRHIPipelineLayout>
    GetPipelineLayout() const noexcept override;
    [[nodiscard]] RHI::ERHIResourceLifecycleState GetLifecycleState()
        const noexcept override;
    RHI::ERHIResult Invalidate() override;

    [[nodiscard]] id<MTLComputePipelineState> GetNativePipeline()
        const noexcept;
    void MarkReused() noexcept;

private:
    RHI::FRHIComputePipelineDesc Desc_;
    __strong id<MTLComputePipelineState> Pipeline_;
};

[[nodiscard]] Core::FString BuildMetalComputePipelineKey(
    const RHI::FRHIComputePipelineDesc& Desc);
[[nodiscard]] RHI::TRHIObjectResult<RHI::IRHIComputePipeline>
CreateMetalComputePipeline(
    const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner,
    void* NativeDevice,
    const RHI::FRHIDeviceCapabilities& Capabilities,
    const RHI::FRHIComputePipelineDesc& Desc) noexcept;

} // namespace Stoner::Backend::Metal::Private
