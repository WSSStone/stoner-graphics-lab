#include "AssetManagerCompletionTests.h"

#include "AssetManagerTestSupport.h"
#include "Helpers/AssetManagerControlledLoader.h"

#include <iostream>
#include <thread>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;

void Record(
    FAssetManagerCompletionTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

void TestReservationAndRollback(FAssetManagerCompletionTestResult& Result)
{
    const FAssetId Id = MakeRuntimeTestId("Runtime/CompletionCapacity");
    auto Extensions = MakeRuntimeTestExtensions(Id);
    Extensions.ImporterToken.Reset();
    auto State = Core::MakeShared<Tests::FAssetManagerControlledState>();
    (void)Extensions.Registry->Register(
        Core::MakeShared<Tests::FAssetManagerControlledLoader>(Id, State),
        Extensions.ImporterToken);
    auto Config = MakeDevelopmentManagerConfig(Extensions);
    Config.Limits.MaxCompletions = 1;
    Core::TSharedPtr<FAssetManager> Manager;
    FAssetDiagnosticList Diagnostics;
    const bool Created = FAssetManager::Create(Config, Manager, Diagnostics) ==
        EAssetResult::Success;
    FAssetRequestHandle First;
    FAssetRequestHandle Rejected;
    FAssetRequestHandle PollOnly;
    const bool FirstAdmitted = Created &&
        Manager->Request<FRuntimeTestPayload>(Id, First,
            [](FAssetRequestHandle, EAssetResult) {}) ==
            EAssetResult::Success && State->WaitUntilStarted(1000);
    const bool Exhausted = Manager->Request<FRuntimeTestPayload>(Id, Rejected,
            [](FAssetRequestHandle, EAssetResult) {}) ==
            EAssetResult::CapacityExceeded && !Rejected.IsValid();
    const bool PollAdmission = Manager->Request<FRuntimeTestPayload>(
        Id, PollOnly) == EAssetResult::Success;
    const bool Released = Manager->ReleaseRequest(First) ==
        EAssetResult::Success;
    FAssetRequestHandle Reused;
    const bool ReusedReservation = Manager->Request<FRuntimeTestPayload>(Id,
        Reused, [](FAssetRequestHandle, EAssetResult) {}) ==
        EAssetResult::Success;
    Record(Result,
        FirstAdmitted && Exhausted && PollAdmission && Released &&
            ReusedReservation &&
            Manager->Inspect().CompletionReservations == 1,
        "callback admission reserves capacity atomically and release rolls it back exactly once");
    State->Release();
    (void)Manager->Shutdown();
}

void TestPumpAndReentrancy(FAssetManagerCompletionTestResult& Result)
{
    const FAssetId Id = MakeRuntimeTestId("Runtime/CompletionPump");
    auto Extensions = MakeRuntimeTestExtensions(Id);
    Core::TSharedPtr<FAssetManager> Manager;
    FAssetDiagnosticList Diagnostics;
    const bool Created = FAssetManager::Create(
        MakeDevelopmentManagerConfig(Extensions), Manager, Diagnostics) ==
        EAssetResult::Success;
    int CallbackCount = 0;
    bool CallbackSafeOperations = false;
    EAssetResult RecursiveResult = EAssetResult::Success;
    std::thread::id CallbackThread;
    const auto PumpThread = std::this_thread::get_id();
    FAssetRequestHandle First;
    FAssetRequestHandle Second;
    const auto Callback = [&](FAssetRequestHandle Request, EAssetResult Value)
    {
        ++CallbackCount;
        CallbackThread = std::this_thread::get_id();
        FAssetRequestSnapshot Snapshot;
        FAssetRequestHandle Nested;
        CallbackSafeOperations = Value == EAssetResult::Success &&
            Manager->Query(Request, Snapshot) == EAssetResult::Success &&
            Snapshot.State == EAssetRequestState::Ready &&
            Manager->Request<FRuntimeTestPayload>(Id, Nested) ==
                EAssetResult::Success;
        if (Nested.IsValid()) (void)Manager->ReleaseRequest(Nested);
        RecursiveResult = Manager->PumpCompletions(1).Result;
    };
    const bool Admitted = Created &&
        Manager->Request<FRuntimeTestPayload>(Id, First, Callback) ==
            EAssetResult::Success &&
        Manager->Request<FRuntimeTestPayload>(Id, Second, Callback) ==
            EAssetResult::Success;
    FAssetRequestSnapshot FirstSnapshot;
    FAssetRequestSnapshot SecondSnapshot;
    const bool Terminal = Admitted &&
        WaitForRequestTerminal(*Manager, First, FirstSnapshot) &&
        WaitForRequestTerminal(*Manager, Second, SecondSnapshot);
    const bool PollOnly = Terminal && CallbackCount == 0 &&
        FirstSnapshot.CompletionSequence > 0 &&
        SecondSnapshot.CompletionSequence > FirstSnapshot.CompletionSequence;
    const auto Prefix = Manager->PumpCompletions(1);
    const auto Rest = Manager->PumpCompletions(8);
    Record(Result,
        PollOnly && Prefix.Result == EAssetResult::Success &&
            Prefix.Dispatched == 1 && Rest.Dispatched == 1 &&
            CallbackCount == 2 && CallbackThread == PumpThread &&
            CallbackSafeOperations &&
            RecursiveResult == EAssetResult::ReentrantPump &&
            Manager->Inspect().CompletionReservations == 0,
        "explicit prefix pump dispatches on its caller thread and rejects recursive pumping");
    (void)Manager->ReleaseRequest(First);
    (void)Manager->ReleaseRequest(Second);
    (void)Manager->Shutdown();
}

void TestReleaseSuppressesQueuedCallback(
    FAssetManagerCompletionTestResult& Result)
{
    const FAssetId Id = MakeRuntimeTestId("Runtime/CompletionRelease");
    auto Extensions = MakeRuntimeTestExtensions(Id);
    Core::TSharedPtr<FAssetManager> Manager;
    FAssetDiagnosticList Diagnostics;
    int Called = 0;
    FAssetRequestHandle Request;
    FAssetRequestSnapshot Snapshot;
    const bool Ready = FAssetManager::Create(
            MakeDevelopmentManagerConfig(Extensions), Manager, Diagnostics) ==
            EAssetResult::Success &&
        Manager->Request<FRuntimeTestPayload>(Id, Request,
            [&](FAssetRequestHandle, EAssetResult) { ++Called; }) ==
            EAssetResult::Success &&
        WaitForRequestTerminal(*Manager, Request, Snapshot);
    const bool Released = Ready &&
        Manager->ReleaseRequest(Request) == EAssetResult::Success;
    const auto Pump = Manager->PumpCompletions(8);
    Record(Result,
        Released && Pump.Result == EAssetResult::Success &&
            Pump.Dispatched == 0 && Called == 0 &&
            Manager->Inspect().CompletionReservations == 0,
        "request release discards an undelivered callback and frees its reservation");
    (void)Manager->Shutdown();
}
} // namespace

FAssetManagerCompletionTestResult RunAssetManagerCompletionTests()
{
    FAssetManagerCompletionTestResult Result;
    TestReservationAndRollback(Result);
    TestPumpAndReentrancy(Result);
    TestReleaseSuppressesQueuedCallback(Result);
    return Result;
}
