#include "FMetalTexture.h"

namespace Stoner::Backend::Metal::Private
{

FMetalTexture::FMetalTexture(
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
    const RHI::FRHITextureDesc& Desc,
    id<MTLTexture> Texture) noexcept
    : FMetalNativeObject(
          std::move(Owner), EMetalOwnershipCategory::Resource),
      Desc_(Desc), Texture_(Texture)
{
}

FMetalTexture::~FMetalTexture()
{
    (void)Invalidate();
}

const RHI::FRHITextureDesc& FMetalTexture::GetDesc() const noexcept
{
    return Desc_;
}

RHI::ERHITextureDimension FMetalTexture::GetDimension() const noexcept
{
    return Desc_.Dimension;
}

RHI::ERHIFormat FMetalTexture::GetFormat() const noexcept
{
    return Desc_.Format;
}

RHI::ERHITextureUsage FMetalTexture::GetUsage() const noexcept
{
    return Desc_.Usage;
}

RHI::ERHIResourceLifecycleState FMetalTexture::GetLifecycleState()
    const noexcept
{
    return GetLifecycle();
}

RHI::ERHIResult FMetalTexture::Invalidate()
{
    return InvalidateObject();
}

id<MTLTexture> FMetalTexture::GetNativeTexture() const noexcept
{
    return Texture_;
}

} // namespace Stoner::Backend::Metal::Private
