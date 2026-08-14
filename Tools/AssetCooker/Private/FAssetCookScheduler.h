#pragma once

#include "FAssetCookGraph.h"

#include <functional>

namespace Stoner::AssetCooker::Private
{

struct FAssetCookScheduledResult
{
    Asset::EAssetResult Result = Asset::EAssetResult::ProcessingFailure;
    Core::TArray<Core::uint8> Artifact;
};

struct FAssetCookScheduleOutput
{
    Core::TArray<FAssetCookScheduledResult> Results;
    Core::uint32 PeakWorkers = 0;
};

using FAssetCookNodeProcessor = std::function<FAssetCookScheduledResult(
    const FAssetCookGraphNode&,
    const Core::TArray<FAssetCookScheduledResult>&)>;

class FAssetCookScheduler
{
public:
    [[nodiscard]] static Asset::EAssetResult Execute(
        const FAssetCookGraphPlan& Plan,
        Core::uint32 WorkerCount,
        const FAssetCookNodeProcessor& Processor,
        FAssetCookScheduleOutput& OutOutput);
};

} // namespace Stoner::AssetCooker::Private
