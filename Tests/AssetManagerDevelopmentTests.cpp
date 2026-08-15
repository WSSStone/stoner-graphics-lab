#include "AssetManagerDevelopmentTests.h"

#include "AssetManagerTestSupport.h"

#include <iostream>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;

void Record(
    FAssetManagerDevelopmentTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

void TestMutationRejectsAtomicPublication(
    FAssetManagerDevelopmentTestResult& Result)
{
    const FAssetId Id = MakeRuntimeTestId("Runtime/Mutation");
    auto Extensions = MakeRuntimeTestExtensions(Id);
    Extensions.MutateAfterImport->store(true);
    Core::TSharedPtr<FAssetManager> Manager;
    FAssetDiagnosticList Diagnostics;
    const auto Config = MakeDevelopmentManagerConfig(Extensions);
    const bool Created = FAssetManager::Create(Config, Manager, Diagnostics) ==
        EAssetResult::Success;
    FAssetRequestHandle Request;
    const bool Accepted = Created &&
        Manager->Request<FRuntimeTestPayload>(Id, Request) ==
            EAssetResult::Success;
    FAssetRequestSnapshot Snapshot;
    const bool Terminal = Accepted &&
        WaitForRequestTerminal(*Manager, Request, Snapshot);
    TAssetHandle<FRuntimeTestPayload> Payload;
    Record(Result,
        Terminal && Snapshot.State == EAssetRequestState::Failed &&
            Snapshot.Result == EAssetResult::SourceChanged &&
            Manager->GetResult(Request, Payload) ==
                EAssetResult::SourceChanged &&
            !Payload.IsValid(),
        "source mutation rejects the entire development publication");
    Record(Result,
        Extensions.ImportCalls->load() == 1,
        "source mutation is not retried implicitly");
    if (Manager) (void)Manager->Shutdown();
}

void TestReloadAfterUnload(FAssetManagerDevelopmentTestResult& Result)
{
    const FAssetId Id = MakeRuntimeTestId("Runtime/ReloadAfterUnload");
    auto Extensions = MakeRuntimeTestExtensions(Id, "reload-payload");
    Core::TSharedPtr<FAssetManager> Manager;
    FAssetDiagnosticList Diagnostics;
    const bool Created = FAssetManager::Create(
        MakeDevelopmentManagerConfig(Extensions), Manager, Diagnostics) ==
        EAssetResult::Success;
    FAssetRequestHandle FirstRequest;
    FAssetRequestSnapshot FirstSnapshot;
    TAssetHandle<FRuntimeTestPayload> First;
    const bool FirstLoaded = Created &&
        Manager->Request<FRuntimeTestPayload>(Id, FirstRequest) ==
            EAssetResult::Success &&
        WaitForRequestTerminal(*Manager, FirstRequest, FirstSnapshot) &&
        Manager->GetResult(FirstRequest, First) == EAssetResult::Success;
    First.Reset();
    const bool Released = FirstLoaded &&
        Manager->ReleaseRequest(FirstRequest) == EAssetResult::Success;

    FAssetRequestHandle SecondRequest;
    FAssetRequestSnapshot SecondSnapshot;
    TAssetHandle<FRuntimeTestPayload> Second;
    const bool Reloaded = Released &&
        Manager->Request<FRuntimeTestPayload>(Id, SecondRequest) ==
            EAssetResult::Success &&
        WaitForRequestTerminal(*Manager, SecondRequest, SecondSnapshot) &&
        Manager->GetResult(SecondRequest, Second) == EAssetResult::Success;
    Record(Result,
        Reloaded && Extensions.ImportCalls->load() == 2,
        "development reload after full unload performs a fresh import");
    if (Manager) (void)Manager->Shutdown();
}
} // namespace

FAssetManagerDevelopmentTestResult RunAssetManagerDevelopmentTests()
{
    FAssetManagerDevelopmentTestResult Result;
    TestMutationRejectsAtomicPublication(Result);
    TestReloadAfterUnload(Result);
    return Result;
}
