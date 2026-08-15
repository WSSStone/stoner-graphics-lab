#include "AssetManagerStressTests.h"

#include "AssetManagerTestSupport.h"
#include "FAssetDependencyScheduler.h"
#include "FAssetRuntimeCache.h"
#include "IAssetLoadingStrategy.h"

#include <chrono>
#include <iostream>
#include <map>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;
using namespace Stoner::Asset::Private;

constexpr Core::uint32 GNodeCount = 1000;
constexpr Core::uint32 GEdgeCount = 5000;

void Record(
    FAssetManagerStressTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FAssetParticipantId Participant()
{
    FAssetParticipantId Result;
    (void)FAssetParticipantId::Create(
        Core::FString("loader.runtime-stress"), Result);
    return Result;
}

FAssetProducerVersion Version()
{
    FAssetProducerVersion Result;
    (void)FAssetProducerVersion::Create(Core::FString("1.0.0"), Result);
    return Result;
}

FAssetMetadata MakeMetadata(
    const FAssetId& Id,
    Core::TArray<FAssetDependency> Dependencies = {})
{
    FAssetMetadata Result;
    Result.Id = Id;
    (void)FAssetSourceLocator::Create(
        Core::FString("asset"), Id.GetLogicalPath(), Result.Source);
    Result.Producer = Participant();
    Result.ProducerVersion = Version();
    const std::string Text = Id.ToString().ToStdString();
    const auto Bytes = std::span<const Core::uint8>(
        reinterpret_cast<const Core::uint8*>(Text.data()), Text.size());
    Result.Version.SourceDigest = FAssetDigest::FromBytes(Bytes);
    Result.Version.ContentDigest = Result.Version.SourceDigest;
    Result.Version.Producer = Result.Producer;
    Result.Version.ProducerVersion = Result.ProducerVersion;
    Result.Dependencies = std::move(Dependencies);
    return Result;
}

FAssetLoadKey MakeKey(const FAssetId& Id)
{
    FAssetLoadKey Key;
    Key.AssetId = Id;
    Key.ExpectedType = Id.GetAssetType();
    Key.TargetDigest = FAssetDigest::FromBytes(
        Core::TArray<Core::uint8>{2, 6});
    return Key;
}

class FPreboundGraphStrategy final : public IAssetLoadingStrategy
{
public:
    std::map<FAssetId, FAssetMetadata> Records;

    FAssetLoadScratchResult Load(
        const FAssetLoadKey& Key,
        const FAssetRuntimeExecutionContext& Context) override
    {
        FAssetLoadScratchResult Result;
        if (Context.ShouldStop())
        {
            Result.Result = EAssetResult::Cancelled;
            return Result;
        }
        const auto Found = Records.find(Key.AssetId);
        if (Found == Records.end())
        {
            Result.Result = EAssetResult::NotFound;
            return Result;
        }
        Result.Metadata.push_back(Found->second);
        Result.Payloads.push_back(Core::MakeShared<FRuntimeTestPayload>(
            Core::FString("")));
        Result.PayloadBytes.push_back(1);
        Result.Result = EAssetResult::Success;
        return Result;
    }
};

struct FStressGraph
{
    Core::TArray<FAssetId> Ids;
    FPreboundGraphStrategy Strategy;
};

FStressGraph MakeStressGraph()
{
    FStressGraph Graph;
    Graph.Ids.reserve(GNodeCount);
    for (Core::uint32 Index = 0; Index < GNodeCount; ++Index)
    {
        const std::string Path = "Stress/Node" + std::to_string(Index);
        Graph.Ids.push_back(MakeRuntimeTestId(Path.c_str()));
    }

    Core::TArray<FAssetDependency> RootDependencies;
    RootDependencies.reserve(GNodeCount - 1);
    for (Core::uint32 Index = 1; Index < GNodeCount; ++Index)
        RootDependencies.push_back({Graph.Ids[Index],
            EAssetDependencyRole::Runtime,
            EAssetDependencyStrength::Required,
            EAssetDependencyResolution::Resolved});
    Graph.Strategy.Records.emplace(
        Graph.Ids[0], MakeMetadata(Graph.Ids[0], RootDependencies));

    for (Core::uint32 Index = 1; Index < GNodeCount; ++Index)
    {
        Core::TArray<FAssetDependency> Dependencies;
        if (Index <= 800)
        {
            Dependencies.reserve(5);
            for (Core::uint32 Edge = 0; Edge < 5; ++Edge)
            {
                const Core::uint32 Target =
                    801 + ((Index * 5 + Edge) % 199);
                Dependencies.push_back({Graph.Ids[Target],
                    EAssetDependencyRole::Runtime,
                    EAssetDependencyStrength::Required,
                    EAssetDependencyResolution::Resolved});
            }
        }
        else if (Index == 801)
        {
            Dependencies.push_back({Graph.Ids[999],
                EAssetDependencyRole::Runtime,
                EAssetDependencyStrength::Required,
                EAssetDependencyResolution::Resolved});
        }
        Graph.Strategy.Records.emplace(
            Graph.Ids[Index], MakeMetadata(Graph.Ids[Index], Dependencies));
    }
    return Graph;
}

FAssetRuntimeExecutionContext MakeContext()
{
    return {Core::MakeShared<FAssetCancellationToken>(),
        std::chrono::steady_clock::now() + std::chrono::seconds(30)};
}

std::string Fingerprint(const FAssetLoadScratchResult& Loaded)
{
    std::string Result;
    for (const auto& Metadata : Loaded.Metadata)
    {
        Result += Metadata.Id.ToString().ToStdString();
        Result.push_back('\n');
    }
    return Result;
}

void TestTwentyRepeatDeterminism(FAssetManagerStressTestResult& Result)
{
    auto Graph = MakeStressGraph();
    FAssetManagerLimits Limits;
    Limits.MaxKnownAssets = GNodeCount;
    Limits.MaxDependencyEdges = GEdgeCount;
    const FAssetLoadKey Root = MakeKey(Graph.Ids[0]);
    std::string Expected;
    bool Stable = true;
    for (int Run = 0; Run < 20 && Stable; ++Run)
    {
        const auto Loaded = FAssetDependencyScheduler::LoadClosure(
            Root, Graph.Strategy, MakeContext(), Limits);
        const std::string Current = Fingerprint(Loaded);
        Stable = Loaded.Result == EAssetResult::Success &&
            Loaded.Metadata.size() == GNodeCount &&
            (Run == 0 || Current == Expected);
        if (Run == 0) Expected = Current;
    }
    Record(Result, Stable,
        "twenty pre-bound graph traces produce identical normalized order");
}

void TestScaleRetentionAndLimits(FAssetManagerStressTestResult& Result)
{
    const auto Workload = RunAssetManagerScaleWorkload();
    std::cout << "[METRIC] runtime-manager-scale nodes=" << GNodeCount
              << " edges=" << GEdgeCount
              << " milliseconds=" << Workload.Milliseconds << '\n';
    Record(Result, Workload.Passed,
        "one-thousand-node five-thousand-edge graph retains and releases every payload");

    auto Graph = MakeStressGraph();
    FAssetManagerLimits Limits;
    Limits.MaxKnownAssets = GNodeCount;
    Limits.MaxDependencyEdges = GEdgeCount;
    const FAssetLoadKey Root = MakeKey(Graph.Ids[0]);
    const auto Loaded = FAssetDependencyScheduler::LoadClosure(
        Root, Graph.Strategy, MakeContext(), Limits);
    Limits.MaxKnownAssets = GNodeCount - 1;
    const auto NodeLimited = FAssetDependencyScheduler::LoadClosure(
        Root, Graph.Strategy, MakeContext(), Limits);
    Limits.MaxKnownAssets = GNodeCount;
    Limits.MaxDependencyEdges = GEdgeCount - 1;
    const auto EdgeLimited = FAssetDependencyScheduler::LoadClosure(
        Root, Graph.Strategy, MakeContext(), Limits);
    FAssetRuntimeCache SmallCache(GNodeCount - 1);
    Core::TSharedPtr<const FAssetPayload> RejectedPayload;
    const bool CacheRejected = SmallCache.Publish(
        Root, Loaded, 1, RejectedPayload) == EAssetResult::CapacityExceeded &&
        SmallCache.Inspect().Entries == 0 && !RejectedPayload;
    Record(Result,
        NodeLimited.Result == EAssetResult::CapacityExceeded &&
            EdgeLimited.Result == EAssetResult::CapacityExceeded &&
            CacheRejected,
        "node, edge, and aggregate-byte limits fail without partial publication");
}
} // namespace

FAssetManagerStressTestResult RunAssetManagerStressTests()
{
    FAssetManagerStressTestResult Result;
    TestTwentyRepeatDeterminism(Result);
    TestScaleRetentionAndLimits(Result);
    return Result;
}

FAssetManagerScaleWorkloadResult RunAssetManagerScaleWorkload()
{
    auto Graph = MakeStressGraph();
    FAssetManagerLimits Limits;
    Limits.MaxKnownAssets = GNodeCount;
    Limits.MaxDependencyEdges = GEdgeCount;
    const FAssetLoadKey Root = MakeKey(Graph.Ids[0]);
    const auto Begin = std::chrono::steady_clock::now();
    const auto Loaded = FAssetDependencyScheduler::LoadClosure(
        Root, Graph.Strategy, MakeContext(), Limits);
    FAssetRuntimeCache Cache(GNodeCount);
    Core::TSharedPtr<const FAssetPayload> RootPayload;
    const bool Published = Cache.Publish(Root, Loaded, 1, RootPayload) ==
        EAssetResult::Success;
    Core::TArray<Core::TSharedPtr<FAssetHandleControl>> Handles;
    Handles.reserve(GNodeCount);
    bool Retained = Published;
    for (const auto& Id : Graph.Ids)
    {
        FAssetLoadKey Key = Root;
        Key.AssetId = Id;
        Key.ExpectedType = Id.GetAssetType();
        Core::TSharedPtr<FAssetHandleControl> Handle;
        Retained = Retained &&
            Cache.AcquireExternal(Key, Handle) == EAssetResult::Success;
        Handles.push_back(std::move(Handle));
    }
    Cache.ReleaseRequest(Root);
    const auto RetainedSnapshot = Cache.Inspect();
    Handles.clear();
    RootPayload.reset();
    const auto EmptySnapshot = Cache.Inspect();
    const auto Milliseconds = std::chrono::duration_cast<
        std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - Begin).count();
    return {Retained && RetainedSnapshot.Entries == GNodeCount &&
            RetainedSnapshot.ExternalHandles == GNodeCount &&
            EmptySnapshot.Entries == 0 && EmptySnapshot.PayloadBytes == 0,
        Milliseconds};
}
