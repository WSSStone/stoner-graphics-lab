#include "FStonerDemoApplication.h"

#include "Asset/AssetMinimal.h"
#include "FProductionContentRuntime.h"
#include "FProductionSubmissionHarness.h"
#include "FProductionWindowCaptureWriter.h"
#include "Renderer/RendererMinimal.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <span>
#include <string>
#include <type_traits>

namespace Stoner::Demo
{

EDemoExitCode FStonerDemoApplication::RunProductionContent()
{
    using namespace Stoner;
    if (!ProductionRuntime || !ProductionRuntime->DeferredRenderSnapshot ||
        !ProductionRuntime->ForwardRenderSnapshot ||
        !BackendRuntime || !BackendRuntime->GetDevice())
        return EDemoExitCode::InitializationFailed;
    ProductionExecutionInspection = {};
    ProductionExecutionInspection.RequestedBackend =
        Configuration.GraphicsBackend;
    ProductionExecutionInspection.ExecutedBackend =
        BackendRuntime->GetBackend();
    ProductionExecutionInspection.RenderPath = Configuration.RenderPath;
    ProductionExecutionInspection.Runtime = BackendRuntime->GetSnapshot();
    if (ProductionExecutionInspection.RequestedBackend !=
            ProductionExecutionInspection.ExecutedBackend ||
        !ProductionExecutionInspection.Runtime.ProvesNativeExecution())
        return FailInitialize(EDemoStage::Runtime,
            EDemoExitCode::RuntimeUnavailable, "ProductionExecution",
            "requested backend did not prove native execution");
    const auto DigestBytes = [](std::span<const Core::uint8> Bytes)
    {
        return Asset::FAssetDigest::FromBytes(Bytes).ToLowerHex();
    };
    const auto DigestText = [&DigestBytes](const std::string& Text)
    {
        return DigestBytes(std::span<const Core::uint8>(
            reinterpret_cast<const Core::uint8*>(Text.data()), Text.size()));
    };
    const auto AppendPod = []<typename T>(
        Core::TArray<Core::uint8>& Bytes, const T& Value)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        const auto* Begin = reinterpret_cast<const Core::uint8*>(&Value);
        Bytes.insert(Bytes.end(), Begin, Begin + sizeof(T));
    };
    const auto AppendString = [&AppendPod](
        Core::TArray<Core::uint8>& Bytes, const Core::FString& Value)
    {
        const Core::uint64 Length = Value.Len();
        AppendPod(Bytes, Length);
        Bytes.insert(Bytes.end(), Value.View().begin(), Value.View().end());
    };
    const auto AppendPayload = [&AppendPod](
        Core::TArray<Core::uint8>& Bytes,
        std::span<const Core::uint8> Payload)
    {
        const Core::uint64 Length = Payload.size();
        AppendPod(Bytes, Length);
        Bytes.insert(Bytes.end(), Payload.begin(), Payload.end());
    };
    const auto& Realization =
        ProductionRuntime->DeferredRealizationInspection;
    Core::TArray<Core::uint8> SnapshotBytes;
    AppendString(SnapshotBytes, Configuration.WorkloadRevision);
    AppendPod(SnapshotBytes, Realization.SnapshotGeneration);
    for (const auto& ResourceId : Realization.OrderedResourceIds)
        AppendString(SnapshotBytes, ResourceId);
    for (const auto& Material :
         ProductionRuntime->DeferredRenderSnapshot->GetMaterials())
        AppendString(SnapshotBytes, Material.Material.Dump());
    ProductionExecutionInspection.SnapshotFingerprint =
        DigestBytes(SnapshotBytes);

    Core::TArray<Core::uint8> UniformBytes;
    const auto ViewUniform = Renderer::BuildDeferredFrameViewUniform(
        ProductionRuntime->DeferredResources.Plan.View);
    AppendPod(UniformBytes, ViewUniform);
    for (const auto& Draw :
         ProductionRuntime->DeferredResources.Plan.AcceptedDraws)
    {
        const auto Uniform = Renderer::BuildDeferredDrawMaterialUniform(Draw);
        AppendPod(UniformBytes, Uniform);
    }
    for (const auto& Light :
         ProductionRuntime->DeferredResources.Plan.Lights.Accepted)
    {
        const auto Uniform = Renderer::BuildDeferredLightUniform(Light);
        AppendPod(UniformBytes, Uniform);
    }
    ProductionExecutionInspection.UniformFingerprint =
        DigestBytes(UniformBytes);

    Core::TArray<Core::uint8> ShaderBytes;
    for (const auto& Shader :
         ProductionRuntime->DeferredResources.OwnedShaders)
    {
        if (!Shader) continue;
        const auto& Desc = Shader->GetDesc();
        AppendPod(ShaderBytes, Desc.Stage);
        AppendString(ShaderBytes, Desc.EntryPoint);
        AppendString(ShaderBytes, Desc.Payload.PayloadIdentity);
        AppendString(ShaderBytes, Desc.Payload.TargetProfile);
        AppendPayload(ShaderBytes, Desc.Payload.Bytes);
        for (const auto& Binding : Desc.InterfaceMetadata.Bindings)
        {
            AppendPod(ShaderBytes, Binding.SetIndex);
            AppendPod(ShaderBytes, Binding.BindingSlot);
            AppendPod(ShaderBytes, Binding.DescriptorType);
            AppendPod(ShaderBytes, Binding.ArrayCount);
            AppendPod(ShaderBytes, Binding.Visibility);
        }
    }
    ProductionExecutionInspection.ShaderFingerprint = DigestBytes(ShaderBytes);

    Core::TArray<Core::uint8> PipelineBytes;
    for (const auto& Pipeline :
         ProductionRuntime->DeferredResources.OwnedPipelines)
    {
        if (!Pipeline) continue;
        const auto& Desc = Pipeline->GetDesc();
        AppendPod(PipelineBytes, Desc.VertexInput.Stride);
        for (const auto& Attribute : Desc.VertexInput.Attributes)
        {
            AppendPod(PipelineBytes, Attribute.Location);
            AppendPod(PipelineBytes, Attribute.Format);
            AppendPod(PipelineBytes, Attribute.Offset);
        }
        AppendPod(PipelineBytes, Desc.Topology);
        AppendPod(PipelineBytes, Desc.Rasterizer.CullMode);
        AppendPod(PipelineBytes, Desc.Rasterizer.FrontFace);
        AppendPod(PipelineBytes, Desc.Rasterizer.bDepthClampEnabled);
        AppendPod(PipelineBytes, Desc.Blend.bEnabled);
        AppendPod(PipelineBytes, Desc.Blend.SourceColor);
        AppendPod(PipelineBytes, Desc.Blend.DestinationColor);
        AppendPod(PipelineBytes, Desc.Blend.ColorOp);
        AppendPod(PipelineBytes, Desc.DepthStencil.bDepthTestEnabled);
        AppendPod(PipelineBytes, Desc.DepthStencil.bDepthWriteEnabled);
        AppendPod(PipelineBytes, Desc.DepthStencil.DepthCompare);
        AppendPod(PipelineBytes, Desc.Multisample.SampleCount);
        AppendPod(PipelineBytes, Desc.Multisample.bSampleShadingEnabled);
        for (const auto Format : Desc.RenderTargets.ColorFormats)
            AppendPod(PipelineBytes, Format);
        AppendPod(PipelineBytes, Desc.RenderTargets.DepthStencilFormat);
        AppendPod(PipelineBytes, Desc.RenderTargets.SampleCount);
        AppendString(PipelineBytes, Desc.CompatibilitySummary);
    }
    ProductionExecutionInspection.PipelineFingerprint =
        DigestBytes(PipelineBytes);

    Core::TArray<Core::uint8> DescriptorBytes;
    for (const auto& Set :
         ProductionRuntime->DeferredResources.OwnedDescriptorSets)
    {
        if (!Set) continue;
        AppendPod(DescriptorBytes, Set->GetSetIndex());
        AppendPod(DescriptorBytes, Set->GetBoundResourceCount());
        const auto Layout = Set->GetPipelineLayout();
        if (!Layout) continue;
        for (const auto& Binding : Layout->GetDesc().Bindings)
        {
            if (Binding.SetIndex != Set->GetSetIndex()) continue;
            AppendPod(DescriptorBytes, Binding.SetIndex);
            AppendPod(DescriptorBytes, Binding.BindingSlot);
            AppendPod(DescriptorBytes, Binding.DescriptorType);
            AppendPod(DescriptorBytes, Binding.ArrayCount);
            AppendPod(DescriptorBytes, Binding.Visibility);
            for (Core::uint32 Index = 0; Index < Binding.ArrayCount; ++Index)
            {
                const auto Kind = Set->GetBoundResourceKind(
                    Binding.BindingSlot, Index);
                AppendPod(DescriptorBytes, Kind);
            }
        }
    }
    ProductionExecutionInspection.DescriptorFingerprint =
        DigestBytes(DescriptorBytes);
    ProductionExecutionInspection.DeviceFingerprint = DigestText(
        ProductionExecutionInspection.Runtime.AdapterName.ToStdString() + "|" +
        std::to_string(static_cast<int>(
            ProductionExecutionInspection.Runtime.ObjectMode)));
    const auto RecordCommands = [this](EDemoRenderPath Path,
        bool bAuthoritativeDeferredReadbacks)
        -> Core::TSharedPtr<RHI::IRHICommandBuffer>
    {
        if (Path == EDemoRenderPath::DeferredFull)
        {
            const auto Bindings = ProductionRuntime->DeferredResources.
                BuildCycleBindings(bAuthoritativeDeferredReadbacks);
            const auto Execution = Renderer::FDeferredFrameExecutor().Execute(
                ProductionRuntime->DeferredResources.Plan,
                ProductionRuntime->DeferredResources.Graph,
                Bindings);
            return Execution.Succeeded()
                ? ProductionRuntime->DeferredResources.Bindings.CommandBuffer
                : Core::TSharedPtr<RHI::IRHICommandBuffer>{};
        }
        const auto Execution = Renderer::FForwardFrameExecutor().Execute(
            ProductionRuntime->ForwardPlan,
            ProductionRuntime->ForwardBindings);
        if (!Execution.Succeeded())
        {
            const Core::FString Reason(
                "result=" + std::to_string(
                    static_cast<int>(Execution.Result)) +
                "; recorded=" +
                std::to_string(Execution.RecordedCommandCount));
            Diagnostics.Add(EDemoStage::Record,
                EDemoExitCode::FrameFailed, "ForwardExecutor",
                Reason.CStr());
        }
        return Execution.Succeeded()
            ? ProductionRuntime->ForwardBindings.CommandBuffer
            : Core::TSharedPtr<RHI::IRHICommandBuffer>{};
    };

    Core::TArray<Core::uint8> LifecycleReadbackScratch;
    Core::uint64 SubmissionFrameToken = 0;
    Core::uint64 CompletedFrameToken = 0;
    FDemoProductionPresentationResult LifecyclePresentationScratch;
    const auto CaptureReadback = [this, &LifecycleReadbackScratch,
        &LifecyclePresentationScratch, &CompletedFrameToken](
        Core::uint32 Cycle,
        Core::uint64 FrameToken,
        const Core::FString& Name,
        const Core::TSharedPtr<RHI::IRHITexture>& Source,
        const Core::TSharedPtr<RHI::IRHIBuffer>& Readback,
        const RHI::FRHITextureBufferCopyRegion& Region,
        RHI::ERHIFormat Format,
        Core::uint64 ReadbackBytes,
        bool bRetainAuthoritativeEvidence,
        bool bRecordLifecycleCapture)
    {
        const bool bSelectedVisiblePath = Configuration.bVisibleCapture &&
            Configuration.RenderPath == EDemoRenderPath::DeferredFull &&
            Name == Core::FString("FinalOutput");
        const bool bAuthoritativeFrameCapture =
            bRetainAuthoritativeEvidence && bSelectedVisiblePath &&
            Name == Core::FString("FinalOutput");
        Core::TArray<Core::uint8> RetainedBytes;
        auto& Bytes = bRetainAuthoritativeEvidence
            ? RetainedBytes : LifecycleReadbackScratch;
        const auto Device = BackendRuntime
            ? BackendRuntime->GetDevice()
            : Core::TSharedPtr<RHI::IRHIDevice>{};
        if (!Device || !Readback || ReadbackBytes == 0 ||
            ReadProductionBuffer(Configuration.GraphicsBackend,
                Device, Readback, ReadbackBytes, Bytes) !=
                RHI::ERHIResult::Success ||
            Bytes.size() != ReadbackBytes)
            return false;
        const bool bNonBlank = std::any_of(
            Bytes.begin(), Bytes.end(),
            [](Core::uint8 Value) { return Value != 0; });
        if (Name == Core::FString("FinalOutput"))
            ProductionExecutionInspection.FormalOutputReadbackDigest =
                Asset::FAssetDigest::FromBytes(Bytes).ToLowerHex();
        const Core::uint32 RowTexels =
            Region.DestinationRowLengthTexels == 0
            ? Region.Width : Region.DestinationRowLengthTexels;
        if (bRetainAuthoritativeEvidence)
        {
            FDemoProductionReadbackEvidence Evidence;
            Evidence.FrameToken = FrameToken;
            Evidence.Name = Name;
            Evidence.Digest =
                Asset::FAssetDigest::FromBytes(Bytes).ToLowerHex();
            Evidence.ByteCount = ReadbackBytes;
            Evidence.Width = Region.Width;
            Evidence.Height = Region.Height;
            Evidence.RowPitchBytes =
                RowTexels * RHI::GetRHIFormatByteSize(Format);
            Evidence.Format = Format;
            Evidence.bNonBlank = bNonBlank;
            Evidence.Bytes = Bytes;
            ProductionExecutionInspection.Readbacks.push_back(
                std::move(Evidence));
        }
        if ((bRecordLifecycleCapture || bAuthoritativeFrameCapture) &&
            bNonBlank &&
            (Name == Core::FString("FinalOutput") ||
                Name == Core::FString("ForwardColor")))
        {
            FDemoProductionCapture Capture;
            Capture.FrameToken = FrameToken;
            Capture.ExpectedFrameToken = CompletedFrameToken;
            Capture.Cycle = Cycle;
            Capture.Name = Name;
            Capture.Width = Region.Width;
            Capture.Height = Region.Height;
            Capture.RowPitchBytes =
                RowTexels * RHI::GetRHIFormatByteSize(Format);
            Capture.CaptureStartedNs = static_cast<Core::uint64>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
            Capture.Format = Format;
            if (bSelectedVisiblePath)
                Capture.Bytes = std::move(Bytes);
            if (bSelectedVisiblePath)
            {
                if (!PresentProductionCaptureWithRecovery(
                        Capture, Source, FrameToken,
                        LifecyclePresentationScratch))
                    return false;
            }
            if (!Capture.Bytes.empty())
                Capture.Digest =
                    Asset::FAssetDigest::FromBytes(Capture.Bytes).ToLowerHex();
            if (bAuthoritativeFrameCapture)
            {
                ProductionExecutionInspection.AuthoritativeFrameToken =
                    FrameToken;
                ProductionExecutionInspection.SettledPresentedFrameToken =
                    FrameToken;
                ProductionExecutionInspection.AuthoritativeCapture =
                    std::move(Capture);
                return true;
            }
            if (Capture.bPresented && Capture.bWindowOnlyCapture)
            {
                ProductionExecutionInspection.LastLifecyclePresentedFrameToken =
                    FrameToken;
                auto& Stale = ProductionExecutionInspection.
                    LastLifecyclePresentedCapture;
                Stale = {};
                Stale.FrameToken = Capture.FrameToken;
                Stale.ExpectedFrameToken = Capture.ExpectedFrameToken;
                Stale.Cycle = Capture.Cycle;
                Stale.Name = Capture.Name;
                Stale.Digest = Capture.Digest;
                Stale.Width = Capture.Width;
                Stale.Height = Capture.Height;
                Stale.RowPitchBytes = Capture.RowPitchBytes;
                Stale.Format = Capture.Format;
                Stale.bPresented = true;
                Stale.bWindowOnlyCapture = true;
                Stale.Bytes = Capture.Bytes;
            }
            const bool bStreamCalibrationCapture =
                bSelectedVisiblePath &&
                !Configuration.ProductionCaptureRoot.IsEmpty() &&
                Capture.Name == Core::FString("FinalOutput") &&
                Capture.Cycle <= 20;
            if (bStreamCalibrationCapture && !WriteProductionWindowCapture(
                    Capture, ToString(Configuration.GraphicsBackend),
                    Configuration.WorkloadRevision.CStr(),
                    Configuration.ProductionCaptureRoot.CStr()))
                return false;
            constexpr Core::usize MaximumRetainedCalibrationCaptures = 20;
            const Core::usize RetainedCalibrationCaptures =
                static_cast<Core::usize>(std::count_if(
                    ProductionExecutionInspection.Captures.begin(),
                    ProductionExecutionInspection.Captures.end(),
                    [&Capture](const FDemoProductionCapture& Existing)
                    {
                        return Existing.Name == Capture.Name &&
                            Existing.bWindowOnlyCapture &&
                            !Existing.Bytes.empty();
                    }));
            if (!bSelectedVisiblePath ||
                !Configuration.ProductionCaptureRoot.IsEmpty() ||
                Configuration.OutputDeviceProfileId.View().starts_with(
                    "Hdr.") ||
                RetainedCalibrationCaptures >=
                    MaximumRetainedCalibrationCaptures)
            {
                if (bSelectedVisiblePath)
                    LifecycleReadbackScratch = std::move(Capture.Bytes);
                else
                {
                    Capture.Bytes.clear();
                    Capture.Bytes.shrink_to_fit();
                }
            }
            RecordProductionCapture(std::move(Capture));
        }
        if (!bNonBlank)
            Diagnostics.Add(EDemoStage::Readback,
                EDemoExitCode::ValidationFailed, Name.CStr(),
                "native GPU readback contained only zero bytes");
        return bNonBlank;
    };

    const auto ExecutePath = [&](EDemoRenderPath Path, Core::uint32 Cycle,
        bool bRetainAuthoritativeEvidence,
        bool bRecordLifecycleCapture)
    {
        if (!ProductionSubmissionHarness) return false;
        const auto Commands = RecordCommands(
            Path, bRetainAuthoritativeEvidence);
        const char* PathName = Path == EDemoRenderPath::DeferredFull
            ? "Deferred" : "Forward";
        if (!Commands)
        {
            Diagnostics.Add(EDemoStage::Submit,
                EDemoExitCode::FrameFailed, PathName,
                "production command recording failed");
            return false;
        }
        const Core::uint64 FrameToken = ++SubmissionFrameToken;
        if (ProductionSubmissionHarness->SubmitAndWait(
                Commands, 30'000'000) != RHI::ERHIResult::Success)
        {
            Diagnostics.Add(EDemoStage::Submit,
                EDemoExitCode::FrameFailed, PathName,
                "production submit, wait, or reusable-fence reset failed");
            return false;
        }
        CompletedFrameToken = FrameToken;
        bool bReadbacksValid = true;
        if (Path == EDemoRenderPath::DeferredFull)
        {
            for (const auto& Binding :
                 ProductionRuntime->DeferredResources.Bindings.Readbacks)
            {
                if (!bRetainAuthoritativeEvidence &&
                    Binding.Name != Core::FString("FinalOutput"))
                    continue;
                Core::uint64 ReadbackBytes = 0;
                const bool bBindingValid = Binding.Source &&
                    RHI::TryGetRHITextureBufferCopyByteSize(
                        Binding.Region, Binding.Source->GetFormat(),
                        ReadbackBytes) &&
                    CaptureReadback(Cycle, FrameToken, Binding.Name,
                        Binding.Source,
                        Binding.Destination, Binding.Region,
                        Binding.Source->GetFormat(), ReadbackBytes,
                        bRetainAuthoritativeEvidence,
                        bRecordLifecycleCapture);
                bReadbacksValid = bBindingValid && bReadbacksValid;
            }
        }
        else
        {
            Core::uint64 ReadbackBytes = 0;
            bReadbacksValid =
                ProductionRuntime->ForwardBindings.OutputTexture &&
                RHI::TryGetRHITextureBufferCopyByteSize(
                    ProductionRuntime->ForwardBindings.ReadbackRegion,
                    ProductionRuntime->ForwardBindings.OutputTexture->GetFormat(),
                    ReadbackBytes) &&
                CaptureReadback(Cycle, FrameToken, "ForwardColor",
                    ProductionRuntime->ForwardBindings.OutputTexture,
                    ProductionRuntime->ForwardBindings.ReadbackBuffer,
                    ProductionRuntime->ForwardBindings.ReadbackRegion,
                    ProductionRuntime->ForwardBindings.OutputTexture->GetFormat(),
                    ReadbackBytes, bRetainAuthoritativeEvidence,
                    bRecordLifecycleCapture);
        }
        if (!bReadbacksValid)
            Diagnostics.Add(EDemoStage::Readback,
                EDemoExitCode::ValidationFailed, PathName,
                "production GPU readback validation failed");
        return bReadbacksValid;
    };

    const auto PrimeNativePath = [&](EDemoRenderPath Path)
    {
        if (!ProductionSubmissionHarness) return false;
        const auto Commands = RecordCommands(Path, false);
        return Commands && ProductionSubmissionHarness->SubmitAndWait(
            Commands, 30'000'000) == RHI::ERHIResult::Success;
    };
    if (!PrimeNativePath(EDemoRenderPath::DeferredFull) ||
        !PrimeNativePath(EDemoRenderPath::ForwardSmoke))
        return FailInitialize(EDemoStage::Submit,
            EDemoExitCode::FrameFailed, "ProductionLifecyclePrime",
            "production native lifecycle prime submission failed");
    ProductionExecutionInspection.Readbacks.clear();
    const FDemoProductionLifecycleCounters PrimeCounters =
        ReleaseProductionContentCycle();
    if (!PrimeCounters.IsAtBaseline() ||
        !PrimeCounters.bStaleHandleRejected)
        return FailInitialize(EDemoStage::Memory,
            EDemoExitCode::ValidationFailed, "ProductionLifecyclePrime",
            "production native lifecycle prime did not return to baseline");
    if (ShouldPrimeProductionBackendForRssComparison())
    {
        if (SuspendProductionBackendForRssComparison() !=
                EDemoExitCode::Success)
            return EDemoExitCode::InitializationFailed;
        Core::FPlatformMemory::ReleaseUnusedHeapPages();
        if (ResumeProductionBackendAfterRssComparison() !=
                EDemoExitCode::Success)
            return EDemoExitCode::InitializationFailed;
        Diagnostics.Add(EDemoStage::Memory, EDemoExitCode::Success,
            "ProductionRssComparison",
            "unmeasured backend restart primed before declared lifecycle");
    }
    if (InitializeProductionContent() != EDemoExitCode::Success)
        return EDemoExitCode::InitializationFailed;

    for (Core::uint32 Cycle = 1;
         Cycle <= Configuration.ProductionLifecycleCycles; ++Cycle)
    {
        ProductionExecutionInspection.Readbacks.clear();
        if (!ExecutePath(
                EDemoRenderPath::DeferredFull, Cycle, false, true) ||
            !ExecutePath(
                EDemoRenderPath::ForwardSmoke, Cycle, false, true))
            return FailInitialize(EDemoStage::Readback,
                EDemoExitCode::ValidationFailed, "ProductionExecution",
                "production Deferred or Forward native execution failed");

        const FDemoProductionLifecycleCounters Counters =
            ReleaseProductionContentCycle();
        const bool bRssComparisonPoint =
            Cycle == Configuration.ProductionWarmupCycles ||
            Cycle == Configuration.ProductionLifecycleCycles;
        if (bRssComparisonPoint &&
            SuspendProductionBackendForRssComparison() !=
                EDemoExitCode::Success)
            return EDemoExitCode::InitializationFailed;
        if (bRssComparisonPoint)
            Core::FPlatformMemory::ReleaseUnusedHeapPages();
        if (!ValidationMonitor.SampleProductionCycle(Cycle, Counters))
            return FailInitialize(EDemoStage::Memory,
                EDemoExitCode::ValidationFailed, "ProductionLifecycle",
                "production lifecycle RSS sampling failed");
        ProductionExecutionInspection.CompletedCycles = Cycle;
        CompletedFrames = Cycle;
        if (Configuration.ProductionLifecycleCycles == 1000 &&
            (Cycle % 100u == 0u ||
                Cycle == Configuration.ProductionLifecycleCycles))
        {
            std::cout << "[OBSERVATION] production-lifecycle-progress"
                      << " backend="
                      << ToString(Configuration.GraphicsBackend)
                      << " cycle=" << Cycle
                      << " total="
                      << Configuration.ProductionLifecycleCycles
                      << std::endl;
        }
        if (Cycle < Configuration.ProductionLifecycleCycles)
        {
            if (bRssComparisonPoint &&
                ResumeProductionBackendAfterRssComparison() !=
                    EDemoExitCode::Success)
                return EDemoExitCode::InitializationFailed;
            if (InitializeProductionContent() != EDemoExitCode::Success)
                return EDemoExitCode::InitializationFailed;
        }
    }
    ProductionExecutionInspection.bSubmissionCompleted = true;
    ProductionExecutionInspection.bSynchronizationCompleted = true;
    ProductionExecutionInspection.LifecycleSamples =
        ValidationMonitor.GetProductionSamples();
    ProductionExecutionInspection.bLifecyclePassed =
        ValidationMonitor.EvaluateProductionLifecycle();
    if (!ProductionExecutionInspection.bLifecyclePassed)
        return FailInitialize(EDemoStage::Memory,
            EDemoExitCode::ValidationFailed, "ProductionLifecycle",
            "production ownership or RSS lifecycle gate failed");
    if (ResumeProductionBackendAfterRssComparison() !=
        EDemoExitCode::Success)
        return EDemoExitCode::InitializationFailed;
    if (InitializeProductionContent() != EDemoExitCode::Success)
        return EDemoExitCode::InitializationFailed;
    ProductionExecutionInspection.Readbacks.clear();
    const Core::uint32 EvidenceCycle =
        Configuration.ProductionLifecycleCycles;
    if (!ExecutePath(
            EDemoRenderPath::DeferredFull, EvidenceCycle, true, false) ||
        !ExecutePath(
            EDemoRenderPath::ForwardSmoke, EvidenceCycle, true, false))
        return FailInitialize(EDemoStage::Readback,
            EDemoExitCode::ValidationFailed, "ProductionEvidenceExtraction",
            "post-lifecycle authoritative GPU readback extraction failed");
    const FDemoProductionLifecycleCounters EvidenceCounters =
        ReleaseProductionContentCycle();
    if (!EvidenceCounters.IsAtBaseline() ||
        !EvidenceCounters.bStaleHandleRejected)
        return FailInitialize(EDemoStage::Memory,
            EDemoExitCode::ValidationFailed, "ProductionEvidenceExtraction",
            "post-lifecycle evidence extraction did not return to baseline");
    LifecycleState = EDemoLifecycleState::Stopping;
    Diagnostics.Add(EDemoStage::Readback, EDemoExitCode::Success,
        "ProductionExecution",
        "native production submission and readback completed");
    return EDemoExitCode::Success;
}

} // namespace Stoner::Demo
