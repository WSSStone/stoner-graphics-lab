#pragma once

#include "Core/CoreMinimal.h"
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
    const Stoner::Core::uint64 BytesPerTexel =
        GetRHIFormatByteSize(Texture.Format);
    if (BytesPerTexel == 0 || Upload.Width == 0 ||
        Upload.Height == 0 || Upload.Depth == 0 ||
        Upload.Width >
            std::numeric_limits<Stoner::Core::uint64>::max() /
                BytesPerTexel)
    {
        return false;
    }

    const Stoner::Core::uint64 TightRowBytes =
        static_cast<Stoner::Core::uint64>(Upload.Width) * BytesPerTexel;
    if (Upload.RowPitchBytes < TightRowBytes)
    {
        return false;
    }

    const Stoner::Core::uint64 RowCount =
        static_cast<Stoner::Core::uint64>(Upload.Height) * Upload.Depth;
    if (RowCount == 0 ||
        RowCount - 1 >
            (std::numeric_limits<Stoner::Core::uint64>::max() -
                TightRowBytes) /
                Upload.RowPitchBytes)
    {
        return false;
    }
    OutRequiredBytes =
        (RowCount - 1) * Upload.RowPitchBytes + TightRowBytes;
    return true;
}

[[nodiscard]] constexpr bool IsValidRHITextureUploadDesc(
    const FRHITextureDesc& Texture,
    const FRHITextureUploadDesc& Upload) noexcept
{
    Stoner::Core::uint64 RequiredBytes = 0;
    return IsValidRHITextureDesc(Texture) &&
        Upload.Data != nullptr &&
        IsRHITextureRegionValid(
            Texture,
            Upload.MipLevel,
            Upload.ArrayLayer,
            Upload.X,
            Upload.Y,
            Upload.Z,
            Upload.Width,
            Upload.Height,
            Upload.Depth) &&
        TryGetRHITextureUploadRequiredBytes(
            Texture, Upload, RequiredBytes) &&
        Upload.DataSizeBytes >= RequiredBytes;
}

} // namespace Stoner::RHI
