#pragma once

#include "Asset/FAssetTargetProfile.h"

#include <span>

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult ParseAssetTargetProfile(
    std::span<const Core::uint8> Bytes,
    FAssetTargetProfileEvidence& OutEvidence);

[[nodiscard]] EAssetResult WriteAssetTargetProfile(
    const FAssetTargetProfile& Profile,
    Core::FString& OutCanonical,
    FAssetTargetProfileEvidence* OutEvidence = nullptr);

[[nodiscard]] EAssetResult BuildAssetProfileProjection(
    const FAssetTargetProfileEvidence& Profile,
    const FAssetParticipantId& Producer,
    Core::uint32 ExpectedSchemaVersion,
    std::span<const Core::FString> RelevantTargetFields,
    FAssetProfileProjectionEvidence& OutProjection);

} // namespace Stoner::Asset::Private
