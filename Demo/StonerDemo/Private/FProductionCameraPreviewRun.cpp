#include "FStonerDemoApplication.h"

#include "FProductionCameraPreview.h"
#include "FProductionContentRuntime.h"
#include "FStonerDemoWindowState.h"
#include "Renderer/FDeferredFrameExecutor.h"
#include "Renderer/FDeferredRenderGraphDeclaration.h"
#include "RHI/IRHICommandBuffer.h"
#include "RHI/IRHICommandQueue.h"
#include "RHI/IRHIDevice.h"
#include "RHI/IRHIFence.h"
#include "RHI/IRHITexture.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

namespace Stoner::Demo
{

EDemoExitCode FStonerDemoApplication::RunProductionCameraPreview()
{
    using Clock = std::chrono::steady_clock;
    using namespace Stoner;
    if (!ProductionRuntime || !ProductionRuntime->DeferredRenderSnapshot ||
        !ProductionRuntime->ForwardRenderSnapshot || !BackendRuntime ||
        !BackendRuntime->GetDevice() || !Window)
        return EDemoExitCode::InitializationFailed;

    const auto Device = BackendRuntime->GetDevice();
    auto Queue = Device->CreateCommandQueue(RHI::ERHIQueueType::Graphics);
    FProductionCameraPreset InitialCamera;
    FProductionCameraPreviewController Controller;
    Core::FString Reason;
    if (!Queue.Succeeded() ||
        !ResolveProductionCameraPreset(
            Configuration.WorkloadRevision, InitialCamera, &Reason) ||
        !Controller.Initialize(InitialCamera,
            ProductionRuntime->Composition.DeferredInputs.View.Extent.Width,
            ProductionRuntime->Composition.DeferredInputs.View.Extent.Height,
            &Reason))
        return FailInitialize(EDemoStage::Pipeline,
            EDemoExitCode::InitializationFailed, "ProductionCameraPreview",
            Reason.IsEmpty() ? "camera preview initialization failed" :
                Reason.CStr());

    const auto ApplyCamera = [&]() -> bool
    {
        Renderer::FDeferredFramePlan DeferredPlan;
        Renderer::FForwardFramePlan ForwardPlan;
        if (!ApplyProductionCameraPreset(
                ProductionRuntime->Composition, Controller.GetCamera(),
                DeferredPlan, ForwardPlan, &Reason) ||
            !BindProductionDeferredDraws(
                *ProductionRuntime->DeferredRenderSnapshot, DeferredPlan,
                ProductionRuntime->DeferredResources.Bindings, &Reason) ||
            !UploadProductionDeferredUniforms(*Device,
                *ProductionRuntime->DeferredRenderSnapshot, DeferredPlan,
                &Reason) ||
            !BindProductionForwardDraws(
                *ProductionRuntime->ForwardRenderSnapshot, ForwardPlan,
                ProductionRuntime->ForwardBindings, &Reason))
            return false;
        auto Graph = Renderer::BuildDeferredRenderGraphDeclaration(
            DeferredPlan);
        if (!Graph.bValid)
        {
            Reason = "camera preview graph rebuild failed";
            return false;
        }
        ProductionRuntime->DeferredResources.Plan = std::move(DeferredPlan);
        ProductionRuntime->DeferredResources.Graph = std::move(Graph);
        ProductionRuntime->ForwardPlan = std::move(ForwardPlan);
        return true;
    };
    if (!ApplyCamera())
        return FailInitialize(EDemoStage::Pipeline,
            EDemoExitCode::InitializationFailed, "ProductionCameraPreview",
            Reason.CStr());

    const auto FindFinalReadback = [&]()
        -> const Renderer::FDeferredReadbackBinding*
    {
        const auto& Readbacks =
            ProductionRuntime->DeferredResources.Bindings.Readbacks;
        const auto Found = std::find_if(Readbacks.begin(), Readbacks.end(),
            [](const auto& Binding)
            {
                return Binding.Name == Core::FString("FinalOutput");
            });
        return Found == Readbacks.end() ? nullptr : &*Found;
    };

    LifecycleState = EDemoLifecycleState::Running;
    bool bSnapshotPending = false;
    auto PreviousTime = Clock::now();
    while (!Window->Value.IsCloseRequested())
    {
        (void)Window->Value.PollEvents();
        const auto Now = Clock::now();
        const double DeltaSeconds =
            std::chrono::duration<double>(Now - PreviousTime).count();
        PreviousTime = Now;
        const FProductionCameraPreviewUpdate Update = Controller.Update(
            Window->Value.PollInputEvents(), DeltaSeconds);
        if (Update.bExitRequested) break;
        bSnapshotPending = bSnapshotPending || Update.bSnapshotRequested;
        if (Update.bCameraChanged && !ApplyCamera())
            return FailInitialize(EDemoStage::Pipeline,
                EDemoExitCode::FrameFailed, "ProductionCameraPreview",
                Reason.CStr());

        if (!Window->Value.HasDrawableArea())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        const auto Execution = Renderer::FDeferredFrameExecutor().Execute(
            ProductionRuntime->DeferredResources.Plan,
            ProductionRuntime->DeferredResources.Graph,
            ProductionRuntime->DeferredResources.Bindings);
        auto Fence = Device->CreateFence();
        if (!Execution.Succeeded() || !Fence.Succeeded() ||
            Queue.Object->Submit(
                ProductionRuntime->DeferredResources.Bindings.CommandBuffer,
                {}, {}, Fence.Object) != RHI::ERHIResult::Success ||
            Fence.Object->Wait(30'000'000) != RHI::ERHIResult::Success)
            return FailInitialize(EDemoStage::Submit,
                EDemoExitCode::FrameFailed, "ProductionCameraPreview",
                "camera preview recording or submission failed");

        const Renderer::FDeferredReadbackBinding* Final = FindFinalReadback();
        Core::uint64 ReadbackBytes = 0;
        Core::TArray<Core::uint8> Bytes;
        if (!Final || !Final->Source ||
            !RHI::TryGetRHITextureBufferCopyByteSize(
                Final->Region, Final->Source->GetFormat(), ReadbackBytes) ||
            ReadProductionBuffer(Configuration.GraphicsBackend, Device,
                Final->Destination, ReadbackBytes, Bytes) !=
                RHI::ERHIResult::Success)
            return FailInitialize(EDemoStage::Readback,
                EDemoExitCode::FrameFailed, "ProductionCameraPreview",
                "camera preview final image readback failed");

        const Core::uint32 RowTexels =
            Final->Region.DestinationRowLengthTexels == 0
            ? Final->Region.Width
            : Final->Region.DestinationRowLengthTexels;
        const Core::uint32 RowPitchBytes = RowTexels *
            RHI::GetRHIFormatByteSize(Final->Source->GetFormat());
        FDemoProductionPresentationResult Presented;
        RHI::ERHIResult PresentResult =
            BackendRuntime->PresentProductionImage(Bytes,
                Final->Region.Width, Final->Region.Height,
                RowPitchBytes, Presented);
        if (PresentResult == RHI::ERHIResult::ResizeRequired)
        {
            const Core::uint32 Width = Window->Value.GetDrawableWidth();
            const Core::uint32 Height = Window->Value.GetDrawableHeight();
            if (Width != 0 && Height != 0 &&
                BackendRuntime->RecreatePresentation(Width, Height) ==
                    RHI::ERHIResult::Success)
                PresentResult = BackendRuntime->PresentProductionImage(Bytes,
                    Final->Region.Width, Final->Region.Height,
                    RowPitchBytes, Presented);
        }
        if (PresentResult != RHI::ERHIResult::Success ||
            !Presented.bPresented)
            return FailInitialize(EDemoStage::Present,
                EDemoExitCode::FrameFailed, "ProductionCameraPreview",
                "camera preview native presentation failed");

        ++CompletedFrames;
        if (bSnapshotPending)
        {
            const FProductionCameraCandidate Candidate =
                Controller.BuildCandidate(
                    ToString(Configuration.GraphicsBackend),
                    Configuration.WorkloadRevision);
            if (!WriteProductionCameraCandidate(
                    Configuration.ProductionCameraPresetOutput,
                    Candidate, &Reason))
                return FailInitialize(EDemoStage::Readback,
                    EDemoExitCode::ReportFailed,
                    "ProductionCameraPreview", Reason.CStr());
            std::cout << "[CAMERA] output="
                      << Configuration.ProductionCameraPresetOutput.CStr()
                      << " matrix-sha256=" << Candidate.MatrixSha256.CStr()
                      << '\n';
            Diagnostics.Add(EDemoStage::Readback,
                EDemoExitCode::Success, "ProductionCameraPreview",
                "camera candidate matrix snapshot written");
            bSnapshotPending = false;
        }
    }

    LifecycleState = EDemoLifecycleState::Stopping;
    Diagnostics.Add(EDemoStage::Present, EDemoExitCode::Success,
        "ProductionCameraPreview", "interactive camera preview stopped");
    return EDemoExitCode::Success;
}

} // namespace Stoner::Demo
