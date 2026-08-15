#include "AssetManagerContractTests.h"

#include "AssetManagerTestSupport.h"

#include <iostream>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;

void Record(FAssetManagerContractTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

void TestConfigAndTypedRequest(FAssetManagerContractTestResult& Result)
{
    const FAssetId Id = MakeRuntimeTestId();
    auto Extensions = MakeRuntimeTestExtensions(Id);
    FAssetManagerConfig Config = MakeDevelopmentManagerConfig(Extensions);
    FAssetDiagnosticList Diagnostics;
    Core::TSharedPtr<FAssetManager> Manager;
    Record(Result,
        FAssetManager::Create(Config, Manager, Diagnostics) ==
                EAssetResult::Success &&
            Manager && Diagnostics.empty(),
        "valid development configuration creates one manager");

    FAssetRequestHandle Request;
    const EAssetResult Requested = Manager->Request<FRuntimeTestPayload>(Id, Request);
    FAssetRequestSnapshot Snapshot;
    const bool Terminal = WaitForRequestTerminal(*Manager, Request, Snapshot);
    TAssetHandle<FRuntimeTestPayload> Payload;
    Record(Result,
        Requested == EAssetResult::Success && Request.IsValid() && Terminal &&
            Snapshot.State == EAssetRequestState::Ready &&
            Manager->GetResult(Request, Payload) == EAssetResult::Success &&
            Payload.IsValid() &&
            Payload->GetValue() == Core::FString("runtime-payload"),
        "typed request publishes one immutable ready result");
    Record(Result,
        Extensions.ResolveCalls->load() == 2 &&
            Extensions.ImportCalls->load() == 1,
        "development load revalidates the authoritative source once");
    Record(Result,
        Manager->ReleaseRequest(Request) == EAssetResult::Success &&
            Manager->Query(Request, Snapshot) == EAssetResult::InvalidHandle &&
            Payload.IsValid(),
        "request release rejects stale observation without invalidating handle");
    (void)Manager->Shutdown();
}

void TestFailuresHaveNoPartialResult(FAssetManagerContractTestResult& Result)
{
    const FAssetId Id = MakeRuntimeTestId("Runtime/Failure");
    auto Extensions = MakeRuntimeTestExtensions(Id);
    FAssetManagerConfig Invalid = MakeDevelopmentManagerConfig(Extensions);
    Invalid.PublicationRoot = Core::FString("Cooked");
    Core::TSharedPtr<FAssetManager> Manager;
    FAssetDiagnosticList Diagnostics;
    Record(Result,
        FAssetManager::Create(Invalid, Manager, Diagnostics) ==
                EAssetResult::InvalidInput &&
            !Manager,
        "mixed source and cooked configuration fails without a manager");

    FAssetManagerConfig Config = MakeDevelopmentManagerConfig(Extensions);
    Record(Result,
        FAssetManager::Create(Config, Manager, Diagnostics) ==
            EAssetResult::Success,
        "failure test creates a valid manager");
    FAssetId WrongType;
    (void)FAssetId::Create(Core::FString("Image"),
        Core::FString("Runtime/Failure"), {}, WrongType);
    FAssetRequestHandle Request;
    FAssetRequestHandle InvalidIdentity;
    Record(Result,
        Manager->Request<FRuntimeTestPayload>(FAssetId{}, InvalidIdentity) ==
                EAssetResult::InvalidIdentity &&
            !InvalidIdentity.IsValid(),
        "malformed asset identity rejects admission without a request");
    Record(Result,
        Manager->Request<FRuntimeTestPayload>(WrongType, Request) ==
                EAssetResult::TypeMismatch &&
            !Request.IsValid(),
        "type mismatch rejects admission without a partial request");
    TAssetHandle<FRuntimeTestPayload> Payload;
    Record(Result,
        Manager->GetResult({}, Payload) == EAssetResult::InvalidHandle &&
            !Payload.IsValid(),
        "invalid request returns no partial typed result");
    (void)Manager->Shutdown();
}
} // namespace

FAssetManagerContractTestResult RunAssetManagerContractTests()
{
    FAssetManagerContractTestResult Result;
    TestConfigAndTypedRequest(Result);
    TestFailuresHaveNoPartialResult(Result);
    return Result;
}
