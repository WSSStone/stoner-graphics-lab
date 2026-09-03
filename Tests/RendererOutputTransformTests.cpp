#include "RendererOutputTransformTests.h"

#include "Renderer/FHDRPostProcessPipeline.h"
#include "Renderer/FOutputTransformExecutor.h"
#include "Renderer/FRenderGraph.h"

#include <iostream>

namespace
{

using namespace Stoner::Renderer;
using namespace Stoner::RHI;

class FOutputTerminalTexture final : public IRHITexture
{
public:
    explicit FOutputTerminalTexture(FRHITextureDesc InDesc)
        : Desc(std::move(InDesc)) {}
    const FRHITextureDesc& GetDesc() const noexcept override { return Desc; }
    ERHITextureDimension GetDimension() const noexcept override
    {
        return Desc.Dimension;
    }
    ERHIFormat GetFormat() const noexcept override { return Desc.Format; }
    ERHITextureUsage GetUsage() const noexcept override { return Desc.Usage; }
    ERHIResourceLifecycleState GetLifecycleState() const noexcept override
    {
        return State;
    }
    ERHIResult Invalidate() override
    {
        State = ERHIResourceLifecycleState::Invalidated;
        return ERHIResult::Success;
    }

private:
    FRHITextureDesc Desc;
    ERHIResourceLifecycleState State = ERHIResourceLifecycleState::Valid;
};

class FOutputTerminalProbe final : public IOutputTransformNativeFrameExecutor
{
public:
    enum class EFailure { None, AcquirePaused, Submit, Readback, Present, Mismatch };

    explicit FOutputTerminalProbe(EFailure InFailure = EFailure::None)
        : Failure(InFailure) {}

    ERHIResult Acquire(const FOutputTransformPlan& Plan,
        FOutputTransformNativeFrameBinding& OutFrame) override
    {
        if (Failure == EFailure::AcquirePaused) return ERHIResult::NotReady;
        bOrderValid = bOrderValid && Step == 0;
        Step = 1;
        Owners = 1;
        FRHITextureDesc Desc;
        Desc.Width = Plan.OutputDesc.Width;
        Desc.Height = Plan.OutputDesc.Height;
        Desc.Format = Plan.OutputDesc.Format;
        Desc.Usage = ERHITextureUsage::ColorAttachment |
            ERHITextureUsage::CopySource | ERHITextureUsage::Present;
        OutFrame.FormalOutput =
            Stoner::Core::MakeShared<FOutputTerminalTexture>(Desc);
        OutFrame.ResolvedState.ModeGeneration = 7;
        OutFrame.ResolvedState.Width = Failure == EFailure::Mismatch
            ? Plan.OutputDesc.Width + 1u : Plan.OutputDesc.Width;
        OutFrame.ResolvedState.Height = Plan.OutputDesc.Height;
        OutFrame.ResolvedState.Format = Plan.OutputDesc.Format;
        OutFrame.ResolvedState.ColorSpace =
            Plan.ResolvedSettings.ColorSpace;
        OutFrame.ResolvedState.NativeEncoding =
            Plan.ResolvedSettings.NativeEncoding;
        OutFrame.ResolvedState.ReferenceWhiteNits =
            Plan.ResolvedSettings.ReferenceWhiteNits;
        OutFrame.ResolvedState.TargetPeakNits =
            Plan.ResolvedSettings.TargetPeakNits;
        OutFrame.ResolvedState.SwapchainImageGeneration = 9;
        OutFrame.PresentationFrame.FrameToken = Plan.FrameToken;
        OutFrame.PresentationFrame.ModeGeneration = 7;
        OutFrame.PresentationFrame.SwapchainImageGeneration = 9;
        OutFrame.PresentationFrame.Width =
            OutFrame.ResolvedState.Width;
        OutFrame.PresentationFrame.Height = Plan.OutputDesc.Height;
        OutFrame.PresentationFrame.Format = Plan.OutputDesc.Format;
        OutFrame.PresentationFrame.ColorSpace =
            Plan.ResolvedSettings.ColorSpace;
        return ERHIResult::Success;
    }

    ERHIResult RecordScheduleEvent(const FOutputTransformPlan&,
        const FOutputTransformGraphDeclaration&,
        const FRenderGraphScheduleEvent&,
        const FOutputTransformNativeFrameBinding&) override
    {
        bOrderValid = bOrderValid && Step == 1;
        ++RecordedEvents;
        return ERHIResult::Success;
    }

    ERHIResult Submit(const FOutputTransformPlan&,
        const FOutputTransformNativeFrameBinding&) override
    {
        bOrderValid = bOrderValid && Step == 1 && RecordedEvents != 0;
        Step = 2;
        return Failure == EFailure::Submit
            ? ERHIResult::Failed : ERHIResult::Success;
    }

    ERHIResult WaitForCompletion(const FOutputTransformPlan&,
        const FOutputTransformNativeFrameBinding&) override
    {
        bOrderValid = bOrderValid && Step == 2;
        Step = 3;
        return ERHIResult::Success;
    }

    ERHIResult CompleteReadback(const FOutputTransformPlan&,
        const FOutputTransformNativeFrameBinding&) override
    {
        bOrderValid = bOrderValid && Step == 3;
        Step = 4;
        return Failure == EFailure::Readback
            ? ERHIResult::Failed : ERHIResult::Success;
    }

    ERHIResult Present(const FOutputTransformPlan&,
        const FOutputTransformNativeFrameBinding&) override
    {
        bOrderValid = bOrderValid && (Step == 3 || Step == 4);
        Step = 5;
        if (Failure == EFailure::Present) return ERHIResult::Failed;
        Owners = 0;
        return ERHIResult::Success;
    }

    ERHIResult ReleaseAfterFailure(
        const FOutputTransformNativeFrameBinding&) noexcept override
    {
        ++ReleaseCount;
        Owners = 0;
        return ERHIResult::Success;
    }

    Stoner::Core::uint32 GetOutstandingTerminalOwnerCount()
        const noexcept override { return Owners; }

    EFailure Failure = EFailure::None;
    Stoner::Core::uint32 Step = 0;
    Stoner::Core::uint32 RecordedEvents = 0;
    Stoner::Core::uint32 Owners = 0;
    Stoner::Core::uint32 ReleaseCount = 0;
    bool bOrderValid = true;
};

void Record(FRendererOutputTransformTestResult& Result, bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FHDRSceneColorHandoff MakeProducedSceneColor(FRenderGraph& Graph,
    EHDRSceneColorProducer Producer = EHDRSceneColorProducer::Forward)
{
    FRenderGraphResourceDesc Desc = FRenderGraphResourceDesc::TypedTexture2D(
        "SceneColor", 64, 32, ERHIFormat::R16G16B16A16_Float,
        ERHISampleCount::One,
        ERHITextureUsage::Sampled | ERHITextureUsage::ColorAttachment,
        ERenderGraphColorDomain::SceneLinearRec709D65);
    Desc.Ownership = ERenderGraphResourceOwnership::Imported;
    Desc.InitialState = ERenderGraphResourceState::External;
    Desc.AliasPolicy = ERenderGraphAliasPolicy::Disabled;
    const FRenderGraphResourceHandle Resource =
        Graph.CreateBuilder().ImportResource(Desc);

    FHDRSceneColorHandoffDesc HandoffDesc;
    HandoffDesc.SceneColorId = 101;
    HandoffDesc.Producer = Producer;
    HandoffDesc.ViewId = 7;
    HandoffDesc.FrameToken = 11;
    HandoffDesc.Width = 64;
    HandoffDesc.Height = 32;
    FHDRSceneColorHandoff Handoff =
        FHDRSceneColorHandoff::Declare(HandoffDesc);
    (void)Handoff.BindProducer(Resource);
    (void)Handoff.MarkProduced();
    return Handoff;
}

void TestHandoffStateAndMetadata(FRendererOutputTransformTestResult& Result)
{
    FRenderGraph Graph("OutputHandoff");
    FHDRSceneColorHandoff Handoff = MakeProducedSceneColor(Graph);
    Record(Result, Handoff.IsReadyForConsumption() &&
            Handoff.GetState() == EHDRSceneColorState::Produced &&
            Handoff.GetFormat() == ERHIFormat::R16G16B16A16_Float &&
            Handoff.GetSampleCount() == ERHISampleCount::One &&
            Handoff.GetPrimaries() == EOutputColorPrimaries::Rec709 &&
            Handoff.GetWhitePoint() == EOutputWhitePoint::D65 &&
            Handoff.GetTransfer() == EOutputTransferFunction::Linear &&
            Handoff.GetAlphaMode() == EOutputAlphaMode::OpaqueOne,
        "SceneColor handoff freezes the RGBA16F linear Rec709 D65 contract");
    Record(Result, !Handoff.BindProducer(Handoff.GetResource()) &&
            !Handoff.MarkProduced(),
        "SceneColor handoff rejects duplicate producer and production transitions");

    FHDRSceneColorHandoff Consumed = Handoff;
    Record(Result, Consumed.MarkConsumed() &&
            Consumed.GetState() == EHDRSceneColorState::Consumed &&
            !Consumed.MarkConsumed(),
        "SceneColor handoff is consumed exactly once");

    FHDRSceneColorHandoffDesc InvalidDesc;
    InvalidDesc.SceneColorId = 1;
    InvalidDesc.Producer = EHDRSceneColorProducer::Deferred;
    InvalidDesc.ViewId = 2;
    InvalidDesc.FrameToken = 3;
    InvalidDesc.Width = 0;
    InvalidDesc.Height = 32;
    InvalidDesc.Format = ERHIFormat::R8G8B8A8_UNorm;
    const FHDRSceneColorHandoff Invalid =
        FHDRSceneColorHandoff::Declare(InvalidDesc);
    Record(Result, Invalid.GetState() == EHDRSceneColorState::Failed &&
            !Invalid.IsReadyForConsumption(),
        "SceneColor declaration fails closed on invalid extent and format metadata");
}

void TestDefaultPlanAndStageOrder(FRendererOutputTransformTestResult& Result)
{
    FRenderGraph Graph("DefaultPlan");
    const FHDRSceneColorHandoff Handoff = MakeProducedSceneColor(Graph);
    FOutputTransformSettings Settings;
    const FOutputTransformPrepareResult Prepared =
        FHDRPostProcessPipeline().Prepare(Handoff, Settings);
    Record(Result, Prepared.Succeeded() &&
            Prepared.Plan.ResolvedSettings.SDRToneMapVersion ==
                "Sdr.KhronosPbrNeutral.v1" &&
            Prepared.Plan.ResolvedSettings.OutputDeviceProfileId ==
                "Sdr.sRGB.v1" &&
            Prepared.Plan.PlanFingerprint.Len() == 64,
        "Default SDR preparation resolves and fingerprints explicit version identities");

    const auto& Stages = Prepared.Plan.Stages;
    Record(Result, Stages.size() == 5 &&
            Stages[0].Kind == EOutputTransformStageKind::SceneColorHandoff &&
            Stages[1].Kind == EOutputTransformStageKind::ManualExposure &&
            Stages[2].Kind == EOutputTransformStageKind::SDRToneMap &&
            Stages[3].Kind == EOutputTransformStageKind::OutputDeviceTransform &&
            Stages[4].Kind == EOutputTransformStageKind::Presentation,
        "Default SDR plan declares the only legal empty-insertion stage order");
    Record(Result, Prepared.Plan.FormalOutputId != 0 &&
            Prepared.Plan.SceneColor.GetFrameToken() ==
                Prepared.Plan.FrameToken &&
            Prepared.Plan.OutputDesc.Format == ERHIFormat::R8G8B8A8_UNorm &&
            Prepared.Plan.OutputDesc.ColorDomain ==
                ERenderGraphColorDomain::EncodedSrgb,
        "Plan owns one typed formal SDR output for the same frame token");

    FOutputTransformSettings InvalidSettings;
    InvalidSettings.bRequirePresentation = false;
    const FOutputTransformPrepareResult Invalid =
        FHDRPostProcessPipeline().Prepare(Handoff, InvalidSettings);
    Record(Result, !Invalid.Succeeded() && Invalid.Diagnostics.HasError(),
        "Preparation rejects a request without presentation or readback");
}

void TestHDRPlanAndAuthorityFingerprint(
    FRendererOutputTransformTestResult& Result)
{
    FRenderGraph Graph("HDRPlan");
    const FHDRSceneColorHandoff Handoff = MakeProducedSceneColor(Graph);

    FOutputTransformSettings PqSettings;
    PqSettings.DynamicRange = EOutputDynamicRange::HDR;
    PqSettings.OutputDeviceProfileId = "Hdr.PQ.Rec2020.1000.v1";
    PqSettings.PreferredNativeEncoding =
        ERHIPresentationNativeEncoding::Pq;
    const FOutputTransformPrepareResult Pq =
        FHDRPostProcessPipeline().Prepare(Handoff, PqSettings);
    Record(Result, Pq.Succeeded() && Pq.Plan.Stages.size() == 5 &&
            Pq.Plan.Stages[2].Kind ==
                EOutputTransformStageKind::HDRViewingTransform &&
            Pq.Plan.Stages[2].VersionId == GInitialHDRViewingVersion &&
            Pq.Plan.Stages[3].Kind ==
                EOutputTransformStageKind::OutputDeviceTransform &&
            Pq.Plan.OutputDesc.Format == ERHIFormat::R10G10B10A2_UNorm &&
            Pq.Plan.OutputDesc.ColorDomain ==
                ERenderGraphColorDomain::EncodedPqRec2020D65,
        "HDR plan selects the viewing transform without an SDR tone map");

    FOutputTransformSettings FirstEdr;
    FirstEdr.DynamicRange = EOutputDynamicRange::HDR;
    FirstEdr.OutputDeviceProfileId = "Hdr.Linear.1000.v1";
    FirstEdr.PreferredNativeEncoding =
        ERHIPresentationNativeEncoding::MetalEdr;
    FirstEdr.NativeReferenceWhiteNits = 100.0f;
    FOutputTransformSettings SecondEdr = FirstEdr;
    SecondEdr.NativeReferenceWhiteNits = 120.0f;
    const FOutputTransformPrepareResult First =
        FHDRPostProcessPipeline().Prepare(Handoff, FirstEdr);
    const FOutputTransformPrepareResult Second =
        FHDRPostProcessPipeline().Prepare(Handoff, SecondEdr);
    Record(Result, First.Succeeded() && Second.Succeeded() &&
            First.Plan.ResolvedSettings.PipelineKey !=
                Second.Plan.ResolvedSettings.PipelineKey &&
            First.Plan.PlanFingerprint != Second.Plan.PlanFingerprint,
        "plan fingerprint binds resolved native reference-white authority");
}

void TestSingleWriterAndNoPartialPublication(
    FRendererOutputTransformTestResult& Result)
{
    FRenderGraph Graph("SingleWriter");
    const FHDRSceneColorHandoff Handoff = MakeProducedSceneColor(Graph);
    const FOutputTransformPrepareResult Prepared =
        FHDRPostProcessPipeline().Prepare(Handoff, {});
    FOutputTransformGraphDeclaration Declaration =
        FHDRPostProcessPipeline().DeclareGraph(Graph, Prepared.Plan);
    Record(Result, Declaration.IsValid() &&
            Declaration.FormalWriterCount == 1 &&
            FHDRPostProcessPipeline().ValidateOutputGraph(
                Graph, Prepared.Plan, Declaration),
        "Output graph validates one and only one formal output writer");

    FRenderGraphPassDesc Duplicate = FRenderGraphPassDesc::Make(
        "DuplicateFormalWriter", ERenderGraphPassType::Graphics);
    Duplicate.Accesses.push_back({Declaration.FormalOutput,
        ERenderGraphAccessType::Write, ERenderGraphResourceState::Write});
    (void)Graph.CreateBuilder().AddPass(Duplicate);
    Record(Result, !FHDRPostProcessPipeline().ValidateOutputGraph(
            Graph, Prepared.Plan, Declaration),
        "Output graph rejects a second formal output writer before execution");

    FRenderGraph BindingGraph("BindingFailure");
    const FHDRSceneColorHandoff BindingHandoff =
        MakeProducedSceneColor(BindingGraph);
    const FOutputTransformPrepareResult BindingPrepared =
        FHDRPostProcessPipeline().Prepare(BindingHandoff, {});
    const FOutputTransformGraphDeclaration BindingDeclaration =
        FHDRPostProcessPipeline().DeclareGraph(
            BindingGraph, BindingPrepared.Plan);
    (void)BindingGraph.Compile();
    FOutputTransformExecutionBindings Bindings;
    const FOutputTransformExecutionResult BindingFailure =
        FOutputTransformExecutor().Execute(BindingPrepared.Plan,
            BindingGraph, BindingDeclaration, Bindings);
    Record(Result, !BindingFailure.Succeeded() &&
            !BindingFailure.bFormalOutputPublished &&
            BindingFailure.Diagnostics.GetFirstError() != nullptr,
        "Missing SceneColor binding publishes no partial formal output");

    FRenderGraph TerminalGraph("TerminalFailure");
    const FHDRSceneColorHandoff TerminalHandoff =
        MakeProducedSceneColor(TerminalGraph);
    const FOutputTransformPrepareResult TerminalPrepared =
        FHDRPostProcessPipeline().Prepare(TerminalHandoff, {});
    const FOutputTransformGraphDeclaration TerminalDeclaration =
        FHDRPostProcessPipeline().DeclareGraph(
            TerminalGraph, TerminalPrepared.Plan);
    (void)TerminalGraph.Compile();
    Bindings.SceneColorExternalToken = 91;
    Bindings.bFailPresentation = true;
    const FOutputTransformExecutionResult TerminalFailure =
        FOutputTransformExecutor().Execute(TerminalPrepared.Plan,
            TerminalGraph, TerminalDeclaration, Bindings);
    Record(Result, TerminalFailure.Result ==
            EOutputTransformResult::TerminalFailed &&
            !TerminalFailure.bFormalOutputPublished &&
            TerminalFailure.PublishedFormalOutputId == 0,
        "Presentation failure publishes neither identity nor partial output");
}

void TestDiagnosticBypassDoesNotMutateFormalOutput(
    FRendererOutputTransformTestResult& Result)
{
    FRenderGraph BaselineGraph("DiagnosticBaseline");
    FRenderGraph RawGraph("DiagnosticRawHdr");
    const FOutputTransformPrepareResult Baseline =
        FHDRPostProcessPipeline().Prepare(
            MakeProducedSceneColor(BaselineGraph), {});
    FOutputTransformSettings RawSettings;
    RawSettings.DiagnosticBypass.StageName = "ManualExposure";
    RawSettings.DiagnosticBypass.Mode =
        EOutputTransformDebugBypassMode::HDRPreservingReadback;
    const FOutputTransformPrepareResult Raw =
        FHDRPostProcessPipeline().Prepare(
            MakeProducedSceneColor(RawGraph), RawSettings);
    Record(Result, Baseline.Succeeded() && Raw.Succeeded() &&
            Raw.Plan.DiagnosticBypass.IsValid() &&
            Raw.Plan.DiagnosticBypass.SourceStageName == "ManualExposure" &&
            Raw.Plan.DiagnosticBypass.SourceDomain ==
                ERenderGraphColorDomain::SceneLinearRec709D65 &&
            Raw.Plan.DiagnosticBypass.bNonAuthoritative &&
            Raw.Plan.DiagnosticBypassRecord.IsValid() &&
            Raw.Plan.DiagnosticBypassRecord.bNonAuthoritative,
        "Named raw-HDR bypass preserves and records its source domain");
    Record(Result, Raw.Plan.FormalOutputId == Baseline.Plan.FormalOutputId &&
            Raw.Plan.OutputDesc.Format == Baseline.Plan.OutputDesc.Format &&
            Raw.Plan.OutputDesc.ColorDomain ==
                Baseline.Plan.OutputDesc.ColorDomain &&
            Raw.Plan.Stages.size() == Baseline.Plan.Stages.size() &&
            Raw.Plan.PlanFingerprint != Baseline.Plan.PlanFingerprint,
        "Diagnostic selection changes diagnostic identity but not formal-output policy");

    const FOutputTransformGraphDeclaration Declaration =
        FHDRPostProcessPipeline().DeclareGraph(RawGraph, Raw.Plan);
    Record(Result, Declaration.IsValid() &&
            Declaration.DiagnosticReadbackBuffer.IsValid() &&
            Declaration.DiagnosticReadbackPass.IsValid() &&
            Declaration.DiagnosticReadbackCopyCount == 1 &&
            Declaration.DiagnosticFullscreenPassCount == 0 &&
            Declaration.bDiagnosticOutputNonAuthoritative,
        "HDR-preserving debug inspection is a separate non-authoritative readback");
    (void)RawGraph.Compile();
    FOutputTransformExecutionBindings Bindings;
    Bindings.SceneColorExternalToken = 900;
    const FOutputTransformExecutionResult Execution =
        FOutputTransformExecutor().Execute(
            Raw.Plan, RawGraph, Declaration, Bindings);
    Record(Result, Execution.Succeeded() &&
            Execution.bDiagnosticBypassProduced &&
            Execution.DiagnosticBypass.IsValid() &&
            Execution.DiagnosticBypass.bNonAuthoritative &&
            Execution.DiagnosticGpuReadbackCopyCount == 1 &&
            Execution.DiagnosticCpuReadbackInitiationCount == 1 &&
            Execution.PublishedFormalOutputId == Baseline.Plan.FormalOutputId,
        "Execution reports diagnostic evidence without masquerading as formal output");
}

void TestSameFrameNativeTerminalSequence(
    FRendererOutputTransformTestResult& Result)
{
    const auto ExecuteWith = [](FOutputTerminalProbe& Probe,
        FOutputTransformExecutionResult& Out) {
        FRenderGraph Graph("NativeTerminalSequence");
        FOutputTransformSettings Settings;
        Settings.bRequireReadback = true;
        const auto Prepared = FHDRPostProcessPipeline().Prepare(
            MakeProducedSceneColor(Graph), Settings);
        const auto Declaration = FHDRPostProcessPipeline().DeclareGraph(
            Graph, Prepared.Plan);
        (void)Graph.Compile();
        FOutputTransformExecutionBindings Bindings;
        Bindings.SceneColorExternalToken = 6001;
        Bindings.bRequireNativeExecution = true;
        Bindings.NativeFrameExecutor = &Probe;
        Out = FOutputTransformExecutor().Execute(
            Prepared.Plan, Graph, Declaration, Bindings);
    };

    FOutputTerminalProbe Success;
    FOutputTransformExecutionResult SuccessResult;
    ExecuteWith(Success, SuccessResult);
    Record(Result, SuccessResult.Succeeded() && Success.bOrderValid &&
            SuccessResult.bNativeFrameAcquired &&
            SuccessResult.bNativeSubmitted &&
            SuccessResult.bNativeCompletionObserved &&
            SuccessResult.bNativeReadbackCompleted &&
            SuccessResult.bNativePresented &&
            SuccessResult.PresentationFrame.FrameToken ==
                SuccessResult.FrameToken &&
            SuccessResult.OutstandingTerminalOwnerCount == 0,
        "Native executor orders acquire graph submit completion readback present for one exact frame");

    FOutputTerminalProbe FailedSubmit(
        FOutputTerminalProbe::EFailure::Submit);
    FOutputTransformExecutionResult FailedSubmitResult;
    ExecuteWith(FailedSubmit, FailedSubmitResult);
    Record(Result, !FailedSubmitResult.Succeeded() &&
            FailedSubmitResult.Result ==
                EOutputTransformResult::TerminalFailed &&
            FailedSubmitResult.bNativeReleasedAfterFailure &&
            FailedSubmit.ReleaseCount == 1 &&
            FailedSubmitResult.OutstandingTerminalOwnerCount == 0,
        "First native terminal failure releases the acquired owner and publishes nothing");

    FOutputTerminalProbe Paused(
        FOutputTerminalProbe::EFailure::AcquirePaused);
    FOutputTransformExecutionResult PausedResult;
    ExecuteWith(Paused, PausedResult);
    Record(Result, !PausedResult.Succeeded() &&
            PausedResult.Result == EOutputTransformResult::Unsupported &&
            PausedResult.NativeResult == ERHIResult::NotReady &&
            Paused.GetOutstandingTerminalOwnerCount() == 0,
        "Zero-drawable acquire remains paused without manufacturing an output");

    FOutputTerminalProbe Mismatch(
        FOutputTerminalProbe::EFailure::Mismatch);
    FOutputTransformExecutionResult MismatchResult;
    ExecuteWith(Mismatch, MismatchResult);
    Record(Result, !MismatchResult.Succeeded() &&
            MismatchResult.Result == EOutputTransformResult::InvalidBinding &&
            MismatchResult.bNativeReleasedAfterFailure &&
            MismatchResult.OutstandingTerminalOwnerCount == 0,
        "Stale or mismatched native generation binding fails transactionally");
}

void TestBoundedVisualizationAndInvalidDebugSelection(
    FRendererOutputTransformTestResult& Result)
{
    FRenderGraph VisualizationGraph("DiagnosticVisualization");
    FOutputTransformSettings Settings;
    FPostProcessOperationDesc Pre;
    Pre.OperationId = "Pre.Debuggable";
    Pre.StrategyVersion = "Test.Debuggable.v1";
    Pre.InsertionPoint = EPostProcessInsertionPoint::PreTonemap;
    Pre.OrderKey = 1;
    Pre.InputDomain = ERenderGraphColorDomain::SceneLinearRec709D65;
    Pre.OutputDomain = ERenderGraphColorDomain::SceneLinearRec709D65;
    (void)Settings.PreTonemapOperations.Add(Pre);
    Settings.DiagnosticBypass.StageName = "Pre.Debuggable";
    Settings.DiagnosticBypass.Mode =
        EOutputTransformDebugBypassMode::BoundedVisualization;
    Settings.DiagnosticBypass.VisualizationMinimum = 0.25f;
    Settings.DiagnosticBypass.VisualizationMaximum = 4.0f;
    const FOutputTransformPrepareResult Prepared =
        FHDRPostProcessPipeline().Prepare(
            MakeProducedSceneColor(VisualizationGraph), Settings);
    const FOutputTransformGraphDeclaration Declaration =
        FHDRPostProcessPipeline().DeclareGraph(
            VisualizationGraph, Prepared.Plan);
    Record(Result, Prepared.Succeeded() && Declaration.IsValid() &&
            Declaration.DiagnosticOutput.IsValid() &&
            Declaration.DiagnosticVisualizationPass.IsValid() &&
            Declaration.DiagnosticReadbackPass.IsValid() &&
            Declaration.DiagnosticFullscreenPassCount == 1 &&
            Declaration.DiagnosticReadbackCopyCount == 1,
        "Bounded visualization is explicit and remains separate from formal presentation");

    FOutputTransformSettings Unknown;
    Unknown.DiagnosticBypass.StageName = "Pre.DoesNotExist";
    Unknown.DiagnosticBypass.Mode =
        EOutputTransformDebugBypassMode::HDRPreservingReadback;
    Record(Result, !FHDRPostProcessPipeline().Prepare(
            MakeProducedSceneColor(VisualizationGraph), Unknown).Succeeded(),
        "Unknown diagnostic stage fails before graph or native execution");

    FOutputTransformSettings Unbounded;
    Unbounded.DiagnosticBypass.StageName = "ManualExposure";
    Unbounded.DiagnosticBypass.Mode =
        EOutputTransformDebugBypassMode::BoundedVisualization;
    Unbounded.DiagnosticBypass.VisualizationMinimum = 1.0f;
    Unbounded.DiagnosticBypass.VisualizationMaximum = 1.0f;
    Record(Result, !FHDRPostProcessPipeline().Prepare(
            MakeProducedSceneColor(VisualizationGraph), Unbounded).Succeeded(),
        "Diagnostic visualization requires an explicit increasing finite range");
}

} // namespace

FRendererOutputTransformTestResult RunRendererOutputTransformTests()
{
    FRendererOutputTransformTestResult Result;
    TestHandoffStateAndMetadata(Result);
    TestDefaultPlanAndStageOrder(Result);
    TestHDRPlanAndAuthorityFingerprint(Result);
    TestSingleWriterAndNoPartialPublication(Result);
    TestSameFrameNativeTerminalSequence(Result);
    TestDiagnosticBypassDoesNotMutateFormalOutput(Result);
    TestBoundedVisualizationAndInvalidDebugSelection(Result);
    return Result;
}
