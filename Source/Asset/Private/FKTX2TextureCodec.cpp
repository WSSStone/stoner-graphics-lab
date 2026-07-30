#include "Asset/FKTX2TextureCodec.h"

#include "FKTX2ContainerCodec.h"
#include "FKTX2Preflight.h"
#include "Core/FUnicode.h"
#include "ktx.h"
#include "vkformat_enum.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <string>

namespace Stoner::Asset
{
namespace
{

using namespace Private;

void AddDiagnostic(
    FAssetDiagnosticList* Diagnostics,
    EAssetResult Result,
    const char* Code,
    const char* Field,
    const char* Reason)
{
    if (Diagnostics == nullptr)
    {
        return;
    }
    FAssetDiagnostic Diagnostic;
    Diagnostic.Stage = EAssetStage::Container;
    Diagnostic.Result = Result;
    Diagnostic.Severity = EAssetDiagnosticSeverity::Error;
    Diagnostic.Code = Core::FString(Code);
    Diagnostic.Participant = Core::FString("codec.ktx2");
    Diagnostic.Field = Core::FString(Field);
    Diagnostic.Reason = Core::FString(Reason);
    Diagnostics->push_back(std::move(Diagnostic));
}

bool GetString(
    ktxTexture2* Texture,
    const char* Key,
    Core::FString& Out)
{
    unsigned int Length = 0;
    void* Value = nullptr;
    if (ktxHashList_FindValue(
            &Texture->kvDataHead,
            Key,
            &Length,
            &Value) != KTX_SUCCESS ||
        Value == nullptr || Length < 2)
    {
        return false;
    }
    const auto* Characters = static_cast<const char*>(Value);
    if (Characters[Length - 1] != '\0')
    {
        return false;
    }
    for (unsigned int Index = 0; Index + 1 < Length; ++Index)
    {
        if (Characters[Index] == '\0')
        {
            return false;
        }
    }
    const Core::FString Text(
        std::string(Characters, Length - 1));
    Core::FString Normalized;
    if (Core::FUnicode::NormalizeNFC(
            Text, Normalized) !=
        Core::EUnicodeResult::Success)
    {
        return false;
    }
    Out = std::move(Normalized);
    return Out == Text;
}

Core::uint32 ReadU32(
    std::span<const Core::uint8> Bytes,
    Core::usize Offset)
{
    return static_cast<Core::uint32>(Bytes[Offset]) |
        (static_cast<Core::uint32>(Bytes[Offset + 1]) << 8U) |
        (static_cast<Core::uint32>(Bytes[Offset + 2]) << 16U) |
        (static_cast<Core::uint32>(Bytes[Offset + 3]) << 24U);
}

EAssetResult ValidateKeyValueData(
    std::span<const Core::uint8> Bytes,
    const FKTX2PreflightResult& Preflight,
    const FTextureCookLimits& Limits,
    FAssetDiagnosticList* Diagnostics)
{
    const Core::uint64 End64 =
        Preflight.KvdOffset + Preflight.KvdLength;
    if (Preflight.KvdLength == 0 ||
        End64 > Bytes.size())
    {
        AddDiagnostic(
            Diagnostics,
            EAssetResult::MalformedContainer,
            "asset.ktx2.kvd",
            "kvd",
            "required key/value data is absent or out of range");
        return EAssetResult::MalformedContainer;
    }

    Core::usize Cursor =
        static_cast<Core::usize>(Preflight.KvdOffset);
    const Core::usize End =
        static_cast<Core::usize>(End64);
    Core::TArray<Core::FString> Keys;
    while (Cursor < End)
    {
        if (End - Cursor < sizeof(Core::uint32))
        {
            AddDiagnostic(
                Diagnostics,
                EAssetResult::MalformedContainer,
                "asset.ktx2.kvd-entry",
                "keyValueLength",
                "key/value length is truncated");
            return EAssetResult::MalformedContainer;
        }
        const Core::uint32 EntryBytes =
            ReadU32(Bytes, Cursor);
        Cursor += sizeof(Core::uint32);
        if (EntryBytes == 0 ||
            EntryBytes > End - Cursor)
        {
            AddDiagnostic(
                Diagnostics,
                EAssetResult::MalformedContainer,
                "asset.ktx2.kvd-entry",
                "keyValueLength",
                "key/value payload is empty or exceeds its range");
            return EAssetResult::MalformedContainer;
        }
        const auto EntryBegin = Bytes.begin() +
            static_cast<std::ptrdiff_t>(Cursor);
        const auto EntryEnd = EntryBegin + EntryBytes;
        const auto Separator =
            std::find(EntryBegin, EntryEnd, Core::uint8{0});
        if (Separator == EntryBegin || Separator == EntryEnd)
        {
            AddDiagnostic(
                Diagnostics,
                EAssetResult::MalformedContainer,
                "asset.ktx2.kvd-key",
                "key",
                "key is empty or lacks its terminator");
            return EAssetResult::MalformedContainer;
        }
        const Core::FString Key(std::string(
            reinterpret_cast<const char*>(&*EntryBegin),
            static_cast<std::size_t>(
                std::distance(EntryBegin, Separator))));
        Core::FString NormalizedKey;
        if (Core::FUnicode::NormalizeNFC(
                Key, NormalizedKey) !=
                Core::EUnicodeResult::Success ||
            NormalizedKey != Key ||
            std::find(Keys.begin(), Keys.end(), Key) !=
                Keys.end())
        {
            AddDiagnostic(
                Diagnostics,
                EAssetResult::MalformedContainer,
                "asset.ktx2.kvd-key",
                "key",
                "key is invalid UTF-8 non-canonical or duplicated");
            return EAssetResult::MalformedContainer;
        }
        Keys.push_back(Key);
        if (Keys.size() > Limits.MaxKeyValuePairs)
        {
            AddDiagnostic(
                Diagnostics,
                EAssetResult::KTX2LimitExceeded,
                "asset.ktx2.kvd-count-limit",
                "keyValuePairs",
                "key/value pair count exceeds the configured limit");
            if (Diagnostics != nullptr)
            {
                Diagnostics->back().Actual =
                    Core::FString(std::to_string(Keys.size()));
                Diagnostics->back().Limit =
                    Core::FString(std::to_string(
                        Limits.MaxKeyValuePairs));
            }
            return EAssetResult::KTX2LimitExceeded;
        }

        const Core::uint64 PaddedEntryBytes =
            (static_cast<Core::uint64>(EntryBytes) + 3U) &
            ~Core::uint64{3U};
        if (PaddedEntryBytes >
            static_cast<Core::uint64>(End - Cursor))
        {
            AddDiagnostic(
                Diagnostics,
                EAssetResult::MalformedContainer,
                "asset.ktx2.kvd-padding",
                "padding",
                "key/value padding exceeds the KVD range");
            return EAssetResult::MalformedContainer;
        }
        Cursor += static_cast<Core::usize>(PaddedEntryBytes);
    }
    return Cursor == End
        ? EAssetResult::Success
        : EAssetResult::MalformedContainer;
}

bool ParseId(const Core::FString& Text, FAssetId& Out)
{
    const std::string Value = Text.ToStdString();
    const std::size_t Colon = Value.find(':');
    if (Colon == std::string::npos)
    {
        return false;
    }
    const std::size_t Hash = Value.find('#', Colon + 1);
    const std::optional<Core::FString> Subresource =
        Hash == std::string::npos
        ? std::nullopt
        : std::optional<Core::FString>(
              Core::FString(Value.substr(Hash + 1)));
    return FAssetId::Create(
               Core::FString(Value.substr(0, Colon)),
               Core::FString(Value.substr(
                   Colon + 1,
                   Hash == std::string::npos
                       ? std::string::npos
                       : Hash - Colon - 1)),
               Subresource,
               Out) == EAssetResult::Success &&
        Out.ToString() == Text;
}

std::optional<EImageTexelFormat> StoredFormat(Core::uint32 VkFormat)
{
    switch (VkFormat)
    {
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SRGB:
        return EImageTexelFormat::R8_UNorm;
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SRGB:
        return EImageTexelFormat::R8G8_UNorm;
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SRGB:
        return EImageTexelFormat::R8G8B8_UNorm;
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
        return EImageTexelFormat::R8G8B8A8_UNorm;
    case VK_FORMAT_R32G32B32_SFLOAT:
        return EImageTexelFormat::R32G32B32_Float;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return EImageTexelFormat::R16G16B16A16_Float;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return EImageTexelFormat::R32G32B32A32_Float;
    default:
        return std::nullopt;
    }
}

bool IsSrgbFormat(Core::uint32 VkFormat)
{
    return VkFormat == VK_FORMAT_R8_SRGB ||
        VkFormat == VK_FORMAT_R8G8_SRGB ||
        VkFormat == VK_FORMAT_R8G8B8_SRGB ||
        VkFormat == VK_FORMAT_R8G8B8A8_SRGB;
}

} // namespace

EAssetResult FKTX2TextureCodec::Inspect(
    std::span<const Core::uint8> Bytes,
    const FTextureCookLimits& Limits,
    FKTX2TextureInfo& OutInfo,
    FAssetDiagnosticList* OutDiagnostics)
{
    OutInfo = {};
    if (OutDiagnostics != nullptr)
    {
        OutDiagnostics->clear();
    }
    FKTX2PreflightResult Preflight;
    EAssetResult Result = PreflightKTX2(
        Bytes, Limits, Preflight, OutDiagnostics);
    if (Result != EAssetResult::Success)
    {
        return Result;
    }
    if (Preflight.Supercompression != KTX_SS_NONE &&
        Preflight.Supercompression != KTX_SS_BASIS_LZ)
    {
        AddDiagnostic(
            OutDiagnostics,
            EAssetResult::UnsupportedCompression,
            "asset.ktx2.supercompression",
            "supercompression",
            "supercompression scheme is outside the portable profile");
        return EAssetResult::UnsupportedCompression;
    }
    Result = ValidateKeyValueData(
        Bytes, Preflight, Limits, OutDiagnostics);
    if (Result != EAssetResult::Success)
    {
        return Result;
    }

    Private::FKTX2TextureHandle Handle;
    Result = Private::FKTX2ContainerCodec::Open(
        Bytes, Handle, OutDiagnostics);
    if (Result != EAssetResult::Success)
    {
        return Result;
    }

    Core::FString IdText;
    Core::FString SourceDigestText;
    Core::FString ContentDigestText;
    Core::FString CookRevisionText;
    Core::FString Profile;
    Core::FString Semantic;
    Core::FString Alpha;
    Core::FString MipPolicy;
    Core::FString ChannelCount;
    Core::FString Orientation;
    Core::FString Writer;
    if (!GetString(Handle.Get(), "stoner.assetId", IdText) ||
        !GetString(
            Handle.Get(), "stoner.sourceDigest", SourceDigestText) ||
        !GetString(
            Handle.Get(), "stoner.contentDigest", ContentDigestText) ||
        !GetString(
            Handle.Get(), "stoner.cookRevision", CookRevisionText) ||
        !GetString(
            Handle.Get(), "stoner.portableProfile", Profile) ||
        !GetString(Handle.Get(), "stoner.semantic", Semantic) ||
        !GetString(Handle.Get(), "stoner.alphaMode", Alpha) ||
        !GetString(
            Handle.Get(), "stoner.channelCount", ChannelCount) ||
        !GetString(Handle.Get(), "stoner.mipPolicy", MipPolicy) ||
        !GetString(Handle.Get(), "KTXorientation", Orientation) ||
        !GetString(Handle.Get(), "KTXwriter", Writer))
    {
        AddDiagnostic(
            OutDiagnostics,
            EAssetResult::MalformedContainer,
            "asset.ktx2.metadata",
            "requiredMetadata",
            "required metadata is missing or malformed");
        return EAssetResult::MalformedContainer;
    }

    FKTX2TextureInfo Info;
    if (!ParseId(IdText, Info.TextureId) ||
        FAssetDigest::ParseLowerHex(
            SourceDigestText, Info.SourceDigest) !=
            EAssetResult::Success ||
        FAssetDigest::ParseLowerHex(
            ContentDigestText, Info.ContentDigest) !=
            EAssetResult::Success ||
        FAssetDigest::ParseLowerHex(
        CookRevisionText, Info.CookRevision) !=
            EAssetResult::Success ||
        Profile != Core::FString("stoner.ktx2.portable.v1") ||
        Orientation != Core::FString("rd") ||
        Writer != Core::FString("StonerGraphicsLab/022-v1"))
    {
        AddDiagnostic(
            OutDiagnostics,
            EAssetResult::MalformedContainer,
            "asset.ktx2.metadata-value",
            "requiredMetadata",
            "required metadata contradicts the portable profile");
        return EAssetResult::MalformedContainer;
    }
    Info.ProducerVersion = Core::FString("022-v1");
    Info.PortableProfile = std::move(Profile);
    Info.ArtifactDigest = FAssetDigest::FromBytes(Bytes);
    Info.BaseExtent = {Preflight.Width, Preflight.Height};
    Info.Levels = std::move(Preflight.Levels);
    Info.Origin = EImageOrigin::TopLeft;
    Info.Writer = std::move(Writer);

    if (Preflight.VkFormat == VK_FORMAT_UNDEFINED &&
        Preflight.Supercompression == KTX_SS_BASIS_LZ)
    {
        Info.CompressionPolicy = ETextureCompressionPolicy::ETC1S;
        Info.BasisModel = EKTX2BasisModel::ETC1S;
        Info.Supercompression = EKTX2Supercompression::BasisLZ;
    }
    else if (Preflight.VkFormat == VK_FORMAT_UNDEFINED &&
             Preflight.Supercompression == KTX_SS_NONE)
    {
        Info.CompressionPolicy = ETextureCompressionPolicy::UASTC;
        Info.BasisModel = EKTX2BasisModel::UASTC;
        Info.Supercompression = EKTX2Supercompression::None;
    }
    else if (Preflight.VkFormat != VK_FORMAT_UNDEFINED &&
             Preflight.Supercompression == KTX_SS_NONE)
    {
        Info.CompressionPolicy =
            ETextureCompressionPolicy::Uncompressed;
        Info.StoredTexelFormat = StoredFormat(Preflight.VkFormat);
        if (!Info.StoredTexelFormat.has_value())
        {
            AddDiagnostic(
                OutDiagnostics,
                EAssetResult::UnsupportedCompression,
                "asset.ktx2.format",
                "vkFormat",
                "uncompressed format is not in the portable profile");
            return EAssetResult::UnsupportedCompression;
        }
    }
    else
    {
        AddDiagnostic(
            OutDiagnostics,
            EAssetResult::UnsupportedCompression,
            "asset.ktx2.compression",
            "supercompression",
            "compression combination is unsupported");
        return EAssetResult::UnsupportedCompression;
    }

    if (Semantic == Core::FString("color"))
    {
        Info.Semantic = ETextureSemantic::Color;
    }
    else if (Semantic == Core::FString("normal"))
    {
        Info.Semantic = ETextureSemantic::Normal;
    }
    else if (Semantic == Core::FString("data"))
    {
        Info.Semantic = ETextureSemantic::Data;
    }
    else
    {
        AddDiagnostic(
            OutDiagnostics,
            EAssetResult::MalformedContainer,
            "asset.ktx2.semantic",
            "semantic",
            "texture semantic is outside the portable profile");
        return EAssetResult::MalformedContainer;
    }
    Info.AlphaMode = Alpha == Core::FString("straight")
        ? EImageAlphaMode::Straight
        : EImageAlphaMode::None;
    if (Alpha != Core::FString("straight") &&
        Alpha != Core::FString("none"))
    {
        AddDiagnostic(
            OutDiagnostics,
            EAssetResult::MalformedContainer,
            "asset.ktx2.alpha",
            "alphaMode",
            "alpha mode is outside the portable profile");
        return EAssetResult::MalformedContainer;
    }
    Info.MipPolicy = MipPolicy == Core::FString("full-chain")
        ? EImageMipPolicy::FullChain
        : EImageMipPolicy::BaseOnly;
    if (MipPolicy != Core::FString("full-chain") &&
        MipPolicy != Core::FString("base-only"))
    {
        AddDiagnostic(
            OutDiagnostics,
            EAssetResult::MalformedContainer,
            "asset.ktx2.mip-policy",
            "mipPolicy",
            "mip policy is outside the portable profile");
        return EAssetResult::MalformedContainer;
    }
    if (ChannelCount == Core::FString("1"))
    {
        Info.SourceChannelCount = 1;
    }
    else if (ChannelCount == Core::FString("2"))
    {
        Info.SourceChannelCount = 2;
    }
    else if (ChannelCount == Core::FString("3"))
    {
        Info.SourceChannelCount = 3;
    }
    else if (ChannelCount == Core::FString("4"))
    {
        Info.SourceChannelCount = 4;
    }
    else
    {
        AddDiagnostic(
            OutDiagnostics,
            EAssetResult::MalformedContainer,
            "asset.ktx2.channels",
            "channelCount",
            "source channel count is outside the portable profile");
        return EAssetResult::MalformedContainer;
    }
    if ((Info.AlphaMode == EImageAlphaMode::Straight &&
         Info.SourceChannelCount != 2 &&
         Info.SourceChannelCount != 4) ||
        (Info.Semantic == ETextureSemantic::Normal &&
         Info.SourceChannelCount < 2) ||
        (Info.StoredTexelFormat.has_value() &&
         GetImageChannelCount(*Info.StoredTexelFormat) !=
             Info.SourceChannelCount))
    {
        AddDiagnostic(
            OutDiagnostics,
            EAssetResult::MalformedContainer,
            "asset.ktx2.semantic-contract",
            "semanticChannels",
            "alpha semantic or stored format contradicts source channels");
        return EAssetResult::MalformedContainer;
    }
    Info.ColorSpace =
        Preflight.VkFormat == VK_FORMAT_UNDEFINED
        ? (ktxTexture2_GetTransferFunction_e(Handle.Get()) ==
                   KHR_DF_TRANSFER_SRGB
               ? EImageColorSpace::SRGB
               : EImageColorSpace::Linear)
        : (IsSrgbFormat(Preflight.VkFormat)
               ? EImageColorSpace::SRGB
               : EImageColorSpace::Linear);
    if (Info.Semantic != ETextureSemantic::Color &&
        Info.ColorSpace == EImageColorSpace::SRGB)
    {
        AddDiagnostic(
            OutDiagnostics,
            EAssetResult::MalformedContainer,
            "asset.ktx2.transfer",
            "transfer",
            "normal and data textures must use linear transfer");
        return EAssetResult::MalformedContainer;
    }
    Core::uint32 ExpectedMipCount = 1;
    if (Info.MipPolicy == EImageMipPolicy::FullChain)
    {
        Core::uint32 Largest =
            std::max(Info.BaseExtent.Width, Info.BaseExtent.Height);
        ExpectedMipCount = 0;
        while (Largest > 0)
        {
            ++ExpectedMipCount;
            Largest >>= 1U;
        }
    }
    if (Info.Levels.size() != ExpectedMipCount)
    {
        AddDiagnostic(
            OutDiagnostics,
            EAssetResult::MalformedContainer,
            "asset.ktx2.mip-contract",
            "levelCount",
            "level count contradicts the declared mip policy");
        return EAssetResult::MalformedContainer;
    }
    OutInfo = std::move(Info);
    return EAssetResult::Success;
}

EAssetResult FKTX2TextureCodec::Open(
    FAssetId ExpectedId,
    std::span<const Core::uint8> Bytes,
    const FTextureCookLimits& Limits,
    FKTX2TextureArtifact& OutArtifact,
    FAssetDiagnosticList* OutDiagnostics)
{
    OutArtifact = {};
    FKTX2TextureInfo Info;
    EAssetResult Result =
        Inspect(Bytes, Limits, Info, OutDiagnostics);
    if (Result != EAssetResult::Success)
    {
        return Result;
    }
    if (Info.TextureId != ExpectedId)
    {
        AddDiagnostic(
            OutDiagnostics,
            EAssetResult::Conflict,
            "asset.ktx2.identity",
            "assetId",
            "container identity does not match the expected identity");
        return EAssetResult::Conflict;
    }
    Core::TArray<Core::uint8> Owned(Bytes.begin(), Bytes.end());
    return FKTX2TextureArtifact::Create(
        std::move(ExpectedId),
        std::move(Info),
        std::move(Owned),
        OutArtifact);
}

} // namespace Stoner::Asset
