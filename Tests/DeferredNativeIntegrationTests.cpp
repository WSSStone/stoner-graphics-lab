#include "DeferredNativeIntegrationTests.h"

#include "VulkanRHI/FVulkanNativeContext.h"

#include <array>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>

namespace
{

void Record(FDeferredNativeIntegrationTestResult& Result, bool bPassed, const char* Name)
{
    if (bPassed)
    {
        ++Result.Passed;
        std::cout << "[PASS] " << Name << '\n';
    }
    else
    {
        ++Result.Failed;
        std::cout << "[FAIL] " << Name << '\n';
    }
}

bool IsRequired()
{
    const char* Value = std::getenv("STONER_REQUIRE_DEFERRED_NATIVE");
    return Value != nullptr && std::string_view(Value) == "1";
}

bool RunNativeFailureInjection()
{
    const char* Value = std::getenv("STONER_RUN_DEFERRED_NATIVE_FAILURES");
    return Value != nullptr && std::string_view(Value) == "1";
}

bool SkipOptionalNative()
{
    const char* Value = std::getenv("STONER_SKIP_OPTIONAL_DEFERRED_NATIVE");
    return Value != nullptr && std::string_view(Value) == "1";
}

void WriteReport(const Stoner::Backend::Vulkan::FVulkanDeferredValidationReport& Report)
{
    const char* Path = std::getenv("STONER_DEFERRED_READBACK_REPORT");
    if (Path == nullptr || *Path == '\0')
    {
        return;
    }
    std::ofstream Stream(Path, std::ios::binary | std::ios::trunc);
    Stream << Report.Dump().CStr();
}

} // namespace

FDeferredNativeIntegrationTestResult RunDeferredNativeIntegrationTests()
{
    using namespace Stoner::Backend::Vulkan;
    using namespace Stoner::RHI;

    FDeferredNativeIntegrationTestResult Result;
    constexpr std::array FailurePoints = {
        EVulkanDeferredFailurePoint::PartialInitialization,
        EVulkanDeferredFailurePoint::Record,
        EVulkanDeferredFailurePoint::Submit,
        EVulkanDeferredFailurePoint::Fence,
        EVulkanDeferredFailurePoint::Copy,
        EVulkanDeferredFailurePoint::Map,
        EVulkanDeferredFailurePoint::Decode,
        EVulkanDeferredFailurePoint::Probe};
    bool bLifecycleFailuresStable = true;
    for (EVulkanDeferredFailurePoint FailurePoint : FailurePoints)
    {
        const FVulkanDeferredValidationReport First =
            FVulkanNativeContext::RunDeferredFailureLifecycleValidation(FailurePoint);
        const FVulkanDeferredValidationReport Repeated =
            FVulkanNativeContext::RunDeferredFailureLifecycleValidation(FailurePoint);
        bLifecycleFailuresStable = bLifecycleFailuresStable &&
            First.InjectedFailure == FailurePoint &&
            First.PrimaryFailureStage == ToString(FailurePoint) &&
            First.PeakLiveObjects > 0 && First.FinalLiveObjects == 0 &&
            !First.bPassed && First.Dump() == Repeated.Dump();
    }
    Record(Result, bLifecycleFailuresStable,
        "Deferred native failure lifecycle is runtime-independent stable and zero-live");

    if (SkipOptionalNative() && !IsRequired())
    {
        Record(Result, true,
            "Deferred native validation skips optional driver execution when explicitly requested");
        return Result;
    }

    FVulkanNativeContext Context;
    const ERHIResult InitializeResult = Context.Initialize(ERHIRuntimeMode::NativeHeadless);
    if (InitializeResult == ERHIResult::Unsupported || InitializeResult == ERHIResult::Unavailable)
    {
        Record(Result, !IsRequired(),
            "Deferred native validation is optional locally and mandatory when explicitly required");
        return Result;
    }

    FVulkanDeferredValidationReport Report;
    const char* ShaderDirectory = std::getenv("STONER_DEFERRED_NATIVE_SHADER_DIR");
    const Stoner::Core::FString Directory =
        ShaderDirectory != nullptr
            ? ShaderDirectory
            : "Source/Renderer/Shaders/Deferred";
    const ERHIResult ExecutionResult =
        Context.ExecuteDeferredOffscreenValidation(Directory, Report);
    WriteReport(Report);

    Record(Result, ExecutionResult == ERHIResult::Success &&
        Report.bNativeSubmissionCompleted,
        "Deferred native validation completes a real Vulkan submission");
    Record(Result,
        Report.ReferencePath == Stoner::Core::FString("NativeDeferredReadback"),
        "Deferred native validation uses mapped attachment readback");
    Record(Result, Report.GetProbeCount("StandardZ") >= 18 &&
        Report.GetProbeCount("ReversedZ") >= 18,
        "Deferred native validation reports extended probes per depth convention");
    std::set<std::string> ProbeIdentities;
    std::set<std::string> LocalLightCases;
    bool bEveryProbeValid = true;
    for (const FVulkanDeferredProbe& Probe : Report.Probes)
    {
        const std::string Identity =
            Probe.Convention.ToStdString() + "/" + Probe.Name.ToStdString();
        bEveryProbeValid = bEveryProbeValid &&
            ProbeIdentities.insert(Identity).second &&
            std::isfinite(Probe.ErrorMeasure) && Probe.bPassed;
        if (Probe.Semantic == Stoner::Core::FString("LocalLightCase"))
        {
            LocalLightCases.insert(Identity);
        }
    }
    Record(Result, bEveryProbeValid,
        "Mapped attachment probes are finite, unique, and within semantic tolerances");
    bool bLocalLightCoverage = true;
    for (const char* Convention : {"StandardZ", "ReversedZ"})
    {
        for (const char* Name : {"point-visible", "point-outside-view",
                 "point-camera-inside", "spot-visible", "spot-outside-cone",
                 "spot-near-plane"})
        {
            bLocalLightCoverage = bLocalLightCoverage &&
                LocalLightCases.count(std::string(Convention) + "/" + Name) == 1;
        }
    }
    Record(Result, bLocalLightCoverage,
        "Deferred native validation covers point and spot local-light edge cases");
    Record(Result, Report.bPassed && Report.FinalLiveObjects == 0,
        "Deferred native validation passes semantic probes and releases frame-owned objects");
    if (RunNativeFailureInjection())
    {
        bool bInjectedFailuresClean = true;
        for (EVulkanDeferredFailurePoint FailurePoint : FailurePoints)
        {
            FVulkanDeferredValidationReport FailureReport;
            const ERHIResult FailureResult =
                Context.ExecuteDeferredOffscreenValidation(Directory, FailureReport, FailurePoint);
            bInjectedFailuresClean = bInjectedFailuresClean &&
                FailureResult == ERHIResult::Failed &&
                FailureReport.InjectedFailure == FailurePoint &&
                FailureReport.PrimaryFailureStage == ToString(FailurePoint) &&
                FailureReport.FinalLiveObjects == 0 &&
                !FailureReport.bNativeSubmissionCompleted && !FailureReport.bPassed;
        }
        Record(Result, bInjectedFailuresClean,
            "Injected native failures stop at their primary stage and release partial state");
    }
    const bool bFirstShutdown = Context.Shutdown() == ERHIResult::Success;
    Record(Result, bFirstShutdown && Context.Shutdown() == ERHIResult::Success &&
        Context.GetSnapshot().GetTotalLiveObjectCount() == 0,
        "Deferred native context shutdown is idempotent and reaches zero live objects");
    return Result;
}
