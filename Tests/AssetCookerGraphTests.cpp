#include "AssetCookerGraphTests.h"

#include "AssetCookerTestSupport.h"
#include "FAssetCookGraph.h"

#include <iostream>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;
using namespace Stoner::AssetCooker::Private;
using namespace Stoner::Tests::AssetCooker;

void Record(FAssetCookerGraphTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

Core::TArray<FAssetImportOutput> Diamond()
{
    const auto A = Id("Image", "Cooker/A");
    const auto B = Id("Texture", "Cooker/B");
    const auto C = Id("Material", "Cooker/C");
    const auto D = Id("StaticModel", "Cooker/D");
    return {
        Output(D, {Required(B), Required(C)}),
        Output(C, {Required(A)}),
        Output(A),
        Output(B, {Required(A)}),
        Output(Id("Image", "Cooker/Unreachable"))};
}

void TestClosureAndOrder(FAssetCookerGraphTestResult& Result)
{
    const auto Root = Id("StaticModel", "Cooker/D");
    FAssetCookGraphPlan First;
    auto Available = Diamond();
    const EAssetResult Built = FAssetCookGraph::Build(
        Available, EAssetCookSelectionMode::ExplicitRoots, {Root}, {}, First);
    std::reverse(Available.begin(), Available.end());
    FAssetCookGraphPlan Second;
    const EAssetResult Rebuilt = FAssetCookGraph::Build(
        Available, EAssetCookSelectionMode::ExplicitRoots, {Root}, {}, Second);
    bool Same = Built == EAssetResult::Success &&
        Rebuilt == EAssetResult::Success && First.Nodes.size() == 4 &&
        Second.Nodes.size() == First.Nodes.size();
    for (Core::usize Index = 0; Same && Index < First.Nodes.size(); ++Index)
        Same = First.Nodes[Index].Metadata.Id == Second.Nodes[Index].Metadata.Id;
    Record(Result, Same,
        "explicit roots select only complete closure in stable topological order");

    FAssetCookGraphPlan All;
    Record(
        Result,
        FAssetCookGraph::Build(
            Diamond(), EAssetCookSelectionMode::CookAll, {}, {}, All) ==
                EAssetResult::Success && All.Nodes.size() == 5,
        "cook-all includes every supported discovered output");
}

void TestFailures(FAssetCookerGraphTestResult& Result)
{
    const auto Missing = Id("Texture", "Cooker/Missing");
    const auto Root = Id("Material", "Cooker/Root");
    FAssetCookGraphPlan Plan;
    Record(Result,
        FAssetCookGraph::Build(
            {Output(Root, {Required(Missing)})},
            EAssetCookSelectionMode::ExplicitRoots, {Root}, {}, Plan) ==
            EAssetResult::UnresolvedDependency && Plan.Nodes.empty(),
        "missing required dependency fails without a partial graph");

    const auto A = Id("Image", "Cooker/CycleA");
    const auto B = Id("Texture", "Cooker/CycleB");
    Record(Result,
        FAssetCookGraph::Build(
            {Output(A, {Required(B)}), Output(B, {Required(A)})},
            EAssetCookSelectionMode::ExplicitRoots, {A}, {}, Plan) ==
            EAssetResult::DependencyCycle && Plan.Nodes.empty(),
        "cycle fails with a stable graph category");

    FAssetCookGraphLimits Limits;
    Limits.MaxAssets = 1;
    Record(Result,
        FAssetCookGraph::Build(
            {Output(A), Output(B, {Required(A)})},
            EAssetCookSelectionMode::ExplicitRoots, {B}, Limits, Plan) ==
            EAssetResult::CapacityExceeded && Plan.Nodes.empty(),
        "graph count limit fails before caller-visible publication");

    auto InvalidType = Output(A);
    InvalidType.Payload = Core::MakeShared<FSyntheticPayload>(
        Core::FString("Texture"));
    Record(Result,
        FAssetCookGraph::Build(
            {InvalidType}, EAssetCookSelectionMode::CookAll, {}, {}, Plan) ==
            EAssetResult::InvalidInput && Plan.Nodes.empty(),
        "payload type mismatch fails before graph publication");

    auto MissingVersion = Output(A);
    MissingVersion.Metadata.Version = {};
    Record(Result,
        FAssetCookGraph::Build(
            {MissingVersion}, EAssetCookSelectionMode::CookAll, {}, {}, Plan) ==
            EAssetResult::InvalidInput && Plan.Nodes.empty(),
        "missing asset version evidence fails before graph publication");

    auto InvalidRole = Output(B, {Required(A)});
    InvalidRole.Metadata.Dependencies[0].Role =
        static_cast<EAssetDependencyRole>(255);
    Record(Result,
        FAssetCookGraph::Build(
            {Output(A), InvalidRole}, EAssetCookSelectionMode::CookAll,
            {}, {}, Plan) == EAssetResult::InvalidInput && Plan.Nodes.empty(),
        "invalid dependency role fails before graph publication");

    const auto C = Id("Material", "Cooker/DepthC");
    Limits = {};
    Limits.MaxDependencyDepth = 2;
    Record(Result,
        FAssetCookGraph::Build(
            {Output(A), Output(B, {Required(A)}), Output(C, {Required(B)})},
            EAssetCookSelectionMode::ExplicitRoots, {C}, Limits, Plan) ==
            EAssetResult::CapacityExceeded && Plan.Nodes.empty(),
        "dependency depth limit fails without a partial graph");
}
} // namespace

FAssetCookerGraphTestResult RunAssetCookerGraphTests()
{
    FAssetCookerGraphTestResult Result;
    TestClosureAndOrder(Result);
    TestFailures(Result);
    return Result;
}
