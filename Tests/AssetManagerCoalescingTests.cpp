#include "AssetManagerCoalescingTests.h"

#include "AssetManagerTestSupport.h"
#include "FAssetDependencyScheduler.h"
#include "FAssetLoadOperationTable.h"
#include "FAssetNodeLoadCoordinator.h"
#include "FAssetRequestTable.h"
#include "IAssetLoadingStrategy.h"

#include <array>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <map>
#include <mutex>
#include <span>
#include <thread>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;
using namespace Stoner::Asset::Private;

void Record(
    FAssetManagerCoalescingTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FAssetMetadata MakeGraphMetadata(
    const FAssetId& Id,
    Core::TArray<FAssetDependency> Dependencies = {})
{
    FAssetMetadata Metadata;
    Metadata.Id = Id;
    (void)FAssetSourceLocator::Create(
        Core::FString("asset"), Id.GetLogicalPath(), Metadata.Source);
    (void)FAssetParticipantId::Create(
        Core::FString("loader.shared-dependency"), Metadata.Producer);
    (void)FAssetProducerVersion::Create(
        Core::FString("1.0.0"), Metadata.ProducerVersion);
    const std::string Text = Id.ToString().ToStdString();
    const auto Bytes = std::span<const Core::uint8>(
        reinterpret_cast<const Core::uint8*>(Text.data()), Text.size());
    Metadata.Version.SourceDigest = FAssetDigest::FromBytes(Bytes);
    Metadata.Version.ContentDigest = Metadata.Version.SourceDigest;
    Metadata.Version.Producer = Metadata.Producer;
    Metadata.Version.ProducerVersion = Metadata.ProducerVersion;
    Metadata.Dependencies = std::move(Dependencies);
    return Metadata;
}

class FSharedDependencyStrategy final : public IAssetLoadingStrategy
{
public:
    std::map<FAssetId, FAssetMetadata> Records;
    FAssetId SharedId;

    FAssetLoadScratchResult Load(
        const FAssetLoadKey& Key,
        const FAssetRuntimeExecutionContext& Context) override
    {
        FAssetLoadScratchResult Result;
        const auto Found = Records.find(Key.AssetId);
        if (Found == Records.end())
        {
            Result.Result = EAssetResult::NotFound;
            return Result;
        }
        if (Key.AssetId == SharedId)
        {
            std::unique_lock Lock(Mutex);
            ++SharedLoads;
            SharedStarted = true;
            Condition.notify_all();
            Condition.wait_for(Lock, std::chrono::seconds(2), [&]
            {
                return ReleaseShared || Context.ShouldStop();
            });
        }
        Result.Metadata.push_back(Found->second);
        Result.Payloads.push_back(Core::MakeShared<FRuntimeTestPayload>(
            Key.AssetId.ToString()));
        Result.PayloadBytes.push_back(1);
        Result.Result = Context.ShouldStop()
            ? EAssetResult::Cancelled
            : EAssetResult::Success;
        return Result;
    }

    bool WaitForShared()
    {
        std::unique_lock Lock(Mutex);
        return Condition.wait_for(Lock, std::chrono::seconds(2), [&]
        {
            return SharedStarted;
        });
    }

    void Release()
    {
        std::lock_guard Lock(Mutex);
        ReleaseShared = true;
        Condition.notify_all();
    }

    int SharedLoads = 0;

private:
    std::mutex Mutex;
    std::condition_variable Condition;
    bool SharedStarted = false;
    bool ReleaseShared = false;
};

FAssetLoadKey MakeGraphKey(const FAssetId& Id)
{
    FAssetLoadKey Key;
    Key.AssetId = Id;
    Key.ExpectedType = Id.GetAssetType();
    Key.TargetDigest = FAssetDigest::FromBytes(
        Core::TArray<Core::uint8>{2, 6});
    return Key;
}

FAssetRuntimeExecutionContext MakeGraphContext()
{
    return {Core::MakeShared<FAssetCancellationToken>(),
        std::chrono::steady_clock::now() + std::chrono::seconds(2)};
}

void TestSharedDependencyOperation(FAssetManagerCoalescingTestResult& Result)
{
    const FAssetId RootA = MakeRuntimeTestId("Runtime/SharedRootA");
    const FAssetId RootB = MakeRuntimeTestId("Runtime/SharedRootB");
    const FAssetId Shared = MakeRuntimeTestId("Runtime/SharedDependency");
    const FAssetDependency Dependency{Shared,
        EAssetDependencyRole::Runtime,
        EAssetDependencyStrength::Required,
        EAssetDependencyResolution::Resolved};
    FSharedDependencyStrategy Strategy;
    Strategy.SharedId = Shared;
    Strategy.Records.emplace(
        RootA, MakeGraphMetadata(RootA, {Dependency}));
    Strategy.Records.emplace(
        RootB, MakeGraphMetadata(RootB, {Dependency}));
    Strategy.Records.emplace(Shared, MakeGraphMetadata(Shared));
    FAssetNodeLoadCoordinator Coordinator;
    FAssetLoadScratchResult LoadedA;
    FAssetLoadScratchResult LoadedB;
    std::thread First([&]
    {
        LoadedA = FAssetDependencyScheduler::LoadClosure(
            MakeGraphKey(RootA), Strategy, MakeGraphContext(), {},
            &Coordinator);
    });
    const bool Started = Strategy.WaitForShared();
    std::thread Second([&]
    {
        LoadedB = FAssetDependencyScheduler::LoadClosure(
            MakeGraphKey(RootB), Strategy, MakeGraphContext(), {},
            &Coordinator);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    Strategy.Release();
    First.join();
    Second.join();
    Record(Result,
        Started && LoadedA.Result == EAssetResult::Success &&
            LoadedB.Result == EAssetResult::Success &&
            LoadedA.Metadata.size() == 2 && LoadedB.Metadata.size() == 2 &&
            Strategy.SharedLoads == 1 && Coordinator.ActiveEntries() == 0,
        "simultaneous roots share one transient dependency load operation");
}

void TestCompleteLoadKeyEquivalence(FAssetManagerCoalescingTestResult& Result)
{
    FAssetLoadOperationTable Operations;
    FAssetRequestTable Requests(0x260012ULL, 4);
    std::array<FAssetRequestHandle, 4> Handles;
    bool Allocated = true;
    for (auto& Handle : Handles) Allocated = Allocated && Requests.Allocate(Handle);

    FAssetLoadKey Base = MakeGraphKey(MakeRuntimeTestId("Runtime/LoadKey"));
    FAssetLoadKey OtherTarget = Base;
    OtherTarget.TargetDigest = FAssetDigest::FromBytes(
        Core::TArray<Core::uint8>{2, 7});
    FAssetLoadKey OtherMode = Base;
    OtherMode.Mode = EAssetManagerMode::StrictCooked;
    FAssetLoadKey OtherGeneration = Base;
    OtherGeneration.CookedGeneration = FAssetDigest::FromBytes(
        Core::TArray<Core::uint8>{2, 6, 1});
    const std::array<FAssetLoadKey, 4> Keys{
        Base, OtherTarget, OtherMode, OtherGeneration};
    bool Created = Allocated;
    for (Core::usize Index = 0; Index < Keys.size(); ++Index)
    {
        Core::TSharedPtr<FSharedAssetLoadOperation> Operation;
        Created = Created && Operations.Attach(
            Keys[Index], Handles[Index], Operation) ==
                EAssetOperationAttachResult::Created;
    }
    Record(Result, Created && Operations.Size() == Keys.size(),
        "non-equivalent complete load keys never coalesce");
}

void TestDependencyOwnerCancellation(FAssetManagerCoalescingTestResult& Result)
{
    const FAssetId RootA = MakeRuntimeTestId("Runtime/CancelledOwner");
    const FAssetId RootB = MakeRuntimeTestId("Runtime/SurvivingRoot");
    const FAssetId Shared = MakeRuntimeTestId("Runtime/SurvivingDependency");
    const FAssetDependency Dependency{Shared,
        EAssetDependencyRole::Runtime,
        EAssetDependencyStrength::Required,
        EAssetDependencyResolution::Resolved};
    FSharedDependencyStrategy Strategy;
    Strategy.SharedId = Shared;
    Strategy.Records.emplace(RootA, MakeGraphMetadata(RootA, {Dependency}));
    Strategy.Records.emplace(RootB, MakeGraphMetadata(RootB, {Dependency}));
    Strategy.Records.emplace(Shared, MakeGraphMetadata(Shared));
    FAssetNodeLoadCoordinator Coordinator;

    FAssetRequestTable Requests(0x260019ULL, 1);
    FAssetRequestHandle OwnerRequest;
    FAssetLoadOperationTable Operations;
    Core::TSharedPtr<FSharedAssetLoadOperation> OwnerOperation;
    const bool Attached = Requests.Allocate(OwnerRequest) &&
        Operations.Attach(MakeGraphKey(RootA), OwnerRequest, OwnerOperation) ==
            EAssetOperationAttachResult::Created;
    const FAssetRuntimeExecutionContext OwnerContext{
        OwnerOperation ? OwnerOperation->Cancellation : nullptr,
        std::chrono::steady_clock::now() + std::chrono::seconds(2)};
    FAssetLoadScratchResult LoadedA;
    FAssetLoadScratchResult LoadedB;
    std::thread First([&]
    {
        LoadedA = FAssetDependencyScheduler::LoadClosure(
            MakeGraphKey(RootA), Strategy, OwnerContext, {}, &Coordinator);
    });
    const bool Started = Strategy.WaitForShared();
    std::thread Second([&]
    {
        LoadedB = FAssetDependencyScheduler::LoadClosure(
            MakeGraphKey(RootB), Strategy, MakeGraphContext(), {},
            &Coordinator);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const bool Detached = Attached &&
        Operations.Detach(OwnerOperation, OwnerRequest);
    Strategy.Release();
    First.join();
    Second.join();
    Record(Result,
        Started && Detached && LoadedA.Result == EAssetResult::Cancelled &&
            LoadedB.Result == EAssetResult::Success &&
            Strategy.SharedLoads == 1 && Coordinator.ActiveEntries() == 0,
        "dependency owner cancellation preserves a surviving root interest");
}
} // namespace

FAssetManagerCoalescingTestResult RunAssetManagerCoalescingTests()
{
    FAssetManagerCoalescingTestResult Result;
    const FAssetId Id = MakeRuntimeTestId("Runtime/Coalesced");
    auto Extensions = MakeRuntimeTestExtensions(Id, "coalesced-payload");
    auto Config = MakeDevelopmentManagerConfig(Extensions);
    Config.WorkerCount = 4;
    Core::TSharedPtr<FAssetManager> Manager;
    FAssetDiagnosticList Diagnostics;
    const bool Created = FAssetManager::Create(Config, Manager, Diagnostics) ==
        EAssetResult::Success;
    std::array<FAssetRequestHandle, 8> Requests;
    bool Admitted = Created;
    for (auto& Request : Requests)
        Admitted = Admitted &&
            Manager->Request<FRuntimeTestPayload>(Id, Request) ==
                EAssetResult::Success;
    std::array<TAssetHandle<FRuntimeTestPayload>, 8> Handles;
    bool Ready = Admitted;
    for (Core::usize Index = 0; Index < Requests.size(); ++Index)
    {
        FAssetRequestSnapshot Snapshot;
        Ready = Ready && WaitForRequestTerminal(
            *Manager, Requests[Index], Snapshot) &&
            Snapshot.State == EAssetRequestState::Ready &&
            Manager->GetResult(Requests[Index], Handles[Index]) ==
                EAssetResult::Success;
    }
    bool Shared = Ready;
    for (Core::usize Index = 1; Index < Handles.size(); ++Index)
        Shared = Shared && Handles[Index].Get() == Handles[0].Get();
    Record(Result,
        Ready && Extensions.ResolveCalls->load() == 2 &&
            Extensions.ImportCalls->load() == 1,
        "eight equivalent callers share one physical load operation");
    Record(Result, Shared,
        "coalesced callers receive independent handles to one payload");
    for (const auto Request : Requests)
        (void)Manager->ReleaseRequest(Request);
    (void)Manager->Shutdown();
    TestCompleteLoadKeyEquivalence(Result);
    TestSharedDependencyOperation(Result);
    TestDependencyOwnerCancellation(Result);
    return Result;
}
