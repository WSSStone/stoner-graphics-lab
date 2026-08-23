#include "FAssetCookerSelection.h"

#include "Asset/FKTX2TextureArtifact.h"
#include "Asset/FTextureAsset.h"

#include <algorithm>
#include <span>

namespace Stoner::AssetCooker::Private
{
namespace
{

void AppendU32(Core::TArray<Core::uint8>& Bytes, Core::uint32 Value)
{
    for (Core::uint32 Index = 0; Index < 4; ++Index)
        Bytes.push_back(static_cast<Core::uint8>(Value >> (Index * 8U)));
}

Asset::FAssetDigest TextureLayoutDigest(
    const Asset::FTextureAsset& Texture)
{
    Core::TArray<Core::uint8> Bytes;
    Bytes.reserve(24U + Texture.GetMips().size() * 12U);
    AppendU32(Bytes, static_cast<Core::uint32>(Texture.GetSemantic()));
    AppendU32(Bytes, static_cast<Core::uint32>(Texture.GetColorSpace()));
    AppendU32(Bytes, static_cast<Core::uint32>(Texture.GetAlphaMode()));
    AppendU32(Bytes, static_cast<Core::uint32>(Texture.GetMipPolicy()));
    AppendU32(Bytes, static_cast<Core::uint32>(Texture.GetMips().size()));
    for (const auto& Mip : Texture.GetMips())
    {
        AppendU32(Bytes, Mip.GetExtent().Width);
        AppendU32(Bytes, Mip.GetExtent().Height);
        AppendU32(Bytes, static_cast<Core::uint32>(Mip.GetFormat()));
    }
    return Asset::FAssetDigest::FromBytes(Bytes);
}

Asset::EAssetResult TextureTargetDecision(
    const Asset::FTextureAsset& Texture,
    const Asset::FAssetTargetProfile& Profile,
    Asset::FAssetCookedTargetDecision& OutDecision)
{
    const Asset::EAssetResult Direct =
        Asset::ResolveAssetCookedTargetDecision(
            Asset::EAssetCookedFamily::ImageTexture,
            Texture, Profile, OutDecision);
    if (Direct == Asset::EAssetResult::Success) return Direct;
    if (Profile.TextureFallback != Asset::EAssetTextureFallback::PortableKTX2)
        return Direct;
    OutDecision.Selection = Core::FString("texture-fallback:portable-ktx2");
    OutDecision.bUsedFallback = true;
    return Asset::EAssetResult::Success;
}

} // namespace

Asset::EAssetResult SelectAssetCooker(
    Asset::EAssetCookedFamily Family,
    const Asset::FAssetPayload& Payload,
    const Asset::FAssetTargetProfileEvidence& Profile,
    const Asset::FAssetExtensionRegistry& Registry,
    FAssetCookerSelection& OutSelection)
{
    OutSelection = {};
    if (Profile.Validate() != Asset::EAssetResult::Success)
        return Asset::EAssetResult::InvalidInput;

    const auto* Texture = dynamic_cast<const Asset::FTextureAsset*>(&Payload);
    if (Texture)
    {
        if (Family != Asset::EAssetCookedFamily::ImageTexture ||
            Asset::FAssetParticipantId::Create(
                Core::FString("cooker.ktx2"), OutSelection.CookerId) !=
                Asset::EAssetResult::Success)
            return Asset::EAssetResult::TypeMismatch;
    }
    else if (Asset::GetAssetCookedParticipant(
                 Family, Asset::EAssetExtensionKind::Cooker,
                 OutSelection.CookerId) != Asset::EAssetResult::Success)
    {
        return Asset::EAssetResult::Unsupported;
    }

    const auto CookerLease = Registry.Acquire(
        Asset::EAssetExtensionKind::Cooker, OutSelection.CookerId);
    const auto Cooker = CookerLease.Get<Asset::IAssetCooker>();
    if (!Cooker || Cooker->GetRelevantProfileEvidence(
            Profile, OutSelection.ProfileProjection) !=
            Asset::EAssetResult::Success)
    {
        OutSelection = {};
        return Asset::EAssetResult::InvalidInput;
    }

    if (Texture)
    {
        if (TextureTargetDecision(
                *Texture, Profile.Profile,
                OutSelection.TargetDecision) != Asset::EAssetResult::Success)
        {
            OutSelection = {};
            return Asset::EAssetResult::Unsupported;
        }
        auto Parameters = Core::MakeShared<Asset::FTextureCookParameters>();
        Parameters->TextureId = Texture->GetId();
        Parameters->Limits.MaxArtifactBytes = std::min(
            Parameters->Limits.MaxArtifactBytes,
            Profile.Profile.Limits.MaxPayloadBytes);
        Parameters->Limits.MaxLevelBytes = std::min(
            Parameters->Limits.MaxLevelBytes,
            Profile.Profile.Limits.MaxPayloadBytes);
        Parameters->Limits.MaxTargetPayloadBytes =
            Profile.Profile.Limits.MaxPayloadBytes;
        if (Parameters->Settings.Validate() != Asset::EAssetResult::Success ||
            Parameters->Limits.Validate() != Asset::EAssetResult::Success)
        {
            OutSelection = {};
            return Asset::EAssetResult::InvalidInput;
        }
        OutSelection.Parameters = std::move(Parameters);
        OutSelection.OutputContract.AssetId = Texture->GetId();
        OutSelection.OutputContract.AssetType = Texture->GetAssetType();
        OutSelection.OutputContract.CodecId = Core::FString("stoner.ktx2");
        OutSelection.OutputContract.CodecVersion = 1;
        OutSelection.OutputContract.PayloadSchemaVersion = 1;
        OutSelection.AdditionalEvidence = {
            {Core::FString("texture.content"), Texture->GetContentDigest()},
            {Core::FString("texture.layout"), TextureLayoutDigest(*Texture)}};
        return Asset::EAssetResult::Success;
    }

    if (Asset::ResolveAssetCookedTargetDecision(
            Family, Payload, Profile.Profile,
            OutSelection.TargetDecision) != Asset::EAssetResult::Success ||
        Asset::FAssetCookContractCodec::DescribeTypedPayload(
            Payload, OutSelection.OutputContract) !=
            Asset::EAssetResult::Success)
    {
        OutSelection = {};
        return Asset::EAssetResult::Unsupported;
    }
    return Asset::EAssetResult::Success;
}

Asset::EAssetResult NormalizeCookedArtifact(
    const Asset::FAssetParticipantId& CookerId,
    const Asset::FAssetCookedPayloadHeader& OutputContract,
    const Asset::FAssetCookResult& Cooked,
    const Asset::FAssetCookedPayloadLimits& Limits,
    Core::TArray<Core::uint8>& OutEnvelopeBytes)
{
    OutEnvelopeBytes.clear();
    if (Cooked.Result != Asset::EAssetResult::Success ||
        Cooked.Artifact.empty())
        return Asset::EAssetResult::InvalidInput;
    Asset::FAssetParticipantId KtxCooker;
    (void)Asset::FAssetParticipantId::Create(
        Core::FString("cooker.ktx2"), KtxCooker);
    if (CookerId != KtxCooker)
    {
        OutEnvelopeBytes = Cooked.Artifact;
        return Asset::EAssetResult::Success;
    }
    const auto Artifact = std::dynamic_pointer_cast<
        const Asset::FKTX2TextureArtifact>(Cooked.Payload);
    if (!Artifact || Artifact->GetId() != OutputContract.AssetId ||
        Artifact->GetBytes().size() != Cooked.Artifact.size() ||
        !std::equal(Artifact->GetBytes().begin(), Artifact->GetBytes().end(),
            Cooked.Artifact.begin(), Cooked.Artifact.end()) ||
        Artifact->GetArtifactDigest() != Cooked.CookDigest)
        return Asset::EAssetResult::TypeMismatch;

    Asset::FAssetCookedPayloadEnvelope Envelope;
    const Asset::EAssetResult Written =
        Asset::FAssetCookContractCodec::WriteTypedPayload(
            *Artifact, Limits, OutEnvelopeBytes, &Envelope);
    if (Written != Asset::EAssetResult::Success ||
        Envelope.Header.AssetId != OutputContract.AssetId ||
        Envelope.Header.AssetType != OutputContract.AssetType ||
        Envelope.Header.CodecId != OutputContract.CodecId ||
        Envelope.Header.CodecVersion != OutputContract.CodecVersion ||
        Envelope.Header.PayloadSchemaVersion !=
            OutputContract.PayloadSchemaVersion)
    {
        OutEnvelopeBytes.clear();
        return Written == Asset::EAssetResult::Success
            ? Asset::EAssetResult::TypeMismatch : Written;
    }
    return Asset::EAssetResult::Success;
}

} // namespace Stoner::AssetCooker::Private
