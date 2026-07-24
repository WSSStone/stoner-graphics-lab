#include "DeferredNativeIntegrationTests.h"

#include "VulkanRHI/FVulkanNativeContext.h"

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
    Record(Result, Report.GetProbeCount("StandardZ") >= 12 &&
        Report.GetProbeCount("ReversedZ") >= 12,
        "Deferred native validation reports at least twelve probes per depth convention");
    std::set<std::string> ProbeIdentities;
    bool bEveryProbeValid = true;
    for (const FVulkanDeferredProbe& Probe : Report.Probes)
    {
        const std::string Identity =
            Probe.Convention.ToStdString() + "/" + Probe.Name.ToStdString();
        bEveryProbeValid = bEveryProbeValid &&
            ProbeIdentities.insert(Identity).second &&
            std::isfinite(Probe.ErrorMeasure) && Probe.bPassed;
    }
    Record(Result, bEveryProbeValid,
        "Mapped attachment probes are finite, unique, and within semantic tolerances");
    Record(Result, Report.bPassed && Report.FinalLiveObjects == 0,
        "Deferred native validation passes semantic probes and releases frame-owned objects");
    Record(Result, Context.Shutdown() == ERHIResult::Success &&
        Context.GetSnapshot().GetTotalLiveObjectCount() == 0,
        "Deferred native context shutdown reaches zero live objects");
    return Result;
}
