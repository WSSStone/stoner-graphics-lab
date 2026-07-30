#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/FRHIFormatInfo.h"

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
    const FRHIFormatInfo Info = GetRHIFormatInfo(Format);
    if (!Info.IsValid() ||
        RowTexels < Region.Width ||
        ImageRows < Region.Height ||
        (Info.bCompressed &&
            ((Region.DestinationRowLengthTexels != 0 &&
                RowTexels % Info.BlockWidth != 0) ||
             (Region.DestinationImageHeightTexels != 0 &&
                ImageRows % Info.BlockHeight != 0))))
    {
        return false;
    }

    FRHITextureFootprint RegionFootprint;
    FRHITextureFootprint DestinationFootprint;
    if (!TryGetRHITextureFootprint(
            Format,
            Region.Width,
            Region.Height,
            Region.Depth,
            RegionFootprint) ||
        !TryGetRHITextureFootprint(
            Format,
            static_cast<Stoner::Core::uint32>(RowTexels),
            static_cast<Stoner::Core::uint32>(ImageRows),
            1,
            DestinationFootprint))
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

    Stoner::Core::uint64 SliceStrideBlocks = 0;
    Stoner::Core::uint64 LastSliceOffset = 0;
    Stoner::Core::uint64 LastRowOffset = 0;
    Stoner::Core::uint64 LastBlockEnd = 0;
    return TryMultiply(
            DestinationFootprint.BlockCountX,
            DestinationFootprint.BlockCountY,
            SliceStrideBlocks) &&
        TryMultiply(
            SliceStrideBlocks,
            RegionFootprint.BlockCountZ - 1,
            LastSliceOffset) &&
        TryMultiply(
            DestinationFootprint.BlockCountX,
            RegionFootprint.BlockCountY - 1,
            LastRowOffset) &&
        TryAdd(
            LastSliceOffset,
            LastRowOffset,
            LastBlockEnd) &&
        TryAdd(
            LastBlockEnd,
            RegionFootprint.BlockCountX,
            LastBlockEnd) &&
        TryMultiply(
            LastBlockEnd,
            Info.BytesPerBlock,
            OutByteSize);
}

} // namespace Stoner::RHI
