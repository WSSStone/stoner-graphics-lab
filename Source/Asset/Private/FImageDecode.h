#pragma once

#include "Asset/FImageInspection.h"
#include "Asset/FImageMip.h"

#include <span>

namespace Stoner::Asset::Private
{

struct FDecodedImageRaster
{
    FImageExtent2D Extent;
    Core::uint32 Channels = 0;
    bool IsFloat = false;
    Core::TArray<Core::uint8> LdrBytes;
    Core::TArray<float> HdrValues;
};

[[nodiscard]] EAssetResult DecodeWithStb(
    std::span<const Core::uint8> Bytes,
    EImageSourceFormat Format,
    FDecodedImageRaster& OutRaster,
    Core::FString& OutReason);

[[nodiscard]] EAssetResult DecodeCanonicalImage(
    std::span<const Core::uint8> Bytes,
    const FImageContainerInspection& Inspection,
    const FImageImportSettings& Settings,
    FImageMip& OutMip,
    FAssetDiagnostic* OutDiagnostic = nullptr);

[[nodiscard]] float DecodeHalf(Core::uint16 Value) noexcept;
[[nodiscard]] EAssetResult EncodeHalf(float Value, Core::uint16& OutValue) noexcept;

} // namespace Stoner::Asset::Private
