#include "FImageDecode.h"

#include "FImageOrientation.h"
#include "FImageValidation.h"

#include <bit>
#include <cmath>
#include <limits>

namespace Stoner::Asset::Private
{
namespace
{

void SetDecodeDiagnostic(
    FAssetDiagnostic* Diagnostic,
    EAssetResult Result,
    const char* Code,
    const Core::FString& Reason)
{
    if (!Diagnostic)
    {
        return;
    }
    *Diagnostic = {};
    Diagnostic->Stage = EAssetStage::Decode;
    Diagnostic->Result = Result;
    Diagnostic->Severity = EAssetDiagnosticSeverity::Error;
    Diagnostic->Code = Core::FString(Code);
    Diagnostic->Reason = Reason;
}

void AppendU16(Core::TArray<Core::uint8>& Bytes, Core::uint16 Value)
{
    Bytes.push_back(static_cast<Core::uint8>(Value));
    Bytes.push_back(static_cast<Core::uint8>(Value >> 8U));
}

void AppendFloat(Core::TArray<Core::uint8>& Bytes, float Value)
{
    const Core::uint32 Bits = std::bit_cast<Core::uint32>(Value);
    for (Core::uint32 Shift = 0; Shift < 32; Shift += 8)
    {
        Bytes.push_back(static_cast<Core::uint8>(Bits >> Shift));
    }
}

} // namespace

float DecodeHalf(Core::uint16 Value) noexcept
{
    const Core::uint32 Sign =
        static_cast<Core::uint32>(Value & 0x8000U) << 16U;
    Core::uint32 Exponent = (Value >> 10U) & 0x1fU;
    Core::uint32 Mantissa = Value & 0x03ffU;
    Core::uint32 Bits = 0;
    if (Exponent == 0)
    {
        if (Mantissa == 0)
        {
            Bits = Sign;
        }
        else
        {
            Exponent = 127U - 15U + 1U;
            while ((Mantissa & 0x0400U) == 0)
            {
                Mantissa <<= 1U;
                --Exponent;
            }
            Mantissa &= 0x03ffU;
            Bits = Sign | (Exponent << 23U) | (Mantissa << 13U);
        }
    }
    else if (Exponent == 31)
    {
        Bits = Sign | 0x7f800000U | (Mantissa << 13U);
    }
    else
    {
        Bits = Sign |
            ((Exponent + (127U - 15U)) << 23U) |
            (Mantissa << 13U);
    }
    return std::bit_cast<float>(Bits);
}

EAssetResult EncodeHalf(float Value, Core::uint16& OutValue) noexcept
{
    OutValue = 0;
    if (!std::isfinite(Value))
    {
        return EAssetResult::NonFiniteImageData;
    }
    if (std::abs(Value) > 65504.0f)
    {
        return EAssetResult::HDRPrecisionRangeExceeded;
    }
    const Core::uint32 Bits = std::bit_cast<Core::uint32>(Value);
    const Core::uint32 Sign = (Bits >> 16U) & 0x8000U;
    const int Exponent = static_cast<int>((Bits >> 23U) & 0xffU) - 127 + 15;
    Core::uint32 Mantissa = Bits & 0x7fffffU;
    if (Exponent <= 0)
    {
        if (Exponent < -10)
        {
            OutValue = static_cast<Core::uint16>(Sign);
            return EAssetResult::Success;
        }
        Mantissa |= 0x800000U;
        const int Shift = 14 - Exponent;
        Core::uint32 Rounded = Mantissa >> Shift;
        const Core::uint32 Remainder = Mantissa & ((1U << Shift) - 1U);
        const Core::uint32 Halfway = 1U << (Shift - 1);
        if (Remainder > Halfway ||
            (Remainder == Halfway && (Rounded & 1U) != 0))
        {
            ++Rounded;
        }
        OutValue = static_cast<Core::uint16>(Sign | Rounded);
        return EAssetResult::Success;
    }
    Core::uint32 RoundedMantissa = Mantissa >> 13U;
    const Core::uint32 Remainder = Mantissa & 0x1fffU;
    if (Remainder > 0x1000U ||
        (Remainder == 0x1000U && (RoundedMantissa & 1U) != 0))
    {
        ++RoundedMantissa;
    }
    int RoundedExponent = Exponent;
    if (RoundedMantissa == 0x400U)
    {
        RoundedMantissa = 0;
        ++RoundedExponent;
    }
    if (RoundedExponent >= 31)
    {
        return EAssetResult::HDRPrecisionRangeExceeded;
    }
    OutValue = static_cast<Core::uint16>(
        Sign |
        static_cast<Core::uint32>(RoundedExponent << 10U) |
        RoundedMantissa);
    return EAssetResult::Success;
}

EAssetResult DecodeCanonicalImage(
    std::span<const Core::uint8> Bytes,
    const FImageContainerInspection& Inspection,
    const FImageImportSettings& Settings,
    FImageMip& OutMip,
    FAssetDiagnostic* OutDiagnostic)
{
    OutMip = {};
    FDecodedImageRaster Raster;
    Core::FString Reason;
    EAssetResult Result = DecodeWithStb(
        Bytes,
        Inspection.SourceFormat,
        Raster,
        Reason);
    if (Result != EAssetResult::Success)
    {
        SetDecodeDiagnostic(
            OutDiagnostic,
            Result,
            "image.decode.failed",
            Reason);
        return Result;
    }
    if (Raster.Extent != Inspection.SourceExtent ||
        Raster.Channels == 0 ||
        Raster.Channels > 4)
    {
        return EAssetResult::MalformedSource;
    }
    Result = NormalizeTopLeft(Inspection.Orientation, Raster);
    if (Result != EAssetResult::Success)
    {
        return Result;
    }

    EImageTexelFormat Target = EImageTexelFormat::Unknown;
    Core::TArray<Core::uint8> Canonical;
    if (!Raster.IsFloat)
    {
        switch (Raster.Channels)
        {
        case 1: Target = EImageTexelFormat::R8_UNorm; break;
        case 2: Target = EImageTexelFormat::R8G8_UNorm; break;
        case 3: Target = EImageTexelFormat::R8G8B8_UNorm; break;
        case 4: Target = EImageTexelFormat::R8G8B8A8_UNorm; break;
        default: return EAssetResult::Unsupported;
        }
        Canonical = std::move(Raster.LdrBytes);
    }
    else
    {
        const Core::uint32 OutputChannels =
            Settings.HDRLayout == EHDRLayout::RGB32F ? 3U : 4U;
        Target = Settings.HDRLayout == EHDRLayout::DefaultRGBA16F
            ? EImageTexelFormat::R16G16B16A16_Float
            : Settings.HDRLayout == EHDRLayout::RGBA32F
                ? EImageTexelFormat::R32G32B32A32_Float
                : EImageTexelFormat::R32G32B32_Float;
        Core::uint64 OutputBytes = 0;
        if (!CheckedMultiply(
                static_cast<Core::uint64>(Raster.Extent.Width) *
                    Raster.Extent.Height,
                GetImageBytesPerTexel(Target),
                OutputBytes) ||
            OutputBytes > Settings.Limits.MaxMipBytes)
        {
            return EAssetResult::ImageLimitExceeded;
        }
        Canonical.reserve(static_cast<Core::usize>(OutputBytes));
        const Core::usize PixelCount =
            static_cast<Core::usize>(Raster.Extent.Width) *
            Raster.Extent.Height;
        for (Core::usize Pixel = 0; Pixel < PixelCount; ++Pixel)
        {
            for (Core::uint32 Channel = 0; Channel < OutputChannels; ++Channel)
            {
                const float Value = Channel < Raster.Channels
                    ? Raster.HdrValues[Pixel * Raster.Channels + Channel]
                    : 1.0f;
                if (!std::isfinite(Value))
                {
                    return EAssetResult::NonFiniteImageData;
                }
                if (Target == EImageTexelFormat::R16G16B16A16_Float)
                {
                    Core::uint16 Half = 0;
                    Result = EncodeHalf(Value, Half);
                    if (Result != EAssetResult::Success)
                    {
                        return Result;
                    }
                    AppendU16(Canonical, Half);
                }
                else
                {
                    AppendFloat(Canonical, Value);
                }
            }
        }
    }

    Result = FImageMip::Create(
        Raster.Extent,
        Target,
        std::move(Canonical),
        OutMip);
    if (Result != EAssetResult::Success)
    {
        return Result;
    }
    return ValidateMip(OutMip, Settings.Limits);
}

} // namespace Stoner::Asset::Private
