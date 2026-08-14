#include "Asset/FTextureCook.h"

#include "Asset/FKTX2TextureCodec.h"
#include "Asset/FTextureAsset.h"
#include "FTextureCookPolicy.h"
#include "FAssetTargetProfileCodec.h"
#include "FKTX2ContainerCodec.h"
#include "IKTX2Encoder.h"

#include <limits>
#include <memory>

namespace Stoner::Asset
{
namespace
{

using namespace Private;

void AddCookDiagnostic(
    FAssetDiagnosticList& Diagnostics,
    EAssetResult Result,
    EAssetStage Stage,
    const char* Code,
    const char* Field,
    const char* Reason)
{
    FAssetDiagnostic Diagnostic;
    Diagnostic.Stage = Stage;
    Diagnostic.Result = Result;
    Diagnostic.Severity = EAssetDiagnosticSeverity::Error;
    Diagnostic.Code = Core::FString(Code);
    Diagnostic.Participant = Core::FString("cooker.ktx2");
    Diagnostic.Field = Core::FString(Field);
    Diagnostic.Reason = Core::FString(Reason);
    Diagnostics.push_back(std::move(Diagnostic));
}

FAssetCookResult Fail(
    EAssetResult Result,
    const FAssetCookRequest& Request,
    const FAssetProfileProjectionEvidence& Projection,
    FAssetDiagnosticList Diagnostics)
{
    FAssetCookResult Failed;
    Failed.Result = Result;
    Failed.TargetProfile = Request.TargetProfile.IsEmpty() &&
            Request.TargetProfileEvidence
        ? Request.TargetProfileEvidence->Profile.DisplayName
        : Request.TargetProfile;
    Failed.Diagnostics = std::move(Diagnostics);
    Failed.TargetProfileEvidence = Request.TargetProfileEvidence;
    Failed.ProfileProjection = Projection;
    return Failed;
}

bool CheckedMultiply(
    Core::uint64 Left,
    Core::uint64 Right,
    Core::uint64& Out)
{
    if (Left != 0 &&
        Right > std::numeric_limits<Core::uint64>::max() / Left)
    {
        return false;
    }
    Out = Left * Right;
    return true;
}

bool ExpandMipToRGBA8(
    const FImageMip& Mip,
    ETextureSemantic Semantic,
    EImageAlphaMode AlphaMode,
    Core::TArray<Core::uint8>& OutBytes)
{
    OutBytes.clear();
    const Core::uint32 Channels = GetImageChannelCount(Mip.GetFormat());
    if (IsImageFloatFormat(Mip.GetFormat()) ||
        Channels == 0 || Channels > 4)
    {
        return false;
    }
    Core::uint64 PixelCount = 0;
    Core::uint64 OutputBytes = 0;
    if (!CheckedMultiply(
            Mip.GetExtent().Width,
            Mip.GetExtent().Height,
            PixelCount) ||
        !CheckedMultiply(PixelCount, 4, OutputBytes) ||
        OutputBytes > std::numeric_limits<Core::usize>::max())
    {
        return false;
    }
    const std::span<const Core::uint8> Source = Mip.GetBytes();
    if (Source.size() != PixelCount * Channels)
    {
        return false;
    }

    OutBytes.resize(static_cast<Core::usize>(OutputBytes));
    for (Core::uint64 Pixel = 0; Pixel < PixelCount; ++Pixel)
    {
        const Core::usize SourceOffset =
            static_cast<Core::usize>(Pixel * Channels);
        const Core::usize TargetOffset =
            static_cast<Core::usize>(Pixel * 4);
        const Core::uint8 First = Source[SourceOffset];
        if (Channels == 1)
        {
            if (Semantic == ETextureSemantic::Color)
            {
                OutBytes[TargetOffset] = First;
                OutBytes[TargetOffset + 1] = First;
                OutBytes[TargetOffset + 2] = First;
            }
            else
            {
                OutBytes[TargetOffset] = First;
                OutBytes[TargetOffset + 1] = 0;
                OutBytes[TargetOffset + 2] = 0;
            }
            OutBytes[TargetOffset + 3] = 255;
        }
        else if (Channels == 2)
        {
            if (Semantic == ETextureSemantic::Color &&
                AlphaMode == EImageAlphaMode::Straight)
            {
                OutBytes[TargetOffset] = First;
                OutBytes[TargetOffset + 1] = First;
                OutBytes[TargetOffset + 2] = First;
                OutBytes[TargetOffset + 3] = Source[SourceOffset + 1];
            }
            else
            {
                OutBytes[TargetOffset] = First;
                OutBytes[TargetOffset + 1] = Source[SourceOffset + 1];
                OutBytes[TargetOffset + 2] = 0;
                OutBytes[TargetOffset + 3] = 255;
            }
        }
        else
        {
            OutBytes[TargetOffset] = First;
            OutBytes[TargetOffset + 1] = Source[SourceOffset + 1];
            OutBytes[TargetOffset + 2] = Source[SourceOffset + 2];
            OutBytes[TargetOffset + 3] =
                Channels == 4 ? Source[SourceOffset + 3] : 255;
        }
    }
    return true;
}

bool MatchesSource(
    const FKTX2TextureInfo& Info,
    const FTextureAsset& Texture,
    const FTextureCookSettings& Settings,
    ETextureCompressionPolicy Policy,
    const FAssetDigest& CookRevision)
{
    if (Info.TextureId != Texture.GetId() ||
        Info.SourceDigest != Texture.GetImage()->GetSourceDigest() ||
        Info.ContentDigest != Texture.GetContentDigest() ||
        Info.CookRevision != CookRevision ||
        Info.ProducerVersion != Settings.ProducerVersion ||
        Info.PortableProfile != Settings.PortableProfile ||
        Info.CompressionPolicy != Policy ||
        Info.Semantic != Texture.GetSemantic() ||
        Info.ColorSpace != Texture.GetColorSpace() ||
        Info.AlphaMode != Texture.GetAlphaMode() ||
        Info.Origin != Texture.GetOrigin() ||
        Info.MipPolicy != Texture.GetMipPolicy() ||
        Info.SourceChannelCount != GetImageChannelCount(
            Texture.GetMips().front().GetFormat()) ||
        Info.BaseExtent != Texture.GetMips().front().GetExtent() ||
        Info.Levels.size() != Texture.GetMips().size())
    {
        return false;
    }
    for (Core::usize Index = 0; Index < Info.Levels.size(); ++Index)
    {
        if (Info.Levels[Index].MipLevel != Index ||
            Info.Levels[Index].Extent !=
                Texture.GetMips()[Index].GetExtent())
        {
            return false;
        }
    }
    return Policy != ETextureCompressionPolicy::Uncompressed ||
        (Info.StoredTexelFormat.has_value() &&
         *Info.StoredTexelFormat ==
             Texture.GetMips().front().GetFormat());
}

} // namespace

FAssetExtensionCapability FKTX2TextureCooker::GetCapability() const
{
    FAssetExtensionCapability Capability;
    Capability.Kind = EAssetExtensionKind::Cooker;
    (void)FAssetParticipantId::Create(
        Core::FString("cooker.ktx2"), Capability.Participant);
    (void)FAssetProducerVersion::Create(
        Core::FString("022-v1"), Capability.ProducerVersion);
    Capability.Priority = 100;
    Capability.FormatHints = {Core::FString("ktx2")};
    return Capability;
}

EAssetResult FKTX2TextureCooker::GetRelevantProfileEvidence(
    const FAssetTargetProfileEvidence& Profile,
    FAssetProfileProjectionEvidence& OutEvidence) const
{
    FAssetParticipantId Producer;
    (void)FAssetParticipantId::Create(
        Core::FString("cooker.ktx2"), Producer);
    return BuildAssetProfileProjection(Profile, Producer, 1, {}, OutEvidence);
}

FAssetCookResult FKTX2TextureCooker::Cook(
    const FAssetCookRequest& Request)
{
    FAssetDiagnosticList Diagnostics;
    const auto Parameters =
        std::dynamic_pointer_cast<const FTextureCookParameters>(
            Request.Parameters);
    const auto Texture =
        std::dynamic_pointer_cast<const FTextureAsset>(Request.Payload);
    FTextureCookSettings Settings;
    FAssetProfileProjectionEvidence ProfileProjection;
    if (!Parameters || !Texture ||
        Parameters->TextureId != Texture->GetId() ||
        Request.Metadata.Id != Texture->GetId() ||
        Request.Metadata.Validate() != EAssetResult::Success ||
        Parameters->Limits.Validate() != EAssetResult::Success ||
        !Texture->GetImage() || Texture->GetMips().empty() ||
        Texture->GetMips().size() > Parameters->Limits.MaxMipLevels)
    {
        AddCookDiagnostic(
            Diagnostics,
            EAssetResult::InvalidInput,
            EAssetStage::Validate,
            "asset.ktx2.cook-request",
            "request",
            "typed parameters, metadata, or texture payload are invalid");
        return Fail(
            EAssetResult::InvalidInput,
            Request,
            ProfileProjection,
            std::move(Diagnostics));
    }
    EAssetResult Result = ResolveTextureProfileSettings(
        Request.TargetProfileEvidence,
        Parameters->Settings,
        Settings,
        ProfileProjection,
        &Diagnostics);
    if (Result != EAssetResult::Success)
    {
        return Fail(Result, Request, ProfileProjection, std::move(Diagnostics));
    }
    for (const FImageMip& Mip : Texture->GetMips())
    {
        if (Mip.GetExtent().Width > Parameters->Limits.MaxDimension ||
            Mip.GetExtent().Height > Parameters->Limits.MaxDimension ||
            Mip.GetBytes().size() > Parameters->Limits.MaxLevelBytes)
        {
            AddCookDiagnostic(
                Diagnostics,
                EAssetResult::KTX2LimitExceeded,
                EAssetStage::Validate,
                "asset.ktx2.cook-limit",
                "mips",
                "source texture exceeds configured cook limits");
            return Fail(
                EAssetResult::KTX2LimitExceeded,
                Request,
                ProfileProjection,
                std::move(Diagnostics));
        }
    }

    ETextureCompressionPolicy Policy =
        ETextureCompressionPolicy::Uncompressed;
    Result = ResolveTextureCookPolicy(
        *Texture,
        Settings,
        Policy,
        &Diagnostics);
    if (Result != EAssetResult::Success)
    {
        return Fail(
            Result, Request, ProfileProjection, std::move(Diagnostics));
    }

    const FAssetDigest CookRevision = BuildTextureCookRevision(
        *Texture, Settings, Policy);
    const Core::TArray<FKTX2EncoderMetadata> Metadata =
        BuildKTX2Metadata(
            *Texture,
            Settings,
            Policy,
            CookRevision);
    Core::TArray<Core::uint8> Bytes;
    if (Policy == ETextureCompressionPolicy::Uncompressed)
    {
        Result = FKTX2ContainerCodec::WriteUncompressed(
            *Texture, Metadata, Bytes, &Diagnostics);
    }
    else
    {
        FKTX2EncoderRequest EncoderRequest;
        EncoderRequest.Policy = Policy;
        EncoderRequest.Quality = Settings.Quality;
        EncoderRequest.bSRGB =
            Texture->GetSemantic() == ETextureSemantic::Color &&
            Texture->GetColorSpace() == EImageColorSpace::SRGB;
        EncoderRequest.bNormal =
            Texture->GetSemantic() == ETextureSemantic::Normal;
        EncoderRequest.bForceAlpha =
            Texture->GetAlphaMode() == EImageAlphaMode::Straight;
        EncoderRequest.Metadata = Metadata;
        EncoderRequest.MaxOutputBytes =
            Parameters->Limits.MaxArtifactBytes;

        Core::TArray<Core::TArray<Core::uint8>> ExpandedMips;
        ExpandedMips.reserve(Texture->GetMips().size());
        EncoderRequest.Mips.reserve(Texture->GetMips().size());
        for (const FImageMip& Mip : Texture->GetMips())
        {
            ExpandedMips.emplace_back();
            if (!ExpandMipToRGBA8(
                    Mip,
                    Texture->GetSemantic(),
                    Texture->GetAlphaMode(),
                    ExpandedMips.back()))
            {
                AddCookDiagnostic(
                    Diagnostics,
                    EAssetResult::UnsupportedCompression,
                    EAssetStage::Validate,
                    "asset.ktx2.basis-source",
                    "format",
                    "Basis cooking requires a supported LDR UNorm source");
                return Fail(
                    EAssetResult::UnsupportedCompression,
                    Request,
                    ProfileProjection,
                    std::move(Diagnostics));
            }
            EncoderRequest.Mips.push_back({
                Mip.GetExtent(),
                std::span<const Core::uint8>(ExpandedMips.back())});
        }
        FCanonicalBasisEncoder Encoder;
        FKTX2EncoderResult Encoded = Encoder.Encode(EncoderRequest);
        Result = Encoded.Result;
        Bytes = std::move(Encoded.Bytes);
        Diagnostics.insert(
            Diagnostics.end(),
            std::make_move_iterator(Encoded.Diagnostics.begin()),
            std::make_move_iterator(Encoded.Diagnostics.end()));
    }
    if (Result != EAssetResult::Success)
    {
        return Fail(
            Result, Request, ProfileProjection, std::move(Diagnostics));
    }
    if (Bytes.empty() ||
        Bytes.size() > Parameters->Limits.MaxArtifactBytes)
    {
        AddCookDiagnostic(
            Diagnostics,
            EAssetResult::KTX2LimitExceeded,
            EAssetStage::Cook,
            "asset.ktx2.artifact-limit",
            "artifactBytes",
            "encoded artifact exceeds configured cook limits");
        return Fail(
            EAssetResult::KTX2LimitExceeded,
            Request,
            ProfileProjection,
            std::move(Diagnostics));
    }

    FKTX2TextureArtifact Artifact;
    Result = FKTX2TextureCodec::Open(
        Texture->GetId(),
        Bytes,
        Parameters->Limits,
        Artifact,
        &Diagnostics);
    if (Result != EAssetResult::Success)
    {
        return Fail(
            Result, Request, ProfileProjection, std::move(Diagnostics));
    }
    if (!MatchesSource(
            Artifact.GetInfo(),
            *Texture,
            Settings,
            Policy,
            CookRevision))
    {
        AddCookDiagnostic(
            Diagnostics,
            EAssetResult::Conflict,
            EAssetStage::Container,
            "asset.ktx2.reopen-mismatch",
            "container",
            "reopened artifact does not match the source contract");
        return Fail(
            EAssetResult::Conflict,
            Request,
            ProfileProjection,
            std::move(Diagnostics));
    }

    FAssetCookResult Cooked;
    Cooked.Result = EAssetResult::Success;
    Cooked.TargetProfile = Request.TargetProfile.IsEmpty() &&
            Request.TargetProfileEvidence
        ? Request.TargetProfileEvidence->Profile.DisplayName
        : Request.TargetProfile;
    Cooked.Artifact.assign(
        Artifact.GetBytes().begin(), Artifact.GetBytes().end());
    Cooked.CookDigest = Artifact.GetArtifactDigest();
    Cooked.Payload =
        Core::MakeShared<FKTX2TextureArtifact>(std::move(Artifact));
    Cooked.Diagnostics = std::move(Diagnostics);
    Cooked.TargetProfileEvidence = Request.TargetProfileEvidence;
    Cooked.ProfileProjection = std::move(ProfileProjection);
    return Cooked;
}

} // namespace Stoner::Asset
