#include "IKTX2Encoder.h"

#include "FWamrEncoderRuntime.h"

#include <algorithm>
#include <limits>

namespace Stoner::Asset::Private
{
namespace
{

constexpr Core::uint32 RequestMagic = 0x324B4753U;
constexpr Core::uint32 AbiVersion = 1;
constexpr Core::uint32 HeaderBytes = 32;
constexpr Core::uint32 DescriptorBytes = 16;

void WriteU32(
    Core::TArray<Core::uint8>& Bytes,
    Core::usize Offset,
    Core::uint32 Value)
{
    for (Core::usize Index = 0; Index < 4; ++Index)
    {
        Bytes[Offset + Index] = static_cast<Core::uint8>(
            Value >> (Index * 8U));
    }
}

FAssetDiagnostic InvalidRequest(const char* Field, const char* Reason)
{
    FAssetDiagnostic Diagnostic;
    Diagnostic.Stage = EAssetStage::Validate;
    Diagnostic.Result = EAssetResult::InvalidInput;
    Diagnostic.Severity = EAssetDiagnosticSeverity::Error;
    Diagnostic.Code = Core::FString("asset.ktx2.encoder.request");
    Diagnostic.Participant = Core::FString("encoder.canonical-basis");
    Diagnostic.Field = Core::FString(Field);
    Diagnostic.Reason = Core::FString(Reason);
    return Diagnostic;
}

bool AddFitsU32(Core::uint64& Total, Core::uint64 Amount)
{
    if (Amount > std::numeric_limits<Core::uint32>::max() - Total)
    {
        return false;
    }
    Total += Amount;
    return true;
}

} // namespace

FKTX2EncoderResult FCanonicalBasisEncoder::Encode(
    const FKTX2EncoderRequest& Request) const
{
    FKTX2EncoderResult Result;
    if ((Request.Policy != ETextureCompressionPolicy::ETC1S &&
         Request.Policy != ETextureCompressionPolicy::UASTC) ||
        Request.Mips.empty() ||
        Request.Mips.size() > 15 ||
        Request.Metadata.size() > 64)
    {
        Result.Diagnostics.push_back(InvalidRequest(
            "policy",
            "Basis encoding requires ETC1S or UASTC and bounded mips"));
        return Result;
    }
    if (!std::is_sorted(
            Request.Metadata.begin(),
            Request.Metadata.end(),
            [](const auto& Left, const auto& Right)
            {
                return Left.Key < Right.Key;
            }))
    {
        Result.Diagnostics.push_back(InvalidRequest(
            "metadata",
            "metadata keys must be in canonical order"));
        return Result;
    }

    Core::uint64 Total = HeaderBytes +
        (Request.Mips.size() + Request.Metadata.size()) *
            DescriptorBytes;
    for (const FKTX2EncoderMip& Mip : Request.Mips)
    {
        if (!Mip.Extent.IsValid() ||
            Mip.Extent.Width > 16384 ||
            Mip.Extent.Height > 16384 ||
            Mip.RGBA8Bytes.size() !=
                static_cast<Core::uint64>(Mip.Extent.Width) *
                    Mip.Extent.Height * 4ULL ||
            !AddFitsU32(Total, Mip.RGBA8Bytes.size()))
        {
            Result.Diagnostics.push_back(InvalidRequest(
                "mips",
                "mip extent or RGBA8 byte count is invalid"));
            return Result;
        }
    }
    for (const FKTX2EncoderMetadata& Entry : Request.Metadata)
    {
        if (Entry.Key.IsEmpty() ||
            Entry.Key == Core::FString("KTXwriter") ||
            Entry.Value.empty() ||
            !AddFitsU32(Total, Entry.Key.Len() + 1) ||
            !AddFitsU32(Total, Entry.Value.size()))
        {
            Result.Diagnostics.push_back(InvalidRequest(
                "metadata",
                "metadata key or value is invalid"));
            return Result;
        }
    }

    Core::TArray<Core::uint8> Serialized(
        static_cast<Core::usize>(Total), 0);
    WriteU32(Serialized, 0, RequestMagic);
    WriteU32(Serialized, 4, AbiVersion);
    WriteU32(
        Serialized,
        8,
        Request.Policy == ETextureCompressionPolicy::ETC1S ? 1 : 2);
    WriteU32(
        Serialized,
        12,
        Request.Quality == ETextureCookQuality::Balanced ? 0 : 1);
    const Core::uint32 Flags =
        (Request.bSRGB ? 1U : 0U) |
        (Request.bNormal ? 2U : 0U) |
        (Request.bForceAlpha ? 4U : 0U);
    WriteU32(Serialized, 16, Flags);
    WriteU32(
        Serialized,
        20,
        static_cast<Core::uint32>(Request.Mips.size()));
    WriteU32(
        Serialized,
        24,
        static_cast<Core::uint32>(Request.Metadata.size()));

    Core::uint32 PayloadOffset = HeaderBytes +
        static_cast<Core::uint32>(
            (Request.Mips.size() + Request.Metadata.size()) *
            DescriptorBytes);
    for (Core::usize Index = 0; Index < Request.Mips.size(); ++Index)
    {
        const FKTX2EncoderMip& Mip = Request.Mips[Index];
        const Core::usize Descriptor = HeaderBytes +
            Index * DescriptorBytes;
        WriteU32(Serialized, Descriptor, Mip.Extent.Width);
        WriteU32(Serialized, Descriptor + 4, Mip.Extent.Height);
        WriteU32(Serialized, Descriptor + 8, PayloadOffset);
        WriteU32(
            Serialized,
            Descriptor + 12,
            static_cast<Core::uint32>(Mip.RGBA8Bytes.size()));
        std::copy(
            Mip.RGBA8Bytes.begin(),
            Mip.RGBA8Bytes.end(),
            Serialized.begin() + PayloadOffset);
        PayloadOffset +=
            static_cast<Core::uint32>(Mip.RGBA8Bytes.size());
    }
    for (Core::usize Index = 0;
         Index < Request.Metadata.size();
         ++Index)
    {
        const FKTX2EncoderMetadata& Entry = Request.Metadata[Index];
        const Core::usize Descriptor = HeaderBytes +
            (Request.Mips.size() + Index) * DescriptorBytes;
        WriteU32(Serialized, Descriptor, PayloadOffset);
        WriteU32(
            Serialized,
            Descriptor + 4,
            static_cast<Core::uint32>(Entry.Key.Len() + 1));
        std::copy(
            Entry.Key.View().begin(),
            Entry.Key.View().end(),
            Serialized.begin() + PayloadOffset);
        PayloadOffset += static_cast<Core::uint32>(
            Entry.Key.Len() + 1);
        WriteU32(Serialized, Descriptor + 8, PayloadOffset);
        WriteU32(
            Serialized,
            Descriptor + 12,
            static_cast<Core::uint32>(Entry.Value.size()));
        std::copy(
            Entry.Value.begin(),
            Entry.Value.end(),
            Serialized.begin() + PayloadOffset);
        PayloadOffset +=
            static_cast<Core::uint32>(Entry.Value.size());
    }

    FWamrEncoderResult Runtime = FWamrEncoderRuntime::Execute(
        Serialized, Request.MaxOutputBytes);
    Result.Result = Runtime.Result;
    Result.Bytes = std::move(Runtime.Bytes);
    Result.Diagnostics = std::move(Runtime.Diagnostics);
    return Result;
}

} // namespace Stoner::Asset::Private
