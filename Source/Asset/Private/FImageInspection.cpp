#include "Asset/FImageInspection.h"

#include <string>

namespace Stoner::Asset
{

EAssetResult FImageContainerInspection::Validate(
    const FImageImportLimits& Limits) const noexcept
{
    if (SourceFormat == EImageSourceFormat::Unknown ||
        !SourceExtent.IsValid() ||
        SourceExtent.Width > Limits.MaxDimension ||
        SourceExtent.Height > Limits.MaxDimension ||
        SourceChannels == 0 ||
        SourceChannels > 4 ||
        SourceBitsPerChannel == 0)
    {
        return SourceExtent.Width > Limits.MaxDimension ||
                SourceExtent.Height > Limits.MaxDimension
            ? EAssetResult::ImageLimitExceeded
            : EAssetResult::MalformedSource;
    }
    return EAssetResult::Success;
}

Core::FString FImageInspection::Format(
    const FImageContainerInspection& Inspection)
{
    std::string Text =
        "format=" + std::to_string(static_cast<int>(Inspection.SourceFormat)) +
        "|extent=" + std::to_string(Inspection.SourceExtent.Width) + "x" +
        std::to_string(Inspection.SourceExtent.Height) +
        "|channels=" + std::to_string(Inspection.SourceChannels) +
        "|bits=" + std::to_string(Inspection.SourceBitsPerChannel) +
        "|alpha=" + std::to_string(static_cast<int>(Inspection.AlphaMode)) +
        "|orientation=" +
        std::to_string(static_cast<int>(Inspection.Orientation)) +
        "|orientation-metadata=" +
        (Inspection.OrientationMetadataPresent ? "yes" : "no");
    if (Inspection.DeclaredColorSpace)
    {
        Text += "|color=" +
            std::to_string(static_cast<int>(*Inspection.DeclaredColorSpace));
    }
    return Core::FString(std::move(Text));
}

} // namespace Stoner::Asset
