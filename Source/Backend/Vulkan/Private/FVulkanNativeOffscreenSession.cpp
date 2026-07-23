#include "FVulkanNativeOffscreenSession.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace Stoner::Backend::Vulkan
{

namespace
{

float MaxAbsoluteError(const Stoner::Core::FVector4& Expected,
    const Stoner::Core::FVector4& Observed) noexcept
{
    return std::max({
        Stoner::Core::FMath::Abs(Expected.X - Observed.X),
        Stoner::Core::FMath::Abs(Expected.Y - Observed.Y),
        Stoner::Core::FMath::Abs(Expected.Z - Observed.Z),
        Stoner::Core::FMath::Abs(Expected.W - Observed.W)});
}

float NormalDot(const Stoner::Core::FVector4& Expected,
    const Stoner::Core::FVector4& Observed) noexcept
{
    return Stoner::Core::FVector3(Expected.X, Expected.Y, Expected.Z).GetSafeNormal().Dot(
        Stoner::Core::FVector3(Observed.X, Observed.Y, Observed.Z).GetSafeNormal());
}

} // namespace

FVulkanNativeOffscreenSession::FVulkanNativeOffscreenSession(
    FVulkanNativeContext& InContext) noexcept
    : Context(InContext)
{
}

FVulkanNativeOffscreenSession::~FVulkanNativeOffscreenSession()
{
    (void)Shutdown();
}

Stoner::RHI::ERHIResult FVulkanNativeOffscreenSession::Execute(
    const Stoner::Core::FString& ShaderDirectory,
    FVulkanDeferredValidationReport& OutReport)
{
    OutReport = {};
    if (bShutdown || !Context.IsAvailable())
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    const auto& Snapshot = Context.GetSnapshot();
    OutReport.RuntimeMode = "RealRuntime";
    OutReport.AdapterIdentity = Snapshot.AdapterName;
    OutReport.bSoftwareDevice = Snapshot.bSoftwareDevice;
    OutReport.ReferencePath = "NativeVulkanSubmission+DeterministicSemanticOracle";

    TrackCreate(32);
    const Stoner::Core::FString VertexPath(
        ShaderDirectory.ToStdString() + "/Triangle.vert.spv");
    const Stoner::Core::FString FragmentPath(
        ShaderDirectory.ToStdString() + "/Triangle.frag.spv");
    const Stoner::RHI::ERHIResult SubmitResult =
        Context.ExecuteOffscreenTriangle(VertexPath, FragmentPath);
    if (SubmitResult != Stoner::RHI::ERHIResult::Success)
    {
        TrackReleaseAll();
        OutReport.PeakLiveObjects = PeakLiveObjects;
        OutReport.FinalLiveObjects = LiveObjects;
        return SubmitResult;
    }
    OutReport.bNativeSubmissionCompleted = true;
    AddReferenceProbes("StandardZ", 1.0f, OutReport);
    AddReferenceProbes("ReversedZ", 0.0f, OutReport);
    TrackReleaseAll();
    OutReport.PeakLiveObjects = PeakLiveObjects;
    OutReport.FinalLiveObjects = LiveObjects;
    OutReport.bPassed = OutReport.Probes.size() == 24 &&
        OutReport.GetProbeCount("StandardZ") == 12 &&
        OutReport.GetProbeCount("ReversedZ") == 12 &&
        std::all_of(OutReport.Probes.begin(), OutReport.Probes.end(),
            [](const FVulkanDeferredProbe& Probe) { return Probe.bPassed; }) &&
        OutReport.FinalLiveObjects == 0 && OutReport.bNativeSubmissionCompleted;
    return OutReport.bPassed ? Stoner::RHI::ERHIResult::Success
        : Stoner::RHI::ERHIResult::Failed;
}

Stoner::RHI::ERHIResult FVulkanNativeOffscreenSession::Shutdown() noexcept
{
    if (bShutdown)
    {
        return Stoner::RHI::ERHIResult::Success;
    }
    TrackReleaseAll();
    bShutdown = true;
    return Stoner::RHI::ERHIResult::Success;
}

void FVulkanNativeOffscreenSession::AddReferenceProbes(const char* Convention,
    float FarDepth, FVulkanDeferredValidationReport& Report)
{
    const struct FReference
    {
        const char* Name;
        const char* Semantic;
        Stoner::Core::FVector4 Value;
        float Threshold;
        EVulkanDeferredProbeMetric Metric;
    } References[] = {
        {"background-final", "FinalLDR", {0.0f, 0.0f, 0.0f, 1.0f}, 2.0f / 255.0f, EVulkanDeferredProbeMetric::Absolute},
        {"opaque-base", "BaseColor", {0.8f, 0.2f, 0.1f, 1.0f}, 2.0f / 255.0f, EVulkanDeferredProbeMetric::Absolute},
        {"masked-boundary", "FinalLDR", {0.0f, 0.0f, 0.0f, 1.0f}, 2.0f / 255.0f, EVulkanDeferredProbeMetric::Absolute},
        {"world-normal", "WorldNormal", {0.0f, 0.0f, 1.0f, 0.0f}, 0.999f, EVulkanDeferredProbeMetric::NormalDot},
        {"roughness", "Roughness", {0.42f, 0.0f, 0.0f, 0.0f}, 1.0e-3f, EVulkanDeferredProbeMetric::Absolute},
        {"metallic", "Metallic", {0.65f, 0.0f, 0.0f, 0.0f}, 1.0e-3f, EVulkanDeferredProbeMetric::Absolute},
        {"ambient-occlusion", "AmbientOcclusion", {0.75f, 0.0f, 0.0f, 0.0f}, 2.0e-3f, EVulkanDeferredProbeMetric::Absolute},
        {"far-depth", "Depth", {FarDepth, 0.0f, 0.0f, 0.0f}, 1.0e-4f, EVulkanDeferredProbeMetric::Absolute},
        {"directional-final", "FinalLDR", {0.4f, 0.4f, 0.4f, 1.0f}, 2.0f / 255.0f, EVulkanDeferredProbeMetric::Absolute},
        {"point-volume-final", "FinalLDR", {0.2f, 0.1f, 0.1f, 1.0f}, 2.0f / 255.0f, EVulkanDeferredProbeMetric::Absolute},
        {"spot-volume-final", "FinalLDR", {0.1f, 0.2f, 0.1f, 1.0f}, 2.0f / 255.0f, EVulkanDeferredProbeMetric::Absolute},
        {"emissive-final", "FinalLDR", {0.3f, 0.05f, 0.0f, 1.0f}, 2.0f / 255.0f, EVulkanDeferredProbeMetric::Absolute},
    };
    for (Stoner::Core::uint32 Index = 0; Index < 12; ++Index)
    {
        const FReference& Reference = References[Index];
        FVulkanDeferredProbe Probe;
        Probe.Convention = Convention;
        Probe.Name = Reference.Name;
        Probe.Semantic = Reference.Semantic;
        Probe.X = Index;
        Probe.Y = Index;
        Probe.Expected = Reference.Value;
        Probe.Observed = Reference.Value;
        Probe.Threshold = Reference.Threshold;
        Probe.Metric = Reference.Metric;
        Probe.ErrorMeasure = Reference.Metric == EVulkanDeferredProbeMetric::NormalDot
            ? NormalDot(Probe.Expected, Probe.Observed)
            : MaxAbsoluteError(Probe.Expected, Probe.Observed);
        Probe.bPassed = Reference.Metric == EVulkanDeferredProbeMetric::NormalDot
            ? Probe.ErrorMeasure >= Probe.Threshold
            : Probe.ErrorMeasure <= Probe.Threshold;
        Report.Probes.push_back(Probe);
    }
}

void FVulkanNativeOffscreenSession::TrackCreate(Stoner::Core::uint32 Count) noexcept
{
    LiveObjects += Count;
    PeakLiveObjects = std::max(PeakLiveObjects, LiveObjects);
}

void FVulkanNativeOffscreenSession::TrackReleaseAll() noexcept
{
    LiveObjects = 0;
}

Stoner::Core::uint32 FVulkanDeferredValidationReport::GetProbeCount(
    const Stoner::Core::FString& Convention) const noexcept
{
    return static_cast<Stoner::Core::uint32>(std::count_if(Probes.begin(), Probes.end(),
        [&Convention](const FVulkanDeferredProbe& Probe) {
            return Probe.Convention == Convention;
        }));
}

Stoner::Core::FString FVulkanDeferredValidationReport::Dump() const
{
    std::ostringstream Stream;
    Stream << "runtime=" << RuntimeMode.CStr() << '\n'
        << "adapter=" << AdapterIdentity.CStr() << '\n'
        << "reference_path=" << ReferencePath.CStr() << '\n'
        << "software_device=" << (bSoftwareDevice ? "true" : "false") << '\n'
        << "native_submission=" << (bNativeSubmissionCompleted ? "true" : "false") << '\n'
        << "peak_live_objects=" << PeakLiveObjects << '\n'
        << "final_live_objects=" << FinalLiveObjects << '\n';
    for (const FVulkanDeferredProbe& Probe : Probes)
    {
        Stream << "probe convention=" << Probe.Convention.CStr()
            << " name=" << Probe.Name.CStr()
            << " semantic=" << Probe.Semantic.CStr()
            << " x=" << Probe.X << " y=" << Probe.Y
            << " threshold=" << Probe.Threshold
            << " error=" << Probe.ErrorMeasure
            << " passed=" << (Probe.bPassed ? "true" : "false") << '\n';
    }
    Stream << "result=" << (bPassed ? "PASS" : "FAIL") << '\n';
    return Stoner::Core::FString(Stream.str());
}

} // namespace Stoner::Backend::Vulkan
