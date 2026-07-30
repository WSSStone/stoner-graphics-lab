#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/FRHIFormatInfo.h"
#include "RHI/FRHITextureDesc.h"

#include <limits>

namespace Stoner::RHI
{

struct FRHITextureUploadDesc
{
    Stoner::Core::uint32 MipLevel = 0;
    Stoner::Core::uint32 ArrayLayer = 0;
    Stoner::Core::uint32 X = 0;
    Stoner::Core::uint32 Y = 0;
    Stoner::Core::uint32 Z = 0;
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
    Stoner::Core::uint32 Depth = 1;
    Stoner::Core::uint64 RowPitchBytes = 0;
    const void* Data = nullptr;
    Stoner::Core::uint64 DataSizeBytes = 0;
};

[[nodiscard]] constexpr bool TryGetRHITextureUploadRequiredBytes(
    const FRHITextureDesc& Texture,
    const FRHITextureUploadDesc& Upload,
    Stoner::Core::uint64& OutRequiredBytes) noexcept
{
    OutRequiredBytes = 0;
    if (!IsValidRHITextureDesc(Texture) ||
        !IsRHITextureRegionValid(
            Texture,
            Upload.MipLevel,
            Upload.ArrayLayer,
            Upload.X,
            Upload.Y,
            Upload.Z,
            Upload.Width,
            Upload.Height,
            Upload.Depth))
    {
        return false;
    }

    const FRHIFormatInfo Info =
        GetRHIFormatInfo(Texture.Format);
    if (!Info.IsValid())
    {
        return false;
    }

    if (Info.bCompressed)
    {
        const Stoner::Core::uint32 MipWidth =
            GetRHIMipExtent(Texture.Width, Upload.MipLevel);
        const Stoner::Core::uint32 MipHeight =
            GetRHIMipExtent(Texture.Height, Upload.MipLevel);
        const Stoner::Core::uint32 MipDepth =
            GetRHIMipExtent(Texture.Depth, Upload.MipLevel);
        if (Upload.X % Info.BlockWidth != 0 ||
            Upload.Y % Info.BlockHeight != 0 ||
            Upload.Z % Info.BlockDepth != 0 ||
            (Upload.Width % Info.BlockWidth != 0 &&
                Upload.X + Upload.Width != MipWidth) ||
            (Upload.Height % Info.BlockHeight != 0 &&
                Upload.Y + Upload.Height != MipHeight) ||
            (Upload.Depth % Info.BlockDepth != 0 &&
                Upload.Z + Upload.Depth != MipDepth))
        {
            return false;
        }
    }

    FRHITextureFootprint Footprint;
    if (!TryGetRHITextureFootprint(
            Texture.Format,
            Upload.Width,
            Upload.Height,
            Upload.Depth,
            Footprint) ||
        Upload.RowPitchBytes < Footprint.TightRowBytes ||
        Upload.RowPitchBytes % Info.BytesPerBlock != 0)
    {
        return false;
    }

    constexpr Stoner::Core::uint64 MaxValue =
        std::numeric_limits<Stoner::Core::uint64>::max();
    if (Footprint.BlockCountY >
            MaxValue / Footprint.BlockCountZ)
    {
        return false;
    }
    const Stoner::Core::uint64 RowCount =
        Footprint.BlockCountY * Footprint.BlockCountZ;
    if (RowCount - 1 >
            (std::numeric_limits<Stoner::Core::uint64>::max() -
                Footprint.TightRowBytes) /
                Upload.RowPitchBytes)
    {
        return false;
    }
    OutRequiredBytes =
        (RowCount - 1) * Upload.RowPitchBytes +
        Footprint.TightRowBytes;
    return true;
}

[[nodiscard]] constexpr bool IsValidRHITextureUploadDesc(
    const FRHITextureDesc& Texture,
    const FRHITextureUploadDesc& Upload) noexcept
{
    Stoner::Core::uint64 RequiredBytes = 0;
    return Upload.Data != nullptr &&
        TryGetRHITextureUploadRequiredBytes(
            Texture, Upload, RequiredBytes) &&
        Upload.DataSizeBytes >= RequiredBytes;
}

} // namespace Stoner::RHI
