#pragma once

#include "Asset/FAssetDigest.h"
#include "Asset/FAssetDerivedKey.h"
#include "Asset/FAssetId.h"
#include "Core/FPlatformTypes.h"
#include "Core/FString.h"
#include "Core/TArray.h"

namespace Stoner::AssetCooker
{

enum class EAssetCookDecision : Core::uint8
{
    Planned,
    Cooked,
    Reused,
    Failed
};

enum class EAssetCookAction : Core::uint8
{
    Hit,
    Miss,
    Invalidate,
    Quarantine,
    Cook,
    Rebuild,
    Fallback,
    Ineligible,
    Reuse,
    Stage,
    Validate,
    Publish,
    Fail
};

struct FAssetCookAssetReport
{
    Core::uint32 PlanIndex = 0;
    Asset::FAssetId AssetId;
    EAssetCookDecision Decision = EAssetCookDecision::Planned;
    EAssetCookAction Action = EAssetCookAction::Ineligible;
    bool bReuseEligible = false;
    Core::FString StableReason;
    Asset::FAssetDigest ArtifactDigest;
    Asset::FAssetDerivedKey DerivedKey;
    Asset::FAssetDigest EffectiveSettingsDigest;
    Asset::FAssetDigest RelevantProfileDigest;
    Core::FString TargetDecision;
    bool bUsedFallback = false;
    Core::TArray<Core::FString> SourceEvidence;
    Core::TArray<Core::FString> DependencyEvidence;
};

struct FAssetCookReportCounts
{
    Core::uint32 DiscoveredSources = 0;
    Core::uint32 SelectedRoots = 0;
    Core::uint32 ReachableAssets = 0;
    Core::uint32 ReuseEligible = 0;
    Core::uint32 ReuseIneligible = 0;
    Core::uint32 Cooked = 0;
    Core::uint32 Reused = 0;
    Core::uint32 CacheMisses = 0;
    Core::uint32 Invalidated = 0;
    Core::uint32 Quarantined = 0;
    Core::uint32 Rebuilt = 0;
    Core::uint32 Failed = 0;
};

struct FAssetCookTelemetry
{
    Core::FString HostLabel;
    Core::uint64 WallClockMilliseconds = 0;
    Core::uint64 PeakResidentBytes = 0;
    Core::uint32 WorkerCount = 0;
};

struct FAssetCookReport
{
    Asset::FAssetDigest EffectiveProfileDigest;
    Asset::FAssetDigest SnapshotDigest;
    Asset::FAssetDigest GenerationDigest;
    FAssetCookReportCounts Counts;
    Core::TArray<FAssetCookAssetReport> Assets;
    FAssetCookTelemetry Telemetry;

    [[nodiscard]] Asset::EAssetResult Validate() const noexcept;
};

} // namespace Stoner::AssetCooker
