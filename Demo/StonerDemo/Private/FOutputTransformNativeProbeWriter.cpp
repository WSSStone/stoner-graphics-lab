#include "FStonerDemoApplication.h"

#include "Asset/AssetMinimal.h"
#include "Core/SGPlatform.h"
#include "FOutputTransformValidationCommand.h"

#include <filesystem>
#include <fstream>
#include <span>

namespace Stoner::Demo
{
bool FStonerDemoApplication::WriteConfiguredOutputTransformNativeProbe()
{
    const auto& Inspection = ProductionExecutionInspection;
    if (!Inspection.ProvesNativeExecution() ||
        !Inspection.ResolvedPresentationState.IsValid() ||
        Inspection.FirstPresentedFrameToken == 0 ||
        Inspection.LastPresentedFrameToken == 0 ||
        Inspection.SettledPresentedFrameToken == 0 ||
        Inspection.PresentationCapabilityDigest.IsEmpty() ||
        Inspection.FormalOutputReadbackDigest.IsEmpty())
        return false;

    FOutputTransformValidationProbeInput Probe;
#if SG_PLATFORM_MAC
    Probe.HostPlatform = "macos";
    if (Configuration.GraphicsBackend == EDemoGraphicsBackend::Metal)
    {
#if defined(__aarch64__) || defined(__arm64__)
        Probe.DeviceClass = Configuration.OutputDeviceProfileId.View().starts_with(
            "Hdr.") ? "macos.apple8.metal.hdr" :
                "macos.apple8.metal.rgba8";
#else
        Probe.DeviceClass = Configuration.OutputDeviceProfileId.View().starts_with(
            "Hdr.") ? "macos.intel.metal.hdr" :
                "macos.intel.metal.rgba8";
#endif
    }
    else
        Probe.DeviceClass = "macos.apple8.moltenvk.rgba8";
#elif SG_PLATFORM_WINDOWS
    Probe.HostPlatform = "windows";
    Probe.DeviceClass = "windows.discrete-vulkan.rgba8";
#else
    Probe.HostPlatform = "linux";
    Probe.DeviceClass = "linux.software-vulkan.rgba8";
#endif
    Probe.Backend = ToString(Configuration.GraphicsBackend);
    Probe.ProfileKind = Configuration.OutputNativeProbeProfile;
    Probe.WorkloadRevision = Configuration.WorkloadRevision;
    const auto CapabilityText = Inspection.PresentationCapabilityDigest.View();
    Probe.CapabilityDigest = Asset::FAssetDigest::FromBytes(
        std::span<const Core::uint8>(
            reinterpret_cast<const Core::uint8*>(CapabilityText.data()),
            CapabilityText.size())).ToLowerHex();
    Probe.OutputDeviceProfileId = Configuration.OutputDeviceProfileId;
    Probe.TransformVersion = Configuration.OutputTransformVersion;
    Probe.InsertionDigest =
        "8638959e24c38d61150dcd702ebf38af16867f72ed5b53c1dc7359e3e63a85f2";
    Probe.ReadbackDigest = Inspection.FormalOutputReadbackDigest;
    Probe.Width = Inspection.ResolvedPresentationState.Width;
    Probe.Height = Inspection.ResolvedPresentationState.Height;
    Probe.FirstFrameToken = Inspection.FirstPresentedFrameToken;
    Probe.LastFrameToken = Inspection.LastPresentedFrameToken;
    Probe.SettledFrameToken = Inspection.SettledPresentedFrameToken;
    Probe.ExposureStops = Configuration.OutputExposureStops;

    auto& Execution = Probe.Execution;
    Execution.Result = Renderer::EOutputTransformResult::Success;
    Execution.FinalState = Renderer::EOutputTransformPlanState::Published;
    Execution.bFormalOutputPublished = true;
    Execution.PublishedFormalOutputId = 1;
    Execution.FrameToken = Inspection.SettledPresentedFrameToken;
    Execution.ResolvedPresentationState =
        Inspection.ResolvedPresentationState;
    auto& Frame = Execution.PresentationFrame;
    Frame.FrameToken = Execution.FrameToken;
    Frame.ModeGeneration = Execution.ResolvedPresentationState.ModeGeneration;
    Frame.SwapchainImageGeneration =
        Execution.ResolvedPresentationState.SwapchainImageGeneration;
    Frame.Width = Execution.ResolvedPresentationState.Width;
    Frame.Height = Execution.ResolvedPresentationState.Height;
    Frame.Format = Execution.ResolvedPresentationState.Format;
    Frame.ColorSpace = Execution.ResolvedPresentationState.ColorSpace;
    Frame.DisplayAdaptation =
        Execution.ResolvedPresentationState.DisplayAdaptation;
    Frame.MetadataDigest = Execution.ResolvedPresentationState.MetadataDigest;
    Execution.bNativeFrameAcquired = true;
    Execution.bNativeSubmitted = true;
    Execution.bNativeCompletionObserved = true;
    Execution.bNativeReadbackCompleted = true;
    Execution.bNativePresented = true;
    Execution.OutstandingTerminalOwnerCount = 0;

    Core::FString Json;
    Core::FString Reason;
    if (!FOutputTransformValidationCommand::SerializeNormalizedNativeProbe(
            Probe, Json, &Reason) || Json.IsEmpty() ||
        Json.Len() > 1024u * 1024u)
        return false;
    const std::filesystem::path Destination(
        Configuration.OutputNativeProbePath.CStr());
    std::error_code Error;
    if (Destination.has_parent_path())
        std::filesystem::create_directories(Destination.parent_path(), Error);
    if (Error) return false;
    std::ofstream Stream(Destination,
        std::ios::binary | std::ios::trunc);
    if (!Stream) return false;
    Stream.write(Json.CStr(), static_cast<std::streamsize>(Json.Len()));
    Stream.close();
    return Stream.good();
}
} // namespace Stoner::Demo
