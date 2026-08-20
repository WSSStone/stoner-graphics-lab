#pragma once

#include "FMetalNativeObject.h"
#include "RHI/IRHIDevice.h"
#include "RHI/IRHIShaderModule.h"

#import <Metal/Metal.h>

namespace Stoner::Backend::Metal::Private
{

class FMetalShaderLibrary final
    : public RHI::IRHIShaderModule,
      public FMetalNativeObject
{
public:
    FMetalShaderLibrary(
        Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
        RHI::FRHIShaderModuleDesc Desc,
        id<MTLLibrary> Library,
        id<MTLFunction> Function) noexcept;
    ~FMetalShaderLibrary() override;

    [[nodiscard]] const RHI::FRHIShaderModuleDesc& GetDesc()
        const noexcept override;
    [[nodiscard]] RHI::ERHIShaderStage GetStage() const noexcept override;
    [[nodiscard]] RHI::ERHIResourceLifecycleState GetLifecycleState()
        const noexcept override;
    RHI::ERHIResult Invalidate() override;
    [[nodiscard]] id<MTLFunction> GetNativeFunction() const noexcept;

private:
    RHI::FRHIShaderModuleDesc Desc_;
    __strong id<MTLLibrary> Library_;
    __strong id<MTLFunction> Function_;
};

[[nodiscard]] RHI::TRHIObjectResult<RHI::IRHIShaderModule>
CreateMetalShaderLibrary(
    const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner,
    void* NativeDevice,
    const RHI::FRHIShaderModuleDesc& Desc) noexcept;

} // namespace Stoner::Backend::Metal::Private
