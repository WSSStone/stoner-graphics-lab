#include "FImageMipGenerator.h"

#include "FImageDecode.h"
#include "FImageValidation.h"
#include "FSrgbTransferTable.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <string>

namespace Stoner::Asset::Private
{
namespace
{

Core::uint16 ReadU16(std::span<const Core::uint8> Bytes, Core::usize Offset)
{
    return static_cast<Core::uint16>(
        Bytes[Offset] |
        static_cast<Core::uint16>(Bytes[Offset + 1]) << 8U);
}

float ReadFloat(std::span<const Core::uint8> Bytes, Core::usize Offset)
{
    const Core::uint32 Bits =
        static_cast<Core::uint32>(Bytes[Offset]) |
        static_cast<Core::uint32>(Bytes[Offset + 1]) << 8U |
        static_cast<Core::uint32>(Bytes[Offset + 2]) << 16U |
        static_cast<Core::uint32>(Bytes[Offset + 3]) << 24U;
    return std::bit_cast<float>(Bits);
}

void WriteU16(Core::TArray<Core::uint8>& Bytes, Core::uint16 Value)
{
    Bytes.push_back(static_cast<Core::uint8>(Value));
    Bytes.push_back(static_cast<Core::uint8>(Value >> 8U));
}

void WriteFloat(Core::TArray<Core::uint8>& Bytes, float Value)
{
    const Core::uint32 Bits = std::bit_cast<Core::uint32>(Value);
    for (Core::uint32 Shift = 0; Shift < 32; Shift += 8)
    {
        Bytes.push_back(static_cast<Core::uint8>(Bits >> Shift));
    }
}

Core::uint8 LinearAverageToSrgb(
    Core::uint64 LinearSum,
    Core::uint32 Count)
{
    const auto FirstAtOrAbove = std::lower_bound(
        SrgbToLinearQ24.begin(),
        SrgbToLinearQ24.end(),
        LinearSum,
        [Count](Core::uint32 Candidate, Core::uint64 Sum)
        {
            return static_cast<Core::uint64>(Candidate) * Count <
                Sum;
        });
    if (FirstAtOrAbove == SrgbToLinearQ24.begin())
    {
        return 0;
    }
    if (FirstAtOrAbove == SrgbToLinearQ24.end())
    {
        return 255;
    }
    const Core::uint64 Upper =
        static_cast<Core::uint64>(*FirstAtOrAbove) * Count;
    const Core::uint64 Lower =
        static_cast<Core::uint64>(*(FirstAtOrAbove - 1)) * Count;
    return static_cast<Core::uint8>(
        Upper - LinearSum < LinearSum - Lower
            ? FirstAtOrAbove - SrgbToLinearQ24.begin()
            : FirstAtOrAbove - SrgbToLinearQ24.begin() - 1);
}

void SourceBounds(
    Core::uint32 DestinationCoordinate,
    Core::uint32 DestinationSize,
    Core::uint32 SourceSize,
    Core::uint32& OutBegin,
    Core::uint32& OutEnd)
{
    OutBegin = static_cast<Core::uint32>(
        static_cast<Core::uint64>(DestinationCoordinate) *
        SourceSize / DestinationSize);
    OutEnd = static_cast<Core::uint32>(
        (static_cast<Core::uint64>(DestinationCoordinate + 1U) *
         SourceSize + DestinationSize - 1U) / DestinationSize);
    OutEnd = std::max(OutBegin + 1U, std::min(OutEnd, SourceSize));
}

EAssetResult GenerateLdrLevel(
    const FImageMip& Source,
    FImageExtent2D DestinationExtent,
    const FImageImportSettings& Settings,
    Core::TArray<Core::uint8>& OutBytes)
{
    const auto SourceBytes = Source.GetBytes();
    const FImageExtent2D SourceExtent = Source.GetExtent();
    const Core::uint32 Channels = GetImageChannelCount(Source.GetFormat());
    OutBytes.clear();
    OutBytes.reserve(
        static_cast<Core::usize>(DestinationExtent.Width) *
        DestinationExtent.Height * Channels);
    for (Core::uint32 Y = 0; Y < DestinationExtent.Height; ++Y)
    {
        Core::uint32 BeginY = 0;
        Core::uint32 EndY = 0;
        SourceBounds(
            Y,
            DestinationExtent.Height,
            SourceExtent.Height,
            BeginY,
            EndY);
        for (Core::uint32 X = 0; X < DestinationExtent.Width; ++X)
        {
            Core::uint32 BeginX = 0;
            Core::uint32 EndX = 0;
            SourceBounds(
                X,
                DestinationExtent.Width,
                SourceExtent.Width,
                BeginX,
                EndX);
            const Core::uint32 Count =
                (EndX - BeginX) * (EndY - BeginY);
            if (Settings.Semantic == ETextureSemantic::Normal)
            {
                double SumX = 0.0;
                double SumY = 0.0;
                double SumZ = 0.0;
                double SumAlpha = 0.0;
                for (Core::uint32 SourceY = BeginY; SourceY < EndY; ++SourceY)
                {
                    for (Core::uint32 SourceX = BeginX; SourceX < EndX; ++SourceX)
                    {
                        const Core::usize Base =
                            (static_cast<Core::usize>(SourceY) *
                             SourceExtent.Width + SourceX) * Channels;
                        const double Nx =
                            static_cast<double>(SourceBytes[Base]) /
                                127.5 - 1.0;
                        const double Ny =
                            static_cast<double>(SourceBytes[Base + 1]) /
                                127.5 - 1.0;
                        const double Nz = Channels >= 3
                            ? static_cast<double>(SourceBytes[Base + 2]) /
                                  127.5 - 1.0
                            : std::sqrt(std::max(
                                  0.0,
                                  1.0 - Nx * Nx - Ny * Ny));
                        SumX += Nx;
                        SumY += Ny;
                        SumZ += Nz;
                        if (Channels == 4)
                        {
                            SumAlpha += SourceBytes[Base + 3];
                        }
                    }
                }
                const double Length = std::sqrt(
                    SumX * SumX + SumY * SumY + SumZ * SumZ);
                const double Nx = Length > 1e-20 ? SumX / Length : 0.0;
                const double Ny = Length > 1e-20 ? SumY / Length : 0.0;
                const double Nz = Length > 1e-20 ? SumZ / Length : 1.0;
                const auto Encode = [](double Value)
                {
                    return static_cast<Core::uint8>(std::floor(
                        std::max(0.0, std::min(1.0, Value * 0.5 + 0.5)) *
                            255.0 +
                        0.5));
                };
                OutBytes.push_back(Encode(Nx));
                OutBytes.push_back(Encode(Ny));
                if (Channels >= 3)
                {
                    OutBytes.push_back(Encode(Nz));
                }
                if (Channels == 4)
                {
                    OutBytes.push_back(static_cast<Core::uint8>(
                        (static_cast<Core::uint64>(SumAlpha) + Count / 2U) /
                        Count));
                }
                continue;
            }

            for (Core::uint32 Channel = 0; Channel < Channels; ++Channel)
            {
                const bool IsAlpha =
                    Settings.Semantic == ETextureSemantic::Color &&
                    ((Channels == 2 && Channel == 1) ||
                     (Channels == 4 && Channel == 3));
                const bool IsSrgb =
                    Settings.Semantic == ETextureSemantic::Color &&
                    Settings.ColorSpace == EImageColorSpace::SRGB &&
                    !IsAlpha;
                if (IsSrgb)
                {
                    Core::uint64 Sum = 0;
                    for (Core::uint32 SourceY = BeginY; SourceY < EndY; ++SourceY)
                    {
                        for (Core::uint32 SourceX = BeginX; SourceX < EndX; ++SourceX)
                        {
                            const Core::usize Base =
                                (static_cast<Core::usize>(SourceY) *
                                 SourceExtent.Width + SourceX) * Channels;
                            Sum += SrgbToLinearQ24[
                                SourceBytes[Base + Channel]];
                        }
                    }
                    OutBytes.push_back(
                        LinearAverageToSrgb(Sum, Count));
                }
                else
                {
                    Core::uint64 Sum = 0;
                    for (Core::uint32 SourceY = BeginY; SourceY < EndY; ++SourceY)
                    {
                        for (Core::uint32 SourceX = BeginX; SourceX < EndX; ++SourceX)
                        {
                            const Core::usize Base =
                                (static_cast<Core::usize>(SourceY) *
                                 SourceExtent.Width + SourceX) * Channels;
                            Sum += SourceBytes[Base + Channel];
                        }
                    }
                    OutBytes.push_back(static_cast<Core::uint8>(
                        (Sum + Count / 2U) / Count));
                }
            }
        }
    }
    return EAssetResult::Success;
}

EAssetResult GenerateFloatLevel(
    const FImageMip& Source,
    FImageExtent2D DestinationExtent,
    Core::TArray<Core::uint8>& OutBytes)
{
    const auto SourceBytes = Source.GetBytes();
    const FImageExtent2D SourceExtent = Source.GetExtent();
    const Core::uint32 Channels = GetImageChannelCount(Source.GetFormat());
    const Core::uint32 ComponentBytes =
        GetImageBytesPerTexel(Source.GetFormat()) / Channels;
    OutBytes.clear();
    OutBytes.reserve(
        static_cast<Core::usize>(DestinationExtent.Width) *
        DestinationExtent.Height *
        GetImageBytesPerTexel(Source.GetFormat()));
    for (Core::uint32 Y = 0; Y < DestinationExtent.Height; ++Y)
    {
        Core::uint32 BeginY = 0;
        Core::uint32 EndY = 0;
        SourceBounds(Y, DestinationExtent.Height, SourceExtent.Height, BeginY, EndY);
        for (Core::uint32 X = 0; X < DestinationExtent.Width; ++X)
        {
            Core::uint32 BeginX = 0;
            Core::uint32 EndX = 0;
            SourceBounds(X, DestinationExtent.Width, SourceExtent.Width, BeginX, EndX);
            const Core::uint32 Count =
                (EndX - BeginX) * (EndY - BeginY);
            for (Core::uint32 Channel = 0; Channel < Channels; ++Channel)
            {
                float Sum = 0.0f;
                for (Core::uint32 SourceY = BeginY; SourceY < EndY; ++SourceY)
                {
                    for (Core::uint32 SourceX = BeginX; SourceX < EndX; ++SourceX)
                    {
                        const Core::usize Offset =
                            ((static_cast<Core::usize>(SourceY) *
                              SourceExtent.Width + SourceX) * Channels +
                             Channel) * ComponentBytes;
                        Sum += ComponentBytes == 2
                            ? DecodeHalf(ReadU16(SourceBytes, Offset))
                            : ReadFloat(SourceBytes, Offset);
                    }
                }
                const float Value = Sum / static_cast<float>(Count);
                if (ComponentBytes == 2)
                {
                    Core::uint16 Half = 0;
                    const EAssetResult Result = EncodeHalf(Value, Half);
                    if (Result != EAssetResult::Success)
                    {
                        return Result;
                    }
                    WriteU16(OutBytes, Half);
                }
                else
                {
                    WriteFloat(OutBytes, Value);
                }
            }
        }
    }
    return EAssetResult::Success;
}

} // namespace

EAssetResult GenerateImageMips(
    const FImageMip& BaseMip,
    const FImageImportSettings& Settings,
    Core::TArray<FImageMip>& OutMips,
    FAssetDiagnostic* OutDiagnostic)
{
    OutMips.clear();
    if (OutDiagnostic)
    {
        *OutDiagnostic = {};
    }
    if (Settings.Validate() != EAssetResult::Success)
    {
        return EAssetResult::InvalidInput;
    }
    const EAssetResult BaseResult =
        ValidateMip(BaseMip, Settings.Limits);
    if (BaseResult != EAssetResult::Success)
    {
        return BaseResult;
    }
    if (Settings.Semantic == ETextureSemantic::Normal &&
        (BaseMip.GetFormat() != EImageTexelFormat::R8G8_UNorm &&
         BaseMip.GetFormat() != EImageTexelFormat::R8G8B8_UNorm &&
         BaseMip.GetFormat() != EImageTexelFormat::R8G8B8A8_UNorm))
    {
        return EAssetResult::InvalidInput;
    }
    OutMips.push_back(BaseMip);
    Core::uint64 ChainBytes = BaseMip.GetBytes().size();
    if (Settings.MipPolicy == EImageMipPolicy::BaseOnly)
    {
        return EAssetResult::Success;
    }

    while (OutMips.back().GetExtent() != FImageExtent2D{1, 1})
    {
        const FImageMip& Source = OutMips.back();
        const FImageExtent2D DestinationExtent =
            NextMipExtent(Source.GetExtent());
        Core::TArray<Core::uint8> Bytes;
        const EAssetResult GenerateResult =
            IsImageFloatFormat(Source.GetFormat())
            ? GenerateFloatLevel(Source, DestinationExtent, Bytes)
            : GenerateLdrLevel(Source, DestinationExtent, Settings, Bytes);
        if (GenerateResult != EAssetResult::Success)
        {
            OutMips.clear();
            return GenerateResult;
        }
        FImageMip Mip;
        EAssetResult Result = FImageMip::Create(
            DestinationExtent,
            Source.GetFormat(),
            std::move(Bytes),
            Mip);
        if (Result != EAssetResult::Success ||
            ValidateMip(Mip, Settings.Limits) != EAssetResult::Success ||
            !CheckedAdd(ChainBytes, Mip.GetBytes().size(), ChainBytes) ||
            ChainBytes > Settings.Limits.MaxDecodedChainBytes)
        {
            OutMips.clear();
            if (OutDiagnostic)
            {
                OutDiagnostic->Stage = EAssetStage::Mip;
                OutDiagnostic->Result = EAssetResult::ImageLimitExceeded;
                OutDiagnostic->Severity = EAssetDiagnosticSeverity::Error;
                OutDiagnostic->Code = Core::FString("image.limit.mip-chain");
                OutDiagnostic->Field = Core::FString("decoded-chain-bytes");
                OutDiagnostic->Limit = Core::FString(
                    std::to_string(
                        Settings.Limits.MaxDecodedChainBytes));
            }
            return EAssetResult::ImageLimitExceeded;
        }
        OutMips.push_back(std::move(Mip));
    }
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
