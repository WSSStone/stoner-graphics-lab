#include "DeferredNativeIntegrationTests.h"

#include "Renderer/FDeferredFrameExecutor.h"
#include "VulkanRHI/FVulkanNativeContext.h"

#include <array>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <iostream>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>

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
    const std::filesystem::path ReportPath(Path);
    std::error_code Error;
    std::filesystem::create_directories(ReportPath.parent_path(), Error);
    if (Error)
    {
        return;
    }
    std::ofstream Stream(ReportPath, std::ios::binary | std::ios::trunc);
    Stream << Report.Dump().CStr();
}

Stoner::Core::TArray<Stoner::Core::uint32> ReadWords(
    const std::string& Path)
{
    std::ifstream Stream(Path, std::ios::binary | std::ios::ate);
    if (!Stream) return {};
    const std::streamsize Size = Stream.tellg();
    if (Size < 20 || Size % 4 != 0) return {};
    Stoner::Core::TArray<Stoner::Core::uint32> Words(
        static_cast<std::size_t>(Size) / 4);
    Stream.seekg(0);
    Stream.read(reinterpret_cast<char*>(Words.data()), Size);
    return Stream.good()
        ? Words
        : Stoner::Core::TArray<Stoner::Core::uint32>{};
}

Stoner::RHI::FRHIShaderModuleDesc Shader(
    const std::string& Directory,
    const char* File,
    Stoner::RHI::ERHIShaderStage Stage)
{
    Stoner::RHI::FRHIShaderModuleDesc Desc;
    Desc.Stage = Stage;
    Desc.EntryPoint = "main";
    Desc.PayloadIdentity =
        Stoner::Core::FString(std::string("Content/") + File);
    Desc.Bytecode.Words = ReadWords(Directory + "/" + File);
    return Desc;
}

std::array<Stoner::RHI::FRHIShaderModuleDesc, 9> DeferredShaders(
    const std::string& Directory)
{
    using Stoner::RHI::ERHIShaderStage;
    return {
        Shader(Directory, "Surface.vert.spv", ERHIShaderStage::Vertex),
        Shader(Directory, "Surface.frag.spv", ERHIShaderStage::Fragment),
        Shader(Directory, "Fullscreen.vert.spv", ERHIShaderStage::Vertex),
        Shader(Directory, "DirectionalLight.frag.spv", ERHIShaderStage::Fragment),
        Shader(Directory, "PointLight.vert.spv", ERHIShaderStage::Vertex),
        Shader(Directory, "PointLight.frag.spv", ERHIShaderStage::Fragment),
        Shader(Directory, "SpotLight.vert.spv", ERHIShaderStage::Vertex),
        Shader(Directory, "SpotLight.frag.spv", ERHIShaderStage::Fragment),
        Shader(Directory, "Composition.frag.spv", ERHIShaderStage::Fragment)};
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
    const std::string Directory =
        ShaderDirectory != nullptr
            ? ShaderDirectory
            : "Content/Shaders/Deferred";
    const auto Shaders = DeferredShaders(Directory);
    const ERHIResult ExecutionResult =
        Context.ExecuteDeferredOffscreenValidation(Shaders, Report);
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

    Stoner::Renderer::FDeferredViewData ProbeView;
    ProbeView.Extent = {32, 32};
    ProbeView.CameraPosition = {0.0f, 0.0f, 0.75f};
    ProbeView.DepthPolicy = Stoner::Renderer::MakeDeferredDepthPolicy(
        Stoner::Renderer::EDeferredDepthConvention::StandardZ, 0.1f, 100.0f);
    Stoner::Renderer::FDeferredDrawRecord ProbeDraw;
    ProbeDraw.Candidate.Model = Stoner::Core::FMatrix4x4::Identity();
    // Translation occupies a different row/column in CPU and GLSL layouts.
    // Keeping the center sample covered lets the same attachment reference
    // prove the Renderer-to-Vulkan packed path without relaxing its checks.
    ProbeDraw.Candidate.Model.M[0][3] = 0.05f;
    ProbeDraw.WorldNormalFromModel = Stoner::Core::FMatrix4x4::Identity();
    ProbeDraw.Candidate.Surface.BaseColor = {0.8f, 0.2f, 0.1f};
    ProbeDraw.Candidate.Surface.AmbientOcclusion = 0.75f;
    ProbeDraw.Candidate.Surface.Normal = {0.0f, 0.0f, 1.0f};
    ProbeDraw.Candidate.Surface.Roughness = 0.42f;
    ProbeDraw.Candidate.Surface.Emissive = {0.3f, 0.05f, 0.0f};
    ProbeDraw.Candidate.Surface.Metallic = 0.65f;
    ProbeDraw.Candidate.Surface.Alpha = 0.5f;
    const Stoner::Renderer::FDeferredFrameViewUniform PackedFrame =
        Stoner::Renderer::BuildDeferredFrameViewUniform(ProbeView);
    const Stoner::Renderer::FDeferredDrawMaterialUniform PackedDraw =
        Stoner::Renderer::BuildDeferredDrawMaterialUniform(ProbeDraw);
    FVulkanDeferredUniformPayload PackedPayload;
    std::memcpy(PackedPayload.FrameBytes.data(), &PackedFrame, sizeof(PackedFrame));
    std::memcpy(PackedPayload.DrawBytes.data(), &PackedDraw, sizeof(PackedDraw));
    FVulkanDeferredValidationReport PackedReport;
    const ERHIResult PackedExecutionResult = Context.ExecuteDeferredOffscreenValidation(
        Shaders, PackedReport, EVulkanDeferredFailurePoint::None, &PackedPayload);
    bool bPackedProbePassed = PackedExecutionResult == ERHIResult::Success &&
        PackedReport.bNativeSubmissionCompleted && PackedReport.bPassed;
    for (const FVulkanDeferredProbe& Probe : PackedReport.Probes)
    {
        bPackedProbePassed = bPackedProbePassed && Probe.bPassed;
    }
    Record(Result, bPackedProbePassed,
        "Renderer-packed non-symmetric matrix survives Vulkan native attachment readback");
    if (RunNativeFailureInjection())
    {
        bool bInjectedFailuresClean = true;
        for (EVulkanDeferredFailurePoint FailurePoint : FailurePoints)
        {
            FVulkanDeferredValidationReport FailureReport;
            const ERHIResult FailureResult =
                Context.ExecuteDeferredOffscreenValidation(
                    Shaders, FailureReport, FailurePoint);
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
