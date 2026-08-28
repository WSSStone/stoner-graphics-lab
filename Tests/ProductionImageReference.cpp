#include "ProductionImageReference.h"

#include "Asset/EAssetResult.h"
#include "Asset/FImageTypes.h"
#include "FImageDecode.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <span>
#include <string>

namespace
{
using namespace Stoner;

constexpr Core::uint64 MaximumReferencePixels = 16384ull * 16384ull;

bool NormalizeRgb(
    const Core::TArray<Core::uint8>& Source,
    Core::uint32 Width,
    Core::uint32 Height,
    Core::uint32 Channels,
    EProductionColorTransfer Transfer,
    FProductionCanonicalImage& OutImage,
    Core::FString& OutFailure)
{
    const Core::uint64 PixelCount =
        static_cast<Core::uint64>(Width) * Height;
    if (Width == 0 || Height == 0 || PixelCount > MaximumReferencePixels ||
        (Channels != 3 && Channels != 4) ||
        Source.size() != static_cast<Core::usize>(PixelCount * Channels))
    {
        OutFailure = "reference-image-bounds";
        return false;
    }
    Core::TArray<Core::uint8> Rgba(
        static_cast<Core::usize>(PixelCount * 4u), 255u);
    for (Core::usize Pixel = 0; Pixel < PixelCount; ++Pixel)
        std::copy_n(Source.data() + Pixel * Channels, 3u,
            Rgba.data() + Pixel * 4u);
    return NormalizeProductionReadback(
        {Rgba, Width, Height, Width * 4u,
         EProductionReadbackPixelFormat::RGBA8UNorm,
         EProductionImageOrigin::TopLeft, Transfer},
        OutImage, OutFailure);
}

bool LoadPpm(
    const std::filesystem::path& Path,
    EProductionColorTransfer Transfer,
    FProductionCanonicalImage& OutImage,
    Core::FString& OutFailure)
{
    std::ifstream Input(Path, std::ios::binary);
    std::string Magic;
    Core::uint32 Width = 0;
    Core::uint32 Height = 0;
    Core::uint32 Maximum = 0;
    if (!Input || !(Input >> Magic >> Width >> Height >> Maximum) ||
        Magic != "P6" || Width == 0 || Height == 0 || Maximum != 255 ||
        Input.get() != '\n')
    {
        OutFailure = "reference-image-contract";
        return false;
    }
    const Core::uint64 PixelCount =
        static_cast<Core::uint64>(Width) * Height;
    if (PixelCount > MaximumReferencePixels)
    {
        OutFailure = "reference-image-bounds";
        return false;
    }
    Core::TArray<Core::uint8> Rgb(
        static_cast<Core::usize>(PixelCount * 3u));
    Input.read(reinterpret_cast<char*>(Rgb.data()),
        static_cast<std::streamsize>(Rgb.size()));
    if (!Input || Input.peek() != std::char_traits<char>::eof())
    {
        OutFailure = "reference-image-size";
        return false;
    }
    return NormalizeRgb(
        Rgb, Width, Height, 3u, Transfer, OutImage, OutFailure);
}

bool LoadPng(
    const std::filesystem::path& Path,
    EProductionColorTransfer Transfer,
    FProductionCanonicalImage& OutImage,
    Core::FString& OutFailure)
{
    std::ifstream Input(Path, std::ios::binary);
    if (!Input)
    {
        OutFailure = "reference-image-contract";
        return false;
    }
    Core::TArray<Core::uint8> Bytes{
        std::istreambuf_iterator<char>(Input), {}};
    Asset::Private::FDecodedImageRaster Raster;
    if ((!Input.good() && !Input.eof()) ||
        Asset::Private::DecodeWithStb(
            std::span<const Core::uint8>(Bytes.data(), Bytes.size()),
            Asset::EImageSourceFormat::PNG,
            Raster,
            OutFailure) != Asset::EAssetResult::Success ||
        Raster.IsFloat)
    {
        OutFailure = "reference-image-contract";
        return false;
    }
    return NormalizeRgb(
        Raster.LdrBytes, Raster.Extent.Width, Raster.Extent.Height,
        Raster.Channels, Transfer, OutImage, OutFailure);
}
} // namespace

bool LoadProductionReferenceImage(
    const std::filesystem::path& Path,
    EProductionColorTransfer Transfer,
    FProductionCanonicalImage& OutImage,
    Stoner::Core::FString& OutFailure)
{
    OutImage = {};
    OutFailure.Clear();
    if (Path.extension() == ".png")
        return LoadPng(Path, Transfer, OutImage, OutFailure);
    if (Path.extension() == ".ppm")
        return LoadPpm(Path, Transfer, OutImage, OutFailure);
    OutFailure = "reference-image-format";
    return false;
}
