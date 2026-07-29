#include "FImageDecode.h"

#define STBI_NO_STDIO
#define STBI_NO_SIMD
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_HDR
#define STB_IMAGE_IMPLEMENTATION
#include "../../../ThirdParty/stb/stb_image.h"

#include <limits>

namespace Stoner::Asset::Private
{

EAssetResult DecodeWithStb(
    std::span<const Core::uint8> Bytes,
    EImageSourceFormat Format,
    FDecodedImageRaster& OutRaster,
    Core::FString& OutReason)
{
    OutRaster = {};
    OutReason.Clear();
    if (Bytes.empty() ||
        Bytes.size() > static_cast<Core::usize>(std::numeric_limits<int>::max()))
    {
        return EAssetResult::ImageLimitExceeded;
    }
    int Width = 0;
    int Height = 0;
    int Channels = 0;
    const int ByteCount = static_cast<int>(Bytes.size());
    if (Format == EImageSourceFormat::RadianceHDR)
    {
        float* Values = stbi_loadf_from_memory(
            Bytes.data(),
            ByteCount,
            &Width,
            &Height,
            &Channels,
            0);
        if (!Values)
        {
            OutReason = Core::FString(
                stbi_failure_reason() ? stbi_failure_reason()
                                      : "stb HDR decode failed");
            return EAssetResult::ProcessingFailure;
        }
        const Core::usize Count =
            static_cast<Core::usize>(Width) *
            static_cast<Core::usize>(Height) *
            static_cast<Core::usize>(Channels);
        OutRaster.Extent = {
            static_cast<Core::uint32>(Width),
            static_cast<Core::uint32>(Height)};
        OutRaster.Channels = static_cast<Core::uint32>(Channels);
        OutRaster.IsFloat = true;
        OutRaster.HdrValues.assign(Values, Values + Count);
        stbi_image_free(Values);
        return EAssetResult::Success;
    }

    if (stbi_is_16_bit_from_memory(Bytes.data(), ByteCount))
    {
        stbi_us* Values = stbi_load_16_from_memory(
            Bytes.data(),
            ByteCount,
            &Width,
            &Height,
            &Channels,
            0);
        if (!Values)
        {
            OutReason = Core::FString(
                stbi_failure_reason() ? stbi_failure_reason()
                                      : "stb 16-bit decode failed");
            return EAssetResult::ProcessingFailure;
        }
        const Core::usize Count =
            static_cast<Core::usize>(Width) *
            static_cast<Core::usize>(Height) *
            static_cast<Core::usize>(Channels);
        OutRaster.LdrBytes.resize(Count);
        for (Core::usize Index = 0; Index < Count; ++Index)
        {
            OutRaster.LdrBytes[Index] =
                static_cast<Core::uint8>((Values[Index] + 128U) / 257U);
        }
        stbi_image_free(Values);
    }
    else
    {
        stbi_uc* Values = stbi_load_from_memory(
            Bytes.data(),
            ByteCount,
            &Width,
            &Height,
            &Channels,
            0);
        if (!Values)
        {
            OutReason = Core::FString(
                stbi_failure_reason() ? stbi_failure_reason()
                                      : "stb decode failed");
            return EAssetResult::ProcessingFailure;
        }
        const Core::usize Count =
            static_cast<Core::usize>(Width) *
            static_cast<Core::usize>(Height) *
            static_cast<Core::usize>(Channels);
        OutRaster.LdrBytes.assign(Values, Values + Count);
        stbi_image_free(Values);
    }
    OutRaster.Extent = {
        static_cast<Core::uint32>(Width),
        static_cast<Core::uint32>(Height)};
    OutRaster.Channels = static_cast<Core::uint32>(Channels);
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
