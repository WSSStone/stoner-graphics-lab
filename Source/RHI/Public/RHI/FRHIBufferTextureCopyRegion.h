#pragma once
#include "RHI/FRHITextureBufferCopyRegion.h"

namespace Stoner::RHI
{
// Reverse transfer uses the same checked footprint rules as readback. Buffer
// pitch is expressed in texels, so callers can provide native-aligned staging.
// Initial native support is single-sample 2D RGBA8 UNorm/sRGB. Use 256-byte
// row pitch and source offsets for a portable Vulkan/Metal staging layout;
// unsupported formats or alignment reject before recording.
struct FRHIBufferTextureCopyRegion
{
    Stoner::Core::uint32 DestinationMipLevel = 0;
    Stoner::Core::uint32 DestinationArrayLayer = 0;
    Stoner::Core::uint32 DestinationX = 0;
    Stoner::Core::uint32 DestinationY = 0;
    Stoner::Core::uint32 DestinationZ = 0;
    Stoner::Core::uint32 Width = 1;
    Stoner::Core::uint32 Height = 1;
    Stoner::Core::uint32 Depth = 1;
    Stoner::Core::uint64 SourceOffsetBytes = 0;
    Stoner::Core::uint32 SourceRowLengthTexels = 0;
    Stoner::Core::uint32 SourceImageHeightTexels = 0;
};
[[nodiscard]] constexpr bool TryGetRHIBufferTextureCopyByteSize(
    const FRHIBufferTextureCopyRegion& Region, ERHIFormat Format,
    Stoner::Core::uint64& OutByteSize) noexcept
{
    return TryGetRHITextureBufferCopyByteSize({
        Region.DestinationMipLevel, Region.DestinationArrayLayer,
        Region.DestinationX, Region.DestinationY, Region.DestinationZ,
        Region.Width, Region.Height, Region.Depth, Region.SourceOffsetBytes,
        Region.SourceRowLengthTexels, Region.SourceImageHeightTexels}, Format, OutByteSize);
}
}
