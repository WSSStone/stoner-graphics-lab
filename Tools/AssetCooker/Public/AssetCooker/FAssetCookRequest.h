#pragma once

#include "Asset/FAssetCookManifest.h"
#include "Asset/FAssetTargetProfile.h"
#include "Core/FPlatformTypes.h"
#include "Core/FString.h"
#include "Core/TArray.h"

#include <chrono>

namespace Stoner::AssetCooker
{

enum class EAssetCookRunMode : Core::uint8
{
    Cook,
    PlanOnly
};

enum class EAssetCookCachePolicy : Core::uint8
{
    Incremental,
    IgnoreExisting,
    StrictValidate
};

struct FAssetCookRequest
{
    Core::TArray<Core::FString> SourceRoots;
    Asset::EAssetCookSelectionMode SelectionMode =
        Asset::EAssetCookSelectionMode::ExplicitRoots;
    Core::TArray<Asset::FAssetId> ExplicitRoots;
    Core::FString TargetProfilePath;
    Asset::FAssetTargetProfileEvidence TargetProfile;
    Core::FString OutputRoot;
    Core::FString DerivedDataRoot;
    Core::FString ScratchRoot;
    Core::FString ReportPath;
    EAssetCookRunMode Mode = EAssetCookRunMode::Cook;
    EAssetCookCachePolicy CachePolicy = EAssetCookCachePolicy::Incremental;
    Core::uint32 WorkerCount = 4;
    std::chrono::milliseconds LeaseTimeout{30000};

    [[nodiscard]] Asset::EAssetResult Validate() const;
};

} // namespace Stoner::AssetCooker
