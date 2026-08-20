#pragma once

#include "FMetalNativeObject.h"
#include "RHI/IRHITexture.h"

#import <Metal/Metal.h>

namespace Stoner::Backend::Metal::Private
{

class FMetalTexture final : public RHI::IRHITexture, public FMetalNativeObject
{
public:
    FMetalTexture(
        Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
        const RHI::FRHITextureDesc& Desc,
        id<MTLTexture> Texture) noexcept;
    ~FMetalTexture() override;

    [[nodiscard]] const RHI::FRHITextureDesc& GetDesc() const noexcept override;
    [[nodiscard]] RHI::ERHITextureDimension GetDimension() const noexcept override;
    [[nodiscard]] RHI::ERHIFormat GetFormat() const noexcept override;
    [[nodiscard]] RHI::ERHITextureUsage GetUsage() const noexcept override;
    [[nodiscard]] RHI::ERHIResourceLifecycleState GetLifecycleState()
        const noexcept override;
    RHI::ERHIResult Invalidate() override;

    [[nodiscard]] id<MTLTexture> GetNativeTexture() const noexcept;

private:
    RHI::FRHITextureDesc Desc_;
    __strong id<MTLTexture> Texture_;
};

} // namespace Stoner::Backend::Metal::Private
