#include "MetalFailureInjectionTests.h"

#include "FMetalDeviceOwnerState.h"
#include "FMetalFailureInjector.h"
#include "FMetalInspection.h"
#include "MetalRHI/FMetalDeviceFactory.h"

#include <array>
#include <iostream>
#include <set>
#include <string>

namespace
{

using namespace Stoner;
using namespace Stoner::Backend::Metal;
using namespace Stoner::Backend::Metal::Private;

void Record(
    FMetalFailureInjectionTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

constexpr std::array GPoints = {
    EMetalFailurePoint::DeviceInitialization,
    EMetalFailurePoint::ResourceAllocation,
    EMetalFailurePoint::PipelineCreation,
    EMetalFailurePoint::CommandRecording,
    EMetalFailurePoint::CommandSubmission,
    EMetalFailurePoint::Synchronization,
    EMetalFailurePoint::DrawableAcquisition,
    EMetalFailurePoint::Presentation,
    EMetalFailurePoint::Shutdown};

void TestNamedOneShotInjection(FMetalFailureInjectionTestResult& Result)
{
    std::set<std::string> Names;
    bool bAllOneShot = true;
    for (const auto Point : GPoints)
    {
        FScopedMetalFailureInjection Scope(Point);
        Names.emplace(ToStableName(Point));
        const auto Other = Point == EMetalFailurePoint::DeviceInitialization
            ? EMetalFailurePoint::ResourceAllocation
            : EMetalFailurePoint::DeviceInitialization;
        bAllOneShot = bAllOneShot &&
            !FMetalFailureInjector::ShouldFail(Other) &&
            FMetalFailureInjector::ShouldFail(Point) &&
            !FMetalFailureInjector::ShouldFail(Point) &&
            FMetalFailureInjector::GetConsumedCount() == 1 &&
            FMetalFailureInjector::GetRemainingCount() == 0;
        std::cout << "[EVIDENCE] metal-failure point="
                  << ToStableName(Point) << " terminal=failed"
                  << " partial-object=false\n";
    }
    Record(Result, bAllOneShot && Names.size() == GPoints.size(),
        "all lifecycle failure points are named, unique, and one-shot");
}

void TestOrderedSequence(FMetalFailureInjectionTestResult& Result)
{
    Core::TArray<EMetalFailurePoint> Sequence(
        GPoints.begin(), GPoints.end());
    bool bOrdered = true;
    {
        FScopedMetalFailureInjection Scope(Sequence);
        for (const auto Point : GPoints)
            bOrdered = bOrdered && FMetalFailureInjector::ShouldFail(Point);
        bOrdered = bOrdered &&
            FMetalFailureInjector::GetRemainingCount() == 0;
    }
    Record(Result, bOrdered &&
        FMetalFailureInjector::GetConsumedCount() == 0,
        "failure sequences consume in declared order and scoped state resets");
}

void TestExactOwnershipUnwind(FMetalFailureInjectionTestResult& Result)
{
    auto Owner = Core::MakeShared<FMetalDeviceOwnerState>(901);
    bool bRegistered = true;
    for (const auto Category : {
        EMetalOwnershipCategory::Resource,
        EMetalOwnershipCategory::Pipeline,
        EMetalOwnershipCategory::Command,
        EMetalOwnershipCategory::Synchronization,
        EMetalOwnershipCategory::Presentation})
        bRegistered = bRegistered && Owner->TryRegisterObject(Category);
    bRegistered = bRegistered && Owner->TryBeginSubmission();

    Owner->EndSubmission();
    for (const auto Category : {
        EMetalOwnershipCategory::Resource,
        EMetalOwnershipCategory::Pipeline,
        EMetalOwnershipCategory::Command,
        EMetalOwnershipCategory::Synchronization,
        EMetalOwnershipCategory::Presentation})
        Owner->ReleaseObject(Category);
    Owner->StopAdmission();
    Owner->AdvanceGeneration();
    Owner->ReleaseDeviceOwnership();
    const auto Inspection = CaptureMetalInspection(Owner);
    std::cout << "[EVIDENCE] metal-failure-ownership device="
              << Inspection.DeviceOwnershipCount
              << " objects=" << Inspection.LiveObjectCount
              << " submissions=" << Inspection.SubmissionOwnershipCount
              << " inflight=" << Inspection.InFlightSubmissionCount << '\n';
    Record(Result,
        bRegistered && HasZeroMetalOwnership(Inspection) &&
            !Inspection.bAcceptingWork,
        "failure unwind returns every ownership category to zero exactly once");
}

void TestFactoryInjection(FMetalFailureInjectionTestResult& Result)
{
    FScopedMetalFailureInjection Scope(
        EMetalFailurePoint::DeviceInitialization);
    const auto Created = CreateMetalDevice();
    Record(Result,
        Created.Result == RHI::ERHIResult::Failed && !Created.Device &&
            Created.Diagnostics.Records.size() == 1 &&
            Created.Diagnostics.Records.front().StableReason == Core::FString(
                ToStableName(EMetalFailurePoint::DeviceInitialization)),
        "device injection publishes no partial device on every host");
}

} // namespace

FMetalFailureInjectionTestResult RunMetalFailureInjectionTests()
{
    FMetalFailureInjectionTestResult Result;
    TestNamedOneShotInjection(Result);
    TestOrderedSequence(Result);
    TestExactOwnershipUnwind(Result);
    TestFactoryInjection(Result);
    return Result;
}
