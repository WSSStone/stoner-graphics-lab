#pragma once

#include "Asset/FAssetDependency.h"
#include "Asset/FAssetDerivedKey.h"
#include "Asset/FAssetParticipant.h"
#include "Asset/FAssetTargetProfile.h"
#include "Core/FPlatformTypes.h"
#include "Core/FString.h"
#include "Core/TArray.h"

#include <optional>

namespace Stoner::Asset
{

enum class EAssetCookSelectionMode : Core::uint8
{
    ExplicitRoots,
    CookAll
};

struct FAssetCookSelection
{
    EAssetCookSelectionMode Mode = EAssetCookSelectionMode::ExplicitRoots;
    Core::TArray<FAssetId> Roots;
    Core::TArray<Core::FString> SourceScopes;
    FAssetDigest DiscoveryRulesVersion;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(const FAssetCookSelection&) const = default;
};

struct FAssetCookManifestSourceRecord
{
    FAssetId AssetId;
    FAssetDigest Version;
    Core::FString Role;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(const FAssetCookManifestSourceRecord&) const = default;
};

struct FAssetCookManifestDependencyRecord
{
    FAssetId AssetId;
    Core::FString Role;
    std::optional<FAssetDigest> RequiredVersion;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(const FAssetCookManifestDependencyRecord&) const = default;
};

struct FAssetCookManifestParticipant
{
    FAssetParticipantId Id;
    FAssetProducerVersion Version;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(const FAssetCookManifestParticipant&) const = default;
};

struct FAssetCookManifestRecord
{
    FAssetId AssetId;
    Core::FString AssetType;
    FAssetDigest SourceVersion;
    Core::TArray<FAssetCookManifestSourceRecord> SourceManifest;
    FAssetCookManifestParticipant Importer;
    FAssetCookManifestParticipant Cooker;
    FAssetCookManifestParticipant Codec;
    FAssetDerivedKey DerivedKey;
    Core::uint32 PayloadSchemaVersion = 0;
    Core::FString PayloadLocator;
    Core::uint64 PayloadBytes = 0;
    FAssetDigest EnvelopeDigest;
    Core::TArray<FAssetCookManifestDependencyRecord> Dependencies;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(const FAssetCookManifestRecord&) const = default;
};

struct FAssetCookManifestLimits
{
    Core::uint64 MaxManifestBytes = 256ULL * 1024ULL * 1024ULL;
    Core::uint32 MaxRecords = 100000;
    Core::uint32 MaxSourcesPerRecord = 100000;
    Core::uint32 MaxDependenciesPerRecord = 100000;
    Core::uint32 MaxRoots = 100000;
    Core::uint32 MaxSourceScopes = 1024;
    Core::uint32 MaxExtensions = 64;
    Core::uint32 MaxTextBytes = 4096;

    [[nodiscard]] EAssetResult Validate() const noexcept;
};

struct FAssetCookManifest
{
    static constexpr Core::uint32 CurrentSchemaVersion = 1;

    Core::FString Schema = Core::FString("stoner.asset-cook-manifest");
    Core::uint32 SchemaVersion = CurrentSchemaVersion;
    FAssetDigest GenerationId;
    FAssetTargetProfileEvidence TargetProfile;
    FAssetCookSelection Selection;
    FAssetDigest SnapshotDigest;
    FAssetDigest LimitsDigest;
    Core::TArray<FAssetCookManifestRecord> Records;
    Core::TArray<Core::FString> RequiredExtensions;

    [[nodiscard]] EAssetResult Validate(
        const FAssetCookManifestLimits& Limits = {}) const noexcept;
    [[nodiscard]] bool operator==(const FAssetCookManifest&) const = default;
};

} // namespace Stoner::Asset
