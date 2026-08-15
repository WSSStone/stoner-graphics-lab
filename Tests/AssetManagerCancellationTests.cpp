#include "AssetManagerCancellationTests.h"

#include "AssetManagerTestSupport.h"
#include "Helpers/AssetManagerControlledLoader.h"

#include <iostream>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;

void Record(
    FAssetManagerCancellationTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

struct FControlledManager
{
    FAssetManagerTestExtensions Extensions;
    Core::TSharedPtr<Tests::FAssetManagerControlledState> State;
    Core::TSharedPtr<FAssetManager> Manager;
};

FControlledManager Make(
    const FAssetId& Id,
    Core::uint64 Deadline = 2000,
    bool Conforming = true)
{
    FControlledManager Result;
    Result.Extensions = MakeRuntimeTestExtensions(Id);
    Result.Extensions.ImporterToken.Reset();
    Result.State = Core::MakeShared<Tests::FAssetManagerControlledState>();
    (void)Result.Extensions.Registry->Register(
        Core::MakeShared<Tests::FAssetManagerControlledLoader>(
            Id, Result.State, Conforming),
        Result.Extensions.ImporterToken);
    auto Config = MakeDevelopmentManagerConfig(Result.Extensions);
    Config.ExtensionDeadlineMilliseconds = Deadline;
    FAssetDiagnosticList Diagnostics;
    (void)FAssetManager::Create(Config, Result.Manager, Diagnostics);
    return Result;
}

void TestAllCallerCancellation(FAssetManagerCancellationTestResult& Result)
{
    const FAssetId Id = MakeRuntimeTestId("Runtime/CancelAll");
    auto Controlled = Make(Id);
    FAssetRequestHandle First;
    FAssetRequestHandle Second;
    const bool Admitted = Controlled.Manager &&
        Controlled.Manager->Request<FRuntimeTestPayload>(Id, First) ==
            EAssetResult::Success &&
        Controlled.Manager->Request<FRuntimeTestPayload>(Id, Second) ==
            EAssetResult::Success &&
        Controlled.State->WaitUntilStarted(1000);
    const bool Cancelled = Admitted &&
        Controlled.Manager->Cancel(First) == EAssetResult::Success &&
        Controlled.Manager->Cancel(First) == EAssetResult::Success &&
        Controlled.Manager->Cancel(Second) == EAssetResult::Success;
    FAssetRequestSnapshot FirstSnapshot;
    FAssetRequestSnapshot SecondSnapshot;
    const bool Terminal = Cancelled &&
        WaitForRequestTerminal(*Controlled.Manager, First, FirstSnapshot) &&
        WaitForRequestTerminal(*Controlled.Manager, Second, SecondSnapshot);
    Record(Result,
        Terminal && FirstSnapshot.State == EAssetRequestState::Cancelled &&
            SecondSnapshot.State == EAssetRequestState::Cancelled &&
            Controlled.State->GetCalls() == 1,
        "all callers cancel one shared physical operation idempotently");
    Controlled.State->Release();
    (void)Controlled.Manager->Shutdown();
}

void TestIndependentCancellation(FAssetManagerCancellationTestResult& Result)
{
    const FAssetId Id = MakeRuntimeTestId("Runtime/CancelOne");
    auto Controlled = Make(Id);
    FAssetRequestHandle Cancelled;
    FAssetRequestHandle Survivor;
    const bool Admitted = Controlled.Manager &&
        Controlled.Manager->Request<FRuntimeTestPayload>(Id, Survivor) ==
            EAssetResult::Success &&
        Controlled.State->WaitUntilStarted(1000) &&
        Controlled.Manager->Request<FRuntimeTestPayload>(Id, Cancelled) ==
            EAssetResult::Success;
    FAssetRequestSnapshot WaitingSnapshot;
    const bool Waiting = Admitted &&
        Controlled.Manager->Query(Cancelled, WaitingSnapshot) ==
            EAssetResult::Success &&
        WaitingSnapshot.State == EAssetRequestState::WaitingForDependencies;
    const auto CancelResult = Controlled.Manager->Cancel(Cancelled);
    Controlled.State->Release();
    FAssetRequestSnapshot CancelledSnapshot;
    FAssetRequestSnapshot SurvivorSnapshot;
    const bool Terminal = Admitted &&
        WaitForRequestTerminal(
            *Controlled.Manager, Cancelled, CancelledSnapshot) &&
        WaitForRequestTerminal(
            *Controlled.Manager, Survivor, SurvivorSnapshot);
    Record(Result,
        Waiting && CancelResult == EAssetResult::Success && Terminal &&
            CancelledSnapshot.State == EAssetRequestState::Cancelled &&
            SurvivorSnapshot.State == EAssetRequestState::Ready &&
            Controlled.State->GetCalls() == 1,
        "one caller cancellation does not cancel a shared operation");
    (void)Controlled.Manager->Shutdown();
}

void TestDeadlineAndLateCancellation(
    FAssetManagerCancellationTestResult& Result)
{
    const FAssetId DeadlineId = MakeRuntimeTestId("Runtime/Deadline");
    auto Deadline = Make(DeadlineId, 20);
    FAssetRequestHandle DeadlineRequest;
    const bool Admitted = Deadline.Manager &&
        Deadline.Manager->Request<FRuntimeTestPayload>(
            DeadlineId, DeadlineRequest) == EAssetResult::Success &&
        Deadline.State->WaitUntilStarted(1000);
    FAssetRequestSnapshot DeadlineSnapshot;
    const bool Terminal = Admitted && WaitForRequestTerminal(
        *Deadline.Manager, DeadlineRequest, DeadlineSnapshot);
    Record(Result,
        Terminal && DeadlineSnapshot.State == EAssetRequestState::Failed &&
            DeadlineSnapshot.Result == EAssetResult::DeadlineExceeded,
        "cooperative extension deadline produces a stable failure");
    Deadline.State->Release();
    (void)Deadline.Manager->Shutdown();

    const FAssetId LateId = MakeRuntimeTestId("Runtime/LateCancel");
    auto Late = Make(LateId);
    FAssetRequestHandle LateRequest;
    const bool LateAdmitted = Late.Manager &&
        Late.Manager->Request<FRuntimeTestPayload>(LateId, LateRequest) ==
            EAssetResult::Success && Late.State->WaitUntilStarted(1000);
    Late.State->Release();
    FAssetRequestSnapshot LateSnapshot;
    const bool LateReady = LateAdmitted && WaitForRequestTerminal(
        *Late.Manager, LateRequest, LateSnapshot);
    TAssetHandle<FRuntimeTestPayload> Handle;
    const bool Got = LateReady &&
        Late.Manager->GetResult(LateRequest, Handle) == EAssetResult::Success;
    Record(Result,
        Got && Late.Manager->Cancel(LateRequest) == EAssetResult::Success &&
            Handle.IsValid(),
        "late cancellation cannot revoke a published typed handle");
    (void)Late.Manager->Shutdown();
}

void TestBoundedNonConformingExtension(
    FAssetManagerCancellationTestResult& Result)
{
    const FAssetId Id = MakeRuntimeTestId("Runtime/BoundedViolation");
    auto Controlled = Make(Id, 20, false);
    FAssetRequestHandle Request;
    const bool Admitted = Controlled.Manager &&
        Controlled.Manager->Request<FRuntimeTestPayload>(Id, Request) ==
            EAssetResult::Success &&
        Controlled.State->WaitUntilStarted(1000);
    FAssetRequestSnapshot Snapshot;
    const bool Terminal = Admitted &&
        WaitForRequestTerminal(*Controlled.Manager, Request, Snapshot);
    Record(Result,
        Terminal && Snapshot.State == EAssetRequestState::Failed &&
            Snapshot.Result == EAssetResult::DeadlineExceeded &&
            Controlled.State->GetCalls() == 1,
        "bounded non-conforming extension cannot publish after its deadline");
    (void)Controlled.Manager->Shutdown();
}
} // namespace

FAssetManagerCancellationTestResult RunAssetManagerCancellationTests()
{
    FAssetManagerCancellationTestResult Result;
    TestIndependentCancellation(Result);
    TestAllCallerCancellation(Result);
    TestDeadlineAndLateCancellation(Result);
    TestBoundedNonConformingExtension(Result);
    return Result;
}
