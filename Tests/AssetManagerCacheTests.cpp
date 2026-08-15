#include "AssetManagerCacheTests.h"

#include "AssetManagerTestSupport.h"
#include "FAssetRuntimeCache.h"

#include <iostream>
#include <span>
#include <thread>
#include <chrono>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;

void Record(FAssetManagerCacheTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

void TestRetentionAndImmediateUnload(FAssetManagerCacheTestResult& Result)
{
    const FAssetId Id = MakeRuntimeTestId("Runtime/CacheRetention");
    auto Extensions = MakeRuntimeTestExtensions(Id);
    FAssetDiagnosticList Diagnostics;
    Core::TSharedPtr<FAssetManager> Manager;
    const bool Created = FAssetManager::Create(
        MakeDevelopmentManagerConfig(Extensions), Manager, Diagnostics) ==
        EAssetResult::Success;
    FAssetRequestHandle Request;
    FAssetRequestSnapshot Snapshot;
    const bool Ready = Created &&
        Manager->Request<FRuntimeTestPayload>(Id, Request) ==
            EAssetResult::Success &&
        WaitForRequestTerminal(*Manager, Request, Snapshot) &&
        Snapshot.State == EAssetRequestState::Ready;
    const auto RequestInspection = Manager->Inspect();
    TAssetHandle<FRuntimeTestPayload> Handle;
    const bool Retained = Ready &&
        Manager->GetResult(Request, Handle) == EAssetResult::Success;
    const auto HandleInspection = Manager->Inspect();
    const bool ReleasedRequest =
        Manager->ReleaseRequest(Request) == EAssetResult::Success;
    const auto ExternalInspection = Manager->Inspect();
    Handle.Reset();
    const auto EmptyInspection = Manager->Inspect();
    Record(Result,
        Retained && ReleasedRequest && RequestInspection.CachedAssets == 1 &&
            RequestInspection.RequestRetentions == 1 &&
            RequestInspection.CachedPayloadBytes > 0 &&
            HandleInspection.ExternalHandleRetentions == 1 &&
            ExternalInspection.CachedAssets == 1 &&
            ExternalInspection.RequestRetentions == 0 &&
            EmptyInspection.CachedAssets == 0 &&
            EmptyInspection.CachedPayloadBytes == 0,
        "three-class cache ownership removes the final unreferenced entry immediately");

    FAssetRequestHandle Reload;
    FAssetRequestSnapshot ReloadSnapshot;
    const bool Reloaded =
        Manager->Request<FRuntimeTestPayload>(Id, Reload) ==
            EAssetResult::Success &&
        WaitForRequestTerminal(*Manager, Reload, ReloadSnapshot) &&
        ReloadSnapshot.State == EAssetRequestState::Ready;
    Record(Result,
        Reloaded && Extensions.ImportCalls->load() == 2,
        "request after zero-retention removal performs a fresh load");
    (void)Manager->ReleaseRequest(Reload);
    (void)Manager->Shutdown();
}

FAssetMetadata MakeMetadata(
    const FAssetId& Id,
    Core::TArray<FAssetDependency> Dependencies = {})
{
    FAssetMetadata Metadata;
    Metadata.Id = Id;
    (void)FAssetSourceLocator::Create(
        Core::FString("asset"), Id.GetLogicalPath(), Metadata.Source);
    (void)FAssetParticipantId::Create(
        Core::FString("importer.runtime-cache-test"), Metadata.Producer);
    (void)FAssetProducerVersion::Create(
        Core::FString("1.0.0"), Metadata.ProducerVersion);
    const auto Text = Id.ToString().ToStdString();
    const auto Bytes = std::span<const Core::uint8>(
        reinterpret_cast<const Core::uint8*>(Text.data()), Text.size());
    Metadata.Version.SourceDigest = FAssetDigest::FromBytes(Bytes);
    Metadata.Version.ContentDigest = Metadata.Version.SourceDigest;
    Metadata.Version.Producer = Metadata.Producer;
    Metadata.Version.ProducerVersion = Metadata.ProducerVersion;
    Metadata.Dependencies = std::move(Dependencies);
    return Metadata;
}

void TestDependencyRetentionAndAtomicLimit(
    FAssetManagerCacheTestResult& Result)
{
    using namespace Stoner::Asset::Private;
    const FAssetId RootId = MakeRuntimeTestId("Runtime/CacheRoot");
    const FAssetId ChildId = MakeRuntimeTestId("Runtime/CacheChild");
    FAssetLoadKey RootKey;
    RootKey.AssetId = RootId;
    RootKey.ExpectedType = RootId.GetAssetType();
    RootKey.Mode = EAssetManagerMode::DevelopmentSource;
    RootKey.TargetDigest = FAssetDigest::FromBytes(
        Core::TArray<Core::uint8>{2, 6});
    FAssetLoadScratchResult Loaded;
    Loaded.Result = EAssetResult::Success;
    Loaded.Metadata = {
        MakeMetadata(ChildId),
        MakeMetadata(RootId, {{ChildId, EAssetDependencyRole::Runtime,
            EAssetDependencyStrength::Required,
            EAssetDependencyResolution::Resolved}})};
    Loaded.Payloads = {
        Core::MakeShared<FRuntimeTestPayload>(Core::FString("child")),
        Core::MakeShared<FRuntimeTestPayload>(Core::FString("root"))};
    Loaded.PayloadBytes = {4, 4};

    FAssetRuntimeCache Cache(8);
    Core::TSharedPtr<const FAssetPayload> RootPayload;
    const bool Published = Cache.Publish(
        RootKey, Loaded, 1, RootPayload) == EAssetResult::Success;
    const auto PublishedInspection = Cache.Inspect();
    Core::TSharedPtr<FAssetHandleControl> Handle;
    const bool External = Cache.AcquireExternal(RootKey, Handle) ==
        EAssetResult::Success;
    Cache.ReleaseRequest(RootKey);
    const auto RetainedInspection = Cache.Inspect();
    Handle.reset();
    const auto EmptyInspection = Cache.Inspect();
    Record(Result,
        Published && External && PublishedInspection.Entries == 2 &&
            PublishedInspection.RequiredDependencies == 1 &&
            PublishedInspection.PayloadBytes == 8 &&
            RetainedInspection.Entries == 2 &&
            RetainedInspection.ExternalHandles == 1 &&
            EmptyInspection.Entries == 0 &&
            EmptyInspection.PayloadBytes == 0,
        "required dependency retention follows the lifetime of its loaded root");

    FAssetRuntimeCache Limited(7);
    RootPayload.reset();
    Record(Result,
        Limited.Publish(RootKey, Loaded, 1, RootPayload) ==
                EAssetResult::CapacityExceeded &&
            Limited.Inspect().Entries == 0 && !RootPayload,
        "aggregate byte rejection leaves no partially visible cache entry");
}

void TestLifecycleStress(FAssetManagerCacheTestResult& Result)
{
    constexpr int Iterations = 10000;
    const FAssetId Id = MakeRuntimeTestId("Runtime/LifecycleStress");
    auto Extensions = MakeRuntimeTestExtensions(Id);
    auto Config = MakeDevelopmentManagerConfig(Extensions);
    Config.WorkerCount = 4;
    Core::TSharedPtr<FAssetManager> Manager;
    FAssetDiagnosticList Diagnostics;
    bool Passed = FAssetManager::Create(Config, Manager, Diagnostics) ==
        EAssetResult::Success;
    const auto Begin = std::chrono::steady_clock::now();
    for (int Iteration = 0; Iteration < Iterations && Passed; ++Iteration)
    {
        FAssetRequestHandle Request;
        Passed = Manager->Request<FRuntimeTestPayload>(Id, Request) ==
            EAssetResult::Success;
        FAssetRequestSnapshot Snapshot;
        for (int Poll = 0; Poll < 10000 && Passed; ++Poll)
        {
            Passed = Manager->Query(Request, Snapshot) ==
                EAssetResult::Success;
            if (Snapshot.State == EAssetRequestState::Ready ||
                Snapshot.State == EAssetRequestState::Failed ||
                Snapshot.State == EAssetRequestState::Cancelled)
                break;
            std::this_thread::yield();
        }
        Passed = Passed && Snapshot.State == EAssetRequestState::Ready &&
            Manager->ReleaseRequest(Request) == EAssetResult::Success;
    }
    const auto Milliseconds = std::chrono::duration_cast<
        std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - Begin).count();
    const auto Inspection = Manager->Inspect();
    std::cout << "[METRIC] runtime-manager-lifecycle iterations="
              << Iterations << " milliseconds=" << Milliseconds << '\n';
    Record(Result,
        Passed && Extensions.ImportCalls->load() == Iterations &&
            Inspection.CachedAssets == 0 &&
            Inspection.CachedPayloadBytes == 0 &&
            Inspection.ExternalHandleRetentions == 0 &&
            Inspection.RequestRetentions == 0 && Milliseconds < 30000,
        "ten thousand lifecycle operations return every cache retention to zero");
    (void)Manager->Shutdown();
}

void TestConcurrentReadyHandoff(FAssetManagerCacheTestResult& Result)
{
    constexpr int Iterations = 200;
    const FAssetId Id = MakeRuntimeTestId("Runtime/ConcurrentReadyHandoff");
    auto Extensions = MakeRuntimeTestExtensions(Id);
    Core::TSharedPtr<FAssetManager> Manager;
    FAssetDiagnosticList Diagnostics;
    bool Passed = FAssetManager::Create(
        MakeDevelopmentManagerConfig(Extensions), Manager, Diagnostics) ==
        EAssetResult::Success;
    for (int Iteration = 0; Iteration < Iterations && Passed; ++Iteration)
    {
        FAssetRequestHandle Previous;
        FAssetRequestSnapshot PreviousSnapshot;
        Passed = Manager->Request<FRuntimeTestPayload>(Id, Previous) ==
                EAssetResult::Success &&
            WaitForRequestTerminal(*Manager, Previous, PreviousSnapshot) &&
            PreviousSnapshot.State == EAssetRequestState::Ready;
        FAssetRequestHandle Next;
        EAssetResult Admission = EAssetResult::ProcessingFailure;
        std::thread Admit([&]
        {
            Admission = Manager->Request<FRuntimeTestPayload>(Id, Next);
        });
        const EAssetResult Released = Manager->ReleaseRequest(Previous);
        Admit.join();
        FAssetRequestSnapshot NextSnapshot;
        Passed = Passed && Released == EAssetResult::Success &&
            Admission == EAssetResult::Success && Next.IsValid() &&
            WaitForRequestTerminal(*Manager, Next, NextSnapshot) &&
            NextSnapshot.State == EAssetRequestState::Ready &&
            Manager->ReleaseRequest(Next) == EAssetResult::Success;
    }
    const auto Inspection = Manager->Inspect();
    Record(Result,
        Passed && Inspection.CachedAssets == 0 &&
            Inspection.RequestRetentions == 0,
        "concurrent ready admission and final release preserve request ownership");
    (void)Manager->Shutdown();
}
} // namespace

FAssetManagerCacheTestResult RunAssetManagerCacheTests()
{
    FAssetManagerCacheTestResult Result;
    TestRetentionAndImmediateUnload(Result);
    TestDependencyRetentionAndAtomicLimit(Result);
    TestLifecycleStress(Result);
    TestConcurrentReadyHandoff(Result);
    return Result;
}
