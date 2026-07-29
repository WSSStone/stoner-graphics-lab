#pragma once

#include "Asset/FAssetDiagnostics.h"
#include "Asset/FImageTypes.h"

namespace Stoner::Asset
{

enum class EImageOrientationTransform : Core::uint8
{
    Identity,
    FlipHorizontal,
    Rotate180,
    FlipVertical,
    Transpose,
    Rotate90Clockwise,
    Transverse,
    Rotate90CounterClockwise
};

struct FImageContainerInspection
{
    EImageSourceFormat SourceFormat = EImageSourceFormat::Unknown;
    FImageExtent2D SourceExtent;
    Core::uint32 SourceChannels = 0;
    Core::uint32 SourceBitsPerChannel = 0;
    std::optional<EImageColorSpace> DeclaredColorSpace;
    EImageAlphaMode AlphaMode = EImageAlphaMode::None;
    EImageOrientationTransform Orientation =
        EImageOrientationTransform::Identity;
    bool OrientationMetadataPresent = false;

    [[nodiscard]] EAssetResult Validate(
        const FImageImportLimits& Limits) const noexcept;
};

class FImageInspection
{
public:
    [[nodiscard]] static Core::FString Format(
        const FImageContainerInspection& Inspection);
};

} // namespace Stoner::Asset
