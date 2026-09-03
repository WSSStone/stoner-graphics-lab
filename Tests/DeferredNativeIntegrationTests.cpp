#include "DeferredNativeIntegrationTests.h"
#include "MetalBackendComparison.h"
#include "MetalDeferredNativeProbe.h"

#include "Asset/FAssetDigest.h"
#include "Renderer/FDeferredFrameExecutor.h"
#include "VulkanRHI/FVulkanNativeContext.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <iostream>
#include <cstring>
#include <filesystem>
#include <set>
#include <span>
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
    const Stoner::Core::FString Identity(std::string("Content/") + File);
    (void)Stoner::RHI::SetRHIShaderSpirvWords(
        Desc.Payload, ReadWords(Directory + "/" + File), Identity);
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

const Stoner::Backend::Vulkan::FVulkanDeferredProbe* FindProbe(
    const Stoner::Backend::Vulkan::FVulkanDeferredValidationReport& Report,
    const char* Name)
{
    for (const auto& Probe : Report.Probes)
        if (Probe.Convention == Stoner::Core::FString("StandardZ") &&
            Probe.Name == Stoner::Core::FString(Name))
            return &Probe;
    return nullptr;
}

FMetalBackendReadback ScalarReadback(
    const char* Backend,
    const Stoner::Core::FString& Evidence,
    const char* Workload,
    EMetalReadbackSemantic Semantic,
    float Value)
{
    FMetalBackendReadback Result;
    Result.Backend = Backend;
    Result.EvidenceReference = Evidence;
    Result.WorkloadIdentity = Workload;
    Result.ShaderVersion = "repository-deferred-v1";
    Result.Width = 1;
    Result.Height = 1;
    Result.RowPitchBytes = sizeof(float);
    Result.Format = EMetalReadbackFormat::R32Float;
    Result.Semantic = Semantic;
    Result.Bytes.resize(sizeof(float));
    std::memcpy(Result.Bytes.data(), &Value, sizeof(float));
    return Result;
}

FMetalBackendReadback ColorReadback(
    const char* Backend,
    const Stoner::Core::FString& Evidence,
    const char* Workload,
    const Stoner::Core::FVector4& Value)
{
    FMetalBackendReadback Result;
    Result.Backend = Backend;
    Result.EvidenceReference = Evidence;
    Result.WorkloadIdentity = Workload;
    Result.ShaderVersion = "repository-deferred-v1";
    Result.Width = 1;
    Result.Height = 1;
    Result.RowPitchBytes = 4;
    Result.Semantic = EMetalReadbackSemantic::FinalLdrColor;
    Result.Bytes = {
        static_cast<Stoner::Core::uint8>(std::lround(
            std::clamp(Value.X, 0.0f, 1.0f) * 255.0f)),
        static_cast<Stoner::Core::uint8>(std::lround(
            std::clamp(Value.Y, 0.0f, 1.0f) * 255.0f)),
        static_cast<Stoner::Core::uint8>(std::lround(
            std::clamp(Value.Z, 0.0f, 1.0f) * 255.0f)),
        static_cast<Stoner::Core::uint8>(std::lround(
            std::clamp(Value.W, 0.0f, 1.0f) * 255.0f))};
    return Result;
}

FMetalBackendReadback NormalReadback(
    const char* Backend,
    const Stoner::Core::FString& Evidence,
    const Stoner::Core::FVector4& Value)
{
    auto Result = ColorReadback(
        Backend, Evidence, "scene/deferred/gbuffer-normal-v1", Value);
    Result.Semantic = EMetalReadbackSemantic::WorldNormal;
    Result.bNormalEncodedUNorm = true;
    for (Stoner::Core::uint32 Channel = 0; Channel < 3; ++Channel)
    {
        const float Component = Channel == 0 ? Value.X :
            Channel == 1 ? Value.Y : Value.Z;
        Result.Bytes[Channel] = static_cast<Stoner::Core::uint8>(std::lround(
            std::clamp(Component * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f));
    }
    return Result;
}

bool CompareNativeDeferredEvidence(
    const FMetalDeferredNativeProbeReport& Metal,
    const Stoner::Backend::Vulkan::FVulkanDeferredValidationReport& Vulkan,
    Stoner::Core::FString& OutDigest)
{
    using Stoner::Asset::FAssetDigest;
    using Stoner::Core::FString;
    const FString VulkanDump = Vulkan.Dump();
    const auto VulkanBytes = std::span<const Stoner::Core::uint8>(
        reinterpret_cast<const Stoner::Core::uint8*>(VulkanDump.View().data()),
        VulkanDump.Len());
    const FString VulkanDigest =
        FAssetDigest::FromBytes(VulkanBytes).ToLowerHex();
    const FString MetalEvidence = FString(
        std::string("metal:") + Metal.FinalOutputDigest.ToStdString());
    const FString VulkanEvidence = FString(
        std::string("vulkan:") + VulkanDigest.ToStdString());

    const auto* Base = FindProbe(Vulkan, "opaque-base");
    const auto* AO = FindProbe(Vulkan, "ambient-occlusion");
    const auto* Normal = FindProbe(Vulkan, "world-normal");
    const auto* Roughness = FindProbe(Vulkan, "roughness");
    const auto* Metallic = FindProbe(Vulkan, "metallic");
    const auto* Depth = FindProbe(Vulkan, "surface-depth");
    if (!Base || !AO || !Normal || !Roughness || !Metallic || !Depth)
        return false;

    const std::array Reports = {
        CompareMetalBackendReadbacks(
            ColorReadback("metal", MetalEvidence,
                "scene/deferred/gbuffer-base-v1", Metal.BaseColorAO),
            ColorReadback("vulkan", VulkanEvidence,
                "scene/deferred/gbuffer-base-v1", Base->Observed)),
        CompareMetalBackendReadbacks(
            ScalarReadback("metal", MetalEvidence,
                "scene/deferred/gbuffer-ao-v1",
                EMetalReadbackSemantic::AmbientOcclusion,
                Metal.BaseColorAO.W),
            ScalarReadback("vulkan", VulkanEvidence,
                "scene/deferred/gbuffer-ao-v1",
                EMetalReadbackSemantic::AmbientOcclusion,
                AO->Observed.W)),
        CompareMetalBackendReadbacks(
            NormalReadback("metal", MetalEvidence, Metal.NormalRoughness),
            NormalReadback("vulkan", VulkanEvidence, Normal->Observed)),
        CompareMetalBackendReadbacks(
            ScalarReadback("metal", MetalEvidence,
                "scene/deferred/gbuffer-roughness-v1",
                EMetalReadbackSemantic::Roughness,
                Metal.NormalRoughness.W),
            ScalarReadback("vulkan", VulkanEvidence,
                "scene/deferred/gbuffer-roughness-v1",
                EMetalReadbackSemantic::Roughness,
                Roughness->Observed.W)),
        CompareMetalBackendReadbacks(
            ScalarReadback("metal", MetalEvidence,
                "scene/deferred/gbuffer-metallic-v1",
                EMetalReadbackSemantic::Metallic,
                Metal.EmissiveMetallic.W),
            ScalarReadback("vulkan", VulkanEvidence,
                "scene/deferred/gbuffer-metallic-v1",
                EMetalReadbackSemantic::Metallic,
                Metallic->Observed.W)),
        CompareMetalBackendReadbacks(
            ScalarReadback("metal", MetalEvidence,
                "scene/deferred/gbuffer-depth-v1",
                EMetalReadbackSemantic::NormalizedDepth,
                Metal.Depth.X),
            ScalarReadback("vulkan", VulkanEvidence,
                "scene/deferred/gbuffer-depth-v1",
                EMetalReadbackSemantic::NormalizedDepth,
                Depth->Observed.X))};
    std::string Canonical;
    for (const auto& Comparison : Reports)
    {
        if (!Comparison.bPassed) return false;
        Canonical += Comparison.Dump().ToStdString();
    }
    const auto Bytes = std::span<const Stoner::Core::uint8>(
        reinterpret_cast<const Stoner::Core::uint8*>(Canonical.data()),
        Canonical.size());
    OutDigest = FAssetDigest::FromBytes(Bytes).ToLowerHex();
    return true;
}

} // namespace

FDeferredNativeIntegrationTestResult RunDeferredNativeIntegrationTests()
{
    using namespace Stoner::Backend::Vulkan;
    using namespace Stoner::RHI;

    FDeferredNativeIntegrationTestResult Result;
    const FMetalDeferredNativeProbeReport MetalReport =
        RunMetalDeferredNativeProbe();
    const bool bRequireMetal = std::getenv("STONER_REQUIRE_METAL_DEFERRED") !=
            nullptr &&
        std::string_view(std::getenv("STONER_REQUIRE_METAL_DEFERRED")) == "1";
    if (MetalReport.Status == EMetalDeferredProbeStatus::Unavailable)
    {
        Record(Result, !bRequireMetal,
            bRequireMetal
                ? "required Metal deferred device and compiler are available"
                : "Metal deferred native validation is controlled unavailable");
    }
    else
    {
        if (MetalReport.Status != EMetalDeferredProbeStatus::Success)
        {
            std::cout << "[INFO] metal-deferred-native-failure reason="
                      << MetalReport.StableReason.CStr() << '\n';
            const auto PrintVector = [](const char* Name,
                const Stoner::Core::FVector4& Value)
            {
                std::cout << "[INFO] metal-deferred-native-observed "
                          << Name << '=' << Value.X << ',' << Value.Y << ','
                          << Value.Z << ',' << Value.W << '\n';
            };
            PrintVector("base-ao", MetalReport.BaseColorAO);
            PrintVector("normal-roughness", MetalReport.NormalRoughness);
            PrintVector("emissive-metallic", MetalReport.EmissiveMetallic);
            PrintVector("depth", MetalReport.Depth);
            PrintVector("lighting", MetalReport.Lighting);
            PrintVector("final-output", MetalReport.FinalOutput);
        }
        for (const auto& Digest : MetalReport.ShaderEvidenceDigests)
            std::cout << "[EVIDENCE] metal-native-shader evidence="
                      << Digest.CStr() << '\n';
        if (!MetalReport.FinalOutputDigest.IsEmpty())
            std::cout << "[EVIDENCE] metal-native-deferred status=passed readback="
                      << MetalReport.FinalOutputDigest.CStr() << '\n';
        Record(Result,
            MetalReport.Status == EMetalDeferredProbeStatus::Success &&
                MetalReport.bUsedSharedRenderer &&
                MetalReport.bNativeSubmissionCompleted,
            "Metal deferred executes the shared Renderer graph through native RHI submission");
        Record(Result,
            MetalReport.bGBufferPassed &&
                MetalReport.bWorldNormalPassed &&
                MetalReport.bDepthPassed,
            "Metal deferred readback preserves GBuffer world normal and depth semantics");
        Record(Result,
            MetalReport.bLightingPassed &&
                MetalReport.bFinalOutputPassed,
            "Metal deferred readback preserves lighting and final-output tolerances");
    }
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

    Stoner::Core::FString ComparisonDigest;
    const bool bComparisonEligible =
        MetalReport.Status == EMetalDeferredProbeStatus::Success;
    const bool bNativeComparison = bComparisonEligible &&
        CompareNativeDeferredEvidence(
            MetalReport, Report, ComparisonDigest);
    if (bNativeComparison)
        std::cout << "[EVIDENCE] metal-vulkan-deferred-comparison status=passed"
                  << " tolerance=" << FMetalBackendComparisonReport::ToleranceSet
                  << " digest=" << ComparisonDigest.CStr() << '\n';
    Record(Result, !bComparisonEligible || bNativeComparison,
        "Metal and Vulkan native GBuffer evidence satisfies frozen cross-backend tolerances");

    Stoner::Renderer::FDeferredViewData ProbeView;
    ProbeView.Extent = {32, 32};
    ProbeView.CameraPosition = {0.0f, 0.0f, 0.75f};
    ProbeView.DepthPolicy = Stoner::Renderer::MakeDeferredDepthPolicy(
        Stoner::Renderer::EDeferredDepthConvention::StandardZ, 0.1f, 100.0f);
    Stoner::Renderer::FDeferredDrawRecord ProbeDraw;
    // Translation occupies a different row/column in CPU and GLSL layouts.
    // The affine probe also exercises an XY rotation and non-uniform scale
    // while keeping the center sample covered by the native attachment oracle.
    ProbeDraw.Candidate.Model = Stoner::Core::FMatrix4x4(
        0.69282f, -0.45f, 0.0f, 0.05f,
        0.4f, 0.779423f, 0.0f, -0.04f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
    const bool bPackedNormalMatrix = Stoner::Renderer::TryBuildWorldNormalFromModel(
        ProbeDraw.Candidate.Model, ProbeDraw.WorldNormalFromModel);
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
    bool bPackedProbePassed = bPackedNormalMatrix &&
        PackedExecutionResult == ERHIResult::Success &&
        PackedReport.bNativeSubmissionCompleted && PackedReport.bPassed;
    for (const FVulkanDeferredProbe& Probe : PackedReport.Probes)
    {
        bPackedProbePassed = bPackedProbePassed && Probe.bPassed;
        if (!Probe.bPassed)
        {
            std::cout << "[INFO] packed-deferred-probe-failure convention="
                      << Probe.Convention.CStr() << " name="
                      << Probe.Name.CStr() << " expected="
                      << Probe.Expected.X << ',' << Probe.Expected.Y << ','
                      << Probe.Expected.Z << ',' << Probe.Expected.W
                      << " observed=" << Probe.Observed.X << ','
                      << Probe.Observed.Y << ',' << Probe.Observed.Z << ','
                      << Probe.Observed.W << " error=" << Probe.ErrorMeasure
                      << '\n';
        }
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
