#pragma once

#include "Asset/FAssetDerivedKey.h"
#include "Core/FPlatformTypes.h"
#include "Core/FString.h"
#include "Core/TArray.h"

namespace Stoner::Asset
{

struct FAssetDerivedDataEntry
{
    static constexpr Core::uint32 CurrentSchemaVersion = 1;

    Core::FString Schema = Core::FString("stoner.asset-derived-entry");
    Core::uint32 SchemaVersion = CurrentSchemaVersion;
    FAssetDerivedKey DerivedKey;
    FAssetDerivedKeyEvidence Evidence;
    FAssetId AssetId;
    FAssetParticipantId CodecId;
    FAssetProducerVersion CodecVersion;
    Core::uint32 PayloadSchemaVersion = 0;
    FAssetDigest RelevantProfileDigest;
    Core::FString PayloadLocator = Core::FString("Payload.sgasset");
    Core::uint64 PayloadBytes = 0;
    FAssetDigest EnvelopeDigest;
    Core::TArray<Core::FString> RequiredExtensions;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(const FAssetDerivedDataEntry&) const = default;
};

struct FAssetDerivedDataEntryLimits
{
    Core::uint64 MaxMetadataBytes = 4ULL * 1024ULL * 1024ULL;
    Core::uint32 MaxSources = 4096;
    Core::uint32 MaxDependencies = 65536;
    Core::uint32 MaxAdditionalEvidence = 64;
    Core::uint32 MaxRequiredExtensions = 256;
};

} // namespace Stoner::Asset
