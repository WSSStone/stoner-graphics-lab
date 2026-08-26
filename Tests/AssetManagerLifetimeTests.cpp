#include "AssetManagerLifetimeTests.h"

#include "AssetManagerTestSupport.h"

#include <iostream>
#include <utility>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;

void Record(
    FAssetManagerLifetimeTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

void TestHandleLifetime(FAssetManagerLifetimeTestResult& Result)
{
    const FAssetId Id = MakeRuntimeTestId("Runtime/HandleLifetime");
    auto Extensions = MakeRuntimeTestExtensions(Id, "survives-manager");
    Core::TSharedPtr<FAssetManager> Manager;
    FAssetDiagnosticList Diagnostics;
    FAssetRequestHandle Request;
    FAssetRequestSnapshot Snapshot;
    TAssetHandle<FRuntimeTestPayload> Original;
    const bool Ready = FAssetManager::Create(
            MakeDevelopmentManagerConfig(Extensions), Manager, Diagnostics) ==
            EAssetResult::Success &&
        Manager->Request<FRuntimeTestPayload>(Id, Request) ==
            EAssetResult::Success &&
        WaitForRequestTerminal(*Manager, Request, Snapshot) &&
        Manager->GetResult(Request, Original) == EAssetResult::Success;
    TAssetHandle<FRuntimeTestPayload> Copy = Original;
    TAssetHandle<FRuntimeTestPayload> Moved = std::move(Copy);
    Core::TSharedPtr<const FRuntimeTestPayload> Shared = Original.GetShared();
    const auto OneControl = Manager->Inspect();
    (void)Manager->ReleaseRequest(Request);
    (void)Manager->Shutdown();
    Manager.reset();
    Record(Result,
        Ready && !Copy.IsValid() && Original.IsValid() && Moved.IsValid() &&
            Shared && Original.Get() == Moved.Get() &&
            Original.Get() == Shared.get() &&
            OneControl.ExternalHandleRetentions == 1 &&
            Moved.GetIdentity() == Id &&
            Moved->GetValue() == Core::FString("survives-manager"),
        "copied and moved typed handles share one manager-independent retention control");
    Original.Reset();
    Moved.Reset();
    Record(Result,
        Shared && Shared->GetValue() ==
            Core::FString("survives-manager"),
        "shared typed payload aliases the retention control after handles and manager are destroyed");
    Shared.reset();
    Record(Result,
        !Shared,
        "shared typed payload releases its final retention control deterministically");
}
} // namespace

FAssetManagerLifetimeTestResult RunAssetManagerLifetimeTests()
{
    FAssetManagerLifetimeTestResult Result;
    TestHandleLifetime(Result);
    return Result;
}
