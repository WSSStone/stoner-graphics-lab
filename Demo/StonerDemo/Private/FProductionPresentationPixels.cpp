#include "FProductionPresentationPixels.h"

#include <algorithm>
#include <limits>

namespace Stoner::Demo
{

bool BuildAspectFitPresentationPixels(
    std::span<const Core::uint8> Source,
    Core::uint32 SourceWidth,
    Core::uint32 SourceHeight,
    Core::uint32 SourceRowPitch,
    Core::uint32 TargetWidth,
    Core::uint32 TargetHeight,
    RHI::ERHIFormat TargetFormat,
    Core::TArray<Core::uint8>& OutNative)
{
    OutNative.clear();
    const bool bBgra = TargetFormat == RHI::ERHIFormat::B8G8R8A8_UNorm;
    const bool bRgba = TargetFormat == RHI::ERHIFormat::R8G8B8A8_UNorm ||
        TargetFormat == RHI::ERHIFormat::R8G8B8A8_sRGB;
    const Core::uint64 TargetBytes =
        static_cast<Core::uint64>(TargetWidth) * TargetHeight * 4u;
    if ((!bBgra && !bRgba) || SourceWidth == 0 || SourceHeight == 0 ||
        TargetWidth == 0 || TargetHeight == 0 ||
        SourceRowPitch < SourceWidth * 4u ||
        Source.size() < static_cast<Core::usize>(SourceRowPitch) * SourceHeight ||
        TargetBytes > std::numeric_limits<Core::usize>::max())
        return false;

    Core::uint32 FitWidth = TargetWidth;
    Core::uint32 FitHeight = TargetHeight;
    if (static_cast<Core::uint64>(SourceWidth) * TargetHeight >
        static_cast<Core::uint64>(TargetWidth) * SourceHeight)
        FitHeight = std::max<Core::uint32>(1,
            static_cast<Core::uint32>(
                static_cast<Core::uint64>(TargetWidth) * SourceHeight /
                SourceWidth));
    else
        FitWidth = std::max<Core::uint32>(1,
            static_cast<Core::uint32>(
                static_cast<Core::uint64>(TargetHeight) * SourceWidth /
                SourceHeight));
    const Core::uint32 OffsetX = (TargetWidth - FitWidth) / 2u;
    const Core::uint32 OffsetY = (TargetHeight - FitHeight) / 2u;

    OutNative.resize(static_cast<Core::usize>(TargetBytes), 0u);
    for (Core::usize Offset = 3; Offset < OutNative.size(); Offset += 4u)
        OutNative[Offset] = 255u;
    for (Core::uint32 Y = 0; Y < FitHeight; ++Y)
    {
        const Core::uint32 SourceY = static_cast<Core::uint32>(
            static_cast<Core::uint64>(Y) * SourceHeight / FitHeight);
        for (Core::uint32 X = 0; X < FitWidth; ++X)
        {
            const Core::uint32 SourceX = static_cast<Core::uint32>(
                static_cast<Core::uint64>(X) * SourceWidth / FitWidth);
            const Core::usize SourceOffset =
                static_cast<Core::usize>(SourceY) * SourceRowPitch +
                static_cast<Core::usize>(SourceX) * 4u;
            const Core::usize TargetOffset =
                (static_cast<Core::usize>(OffsetY + Y) * TargetWidth +
                    OffsetX + X) * 4u;
            OutNative[TargetOffset] =
                Source[SourceOffset + (bBgra ? 2u : 0u)];
            OutNative[TargetOffset + 1u] = Source[SourceOffset + 1u];
            OutNative[TargetOffset + 2u] =
                Source[SourceOffset + (bBgra ? 0u : 2u)];
            OutNative[TargetOffset + 3u] = Source[SourceOffset + 3u];
        }
    }
    return true;
}

} // namespace Stoner::Demo
