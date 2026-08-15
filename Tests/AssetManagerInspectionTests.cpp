#include "AssetManagerInspectionTests.h"

#include "AssetManagerTestSupport.h"
#include "Helpers/AssetManagerControlledLoader.h"
#include "FAssetManagerInspection.h"

#include <algorithm>
#include <iostream>
#include <utility>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;

void Record(
    FAssetManagerInspectionTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FAssetRequestInspectionRecord MakeRequestRecord(Core::uint32 Slot)
{
    FAssetRequestInspectionRecord RecordValue;
    RecordValue.Request.Handle = FAssetRequestHandle();
    RecordValue.AssetId = MakeRuntimeTestId(
        Slot == 0 ? "Runtime/InspectionA" : "Runtime/InspectionB");
    RecordValue.ExpectedType = Core::FString("RuntimeTest");
    return RecordValue;
}

void TestBoundedNormalization(FAssetManagerInspectionTestResult& Result)
{
    FAssetManagerInspection Inspection;
    Inspection.Requests = {MakeRequestRecord(1), MakeRequestRecord(0)};
    FAssetOperationInspectionRecord Operation;
    Operation.AssetId = MakeRuntimeTestId("Runtime/Z");
    Operation.ExpectedType = Core::FString("RuntimeTest");
    Inspection.Operations.push_back(std::move(Operation));
    FAssetCacheInspectionRecord Cache;
    Cache.AssetId = MakeRuntimeTestId("Runtime/Y");
    Cache.ExpectedType = Core::FString("RuntimeTest");
    Inspection.Cache.push_back(std::move(Cache));
    Private::NormalizeAssetManagerInspection(Inspection, 2);
    Record(Result,
        Inspection.Requests.size() == 2 && Inspection.Operations.empty() &&
            Inspection.Cache.empty() && Inspection.bInspectionTruncated,
        "inspection applies one deterministic global record bound");
}

void TestManagerInspection(FAssetManagerInspectionTestResult& Result)
{
    const FAssetId Id = MakeRuntimeTestId("Runtime/Inspection");
    auto Extensions = MakeRuntimeTestExtensions(Id);
    auto Config = MakeDevelopmentManagerConfig(Extensions);
    Config.Limits.MaxDiagnostics = 1;
    Core::TSharedPtr<FAssetManager> Manager;
    FAssetDiagnosticList Diagnostics;
    const bool Created = FAssetManager::Create(Config, Manager, Diagnostics) ==
        EAssetResult::Success;
    FAssetRequestHandle First;
    FAssetRequestHandle Second;
    FAssetRequestSnapshot FirstSnapshot;
    FAssetRequestSnapshot SecondSnapshot;
    const bool Ready = Created &&
        Manager->Request<FRuntimeTestPayload>(Id, First) ==
            EAssetResult::Success &&
        Manager->Request<FRuntimeTestPayload>(Id, Second) ==
            EAssetResult::Success &&
        WaitForRequestTerminal(*Manager, First, FirstSnapshot) &&
        WaitForRequestTerminal(*Manager, Second, SecondSnapshot);
    const auto Inspection = Manager->Inspect();
    const bool Redacted = Inspection.Requests.empty() ||
        Inspection.Requests.front().AssetId.ToString().ToStdString().find(
            Config.SourceRoot.ToStdString()) == std::string::npos;
    Record(Result,
        Ready && Inspection.ReadyRequests == 2 &&
            Inspection.CachedAssets == 1 &&
            Inspection.RequestRetentions == 2 &&
            Inspection.Requests.size() == 1 &&
            Inspection.Operations.empty() && Inspection.Cache.empty() &&
            Inspection.bInspectionTruncated && Redacted,
        "manager inspection is bounded, stable, and excludes native source paths");
    (void)Manager->ReleaseRequest(First);
    (void)Manager->ReleaseRequest(Second);
    (void)Manager->Shutdown();
}

const FAssetRequestInspectionRecord* FindRequest(
    const FAssetManagerInspection& Inspection,
    FAssetRequestHandle Handle)
{
    const auto Found = std::find_if(
        Inspection.Requests.begin(), Inspection.Requests.end(),
        [Handle](const FAssetRequestInspectionRecord& RecordValue)
        {
            return RecordValue.Request.Handle == Handle;
        });
    return Found == Inspection.Requests.end() ? nullptr : &*Found;
}

void TestCoalescingAndCacheDecisions(
    FAssetManagerInspectionTestResult& Result)
{
    const FAssetId Id = MakeRuntimeTestId("Runtime/InspectionDecisions");
    auto Extensions = MakeRuntimeTestExtensions(Id);
    Extensions.ImporterToken.Reset();
    auto State = Core::MakeShared<Tests::FAssetManagerControlledState>();
    (void)Extensions.Registry->Register(
        Core::MakeShared<Tests::FAssetManagerControlledLoader>(Id, State),
        Extensions.ImporterToken);
    auto Config = MakeDevelopmentManagerConfig(Extensions);
    Config.Limits.MaxDiagnostics = 16;
    Core::TSharedPtr<FAssetManager> Manager;
    FAssetDiagnosticList Diagnostics;
    FAssetRequestHandle First;
    FAssetRequestHandle Second;
    const bool Admitted = FAssetManager::Create(
            Config, Manager, Diagnostics) == EAssetResult::Success &&
        Manager->Request<FRuntimeTestPayload>(Id, First) ==
            EAssetResult::Success && State->WaitUntilStarted(1000) &&
        Manager->Request<FRuntimeTestPayload>(Id, Second) ==
            EAssetResult::Success;
    const auto InFlight = Manager->Inspect();
    const auto* FirstRecord = FindRequest(InFlight, First);
    const auto* SecondRecord = FindRequest(InFlight, Second);
    const bool Coalesced = Admitted && FirstRecord && SecondRecord &&
        !FirstRecord->bCoalesced && SecondRecord->bCoalesced &&
        !SecondRecord->bCacheHit;
    State->Release();
    FAssetRequestSnapshot FirstSnapshot;
    FAssetRequestSnapshot SecondSnapshot;
    const bool Ready = WaitForRequestTerminal(
            *Manager, First, FirstSnapshot) &&
        WaitForRequestTerminal(*Manager, Second, SecondSnapshot);
    FAssetRequestHandle Third;
    const bool CacheAdmission = Ready &&
        Manager->Request<FRuntimeTestPayload>(Id, Third) ==
            EAssetResult::Success;
    const auto Cached = Manager->Inspect();
    const auto* ThirdRecord = FindRequest(Cached, Third);
    Record(Result,
        Coalesced && CacheAdmission && ThirdRecord &&
            ThirdRecord->bCacheHit && !ThirdRecord->bCoalesced,
        "inspection distinguishes in-flight coalescing from a ready cache hit");
    (void)Manager->ReleaseRequest(First);
    (void)Manager->ReleaseRequest(Second);
    (void)Manager->ReleaseRequest(Third);
    (void)Manager->Shutdown();
}
} // namespace

FAssetManagerInspectionTestResult RunAssetManagerInspectionTests()
{
    FAssetManagerInspectionTestResult Result;
    TestBoundedNormalization(Result);
    TestManagerInspection(Result);
    TestCoalescingAndCacheDecisions(Result);
    return Result;
}
