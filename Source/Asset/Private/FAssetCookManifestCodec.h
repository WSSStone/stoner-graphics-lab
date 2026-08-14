#pragma once

#include "Asset/FAssetCookManifest.h"

#include <span>

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult WriteAssetCookManifest(
    FAssetCookManifest& InOutManifest,
    const FAssetCookManifestLimits& Limits,
    Core::FString& OutCanonical);

[[nodiscard]] EAssetResult ParseAssetCookManifest(
    std::span<const Core::uint8> Bytes,
    const FAssetCookManifestLimits& Limits,
    FAssetCookManifest& OutManifest);

[[nodiscard]] EAssetResult ComputeAssetCookManifestGenerationId(
    const FAssetCookManifest& Manifest,
    FAssetDigest& OutGenerationId);

} // namespace Stoner::Asset::Private
