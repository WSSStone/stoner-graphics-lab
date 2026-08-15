#include "AssetManagerDependencyTests.h"

#include "AssetManagerTestSupport.h"
#include "FAssetDependencyScheduler.h"
#include "IAssetLoadingStrategy.h"

#include <chrono>
#include <iostream>
#include <map>
#include <span>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;
using namespace Stoner::Asset::Private;

void Record(
    FAssetManagerDependencyTestResult& Result,
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
        Core::FString("importer.runtime-test"), Result);
    return Result;
}

FAssetProducerVersion Version()
{
    FAssetProducerVersion Result;
    (void)FAssetProducerVersion::Create(Core::FString("1.0.0"), Result);
    return Result;
}

FAssetMetadata Metadata(
    const FAssetId& Id,
    Core::TArray<FAssetDependency> Dependencies = {})
{
    FAssetMetadata Result;
    Result.Id = Id;
    (void)FAssetSourceLocator::Create(Core::FString("asset"),
        Id.GetLogicalPath(), Result.Source);
    Result.Producer = Participant();
    Result.ProducerVersion = Version();
    const auto Text = Id.ToString().ToStdString();
    const auto Bytes = std::span<const Core::uint8>(
        reinterpret_cast<const Core::uint8*>(Text.data()), Text.size());
    Result.Version.SourceDigest = FAssetDigest::FromBytes(Bytes);
    Result.Version.ContentDigest = FAssetDigest::FromBytes(Bytes);
    Result.Version.Producer = Result.Producer;
    Result.Version.ProducerVersion = Result.ProducerVersion;
    Result.Dependencies = std::move(Dependencies);
    return Result;
}

class FGraphStrategy final : public IAssetLoadingStrategy
{
public:
    std::map<FAssetId, FAssetMetadata> Records;
    Core::TArray<FAssetOptionalFallback> Fallbacks;

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
        Result.Payloads.push_back(
            Core::MakeShared<FRuntimeTestPayload>(Key.AssetId.ToString()));
        Result.PayloadBytes.push_back(1);
        Result.OptionalFallbacks = Fallbacks;
        Result.Result = EAssetResult::Success;
        return Result;
    }
};

FAssetLoadKey Key(const FAssetId& Id)
{
    FAssetLoadKey Result;
    Result.AssetId = Id;
    Result.ExpectedType = Id.GetAssetType();
    Result.Mode = EAssetManagerMode::DevelopmentSource;
    Result.TargetDigest = FAssetDigest::FromBytes(
        Core::TArray<Core::uint8>{2, 6});
    return Result;
}

FAssetRuntimeExecutionContext Context()
{
    return {Core::MakeShared<FAssetCancellationToken>(),
        std::chrono::steady_clock::now() + std::chrono::seconds(2)};
}

void TestRequiredClosure(FAssetManagerDependencyTestResult& Result)
{
    const FAssetId Root = MakeRuntimeTestId("Graph/Root");
    const FAssetId Child = MakeRuntimeTestId("Graph/Child");
    FGraphStrategy Strategy;
    Strategy.Records.emplace(Root,
        Metadata(Root, {{Child, EAssetDependencyRole::Runtime,
            EAssetDependencyStrength::Required,
            EAssetDependencyResolution::Resolved}}));
    Strategy.Records.emplace(Child, Metadata(Child));
    const auto Loaded = FAssetDependencyScheduler::LoadClosure(
        Key(Root), Strategy, Context(), {});
    Record(Result,
        Loaded.Result == EAssetResult::Success &&
            Loaded.Metadata.size() == 2 &&
            Loaded.Metadata[0].Id == Child && Loaded.Metadata[1].Id == Root,
        "required dependency closure publishes in stable identity order");

    Strategy.Records.erase(Child);
    const auto Missing = FAssetDependencyScheduler::LoadClosure(
        Key(Root), Strategy, Context(), {});
    Record(Result,
        Missing.Result == EAssetResult::NotFound &&
            Missing.FailurePath == Core::TArray<FAssetId>({Root, Child}),
        "missing required dependency reports the root-specific failure path");
}

void TestCycleAndOptionalFallback(FAssetManagerDependencyTestResult& Result)
{
    const FAssetId Root = MakeRuntimeTestId("Graph/CycleRoot");
    const FAssetId Child = MakeRuntimeTestId("Graph/CycleChild");
    FGraphStrategy Strategy;
    Strategy.Records.emplace(Root,
        Metadata(Root, {{Child, EAssetDependencyRole::Runtime,
            EAssetDependencyStrength::Required,
            EAssetDependencyResolution::Resolved}}));
    Strategy.Records.emplace(Child,
        Metadata(Child, {{Root, EAssetDependencyRole::Runtime,
            EAssetDependencyStrength::Required,
            EAssetDependencyResolution::Resolved}}));
    const auto Cycle = FAssetDependencyScheduler::LoadClosure(
        Key(Root), Strategy, Context(), {});
    Record(Result,
        Cycle.Result == EAssetResult::Conflict &&
            Cycle.FailurePath ==
                Core::TArray<FAssetId>({Root, Child, Root}),
        "dependency cycle reports a closed root-specific path");

    const FAssetId Optional = MakeRuntimeTestId("Graph/OptionalMissing");
    Strategy.Records.clear();
    Strategy.Records.emplace(Root,
        Metadata(Root, {{Optional, EAssetDependencyRole::Runtime,
            EAssetDependencyStrength::Soft,
            EAssetDependencyResolution::Unresolved}}));
    const auto Undeclared = FAssetDependencyScheduler::LoadClosure(
        Key(Root), Strategy, Context(), {});
    Record(Result, Undeclared.Result == EAssetResult::NotFound,
        "soft strength alone never implies a valid fallback");

    Strategy.Fallbacks = {{Root, Optional,
        EAssetOptionalFallbackDecision::ValidatedFallback,
        Core::FString("runtime-test.explicit-fallback")}};
    const auto Fallback = FAssetDependencyScheduler::LoadClosure(
        Key(Root), Strategy, Context(), {});
    Record(Result,
        Fallback.Result == EAssetResult::Success &&
            Fallback.Metadata.size() == 1 &&
            Fallback.OptionalFallbacks.size() == 1,
        "only an explicit validated fallback tolerates optional failure");
}
} // namespace

FAssetManagerDependencyTestResult RunAssetManagerDependencyTests()
{
    FAssetManagerDependencyTestResult Result;
    TestRequiredClosure(Result);
    TestCycleAndOptionalFallback(Result);
    return Result;
}
