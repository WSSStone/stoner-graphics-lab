#pragma once

#include "Asset/FAssetCookContractCodec.h"
#include "Asset/FAssetCookedExtensions.h"
#include "Asset/FAssetDerivedKey.h"
#include "Asset/IAssetCooker.h"

namespace Stoner::AssetCooker::Private
{

struct FAssetCookerSelection
{
    Asset::FAssetParticipantId CookerId;
    Core::TSharedPtr<const Asset::FAssetCookParameters> Parameters;
    Asset::FAssetCookedPayloadHeader OutputContract;
    Asset::FAssetCookedTargetDecision TargetDecision;
    Asset::FAssetProfileProjectionEvidence ProfileProjection;
    Core::TArray<Asset::FAssetDerivedNamedEvidence> AdditionalEvidence;
};

[[nodiscard]] Asset::EAssetResult SelectAssetCooker(
    Asset::EAssetCookedFamily Family,
    const Asset::FAssetPayload& Payload,
    const Asset::FAssetTargetProfileEvidence& Profile,
    const Asset::FAssetExtensionRegistry& Registry,
    FAssetCookerSelection& OutSelection);

[[nodiscard]] Asset::EAssetResult NormalizeCookedArtifact(
    const Asset::FAssetParticipantId& CookerId,
    const Asset::FAssetCookedPayloadHeader& OutputContract,
    const Asset::FAssetCookResult& Cooked,
    const Asset::FAssetCookedPayloadLimits& Limits,
    Core::TArray<Core::uint8>& OutEnvelopeBytes);

} // namespace Stoner::AssetCooker::Private
