#include "FImageOrientation.h"

namespace Stoner::Asset::Private
{
namespace
{

template <typename T>
void TransformPixels(
    EImageOrientationTransform Transform,
    Core::uint32 Channels,
    FImageExtent2D SourceExtent,
    Core::TArray<T>& Pixels)
{
    if (Transform == EImageOrientationTransform::Identity)
    {
        return;
    }
    const bool SwapsAxes =
        Transform == EImageOrientationTransform::Transpose ||
        Transform == EImageOrientationTransform::Rotate90Clockwise ||
        Transform == EImageOrientationTransform::Transverse ||
        Transform == EImageOrientationTransform::Rotate90CounterClockwise;
    const FImageExtent2D OutputExtent = SwapsAxes
        ? FImageExtent2D{SourceExtent.Height, SourceExtent.Width}
        : SourceExtent;
    Core::TArray<T> Output(Pixels.size());
    for (Core::uint32 Y = 0; Y < OutputExtent.Height; ++Y)
    {
        for (Core::uint32 X = 0; X < OutputExtent.Width; ++X)
        {
            Core::uint32 SourceX = X;
            Core::uint32 SourceY = Y;
            switch (Transform)
            {
            case EImageOrientationTransform::Identity: break;
            case EImageOrientationTransform::FlipHorizontal:
                SourceX = SourceExtent.Width - 1U - X;
                break;
            case EImageOrientationTransform::Rotate180:
                SourceX = SourceExtent.Width - 1U - X;
                SourceY = SourceExtent.Height - 1U - Y;
                break;
            case EImageOrientationTransform::FlipVertical:
                SourceY = SourceExtent.Height - 1U - Y;
                break;
            case EImageOrientationTransform::Transpose:
                SourceX = Y;
                SourceY = X;
                break;
            case EImageOrientationTransform::Rotate90Clockwise:
                SourceX = Y;
                SourceY = SourceExtent.Height - 1U - X;
                break;
            case EImageOrientationTransform::Transverse:
                SourceX = SourceExtent.Width - 1U - Y;
                SourceY = SourceExtent.Height - 1U - X;
                break;
            case EImageOrientationTransform::Rotate90CounterClockwise:
                SourceX = SourceExtent.Width - 1U - Y;
                SourceY = X;
                break;
            }
            const Core::usize SourceBase =
                (static_cast<Core::usize>(SourceY) * SourceExtent.Width +
                 SourceX) * Channels;
            const Core::usize OutputBase =
                (static_cast<Core::usize>(Y) * OutputExtent.Width + X) *
                Channels;
            for (Core::uint32 Channel = 0; Channel < Channels; ++Channel)
            {
                Output[OutputBase + Channel] = Pixels[SourceBase + Channel];
            }
        }
    }
    Pixels = std::move(Output);
}

} // namespace

EAssetResult NormalizeTopLeft(
    EImageOrientationTransform Transform,
    FDecodedImageRaster& Raster)
{
    if (!Raster.Extent.IsValid() || Raster.Channels == 0)
    {
        return EAssetResult::InvalidInput;
    }
    const bool SwapsAxes =
        Transform == EImageOrientationTransform::Transpose ||
        Transform == EImageOrientationTransform::Rotate90Clockwise ||
        Transform == EImageOrientationTransform::Transverse ||
        Transform == EImageOrientationTransform::Rotate90CounterClockwise;
    if (Raster.IsFloat)
    {
        TransformPixels(
            Transform,
            Raster.Channels,
            Raster.Extent,
            Raster.HdrValues);
    }
    else
    {
        TransformPixels(
            Transform,
            Raster.Channels,
            Raster.Extent,
            Raster.LdrBytes);
    }
    if (SwapsAxes)
    {
        std::swap(Raster.Extent.Width, Raster.Extent.Height);
    }
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
