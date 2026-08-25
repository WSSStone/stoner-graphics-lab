#pragma once

#include "Asset/FAssetCookedPayload.h"

#include <span>

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult WriteAssetCookedPayload(
    const FAssetCookedPayloadHeader& Header,
    std::span<const Core::uint8> ReservedHeaderExtensions,
    std::span<const Core::uint8> Body,
    const FAssetCookedPayloadLimits& Limits,
    Core::TArray<Core::uint8>& OutBytes,
    FAssetCookedPayloadEnvelope* OutEnvelope);

[[nodiscard]] EAssetResult ParseAssetCookedPayload(
    std::span<const Core::uint8> Bytes,
    const FAssetCookedPayloadLimits& Limits,
    FAssetCookedPayloadEnvelope& OutEnvelope);

[[nodiscard]] EAssetResult ParseManifestAuthenticatedAssetCookedPayload(
    std::span<const Core::uint8> Bytes,
    const FAssetDigest& ExpectedEnvelopeDigest,
    const FAssetCookedPayloadLimits& Limits,
    FAssetCookedPayloadEnvelope& OutEnvelope);

} // namespace Stoner::Asset::Private
