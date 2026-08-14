#pragma once

#include "Asset/AssetMinimal.h"
#include "Core/FPlatformFileSystem.h"
#include "Core/FString.h"
#include "Core/TArray.h"

#include <chrono>

namespace Stoner::AssetCooker::Private
{

enum class EDerivedDataLookupStatus : Core::uint8
{
    Miss,
    Hit,
    Invalid,
    IoFailure
};

enum class EDerivedDataInstallStatus : Core::uint8
{
    Installed,
    EquivalentWinner,
    Failed
};

struct FDerivedDataStorePaths
{
    Core::FString EntryDirectory;
    Core::FString EntryMetadata;
    Core::FString EntryPayload;
    Core::FString LeaseFile;
    Core::FString StagingRoot;
    Core::FString QuarantineRoot;
};

struct FDerivedDataLookupRequest
{
    Core::FString Root;
    Asset::FAssetDerivedKey DerivedKey;
    Asset::FAssetDerivedKeyEvidence Evidence;
    Core::TArray<Core::FString> RequiredExtensions;
    Asset::FAssetCookedPayloadLimits PayloadLimits;
    Asset::FAssetDerivedDataEntryLimits EntryLimits;
};

struct FDerivedDataLookupResult
{
    EDerivedDataLookupStatus Status = EDerivedDataLookupStatus::IoFailure;
    Asset::EAssetResult Result = Asset::EAssetResult::ProcessingFailure;
    Core::FString StableReason;
    Asset::FAssetDerivedDataEntry Entry;
    Core::TArray<Core::uint8> Payload;
    Asset::FAssetDigest FailureEvidenceDigest;
};

struct FDerivedDataInstallResult
{
    EDerivedDataInstallStatus Status = EDerivedDataInstallStatus::Failed;
    Asset::EAssetResult Result = Asset::EAssetResult::ProcessingFailure;
    Core::FString StableReason;
    Asset::FAssetDerivedDataEntry Entry;
    Core::TArray<Core::uint8> Payload;
};

struct FDerivedDataQuarantineResult
{
    Asset::EAssetResult Result = Asset::EAssetResult::ProcessingFailure;
    Core::FString StableReason;
    Core::FString PhysicalDirectory;
    bool bEntryWasReplaced = false;
};

class FDerivedDataStore
{
public:
    [[nodiscard]] static FDerivedDataStorePaths PathsFor(
        const Core::FString& Root,
        const Asset::FAssetDerivedKey& Key);

    [[nodiscard]] static FDerivedDataLookupResult Lookup(
        const FDerivedDataLookupRequest& Request);

    [[nodiscard]] static FDerivedDataInstallResult Install(
        const FDerivedDataLookupRequest& Request,
        const Core::TArray<Core::uint8>& Payload,
        std::chrono::milliseconds LeaseTimeout);

    [[nodiscard]] static FDerivedDataQuarantineResult Quarantine(
        const FDerivedDataLookupRequest& Request,
        const FDerivedDataLookupResult& InvalidEntry,
        std::chrono::milliseconds LeaseTimeout);
};

} // namespace Stoner::AssetCooker::Private
