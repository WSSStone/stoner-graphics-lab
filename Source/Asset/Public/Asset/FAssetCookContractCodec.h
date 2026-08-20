#pragma once

#include "Asset/FAssetCookedPayload.h"
#include "Asset/FAssetDerivedDataEntry.h"
#include "Asset/FAssetCookManifest.h"
#include "Asset/FAssetPayload.h"
#include "Asset/FAssetTargetProfile.h"
#include "Asset/FCurrentGenerationPointer.h"
#include "Core/TSharedPtr.h"

#include <span>

namespace Stoner::Asset
{

class FAssetCookContractCodec
{
public:
    [[nodiscard]] static EAssetResult ParseTargetProfile(
        std::span<const Core::uint8> Bytes,
        FAssetTargetProfileEvidence& OutEvidence);

    [[nodiscard]] static EAssetResult WriteTargetProfile(
        const FAssetTargetProfile& Profile,
        Core::FString& OutCanonical,
        FAssetTargetProfileEvidence* OutEvidence = nullptr);

    [[nodiscard]] static EAssetResult WriteCookedPayload(
        const FAssetCookedPayloadHeader& Header,
        std::span<const Core::uint8> ReservedHeaderExtensions,
        std::span<const Core::uint8> Body,
        const FAssetCookedPayloadLimits& Limits,
        Core::TArray<Core::uint8>& OutBytes,
        FAssetCookedPayloadEnvelope* OutEnvelope = nullptr);

    [[nodiscard]] static EAssetResult ParseCookedPayload(
        std::span<const Core::uint8> Bytes,
        const FAssetCookedPayloadLimits& Limits,
        FAssetCookedPayloadEnvelope& OutEnvelope);

    [[nodiscard]] static EAssetResult WriteTypedPayload(
        const FAssetPayload& Payload,
        const FAssetCookedPayloadLimits& Limits,
        Core::TArray<Core::uint8>& OutBytes,
        FAssetCookedPayloadEnvelope* OutEnvelope = nullptr);

    [[nodiscard]] static EAssetResult DescribeTypedPayload(
        const FAssetPayload& Payload,
        FAssetCookedPayloadHeader& OutHeader);

    [[nodiscard]] static EAssetResult LoadTypedPayload(
        std::span<const Core::uint8> Bytes,
        const FAssetCookedPayloadLimits& Limits,
        Core::TSharedPtr<const FAssetPayload>& OutPayload,
        FAssetCookedPayloadEnvelope* OutEnvelope = nullptr);

    [[nodiscard]] static EAssetResult WriteManifest(
        FAssetCookManifest& InOutManifest,
        const FAssetCookManifestLimits& Limits,
        Core::FString& OutCanonical);

    [[nodiscard]] static EAssetResult ParseManifest(
        std::span<const Core::uint8> Bytes,
        const FAssetCookManifestLimits& Limits,
        FAssetCookManifest& OutManifest);

    [[nodiscard]] static EAssetResult ComputeManifestGenerationId(
        const FAssetCookManifest& Manifest,
        FAssetDigest& OutGenerationId);

    [[nodiscard]] static EAssetResult BuildDerivedKey(
        const FAssetDerivedKeyEvidence& Evidence,
        FAssetDerivedKey& OutKey);

    [[nodiscard]] static EAssetResult BuildProfileProjection(
        const FAssetTargetProfileEvidence& Profile,
        const FAssetParticipantId& Producer,
        Core::uint32 ExpectedSchemaVersion,
        std::span<const Core::FString> RelevantTargetFields,
        FAssetProfileProjectionEvidence& OutProjection);

    [[nodiscard]] static EAssetResult WriteDerivedDataEntry(
        const FAssetDerivedDataEntry& Entry,
        Core::FString& OutCanonical);

    [[nodiscard]] static EAssetResult ParseDerivedDataEntry(
        std::span<const Core::uint8> Bytes,
        const FAssetDerivedDataEntryLimits& Limits,
        FAssetDerivedDataEntry& OutEntry);

    [[nodiscard]] static EAssetResult WriteCurrentPointer(
        const FCurrentGenerationPointer& Pointer,
        Core::FString& OutCanonical);

    [[nodiscard]] static EAssetResult ParseCurrentPointer(
        std::span<const Core::uint8> Bytes,
        FCurrentGenerationPointer& OutPointer);
};

} // namespace Stoner::Asset
