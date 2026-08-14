#include "AssetCookerSchedulerTests.h"

#include "AssetCookerTestSupport.h"
#include "FAssetCookGraph.h"
#include "FAssetCookScheduler.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;
using namespace Stoner::AssetCooker::Private;
using namespace Stoner::Tests::AssetCooker;

void Record(FAssetCookerSchedulerTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FAssetCookGraphPlan Plan()
{
    const auto A = Id("Image", "Cooker/ScheduleA");
    const auto B = Id("Texture", "Cooker/ScheduleB");
    const auto C = Id("Material", "Cooker/ScheduleC");
    FAssetCookGraphPlan Value;
    (void)FAssetCookGraph::Build(
        {Output(C, {Required(A), Required(B)}), Output(B), Output(A)},
        EAssetCookSelectionMode::ExplicitRoots, {C}, {}, Value);
    return Value;
}

FAssetCookScheduleOutput Execute(const FAssetCookGraphPlan& Plan, Core::uint32 Workers)
{
    FAssetCookScheduleOutput Output;
    (void)FAssetCookScheduler::Execute(
        Plan, Workers,
        [](const FAssetCookGraphNode& Node, const auto& Results)
        {
            (void)Results;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            const Core::FString Identity = Node.Metadata.Id.ToString();
            const auto Text = Identity.View();
            return FAssetCookScheduledResult{
                EAssetResult::Success,
                Core::TArray<Core::uint8>(Text.begin(), Text.end())};
        },
        Output);
    return Output;
}

void TestDeterminism(FAssetCookerSchedulerTestResult& Result)
{
    const auto Graph = Plan();
    const auto Serial = Execute(Graph, 1);
    const auto Parallel = Execute(Graph, 8);
    bool Equal = Serial.Results.size() == Parallel.Results.size();
    for (Core::usize Index = 0; Equal && Index < Serial.Results.size(); ++Index)
        Equal = Serial.Results[Index].Artifact == Parallel.Results[Index].Artifact;
    if (!(Equal && Serial.PeakWorkers == 1 && Parallel.PeakWorkers == 2))
    {
        std::cout << "[DETAIL] equal=" << Equal
                  << " serial-peak=" << Serial.PeakWorkers
                  << " parallel-peak=" << Parallel.PeakWorkers
                  << " serial-results=" << Serial.Results.size()
                  << " parallel-results=" << Parallel.Results.size() << '\n';
    }
    Record(Result, Equal && Serial.PeakWorkers == 1 && Parallel.PeakWorkers == 2,
        "one-worker and eight-worker runs commit identical plan-index results");

    FAssetCookScheduleOutput Invalid;
    Record(Result,
        FAssetCookScheduler::Execute(Graph, 0, {}, Invalid) ==
            EAssetResult::InvalidInput && Invalid.Results.empty(),
        "scheduler enforces the bounded worker contract");
}

void TestFailureAtomicity(FAssetCookerSchedulerTestResult& Result)
{
    const auto Graph = Plan();
    FAssetCookScheduleOutput Output;
    const EAssetResult Executed = FAssetCookScheduler::Execute(
        Graph, 4,
        [](const FAssetCookGraphNode& Node, const auto& Results)
        {
            (void)Results;
            return FAssetCookScheduledResult{
                Node.PlanIndex == 1 ? EAssetResult::CookFailure : EAssetResult::Success,
                {1}};
        }, Output);
    Record(Result,
        Executed == EAssetResult::CookFailure && Output.Results.empty(),
        "one node failure publishes no partial scheduled result set");
}
} // namespace

FAssetCookerSchedulerTestResult RunAssetCookerSchedulerTests()
{
    FAssetCookerSchedulerTestResult Result;
    TestDeterminism(Result);
    TestFailureAtomicity(Result);
    return Result;
}
