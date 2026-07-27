#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"

#include <limits>

namespace Stoner::RHI
{

struct FRHITextureBufferCopyRegion
{
    Stoner::Core::uint32 SourceMipLevel = 0;
    Stoner::Core::uint32 SourceArrayLayer = 0;
    Stoner::Core::uint32 SourceX = 0;
    Stoner::Core::uint32 SourceY = 0;
    Stoner::Core::uint32 SourceZ = 0;
    Stoner::Core::uint32 Width = 1;
    Stoner::Core::uint32 Height = 1;
    Stoner::Core::uint32 Depth = 1;
    Stoner::Core::uint64 DestinationOffsetBytes = 0;
    Stoner::Core::uint32 DestinationRowLengthTexels = 0;
    Stoner::Core::uint32 DestinationImageHeightTexels = 0;
};

[[nodiscard]] constexpr bool TryGetRHITextureBufferCopyByteSize(
    const FRHITextureBufferCopyRegion& Region,
    ERHIFormat Format,
    Stoner::Core::uint64& OutByteSize) noexcept
{
    OutByteSize = 0;
    if (Region.Width == 0 || Region.Height == 0 || Region.Depth == 0)
    {
        return false;
    }

    const Stoner::Core::uint64 RowTexels =
        Region.DestinationRowLengthTexels == 0
        ? Region.Width
        : Region.DestinationRowLengthTexels;
    const Stoner::Core::uint64 ImageRows =
        Region.DestinationImageHeightTexels == 0
        ? Region.Height
        : Region.DestinationImageHeightTexels;
    const Stoner::Core::uint64 FormatBytes =
        GetRHIFormatByteSize(Format);
    if (RowTexels < Region.Width || ImageRows < Region.Height ||
        FormatBytes == 0)
    {
        return false;
    }

    constexpr Stoner::Core::uint64 MaxValue =
        std::numeric_limits<Stoner::Core::uint64>::max();
    const auto TryMultiply = [](Stoner::Core::uint64 Left,
                                 Stoner::Core::uint64 Right,
                                 Stoner::Core::uint64& OutValue) constexpr
    {
        if (Left != 0 && Right > MaxValue / Left)
        {
            return false;
        }
        OutValue = Left * Right;
        return true;
    };
    const auto TryAdd = [](Stoner::Core::uint64 Left,
                            Stoner::Core::uint64 Right,
                            Stoner::Core::uint64& OutValue) constexpr
    {
        if (Right > MaxValue - Left)
        {
            return false;
        }
        OutValue = Left + Right;
        return true;
    };

    Stoner::Core::uint64 SliceStride = 0;
    Stoner::Core::uint64 LastSliceOffset = 0;
    Stoner::Core::uint64 LastRowOffset = 0;
    Stoner::Core::uint64 LastTexelEnd = 0;
    return TryMultiply(RowTexels, ImageRows, SliceStride) &&
        TryMultiply(SliceStride, Region.Depth - 1, LastSliceOffset) &&
        TryMultiply(RowTexels, Region.Height - 1, LastRowOffset) &&
        TryAdd(LastSliceOffset, LastRowOffset, LastTexelEnd) &&
        TryAdd(LastTexelEnd, Region.Width, LastTexelEnd) &&
        TryMultiply(LastTexelEnd, FormatBytes, OutByteSize);
}

} // namespace Stoner::RHI
