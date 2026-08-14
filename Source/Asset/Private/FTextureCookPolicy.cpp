#include "Asset/FTextureCook.h"

#include "FTextureCookPolicy.h"
#include "FAssetTargetProfileCodec.h"
#include "FWamrEncoderRuntime.h"

#include <algorithm>
#include <string>

namespace Stoner::Asset
{

EAssetResult FTextureCookLimits::Validate() const noexcept
{
    if (MaxDimension == 0 ||
        MaxArtifactBytes == 0 ||
        MaxMetadataBytes == 0 ||
        MaxKeyValuePairs == 0 ||
        MaxLevelBytes == 0 ||
        MaxTargetPayloadBytes == 0 ||
        MaxMipLevels == 0)
    {
        return EAssetResult::InvalidInput;
    }
    return EAssetResult::Success;
}

EAssetResult FTextureCookSettings::Validate() const noexcept
{
    if (PortableProfile != Core::FString("stoner.ktx2.portable.v1") ||
        ProducerVersion != Core::FString("022-v1"))
    {
        return EAssetResult::InvalidInput;
    }
    switch (CompressionPolicy)
    {
    case ETextureCompressionPolicy::DefaultBySemantic:
    case ETextureCompressionPolicy::ETC1S:
    case ETextureCompressionPolicy::UASTC:
    case ETextureCompressionPolicy::Uncompressed:
        break;
    default:
        return EAssetResult::UnsupportedCompression;
    }
    switch (Quality)
    {
    case ETextureCookQuality::Balanced:
    case ETextureCookQuality::High:
        return EAssetResult::Success;
    }
    return EAssetResult::InvalidInput;
}

} // namespace Stoner::Asset

namespace Stoner::Asset::Private
{
namespace
{

void AppendU32(Core::TArray<Core::uint8>& Bytes, Core::uint32 Value)
{
    for (Core::uint32 Index = 0; Index < 4; ++Index)
    {
        Bytes.push_back(static_cast<Core::uint8>(Value >> (Index * 8U)));
    }
}

void AppendU64(Core::TArray<Core::uint8>& Bytes, Core::uint64 Value)
{
    for (Core::uint32 Index = 0; Index < 8; ++Index)
    {
        Bytes.push_back(static_cast<Core::uint8>(Value >> (Index * 8U)));
    }
}

void AppendString(Core::TArray<Core::uint8>& Bytes, std::string_view Text)
{
    AppendU32(Bytes, static_cast<Core::uint32>(Text.size()));
    Bytes.insert(Bytes.end(), Text.begin(), Text.end());
}

void AddDiagnostic(
    FAssetDiagnosticList* Diagnostics,
    const char* Field,
    const char* Reason)
{
    if (Diagnostics == nullptr)
    {
        return;
    }
    FAssetDiagnostic Diagnostic;
    Diagnostic.Stage = EAssetStage::Validate;
    Diagnostic.Result = EAssetResult::UnsupportedCompression;
    Diagnostic.Severity = EAssetDiagnosticSeverity::Error;
    Diagnostic.Code = Core::FString("asset.ktx2.policy");
    Diagnostic.Participant = Core::FString("cooker.ktx2");
    Diagnostic.Field = Core::FString(Field);
    Diagnostic.Reason = Core::FString(Reason);
    Diagnostics->push_back(std::move(Diagnostic));
}

const char* ColorSpaceToken(EImageColorSpace ColorSpace)
{
    return ColorSpace == EImageColorSpace::SRGB ? "srgb" : "linear";
}

const char* AlphaToken(EImageAlphaMode Alpha)
{
    return Alpha == EImageAlphaMode::Straight ? "straight" : "none";
}

const char* MipToken(EImageMipPolicy Policy)
{
    return Policy == EImageMipPolicy::FullChain
        ? "full-chain"
        : "base-only";
}

const char* QualityToken(ETextureCookQuality Quality)
{
    return Quality == ETextureCookQuality::High ? "high" : "balanced";
}

const char* FormatToken(EImageTexelFormat Format)
{
    switch (Format)
    {
    case EImageTexelFormat::R8_UNorm: return "r8-unorm";
    case EImageTexelFormat::R8G8_UNorm: return "rg8-unorm";
    case EImageTexelFormat::R8G8B8_UNorm: return "rgb8-unorm";
    case EImageTexelFormat::R8G8B8A8_UNorm: return "rgba8-unorm";
    case EImageTexelFormat::R32G32B32_Float: return "rgb32-float";
    case EImageTexelFormat::R16G16B16A16_Float:
        return "rgba16-float";
    case EImageTexelFormat::R32G32B32A32_Float:
        return "rgba32-float";
    case EImageTexelFormat::Unknown: break;
    }
    return "unknown";
}

FKTX2EncoderMetadata MetadataString(
    const char* Key,
    const Core::FString& Value)
{
    FKTX2EncoderMetadata Entry;
    Entry.Key = Core::FString(Key);
    Entry.Value.insert(
        Entry.Value.end(), Value.View().begin(), Value.View().end());
    Entry.Value.push_back(0);
    return Entry;
}

} // namespace

EAssetResult ResolveTextureProfileSettings(
    const Core::TSharedPtr<const FAssetTargetProfileEvidence>& Profile,
    const FTextureCookSettings& LegacySettings,
    FTextureCookSettings& OutSettings,
    FAssetProfileProjectionEvidence& OutProjection,
    FAssetDiagnosticList* OutDiagnostics)
{
    OutSettings = LegacySettings;
    OutProjection = {};
    if (!Profile)
    {
        return OutSettings.Validate();
    }
    FAssetParticipantId Producer;
    (void)FAssetParticipantId::Create(
        Core::FString("cooker.ktx2"), Producer);
    const EAssetResult ProjectionResult = BuildAssetProfileProjection(
        *Profile, Producer, 1, {}, OutProjection);
    if (ProjectionResult != EAssetResult::Success)
    {
        AddDiagnostic(
            OutDiagnostics,
            "targetProfile.buildPolicy.producerSettings",
            "missing or invalid cooker.ktx2 schema v1 settings");
        return ProjectionResult;
    }
    const FAssetProducerSettingsRecord* Record =
        Profile->Profile.BuildPolicy.FindProducer(Producer);
    if (!Record || Record->Settings.size() != 4)
    {
        AddDiagnostic(
            OutDiagnostics,
            "targetProfile.buildPolicy.producerSettings",
            "cooker.ktx2 requires exactly four settings");
        return EAssetResult::InvalidInput;
    }
    const FAssetProducerSetting* AllowLossy =
        Record->Find(Core::FString("allowLossyData"));
    const FAssetProducerSetting* Compression =
        Record->Find(Core::FString("compressionPolicy"));
    const FAssetProducerSetting* Portable =
        Record->Find(Core::FString("portableProfile"));
    const FAssetProducerSetting* Quality =
        Record->Find(Core::FString("quality"));
    const auto* AllowLossyValue = AllowLossy
        ? std::get_if<bool>(&AllowLossy->Value) : nullptr;
    const auto* CompressionValue = Compression
        ? std::get_if<Core::FString>(&Compression->Value) : nullptr;
    const auto* PortableValue = Portable
        ? std::get_if<Core::FString>(&Portable->Value) : nullptr;
    const auto* QualityValue = Quality
        ? std::get_if<Core::FString>(&Quality->Value) : nullptr;
    if (!AllowLossyValue || !CompressionValue || !PortableValue || !QualityValue)
    {
        AddDiagnostic(
            OutDiagnostics,
            "targetProfile.buildPolicy.producerSettings",
            "cooker.ktx2 setting types are invalid");
        return EAssetResult::InvalidInput;
    }
    if (*CompressionValue == Core::FString("default-by-semantic"))
        OutSettings.CompressionPolicy = ETextureCompressionPolicy::DefaultBySemantic;
    else if (*CompressionValue == Core::FString("etc1s"))
        OutSettings.CompressionPolicy = ETextureCompressionPolicy::ETC1S;
    else if (*CompressionValue == Core::FString("uastc"))
        OutSettings.CompressionPolicy = ETextureCompressionPolicy::UASTC;
    else if (*CompressionValue == Core::FString("uncompressed"))
        OutSettings.CompressionPolicy = ETextureCompressionPolicy::Uncompressed;
    else
        return EAssetResult::InvalidInput;
    if (*QualityValue == Core::FString("balanced"))
        OutSettings.Quality = ETextureCookQuality::Balanced;
    else if (*QualityValue == Core::FString("high"))
        OutSettings.Quality = ETextureCookQuality::High;
    else
        return EAssetResult::InvalidInput;
    OutSettings.bAllowLossyData = *AllowLossyValue;
    OutSettings.PortableProfile = *PortableValue;
    OutSettings.ProducerVersion = Core::FString("022-v1");
    return OutSettings.Validate();
}

const char* TexturePolicyToken(ETextureCompressionPolicy Policy) noexcept
{
    switch (Policy)
    {
    case ETextureCompressionPolicy::DefaultBySemantic:
        return "default-by-semantic";
    case ETextureCompressionPolicy::ETC1S: return "etc1s";
    case ETextureCompressionPolicy::UASTC: return "uastc";
    case ETextureCompressionPolicy::Uncompressed:
        return "uncompressed";
    }
    return "unknown";
}

const char* TextureSemanticToken(ETextureSemantic Semantic) noexcept
{
    switch (Semantic)
    {
    case ETextureSemantic::Color: return "color";
    case ETextureSemantic::Normal: return "normal";
    case ETextureSemantic::Data: return "data";
    case ETextureSemantic::Unspecified: break;
    }
    return "unspecified";
}

EAssetResult ResolveTextureCookPolicy(
    const FTextureAsset& Texture,
    const FTextureCookSettings& Settings,
    ETextureCompressionPolicy& OutPolicy,
    FAssetDiagnosticList* OutDiagnostics)
{
    OutPolicy = ETextureCompressionPolicy::Uncompressed;
    if (Settings.Validate() != EAssetResult::Success ||
        Texture.GetSemantic() == ETextureSemantic::Unspecified ||
        Texture.GetMips().empty())
    {
        AddDiagnostic(
            OutDiagnostics, "settings", "invalid texture cook settings");
        return EAssetResult::InvalidInput;
    }
    if (Texture.GetSemantic() != ETextureSemantic::Color &&
        Texture.GetColorSpace() == EImageColorSpace::SRGB)
    {
        AddDiagnostic(
            OutDiagnostics,
            "colorSpace",
            "normal and data textures must remain linear");
        return EAssetResult::InvalidInput;
    }

    const bool HDR = IsImageFloatFormat(
        Texture.GetMips().front().GetFormat());
    ETextureCompressionPolicy Policy = Settings.CompressionPolicy;
    if (Policy == ETextureCompressionPolicy::DefaultBySemantic)
    {
        if (HDR || Texture.GetSemantic() == ETextureSemantic::Data)
        {
            Policy = ETextureCompressionPolicy::Uncompressed;
        }
        else if (Texture.GetSemantic() == ETextureSemantic::Normal)
        {
            Policy = ETextureCompressionPolicy::UASTC;
        }
        else
        {
            Policy = ETextureCompressionPolicy::ETC1S;
        }
    }
    if (HDR && Policy != ETextureCompressionPolicy::Uncompressed)
    {
        AddDiagnostic(
            OutDiagnostics,
            "compressionPolicy",
            "HDR cannot enter an 8-bit Basis policy");
        return EAssetResult::UnsupportedCompression;
    }
    if (Policy == ETextureCompressionPolicy::ETC1S &&
        Texture.GetSemantic() != ETextureSemantic::Color)
    {
        AddDiagnostic(
            OutDiagnostics,
            "compressionPolicy",
            "ETC1S is supported only for color textures");
        return EAssetResult::UnsupportedCompression;
    }
    if (Policy == ETextureCompressionPolicy::UASTC &&
        Texture.GetSemantic() == ETextureSemantic::Data &&
        !Settings.bAllowLossyData)
    {
        AddDiagnostic(
            OutDiagnostics,
            "allowLossyData",
            "data UASTC requires explicit lossy permission");
        return EAssetResult::UnsupportedCompression;
    }
    OutPolicy = Policy;
    return EAssetResult::Success;
}

FAssetDigest BuildTextureCookRevision(
    const FTextureAsset& Texture,
    const FTextureCookSettings& Settings,
    ETextureCompressionPolicy ResolvedPolicy)
{
    Core::TArray<Core::uint8> Bytes;
    AppendString(Bytes, Settings.PortableProfile.View());
    AppendString(Bytes, Settings.ProducerVersion.View());
    AppendString(Bytes, FWamrEncoderRuntime::ExpectedModuleSha256);
    AppendString(Bytes, Texture.GetId().ToString().View());
    AppendString(
        Bytes, Texture.GetImage()->GetSourceDigest().ToLowerHex().View());
    AppendString(Bytes, Texture.GetContentDigest().ToLowerHex().View());
    AppendString(Bytes, TextureSemanticToken(Texture.GetSemantic()));
    AppendString(Bytes, ColorSpaceToken(Texture.GetColorSpace()));
    AppendString(Bytes, AlphaToken(Texture.GetAlphaMode()));
    AppendString(Bytes, "top-left");
    AppendString(Bytes, MipToken(Texture.GetMipPolicy()));
    AppendString(Bytes, TexturePolicyToken(ResolvedPolicy));
    AppendString(Bytes, QualityToken(Settings.Quality));
    AppendU32(Bytes, Settings.bAllowLossyData ? 1U : 0U);
    AppendU32(
        Bytes, static_cast<Core::uint32>(Texture.GetMips().size()));
    for (const FImageMip& Mip : Texture.GetMips())
    {
        AppendU32(Bytes, Mip.GetExtent().Width);
        AppendU32(Bytes, Mip.GetExtent().Height);
        AppendString(Bytes, FormatToken(Mip.GetFormat()));
        AppendU64(Bytes, Mip.GetRowPitchBytes());
        AppendU64(Bytes, Mip.GetBytes().size());
        Bytes.insert(
            Bytes.end(), Mip.GetBytes().begin(), Mip.GetBytes().end());
    }
    return FAssetDigest::FromBytes(Bytes);
}

Core::TArray<FKTX2EncoderMetadata> BuildKTX2Metadata(
    const FTextureAsset& Texture,
    const FTextureCookSettings& Settings,
    ETextureCompressionPolicy ResolvedPolicy,
    const FAssetDigest& CookRevision)
{
    Core::TArray<FKTX2EncoderMetadata> Metadata = {
        MetadataString("KTXorientation", Core::FString("rd")),
        MetadataString(
            "stoner.alphaMode",
            Core::FString(AlphaToken(Texture.GetAlphaMode()))),
        MetadataString("stoner.assetId", Texture.GetId().ToString()),
        MetadataString(
            "stoner.channelCount",
            Core::FString(std::to_string(GetImageChannelCount(
                Texture.GetMips().front().GetFormat())))),
        MetadataString(
            "stoner.contentDigest",
            Texture.GetContentDigest().ToLowerHex()),
        MetadataString(
            "stoner.cookRevision", CookRevision.ToLowerHex()),
        MetadataString(
            "stoner.mipPolicy",
            Core::FString(MipToken(Texture.GetMipPolicy()))),
        MetadataString(
            "stoner.portableProfile", Settings.PortableProfile),
        MetadataString(
            "stoner.semantic",
            Core::FString(TextureSemanticToken(Texture.GetSemantic()))),
        MetadataString(
            "stoner.sourceDigest",
            Texture.GetImage()->GetSourceDigest().ToLowerHex())};
    std::sort(
        Metadata.begin(),
        Metadata.end(),
        [](const auto& Left, const auto& Right)
        {
            return Left.Key < Right.Key;
        });
    (void)ResolvedPolicy;
    return Metadata;
}

} // namespace Stoner::Asset::Private
