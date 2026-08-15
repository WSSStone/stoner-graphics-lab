#include "AssetManagerShutdownTests.h"

#include "AssetManagerTestSupport.h"
#include "Helpers/AssetManagerControlledLoader.h"

#include <iostream>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;

void Record(
    FAssetManagerShutdownTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

struct FControlled
{
    FAssetManagerTestExtensions Extensions;
    Core::TSharedPtr<Tests::FAssetManagerControlledState> State;
    Core::TSharedPtr<FAssetManager> Manager;
};

FControlled MakeControlled(
    const FAssetId& Id,
    bool Conforming,
    Core::uint64 DeadlineMilliseconds)
{
    FControlled Result;
    Result.Extensions = MakeRuntimeTestExtensions(Id);
    Result.Extensions.ImporterToken.Reset();
    Result.State = Core::MakeShared<Tests::FAssetManagerControlledState>();
    (void)Result.Extensions.Registry->Register(
        Core::MakeShared<Tests::FAssetManagerControlledLoader>(
            Id, Result.State, Conforming),
        Result.Extensions.ImporterToken);
    auto Config = MakeDevelopmentManagerConfig(Result.Extensions);
    Config.ExtensionDeadlineMilliseconds = DeadlineMilliseconds;
    FAssetDiagnosticList Diagnostics;
    (void)FAssetManager::Create(Config, Result.Manager, Diagnostics);
    return Result;
}

void TestCooperativeShutdown(FAssetManagerShutdownTestResult& Result)
{
    const FAssetId Id = MakeRuntimeTestId("Runtime/ShutdownInFlight");
    auto Controlled = MakeControlled(Id, true, 1000);
    FAssetRequestHandle Request;
    const bool Started = Controlled.Manager &&
        Controlled.Manager->Request<FRuntimeTestPayload>(Id, Request) ==
            EAssetResult::Success &&
        Controlled.State->WaitUntilStarted(1000);
    const bool ShutDown = Started &&
        Controlled.Manager->Shutdown() == EAssetResult::Success &&
        Controlled.Manager->Shutdown() == EAssetResult::Success;
    FAssetRequestSnapshot Snapshot;
    FAssetRequestHandle Rejected;
    const bool Terminal = Controlled.Manager->Query(Request, Snapshot) ==
        EAssetResult::Success;
    Record(Result,
        ShutDown && Terminal &&
            Snapshot.State == EAssetRequestState::Cancelled &&
            Controlled.Manager->Request<FRuntimeTestPayload>(Id, Rejected) ==
                EAssetResult::ShuttingDown && !Rejected.IsValid() &&
            Controlled.Manager->Inspect().CachedAssets == 0,
        "shutdown cancels and joins in-flight work, rejects admission, and is idempotent");
}

void TestBoundedViolationDiagnostic(FAssetManagerShutdownTestResult& Result)
{
    const FAssetId Id = MakeRuntimeTestId("Runtime/ShutdownViolation");
    auto Controlled = MakeControlled(Id, false, 20);
    FAssetRequestHandle Request;
    FAssetRequestSnapshot Snapshot;
    const bool Terminal = Controlled.Manager &&
        Controlled.Manager->Request<FRuntimeTestPayload>(Id, Request) ==
            EAssetResult::Success &&
        WaitForRequestTerminal(*Controlled.Manager, Request, Snapshot);
    Record(Result,
        Terminal && Snapshot.Result == EAssetResult::DeadlineExceeded &&
            Controlled.Manager->Inspect().ExtensionContractViolations == 1,
        "bounded non-conforming extension produces normalized violation evidence");
    (void)Controlled.Manager->Shutdown();
}

void TestRepeatedTerminalAudit(FAssetManagerShutdownTestResult& Result)
{
    bool Passed = true;
    for (int Iteration = 0; Iteration < 100 && Passed; ++Iteration)
    {
        const FAssetId Id = MakeRuntimeTestId("Runtime/ShutdownRepeat");
        auto Extensions = MakeRuntimeTestExtensions(Id);
        Core::TSharedPtr<FAssetManager> Manager;
        FAssetDiagnosticList Diagnostics;
        FAssetRequestHandle Request;
        Passed = FAssetManager::Create(
                MakeDevelopmentManagerConfig(Extensions), Manager,
                Diagnostics) == EAssetResult::Success &&
            Manager->Request<FRuntimeTestPayload>(Id, Request) ==
                EAssetResult::Success &&
            Manager->Shutdown() == EAssetResult::Success;
        FAssetRequestSnapshot Snapshot;
        Passed = Passed && Manager->Query(Request, Snapshot) ==
            EAssetResult::Success &&
            (Snapshot.State == EAssetRequestState::Ready ||
             Snapshot.State == EAssetRequestState::Cancelled);
    }
    Record(Result, Passed,
        "one hundred startup/shutdown races leave every accepted request terminal");
}
} // namespace

FAssetManagerShutdownTestResult RunAssetManagerShutdownTests()
{
    FAssetManagerShutdownTestResult Result;
    TestCooperativeShutdown(Result);
    TestBoundedViolationDiagnostic(Result);
    TestRepeatedTerminalAudit(Result);
    return Result;
}
