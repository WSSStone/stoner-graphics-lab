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

[[nodiscard]] EAssetResult ParsePreviouslyAuthenticatedAssetCookedPayload(
    std::span<const Core::uint8> Bytes,
    const FAssetDigest& ExpectedEnvelopeDigest,
    const FAssetCookedPayloadLimits& Limits,
    FAssetCookedPayloadEnvelope& OutEnvelope);

// The returned body view borrows Bytes and remains valid only while Bytes is
// alive. The envelope intentionally contains only the parsed header and
// bounded header extensions so the strict loader can typed-decode a previously
// authenticated body without allocating a second full-size body buffer.
[[nodiscard]] EAssetResult ParsePreviouslyAuthenticatedAssetCookedPayloadView(
    std::span<const Core::uint8> Bytes,
    const FAssetDigest& ExpectedEnvelopeDigest,
    const FAssetCookedPayloadLimits& Limits,
    FAssetCookedPayloadEnvelope& OutEnvelope,
    std::span<const Core::uint8>& OutBody);

} // namespace Stoner::Asset::Private
