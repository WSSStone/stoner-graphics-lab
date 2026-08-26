#include "FStonerDemoApplication.h"

#include "Asset/AssetMinimal.h"
#include "FProductionContentRuntime.h"
#include "FProductionSubmissionHarness.h"
#include "FProductionWindowCaptureWriter.h"
#include "Renderer/RendererMinimal.h"

#include <algorithm>
#include <chrono>
#include <string>

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

    const auto CaptureReadback = [this](
        Core::uint32 Cycle,
        const Core::FString& Name,
        const Core::TSharedPtr<RHI::IRHIBuffer>& Readback,
        const RHI::FRHITextureBufferCopyRegion& Region,
        RHI::ERHIFormat Format,
        Core::uint64 ReadbackBytes,
        bool bRetainAuthoritativeEvidence,
        bool bRecordLifecycleCapture)
    {
        Core::TArray<Core::uint8> Bytes;
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
        const Core::uint32 RowTexels =
            Region.DestinationRowLengthTexels == 0
            ? Region.Width : Region.DestinationRowLengthTexels;
        if (bRetainAuthoritativeEvidence)
        {
            FDemoProductionReadbackEvidence Evidence;
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
        if (bRecordLifecycleCapture && bNonBlank &&
            (Name == Core::FString("FinalOutput") ||
                Name == Core::FString("ForwardColor")))
        {
            FDemoProductionCapture Capture;
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
            const bool bSelectedVisiblePath = Configuration.bVisibleCapture &&
                ((Configuration.RenderPath == EDemoRenderPath::DeferredFull &&
                    Name == Core::FString("FinalOutput")) ||
                 (Configuration.RenderPath == EDemoRenderPath::ForwardSmoke &&
                    Name == Core::FString("ForwardColor")));
            if (bSelectedVisiblePath || bRetainAuthoritativeEvidence)
                Capture.Bytes = std::move(Bytes);
            if (bSelectedVisiblePath)
            {
                if (!PresentProductionCaptureWithRecovery(Capture)) return false;
            }
            if (!Capture.Bytes.empty())
                Capture.Digest =
                    Asset::FAssetDigest::FromBytes(Capture.Bytes).ToLowerHex();
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
                RetainedCalibrationCaptures >=
                    MaximumRetainedCalibrationCaptures)
            {
                Capture.Bytes.clear();
                Capture.Bytes.shrink_to_fit();
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
        if (ProductionSubmissionHarness->SubmitAndWait(
                Commands, 30'000'000) != RHI::ERHIResult::Success)
        {
            Diagnostics.Add(EDemoStage::Submit,
                EDemoExitCode::FrameFailed, PathName,
                "production submit, wait, or reusable-fence reset failed");
            return false;
        }
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
                    CaptureReadback(Cycle, Binding.Name,
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
                CaptureReadback(Cycle, "ForwardColor",
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
