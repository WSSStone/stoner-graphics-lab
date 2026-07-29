#include "FImageContainerInspector.h"

#include <array>
#include <charconv>
#include <cstring>
#include <string>
#include <string_view>

namespace Stoner::Asset::Private
{
namespace
{

constexpr std::array<Core::uint8, 8> PngSignature = {
    0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};

Core::uint16 ReadU16(
    std::span<const Core::uint8> Bytes,
    Core::usize Offset,
    bool LittleEndian)
{
    return LittleEndian
        ? static_cast<Core::uint16>(
              Bytes[Offset] |
              static_cast<Core::uint16>(Bytes[Offset + 1]) << 8U)
        : static_cast<Core::uint16>(
              static_cast<Core::uint16>(Bytes[Offset]) << 8U |
              Bytes[Offset + 1]);
}

Core::uint32 ReadU32BE(std::span<const Core::uint8> Bytes, Core::usize Offset)
{
    return static_cast<Core::uint32>(Bytes[Offset]) << 24U |
        static_cast<Core::uint32>(Bytes[Offset + 1]) << 16U |
        static_cast<Core::uint32>(Bytes[Offset + 2]) << 8U |
        static_cast<Core::uint32>(Bytes[Offset + 3]);
}

Core::uint32 ReadU32(
    std::span<const Core::uint8> Bytes,
    Core::usize Offset,
    bool LittleEndian)
{
    if (!LittleEndian)
    {
        return ReadU32BE(Bytes, Offset);
    }
    return static_cast<Core::uint32>(Bytes[Offset]) |
        static_cast<Core::uint32>(Bytes[Offset + 1]) << 8U |
        static_cast<Core::uint32>(Bytes[Offset + 2]) << 16U |
        static_cast<Core::uint32>(Bytes[Offset + 3]) << 24U;
}

Core::uint32 Crc32(std::span<const Core::uint8> Bytes)
{
    Core::uint32 Crc = 0xffffffffU;
    for (const Core::uint8 Byte : Bytes)
    {
        Crc ^= Byte;
        for (int Bit = 0; Bit < 8; ++Bit)
        {
            const Core::uint32 Mask =
                static_cast<Core::uint32>(-static_cast<int>(Crc & 1U));
            Crc = (Crc >> 1U) ^ (0xedb88320U & Mask);
        }
    }
    return ~Crc;
}

void SetDiagnostic(
    FAssetDiagnostic* Diagnostic,
    EAssetResult Result,
    const char* Code,
    const char* Field,
    const char* Reason)
{
    if (!Diagnostic)
    {
        return;
    }
    *Diagnostic = {};
    Diagnostic->Stage = EAssetStage::Inspect;
    Diagnostic->Result = Result;
    Diagnostic->Severity = EAssetDiagnosticSeverity::Error;
    Diagnostic->Code = Core::FString(Code);
    Diagnostic->Field = Core::FString(Field);
    Diagnostic->Reason = Core::FString(Reason);
}

EAssetResult ParseExifOrientation(
    std::span<const Core::uint8> Tiff,
    EImageOrientationTransform& OutOrientation)
{
    if (Tiff.size() < 8)
    {
        return EAssetResult::TruncatedSource;
    }
    const bool LittleEndian = Tiff[0] == 'I' && Tiff[1] == 'I';
    if (!LittleEndian && !(Tiff[0] == 'M' && Tiff[1] == 'M'))
    {
        return EAssetResult::MalformedSource;
    }
    if (ReadU16(Tiff, 2, LittleEndian) != 42)
    {
        return EAssetResult::MalformedSource;
    }
    const Core::uint32 IfdOffset = ReadU32(Tiff, 4, LittleEndian);
    if (IfdOffset > Tiff.size() || Tiff.size() - IfdOffset < 2)
    {
        return EAssetResult::TruncatedSource;
    }
    const Core::uint16 Count = ReadU16(Tiff, IfdOffset, LittleEndian);
    const Core::uint64 EntriesEnd =
        static_cast<Core::uint64>(IfdOffset) + 2ULL +
        static_cast<Core::uint64>(Count) * 12ULL;
    if (EntriesEnd > Tiff.size())
    {
        return EAssetResult::TruncatedSource;
    }
    for (Core::uint16 Index = 0; Index < Count; ++Index)
    {
        const Core::usize Entry =
            static_cast<Core::usize>(IfdOffset) + 2U +
            static_cast<Core::usize>(Index) * 12U;
        if (ReadU16(Tiff, Entry, LittleEndian) != 0x0112)
        {
            continue;
        }
        if (ReadU16(Tiff, Entry + 2, LittleEndian) != 3 ||
            ReadU32(Tiff, Entry + 4, LittleEndian) != 1)
        {
            return EAssetResult::MalformedSource;
        }
        const Core::uint16 Value = ReadU16(Tiff, Entry + 8, LittleEndian);
        if (Value < 1 || Value > 8)
        {
            return EAssetResult::MalformedSource;
        }
        OutOrientation =
            static_cast<EImageOrientationTransform>(Value - 1);
        return EAssetResult::Success;
    }
    return EAssetResult::Success;
}

bool IsValidPngBitDepth(Core::uint8 ColorType, Core::uint8 BitDepth)
{
    switch (ColorType)
    {
    case 0: return BitDepth == 1 || BitDepth == 2 || BitDepth == 4 ||
            BitDepth == 8 || BitDepth == 16;
    case 2:
    case 4:
    case 6: return BitDepth == 8 || BitDepth == 16;
    case 3: return BitDepth == 1 || BitDepth == 2 ||
            BitDepth == 4 || BitDepth == 8;
    default: return false;
    }
}

EAssetResult InspectPng(
    std::span<const Core::uint8> Bytes,
    const FImageImportLimits& Limits,
    FImageContainerInspection& Out,
    FAssetDiagnostic* Diagnostic)
{
    if (Bytes.size() < PngSignature.size() ||
        !std::equal(PngSignature.begin(), PngSignature.end(), Bytes.begin()))
    {
        return EAssetResult::MalformedSource;
    }
    bool HasHeader = false;
    bool HasData = false;
    bool HasEnd = false;
    Core::uint8 ColorType = 0;
    Core::usize Offset = PngSignature.size();
    while (Offset < Bytes.size())
    {
        if (Bytes.size() - Offset < 12)
        {
            return EAssetResult::TruncatedSource;
        }
        const Core::uint32 Length = ReadU32BE(Bytes, Offset);
        const Core::uint64 End64 =
            static_cast<Core::uint64>(Offset) + 12ULL + Length;
        if (End64 > Bytes.size())
        {
            return EAssetResult::TruncatedSource;
        }
        const Core::usize TypeOffset = Offset + 4;
        const Core::usize DataOffset = Offset + 8;
        const Core::usize CrcOffset = DataOffset + Length;
        const std::string_view Type(
            reinterpret_cast<const char*>(Bytes.data() + TypeOffset),
            4);
        if (Crc32(Bytes.subspan(TypeOffset, 4U + Length)) !=
            ReadU32BE(Bytes, CrcOffset))
        {
            SetDiagnostic(
                Diagnostic,
                EAssetResult::MalformedSource,
                "image.png.bad-crc",
                "crc",
                "PNG chunk CRC does not match");
            return EAssetResult::MalformedSource;
        }
        const auto Data = Bytes.subspan(DataOffset, Length);
        if (Type == "CgBI")
        {
            return EAssetResult::Unsupported;
        }
        if (Type == "IHDR")
        {
            if (HasHeader || Length != 13 || Offset != 8)
            {
                return EAssetResult::MalformedSource;
            }
            Out.SourceExtent = {ReadU32BE(Data, 0), ReadU32BE(Data, 4)};
            Out.SourceBitsPerChannel = Data[8];
            ColorType = Data[9];
            if (!IsValidPngBitDepth(ColorType, Data[8]) ||
                Data[10] != 0 || Data[11] != 0 || Data[12] > 1)
            {
                return EAssetResult::Unsupported;
            }
            switch (ColorType)
            {
            case 0: Out.SourceChannels = 1; break;
            case 2: Out.SourceChannels = 3; break;
            case 3: Out.SourceChannels = 3; break;
            case 4: Out.SourceChannels = 2; break;
            case 6: Out.SourceChannels = 4; break;
            default: return EAssetResult::Unsupported;
            }
            Out.AlphaMode =
                ColorType == 4 || ColorType == 6
                ? EImageAlphaMode::Straight
                : EImageAlphaMode::None;
            HasHeader = true;
        }
        else if (!HasHeader)
        {
            return EAssetResult::MalformedSource;
        }
        else if (Type == "IDAT")
        {
            HasData = true;
        }
        else if (Type == "IEND")
        {
            if (Length != 0)
            {
                return EAssetResult::MalformedSource;
            }
            HasEnd = true;
            Offset = static_cast<Core::usize>(End64);
            break;
        }
        else if (Type == "tRNS")
        {
            if (ColorType == 4 || ColorType == 6)
            {
                return EAssetResult::MalformedSource;
            }
            Out.AlphaMode = EImageAlphaMode::Straight;
            ++Out.SourceChannels;
        }
        else if (Type == "sRGB")
        {
            if (Length != 1)
            {
                return EAssetResult::MalformedSource;
            }
            Out.DeclaredColorSpace = EImageColorSpace::SRGB;
        }
        else if (Type == "gAMA")
        {
            if (Length != 4)
            {
                return EAssetResult::MalformedSource;
            }
            const Core::uint32 Gamma = ReadU32BE(Data, 0);
            if (Gamma == 45455)
            {
                Out.DeclaredColorSpace = EImageColorSpace::SRGB;
            }
            else if (Gamma == 100000)
            {
                Out.DeclaredColorSpace = EImageColorSpace::Linear;
            }
            else
            {
                return EAssetResult::UnsupportedColorProfile;
            }
        }
        else if (Type == "iCCP")
        {
            return EAssetResult::UnsupportedColorProfile;
        }
        else if (Type == "eXIf")
        {
            const EAssetResult Result =
                ParseExifOrientation(Data, Out.Orientation);
            if (Result != EAssetResult::Success)
            {
                return Result;
            }
            Out.OrientationMetadataPresent = true;
        }
        Offset = static_cast<Core::usize>(End64);
    }
    if (!HasHeader || !HasData || !HasEnd || Offset != Bytes.size())
    {
        return HasEnd ? EAssetResult::MalformedSource
                      : EAssetResult::TruncatedSource;
    }
    Out.SourceFormat = EImageSourceFormat::PNG;
    if (!Out.DeclaredColorSpace)
    {
        Out.DeclaredColorSpace = EImageColorSpace::SRGB;
    }
    return Out.Validate(Limits);
}

bool IsSofMarker(Core::uint8 Marker)
{
    return (Marker >= 0xc0 && Marker <= 0xc3) ||
        (Marker >= 0xc5 && Marker <= 0xc7) ||
        (Marker >= 0xc9 && Marker <= 0xcb) ||
        (Marker >= 0xcd && Marker <= 0xcf);
}

EAssetResult InspectJpeg(
    std::span<const Core::uint8> Bytes,
    const FImageImportLimits& Limits,
    FImageContainerInspection& Out)
{
    if (Bytes.size() < 4 || Bytes[0] != 0xff || Bytes[1] != 0xd8)
    {
        return EAssetResult::MalformedSource;
    }
    bool HasFrame = false;
    Core::usize Offset = 2;
    while (Offset < Bytes.size())
    {
        if (Bytes[Offset] != 0xff)
        {
            return EAssetResult::MalformedSource;
        }
        while (Offset < Bytes.size() && Bytes[Offset] == 0xff)
        {
            ++Offset;
        }
        if (Offset >= Bytes.size())
        {
            return EAssetResult::TruncatedSource;
        }
        const Core::uint8 Marker = Bytes[Offset++];
        if (Marker == 0xd9)
        {
            break;
        }
        if (Marker == 0xda)
        {
            if (!HasFrame)
            {
                return EAssetResult::MalformedSource;
            }
            break;
        }
        if (Marker == 0x01 || (Marker >= 0xd0 && Marker <= 0xd7))
        {
            continue;
        }
        if (Bytes.size() - Offset < 2)
        {
            return EAssetResult::TruncatedSource;
        }
        const Core::uint16 Length = ReadU16(Bytes, Offset, false);
        if (Length < 2 || Bytes.size() - Offset < Length)
        {
            return EAssetResult::TruncatedSource;
        }
        const auto Data = Bytes.subspan(Offset + 2, Length - 2);
        if (IsSofMarker(Marker))
        {
            if (Data.size() < 6)
            {
                return EAssetResult::TruncatedSource;
            }
            if (Data[0] != 8)
            {
                return EAssetResult::Unsupported;
            }
            Out.SourceExtent = {
                ReadU16(Data, 3, false),
                ReadU16(Data, 1, false)};
            Out.SourceBitsPerChannel = 8;
            Out.SourceChannels = Data[5];
            Out.AlphaMode = EImageAlphaMode::None;
            HasFrame = true;
        }
        else if (Marker == 0xe1 && Data.size() >= 6 &&
                 std::memcmp(Data.data(), "Exif\0\0", 6) == 0)
        {
            const EAssetResult Result =
                ParseExifOrientation(Data.subspan(6), Out.Orientation);
            if (Result != EAssetResult::Success)
            {
                return Result;
            }
            Out.OrientationMetadataPresent = true;
        }
        else if (Marker == 0xe2 && Data.size() >= 11 &&
                 std::memcmp(Data.data(), "ICC_PROFILE", 11) == 0)
        {
            return EAssetResult::UnsupportedColorProfile;
        }
        Offset += Length;
    }
    if (!HasFrame)
    {
        return EAssetResult::TruncatedSource;
    }
    Out.SourceFormat = EImageSourceFormat::JPEG;
    Out.DeclaredColorSpace = EImageColorSpace::SRGB;
    return Out.Validate(Limits);
}

EAssetResult ParsePositive(
    std::string_view Text,
    Core::uint32& OutValue)
{
    OutValue = 0;
    const auto Result = std::from_chars(
        Text.data(),
        Text.data() + Text.size(),
        OutValue);
    return Result.ec == std::errc{} &&
            Result.ptr == Text.data() + Text.size() &&
            OutValue > 0
        ? EAssetResult::Success
        : EAssetResult::MalformedSource;
}

EAssetResult InspectHdr(
    std::span<const Core::uint8> Bytes,
    const FImageImportLimits& Limits,
    FImageContainerInspection& Out)
{
    const std::string_view Text(
        reinterpret_cast<const char*>(Bytes.data()),
        Bytes.size());
    if (!Text.starts_with("#?RADIANCE\n") && !Text.starts_with("#?RGBE\n"))
    {
        return EAssetResult::MalformedSource;
    }
    const std::size_t HeaderEnd = Text.find("\n\n");
    if (HeaderEnd == std::string_view::npos)
    {
        return EAssetResult::TruncatedSource;
    }
    if (Text.substr(0, HeaderEnd).find("FORMAT=32-bit_rle_rgbe") ==
        std::string_view::npos)
    {
        return EAssetResult::Unsupported;
    }
    const std::size_t ResolutionEnd = Text.find('\n', HeaderEnd + 2);
    if (ResolutionEnd == std::string_view::npos)
    {
        return EAssetResult::TruncatedSource;
    }
    const std::string_view Resolution =
        Text.substr(HeaderEnd + 2, ResolutionEnd - HeaderEnd - 2);
    const std::size_t FirstSpace = Resolution.find(' ');
    const std::size_t SecondSpace =
        FirstSpace == std::string_view::npos
        ? std::string_view::npos
        : Resolution.find(' ', FirstSpace + 1);
    const std::size_t ThirdSpace =
        SecondSpace == std::string_view::npos
        ? std::string_view::npos
        : Resolution.find(' ', SecondSpace + 1);
    if (FirstSpace == std::string_view::npos ||
        SecondSpace == std::string_view::npos ||
        ThirdSpace == std::string_view::npos)
    {
        return EAssetResult::MalformedSource;
    }
    const std::string_view Axis1 = Resolution.substr(0, FirstSpace);
    const std::string_view Value1 = Resolution.substr(
        FirstSpace + 1,
        SecondSpace - FirstSpace - 1);
    const std::string_view Axis2 = Resolution.substr(
        SecondSpace + 1,
        ThirdSpace - SecondSpace - 1);
    const std::string_view Value2 = Resolution.substr(ThirdSpace + 1);
    Core::uint32 Size1 = 0;
    Core::uint32 Size2 = 0;
    if (ParsePositive(Value1, Size1) != EAssetResult::Success ||
        ParsePositive(Value2, Size2) != EAssetResult::Success ||
        (Axis1 != "-Y" && Axis1 != "+Y") ||
        (Axis2 != "-X" && Axis2 != "+X") ||
        Axis1[1] == Axis2[1])
    {
        return EAssetResult::MalformedSource;
    }
    const bool FirstIsY = Axis1[1] == 'Y';
    Out.SourceExtent = FirstIsY
        ? FImageExtent2D{Size2, Size1}
        : FImageExtent2D{Size1, Size2};
    if (Axis1 == "-Y" && Axis2 == "+X")
    {
        Out.Orientation = EImageOrientationTransform::Identity;
    }
    else
    {
        return EAssetResult::Unsupported;
    }
    Out.SourceFormat = EImageSourceFormat::RadianceHDR;
    Out.SourceChannels = 3;
    Out.SourceBitsPerChannel = 32;
    Out.DeclaredColorSpace = EImageColorSpace::Linear;
    Out.AlphaMode = EImageAlphaMode::None;
    return Out.Validate(Limits);
}

} // namespace

EImageSourceFormat FImageContainerInspector::Detect(
    std::span<const Core::uint8> Prefix) noexcept
{
    if (Prefix.size() >= PngSignature.size() &&
        std::equal(PngSignature.begin(), PngSignature.end(), Prefix.begin()))
    {
        return EImageSourceFormat::PNG;
    }
    if (Prefix.size() >= 2 && Prefix[0] == 0xff && Prefix[1] == 0xd8)
    {
        return EImageSourceFormat::JPEG;
    }
    if (Prefix.size() >= 6 &&
        (std::memcmp(Prefix.data(), "#?RGBE", 6) == 0 ||
         (Prefix.size() >= 10 &&
          std::memcmp(Prefix.data(), "#?RADIANCE", 10) == 0)))
    {
        return EImageSourceFormat::RadianceHDR;
    }
    return EImageSourceFormat::Unknown;
}

EAssetResult FImageContainerInspector::Inspect(
    std::span<const Core::uint8> Bytes,
    const FImageImportLimits& Limits,
    FImageContainerInspection& OutInspection,
    FAssetDiagnostic* OutDiagnostic)
{
    OutInspection = {};
    if (OutDiagnostic)
    {
        *OutDiagnostic = {};
    }
    if (Limits.Validate() != EAssetResult::Success)
    {
        return EAssetResult::InvalidInput;
    }
    if (Bytes.size() > Limits.MaxSourceBytes)
    {
        SetDiagnostic(
            OutDiagnostic,
            EAssetResult::ImageLimitExceeded,
            "image.limit.source-bytes",
            "source-bytes",
            "source exceeds configured byte limit");
        if (OutDiagnostic)
        {
            OutDiagnostic->Limit =
                Core::FString(std::to_string(Limits.MaxSourceBytes));
        }
        return EAssetResult::ImageLimitExceeded;
    }
    switch (Detect(Bytes.first(std::min<Core::usize>(Bytes.size(), 64U))))
    {
    case EImageSourceFormat::PNG:
        return InspectPng(Bytes, Limits, OutInspection, OutDiagnostic);
    case EImageSourceFormat::JPEG:
        return InspectJpeg(Bytes, Limits, OutInspection);
    case EImageSourceFormat::RadianceHDR:
        return InspectHdr(Bytes, Limits, OutInspection);
    case EImageSourceFormat::Unknown:
        return EAssetResult::Unsupported;
    }
    return EAssetResult::Unsupported;
}

} // namespace Stoner::Asset::Private
