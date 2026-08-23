#pragma once

#include "Asset/AssetMinimal.h"
#include "ProductionAssetClosureTestSupport.h"

struct FProductionAssetEquivalenceReport
{
    Stoner::Core::uint32 ComparedAssets = 0;
    Stoner::Core::uint32 ComparedGeometryValues = 0;
    Stoner::Core::uint64 ComparedTextureSamples = 0;
    Stoner::Core::FString FirstFailure;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return FirstFailure.IsEmpty();
    }
};

[[nodiscard]] bool CompareProductionAssetClosures(
    const FProductionAssetClosure& Development,
    const FProductionAssetClosure& Cooked,
    const Stoner::Asset::FAssetTargetProfileEvidence& Target,
    FProductionAssetEquivalenceReport& Out);
