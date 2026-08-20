#include "MetalDeviceTests.h"

#include "MetalRHI/FMetalDeviceFactory.h"
#include "Core/SGPlatform.h"
#if SG_PLATFORM_MAC
#include "FMetalAdapter.h"
#include "FMetalDeviceOwnerState.h"
#endif
#include "RHI/FRHIBufferDesc.h"
#include "RHI/FRHIBufferUploadDesc.h"

#include <iostream>
#include <limits>

namespace
{

using namespace Stoner;
using namespace Stoner::Backend::Metal;
using namespace Stoner::Core;
using namespace Stoner::RHI;

void Record(FMetalDeviceTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

void TestSelection(FMetalDeviceTestResult& Result)
{
#if SG_PLATFORM_MAC
    Core::TArray<FMetalAdapterSummary> Synthetic = {
        {30, FString("Headless"), false, false, true},
        {20, FString("LowPower"), true, false, false},
        {10, FString("HighPerformance"), false, false, false}};
    FMetalBackendConfig Policy;
    usize Index = 99;
    FString Reason;
    const bool Canonical = Private::SelectMetalAdapterCandidate(
        Synthetic, Policy, Index, Reason) == ERHIResult::Success &&
        Synthetic[Index].RegistryId == 10;
    Policy.AdapterPreference = EMetalAdapterPreference::LowPower;
    const bool LowPower = Private::SelectMetalAdapterCandidate(
        Synthetic, Policy, Index, Reason) == ERHIResult::Success &&
        Synthetic[Index].RegistryId == 20;
    Policy.PreferredRegistryId = 30;
    const bool ExplicitPolicy = Private::SelectMetalAdapterCandidate(
        Synthetic, Policy, Index, Reason) == ERHIResult::Success &&
        Synthetic[Index].RegistryId == 30;
    Policy.bRequirePresentation = true;
    const bool HeadlessRejected = Private::SelectMetalAdapterCandidate(
        Synthetic, Policy, Index, Reason) == ERHIResult::Unavailable &&
        Reason == FString("metal-explicit-adapter-unavailable");
    Record(Result, Canonical && LowPower && ExplicitPolicy && HeadlessRejected,
        "adapter policy is canonical and explicit selection has precedence");

    const FMetalDeviceCreateResult Created = CreateMetalDevice();
    if (!Created.Succeeded())
    {
        const bool Controlled = Created.Result == ERHIResult::Unavailable &&
            !Created.Diagnostics.Records.empty() &&
            Created.Diagnostics.Records.front().StableReason ==
                FString("metal-no-eligible-adapter");
        Record(Result, Controlled,
            "Metal device probe reports controlled unavailable honestly");
        return;
    }
    Record(Result,
        Created.Device->IsActive() &&
            Created.Device->GetRuntimeSnapshot().ProvesNativeExecution() &&
            IsValidRHIDeviceCapabilities(Created.Device->GetCapabilities()) &&
            Created.Diagnostics.SelectedAdapter.RegistryId != 0 &&
            !Created.Diagnostics.SelectedAdapter.Name.IsEmpty(),
        "Metal selects a real adapter and publishes valid native capabilities");

    FMetalBackendConfig Explicit;
    Explicit.PreferredRegistryId =
        Created.Diagnostics.SelectedAdapter.RegistryId;
    const FMetalDeviceCreateResult Repeated = CreateMetalDevice(Explicit);
    Record(Result,
        Repeated.Succeeded() &&
            Repeated.Diagnostics.SelectedAdapter.RegistryId ==
                *Explicit.PreferredRegistryId,
        "explicit registry identity takes precedence deterministically");

    Explicit.PreferredRegistryId = std::numeric_limits<uint64>::max();
    const FMetalDeviceCreateResult Missing = CreateMetalDevice(Explicit);
    Record(Result,
        !Missing.Succeeded() && Missing.Result == ERHIResult::Unavailable &&
            !Missing.Diagnostics.Records.empty() &&
            Missing.Diagnostics.Records.front().StableReason ==
                FString("metal-explicit-adapter-unavailable"),
        "missing explicit adapter fails without fallback");
#else
    const auto Unsupported = CreateMetalDevice();
    Record(Result,
        Unsupported.Result == ERHIResult::Unsupported &&
            !Unsupported.Device &&
            Unsupported.Diagnostics.Records.size() == 1 &&
            Unsupported.Diagnostics.Records.front().StableReason ==
                FString("metal-host-unsupported"),
        "Metal factory is API-free and explicitly unsupported off macOS");
#endif
}

void TestOwnershipAndFailure(FMetalDeviceTestResult& Result)
{
#if SG_PLATFORM_MAC
    auto SyntheticOwner = MakeShared<Private::FMetalDeviceOwnerState>(77);
    const uint64 Generation = SyntheticOwner->GetGeneration();
    const bool Registered = SyntheticOwner->TryRegisterObject(
        Private::EMetalOwnershipCategory::Resource);
    const bool Compatible = SyntheticOwner->IsCompatible(77, Generation);
    SyntheticOwner->StopAdmission();
    SyntheticOwner->AdvanceGeneration();
    SyntheticOwner->ReleaseObject(
        Private::EMetalOwnershipCategory::Resource);
    const auto SyntheticInspection = SyntheticOwner->Inspect();
    Record(Result,
        Registered && Compatible &&
            !SyntheticOwner->IsCompatible(77, Generation) &&
            SyntheticInspection.LiveObjectCount == 0 &&
            SyntheticInspection.Generation > Generation &&
            SyntheticOwner->IsShutdownReady(),
        "owner state tracks generation admission and exact live counts");

    FMetalBackendConfig Injected;
    Injected.FailurePoint =
        EMetalInitializationFailurePoint::AfterAdapterSelection;
    const auto AfterSelection = CreateMetalDevice(Injected);
    Injected.FailurePoint =
        EMetalInitializationFailurePoint::BeforeQueueCreation;
    const auto BeforeQueue = CreateMetalDevice(Injected);
    Record(Result,
        !AfterSelection.Succeeded() && !AfterSelection.Device &&
            !BeforeQueue.Succeeded() && !BeforeQueue.Device,
        "partial initialization failure publishes no device");

    auto First = CreateMetalDevice();
    auto Second = CreateMetalDevice();
    if (!First.Succeeded() || !Second.Succeeded())
    {
        Record(Result,
            First.Result == ERHIResult::Unavailable &&
                Second.Result == ERHIResult::Unavailable,
            "ownership native checks are controlled unavailable without a device");
        return;
    }
    FRHIBufferDesc Desc;
    Desc.SizeInBytes = 16;
    Desc.Usage = ERHIBufferUsage::CopySource |
        ERHIBufferUsage::CopyDestination;
    Desc.MemoryAccess = ERHIMemoryAccess::HostVisible;
    const auto Buffer = First.Succeeded()
        ? First.Device->CreateBuffer(Desc)
        : TRHIObjectResult<IRHIBuffer>{};
    const uint32 Value = 42;
    const FRHIBufferUploadDesc Upload{0, &Value, sizeof(Value)};
    const bool ForeignRejected = Second.Succeeded() && Buffer.Succeeded() &&
        Second.Device->UploadBuffer(Buffer.Object, Upload) ==
            ERHIResult::InvalidState;
    FMetalBackendInspection Before;
    const bool Inspected = InspectMetalDevice(First.Device, Before);
    const ERHIResult Shutdown = First.Succeeded()
        ? First.Device->Shutdown() : ERHIResult::Failed;
    const bool StaleRejected = Buffer.Succeeded() &&
        First.Device->UploadBuffer(Buffer.Object, Upload) ==
            ERHIResult::InvalidState;
    FMetalBackendInspection After;
    const bool InspectedAfter = InspectMetalDevice(First.Device, After);
    Record(Result,
        ForeignRejected && Inspected && Before.LiveObjectCount == 1 &&
            Shutdown == ERHIResult::Success && StaleRejected &&
            InspectedAfter && !After.bAcceptingWork &&
            After.Generation > Before.Generation,
        "owner identity rejects foreign and stale resources across shutdown");
#else
    Record(Result, true, "native ownership checks are macOS-only");
#endif
}

} // namespace

FMetalDeviceTestResult RunMetalDeviceTests()
{
    FMetalDeviceTestResult Result;
    TestSelection(Result);
    TestOwnershipAndFailure(Result);
    return Result;
}
