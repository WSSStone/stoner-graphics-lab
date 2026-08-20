#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIResult.h"
#include "RHI/FRHITextureUploadDesc.h"

#import <Metal/Metal.h>

namespace Stoner::Backend::Metal::Private
{

[[nodiscard]] RHI::ERHIResult UploadMetalPrivateBuffer(
    id<MTLCommandQueue> Queue,
    id<MTLBuffer> Destination,
    const void* Data,
    Core::uint64 Size,
    Core::uint64 Offset) noexcept;

[[nodiscard]] RHI::ERHIResult UploadMetalTexture(
    id<MTLCommandQueue> Queue,
    id<MTLTexture> Destination,
    const RHI::FRHITextureDesc& TextureDesc,
    const RHI::FRHITextureUploadDesc& Upload) noexcept;

[[nodiscard]] RHI::ERHIResult ReadbackMetalBuffer(
    id<MTLCommandQueue> Queue,
    id<MTLBuffer> Source,
    Core::uint64 Offset,
    Core::uint64 Size,
    Core::TArray<Core::uint8>& OutBytes) noexcept;

} // namespace Stoner::Backend::Metal::Private
