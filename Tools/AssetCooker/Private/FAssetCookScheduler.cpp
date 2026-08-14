#include "FAssetCookScheduler.h"

#include <algorithm>
#include <future>

namespace Stoner::AssetCooker::Private
{

Asset::EAssetResult FAssetCookScheduler::Execute(
    const FAssetCookGraphPlan& Plan,
    Core::uint32 WorkerCount,
    const FAssetCookNodeProcessor& Processor,
    FAssetCookScheduleOutput& OutOutput)
{
    OutOutput = {};
    if (Plan.Validate() != Asset::EAssetResult::Success ||
        WorkerCount == 0 || WorkerCount > 32 || !Processor)
        return Asset::EAssetResult::InvalidInput;
    Core::TArray<FAssetCookScheduledResult> Results(Plan.Nodes.size());
    Core::TArray<bool> Complete(Plan.Nodes.size(), false);
    Core::usize CompletedCount = 0;
    while (CompletedCount < Plan.Nodes.size())
    {
        Core::TArray<Core::uint32> Ready;
        for (Core::uint32 Index = 0; Index < Plan.Nodes.size(); ++Index)
        {
            if (Complete[Index]) continue;
            const bool DependenciesComplete = std::all_of(
                Plan.Nodes[Index].Dependencies.begin(),
                Plan.Nodes[Index].Dependencies.end(),
                [&Complete](Core::uint32 Dependency) { return Complete[Dependency]; });
            if (DependenciesComplete) Ready.push_back(Index);
        }
        if (Ready.empty()) return Asset::EAssetResult::DependencyCycle;
        for (Core::usize Begin = 0; Begin < Ready.size(); Begin += WorkerCount)
        {
            const Core::usize Count = std::min<Core::usize>(
                WorkerCount, Ready.size() - Begin);
            OutOutput.PeakWorkers = std::max(
                OutOutput.PeakWorkers, static_cast<Core::uint32>(Count));
            Core::TArray<std::future<FAssetCookScheduledResult>> Futures;
            Futures.reserve(Count);
            const Core::TArray<FAssetCookScheduledResult> StableResults = Results;
            for (Core::usize Offset = 0; Offset < Count; ++Offset)
            {
                const Core::uint32 Index = Ready[Begin + Offset];
                Futures.push_back(std::async(
                    std::launch::async,
                    [&Plan, &StableResults, &Processor, Index]
                    {
                        return Processor(Plan.Nodes[Index], StableResults);
                    }));
            }
            bool Failed = false;
            Asset::EAssetResult FirstFailure = Asset::EAssetResult::Success;
            for (Core::usize Offset = 0; Offset < Count; ++Offset)
            {
                const Core::uint32 Index = Ready[Begin + Offset];
                Results[Index] = Futures[Offset].get();
                if (Results[Index].Result != Asset::EAssetResult::Success)
                {
                    Failed = true;
                    if (FirstFailure == Asset::EAssetResult::Success)
                        FirstFailure = Results[Index].Result;
                }
            }
            if (Failed)
            {
                OutOutput = {};
                return FirstFailure;
            }
            for (Core::usize Offset = 0; Offset < Count; ++Offset)
            {
                Complete[Ready[Begin + Offset]] = true;
                ++CompletedCount;
            }
        }
    }
    OutOutput.Results = std::move(Results);
    return Asset::EAssetResult::Success;
}

} // namespace Stoner::AssetCooker::Private
