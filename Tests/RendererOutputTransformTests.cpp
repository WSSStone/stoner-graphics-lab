#include "RendererOutputTransformTests.h"

#include "Renderer/FHDRPostProcessPipeline.h"
#include "Renderer/FOutputTransformExecutor.h"
#include "Renderer/FRenderGraph.h"

#include <iostream>
#include <memory>

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

    ERHIResult AcquirePreview(const FOutputTransformPlan& Plan,
        FOutputTransformNativeFrameBinding& OutFrame) override
    {
        ++PreviewAcquireCount;
        const ERHIResult Result = Acquire(Plan, OutFrame);
        if (Result == ERHIResult::Success && bPreviewMetadataMismatch)
        {
            OutFrame.ResolvedState.ReferenceWhiteNits += 1.0f;
        }
        return Result;
    }

    ERHIResult SubmitPreview(const FOutputTransformPlan& Plan,
        const FOutputTransformNativeFrameBinding& Frame,
        bool& bOutSubmitted) override
    {
        (void)Plan;
        (void)Frame;
        ++PreviewSubmitCount;
        bOrderValid = bOrderValid && Step == 1 && RecordedEvents != 0;
        bOutSubmitted = bPreviewSubmitAcceptedOnFailure;
        if (PreviewSubmitResult == ERHIResult::Success)
        {
            Step = 2;
            bOutSubmitted = true;
        }
        const ERHIResult Result = PreviewSubmitResult;
        return Result;
    }

    ERHIResult PollPreview(const FOutputTransformPlan& Plan,
        const FOutputTransformNativeFrameBinding& Frame,
        bool& bOutCompleted) override
    {
        (void)Plan;
        (void)Frame;
        ++PreviewPollCount;
        if (bDelayPreviewCompletion && PreviewPollCount == 1)
        {
            bOutCompleted = false;
            return ERHIResult::NotReady;
        }
        Step = 3;
        const ERHIResult Result = PreviewPollResult;
        bOutCompleted = Result == ERHIResult::Success ||
            (Result == ERHIResult::Failed &&
                bPreviewPollCompletesFailure);
        return Result;
    }

    ERHIResult RetirePreview(const FOutputTransformPlan&,
        const FOutputTransformNativeFrameBinding&) noexcept override
    {
        ++PreviewRetireCount;
        if (PreviewRetireResult != ERHIResult::Success)
        {
            return PreviewRetireResult;
        }
        Owners = PreviewRetireRemainingOwners;
        return ERHIResult::Success;
    }

    ERHIResult ReleasePreviewAfterFailure(
        const FOutputTransformNativeFrameBinding& Frame) noexcept override
    {
        ++PreviewReleaseCount;
        if (PreviewReleaseResult != ERHIResult::Success)
        {
            return PreviewReleaseResult;
        }
        return ReleaseAfterFailure(Frame);
    }

    ERHIResult WaitForCompletion(const FOutputTransformPlan&,
        const FOutputTransformNativeFrameBinding&) override
    {
        bWaitCalled = true;
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
    Stoner::Core::uint32 PreviewAcquireCount = 0;
    Stoner::Core::uint32 PreviewSubmitCount = 0;
    Stoner::Core::uint32 PreviewPollCount = 0;
    Stoner::Core::uint32 PreviewRetireCount = 0;
    Stoner::Core::uint32 PreviewReleaseCount = 0;
    ERHIResult PreviewSubmitResult = ERHIResult::Success;
    ERHIResult PreviewPollResult = ERHIResult::Success;
    ERHIResult PreviewRetireResult = ERHIResult::Success;
    Stoner::Core::uint32 PreviewRetireRemainingOwners = 0;
    ERHIResult PreviewReleaseResult = ERHIResult::Success;
    bool bDelayPreviewCompletion = false;
    bool bPreviewMetadataMismatch = false;
    bool bPreviewSubmitAcceptedOnFailure = false;
    bool bPreviewPollCompletesFailure = false;
    bool bWaitCalled = false;
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
            Prepared.Plan.ExecutionPurpose ==
                EFrameExecutionPurpose::FormalValidation &&
            Prepared.Plan.ReadbackSelection ==
                EFrameReadbackSelection::Formal &&
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

    FOutputTransformPlan InvalidFormalSelection = Prepared.Plan;
    InvalidFormalSelection.ReadbackSelection = EFrameReadbackSelection::None;
    Record(Result, !InvalidFormalSelection.IsValid(),
        "Formal validation rejects an explicit no-readback selection");

    FOutputTransformPlan PreviewSelection = Prepared.Plan;
    PreviewSelection.ExecutionPurpose = EFrameExecutionPurpose::InteractivePreview;
    PreviewSelection.ReadbackSelection = EFrameReadbackSelection::None;
    Record(Result, PreviewSelection.IsValid() &&
            PreviewSelection.PlanFingerprint == Prepared.Plan.PlanFingerprint &&
            PreviewSelection.FormalOutputId == Prepared.Plan.FormalOutputId,
        "Interactive preview selection permits no readback without changing formal identity");
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

struct FPreviewGraphFixture
{
    FRenderGraph Graph{"PreviewGraph"};
    FOutputTransformPlan Plan;
    FOutputTransformGraphDeclaration Declaration;

    FPreviewGraphFixture()
    {
        const FOutputTransformPrepareResult Prepared =
            FHDRPostProcessPipeline().Prepare(MakeProducedSceneColor(Graph),
                {});
        Plan = Prepared.Plan;
        Plan.ExecutionPurpose = EFrameExecutionPurpose::InteractivePreview;
        Plan.ReadbackSelection = EFrameReadbackSelection::None;
        Declaration = FHDRPostProcessPipeline().DeclareGraph(Graph, Plan);
        (void)Graph.Compile();
    }
};

void TestAsynchronousPreviewLifecycle(
    FRendererOutputTransformTestResult& Result)
{
    FPreviewGraphFixture LegacyFixture;
    FOutputTerminalProbe Legacy;
    FOutputTransformExecutionBindings LegacyBindings;
    LegacyBindings.SceneColorExternalToken = 7001;
    LegacyBindings.NativeFrameExecutor = &Legacy;
    FOutputTransformPreviewTicket LegacyTicket;
    const FOutputTransformPreviewResult LegacyResult =
        FOutputTransformExecutor().RecordPreview(
            LegacyFixture.Plan, LegacyFixture.Graph, LegacyFixture.Declaration,
            LegacyBindings, LegacyTicket);
    Record(Result, LegacyResult.Result == EOutputTransformResult::Unsupported &&
            Legacy.PreviewAcquireCount == 0 && !LegacyTicket.IsValid(),
        "Preview rejects a raw formal executor before acquiring a native frame");

    FPreviewGraphFixture Fixture;
    Stoner::Core::TSharedPtr<FOutputTerminalProbe> Probe =
        Stoner::Core::MakeShared<FOutputTerminalProbe>();
    Probe->bDelayPreviewCompletion = true;
    FOutputTransformExecutionBindings Bindings;
    Bindings.SceneColorExternalToken = 7002;
    Bindings.PreviewFrameExecutor = Probe;
    FOutputTransformPreviewTicket Ticket;
    const FOutputTransformPreviewResult Recorded =
        FOutputTransformExecutor().RecordPreview(
            Fixture.Plan, Fixture.Graph, Fixture.Declaration, Bindings, Ticket);
    Record(Result, Recorded.Result == EOutputTransformResult::Success &&
            Recorded.State == EOutputTransformPreviewState::Recorded &&
            Recorded.Execution.GpuReadbackCopyCount == 0 &&
            Recorded.Execution.CpuReadbackInitiationCount == 0 &&
            !Recorded.Execution.Succeeded() && Ticket.IsValid(),
        "Preview recording accepts a zero-readback graph without formal publication");

    FPreviewGraphFixture MetadataFixture;
    Stoner::Core::TSharedPtr<FOutputTerminalProbe> MetadataProbe =
        Stoner::Core::MakeShared<FOutputTerminalProbe>();
    MetadataProbe->bPreviewMetadataMismatch = true;
    FOutputTransformExecutionBindings MetadataBindings;
    MetadataBindings.SceneColorExternalToken = 7005;
    MetadataBindings.PreviewFrameExecutor = MetadataProbe;
    FOutputTransformPreviewTicket MetadataTicket;
    const FOutputTransformPreviewResult MetadataRejected =
        FOutputTransformExecutor().RecordPreview(
            MetadataFixture.Plan, MetadataFixture.Graph,
            MetadataFixture.Declaration, MetadataBindings, MetadataTicket);
    Record(Result, MetadataRejected.Result == EOutputTransformResult::InvalidBinding &&
            MetadataRejected.bRetired && !MetadataTicket.IsValid() &&
            MetadataProbe->PreviewReleaseCount == 1,
        "Preview rejects a valid native target with mismatched reference white metadata");

    const FOutputTransformExecutionResult FormalRejected =
        FOutputTransformExecutor().Execute(
            Fixture.Plan, Fixture.Graph, Fixture.Declaration, Bindings);
    Record(Result, FormalRejected.Result == EOutputTransformResult::InvalidBinding &&
            !FormalRejected.Succeeded() && !FormalRejected.bFormalOutputPublished,
        "Formal execution rejects a preview plan before native work or publication");

    const FOutputTransformPreviewResult Queued =
        FOutputTransformExecutor().SubmitPreview(Ticket);
    Record(Result, Queued.Accepted() &&
            Queued.State == EOutputTransformPreviewState::Queued &&
            !Queued.bRenderCompleted && !Queued.bFormalOutputPublished &&
            !Probe->bWaitCalled && Probe->PreviewSubmitCount == 1,
        "Preview submission is queued without calling the synchronous wait seam");

    const FOutputTransformPreviewResult EarlyRetire =
        FOutputTransformExecutor().RetirePreview(Ticket);
    Record(Result, EarlyRetire.State == EOutputTransformPreviewState::Queued &&
            !EarlyRetire.bRetired && EarlyRetire.NativeResult == ERHIResult::NotReady &&
            EarlyRetire.OutstandingOwnerCount == 1 &&
            Probe->PreviewRetireCount == 0,
        "Queued preview retains its owner until delayed render completion");

    const FOutputTransformPreviewResult Pending =
        FOutputTransformExecutor().PollPreview(Ticket);
    Record(Result, Pending.State == EOutputTransformPreviewState::Queued &&
            !Pending.Completed() && Pending.NativeResult == ERHIResult::NotReady &&
            Pending.OutstandingOwnerCount == 1,
        "Preview polling reports delayed completion without blocking");

    const FOutputTransformPreviewResult Completed =
        FOutputTransformExecutor().PollPreview(Ticket);
    Record(Result, Completed.Completed() &&
            Completed.State == EOutputTransformPreviewState::RenderCompleted &&
            Completed.Execution.bNativeCompletionObserved &&
            !Completed.Execution.Succeeded() &&
            !Completed.bFormalOutputPublished && Probe->bWaitCalled == false,
        "Preview completion is distinct from formal Succeeded and publication");

    FOutputTransformPreviewTicket TicketCopy = Ticket;
    std::weak_ptr<FOutputTerminalProbe> ProbeWeak = Probe;
    const FOutputTransformPreviewResult Retired =
        FOutputTransformExecutor().RetirePreview(TicketCopy);
    Record(Result, Retired.Retired() &&
            Retired.State == EOutputTransformPreviewState::Retired &&
            Retired.OutstandingOwnerCount == 0 &&
            Probe->PreviewRetireCount == 1 && !Ticket.IsValid(),
        "Retiring one ticket copy retires the shared native owner exactly once");
    const FOutputTransformPreviewResult Repeated =
        FOutputTransformExecutor().RetirePreview(Ticket);
    Record(Result, Repeated.Result == EOutputTransformResult::InvalidBinding &&
            Repeated.NativeResult == ERHIResult::InvalidState,
        "Repeated or expired preview tickets are rejected");
    Bindings.PreviewFrameExecutor.reset();
    Probe.reset();
    Record(Result, ProbeWeak.expired(),
        "Retired ticket state releases its shared native executor ownership");

    FPreviewGraphFixture UnsupportedFixture;
    const Stoner::Core::TSharedPtr<FOutputTerminalProbe> UnsupportedProbe =
        Stoner::Core::MakeShared<FOutputTerminalProbe>();
    UnsupportedProbe->PreviewSubmitResult = ERHIResult::Unsupported;
    FOutputTransformExecutionBindings UnsupportedBindings;
    UnsupportedBindings.SceneColorExternalToken = 7003;
    UnsupportedBindings.PreviewFrameExecutor = UnsupportedProbe;
    FOutputTransformPreviewTicket UnsupportedTicket;
    (void)FOutputTransformExecutor().RecordPreview(
        UnsupportedFixture.Plan, UnsupportedFixture.Graph,
        UnsupportedFixture.Declaration, UnsupportedBindings, UnsupportedTicket);
    const FOutputTransformPreviewResult UnsupportedSubmit =
        FOutputTransformExecutor().SubmitPreview(UnsupportedTicket);
    Record(Result, UnsupportedSubmit.Result == EOutputTransformResult::Unsupported &&
            !UnsupportedSubmit.bQueued && UnsupportedSubmit.bRetired &&
            UnsupportedSubmit.OutstandingOwnerCount == 0 &&
            UnsupportedProbe->PreviewReleaseCount == 1,
        "Unsupported preview submission does not queue and releases only the unsubmitted owner");

    FPreviewGraphFixture FailureFixture;
    const Stoner::Core::TSharedPtr<FOutputTerminalProbe> FailureProbe =
        Stoner::Core::MakeShared<FOutputTerminalProbe>();
    FailureProbe->PreviewSubmitResult = ERHIResult::Failed;
    FailureProbe->bPreviewSubmitAcceptedOnFailure = true;
    FailureProbe->PreviewPollResult = ERHIResult::Failed;
    FOutputTransformExecutionBindings FailureBindings;
    FailureBindings.SceneColorExternalToken = 7004;
    FailureBindings.PreviewFrameExecutor = FailureProbe;
    FOutputTransformPreviewTicket FailureTicket;
    (void)FOutputTransformExecutor().RecordPreview(
        FailureFixture.Plan, FailureFixture.Graph, FailureFixture.Declaration,
        FailureBindings, FailureTicket);
    const FOutputTransformPreviewResult FailedSubmission =
        FOutputTransformExecutor().SubmitPreview(FailureTicket);
    const FOutputTransformPreviewResult FailedCompletion =
        FOutputTransformExecutor().PollPreview(FailureTicket);
    const FOutputTransformPreviewResult FailedRetirePending =
        FOutputTransformExecutor().RetirePreview(FailureTicket);
    FailureProbe->bPreviewPollCompletesFailure = true;
    const FOutputTransformPreviewResult FailedCompletionObserved =
        FOutputTransformExecutor().PollPreview(FailureTicket);
    const FOutputTransformPreviewResult FailedRetire =
        FOutputTransformExecutor().RetirePreview(FailureTicket);
    Record(Result, FailedSubmission.bQueued &&
            FailedSubmission.Execution.bNativeSubmitted &&
            FailedCompletion.Result == EOutputTransformResult::TerminalFailed &&
            !FailedCompletion.bRenderCompleted &&
            FailedRetirePending.OutstandingOwnerCount == 1 &&
            !FailedRetirePending.bRetired &&
            FailedCompletionObserved.Result == EOutputTransformResult::TerminalFailed &&
            FailedCompletionObserved.bRenderCompleted &&
            FailedRetire.bRetired && FailedRetire.OutstandingOwnerCount == 0 &&
            FailedRetire.Result == EOutputTransformResult::TerminalFailed &&
            FailedRetire.Execution.Result == EOutputTransformResult::TerminalFailed &&
            FailedRetire.NativeResult == ERHIResult::Failed,
        "Failed completion requires independent proof, then retires while preserving first failure identity");

    FPreviewGraphFixture AdmissionFixture;
    const Stoner::Core::TSharedPtr<FOutputTerminalProbe> AdmissionProbe =
        Stoner::Core::MakeShared<FOutputTerminalProbe>();
    AdmissionProbe->PreviewSubmitResult = ERHIResult::NotReady;
    FOutputTransformExecutionBindings AdmissionBindings;
    AdmissionBindings.SceneColorExternalToken = 7006;
    AdmissionBindings.PreviewFrameExecutor = AdmissionProbe;
    FOutputTransformPreviewTicket AdmissionTicket;
    (void)FOutputTransformExecutor().RecordPreview(
        AdmissionFixture.Plan, AdmissionFixture.Graph,
        AdmissionFixture.Declaration, AdmissionBindings, AdmissionTicket);
    const FOutputTransformPreviewResult AdmissionPending =
        FOutputTransformExecutor().SubmitPreview(AdmissionTicket);
    AdmissionProbe->PreviewSubmitResult = ERHIResult::Success;
    const FOutputTransformPreviewResult AdmissionQueued =
        FOutputTransformExecutor().SubmitPreview(AdmissionTicket);
    (void)FOutputTransformExecutor().PollPreview(AdmissionTicket);
    (void)FOutputTransformExecutor().RetirePreview(AdmissionTicket);
    Record(Result, AdmissionPending.State == EOutputTransformPreviewState::Recorded &&
            !AdmissionPending.bQueued && AdmissionPending.OutstandingOwnerCount == 1 &&
            AdmissionQueued.Accepted() && AdmissionProbe->PreviewSubmitCount == 2,
        "Preview admission NotReady remains recorded and can be retried without reacquiring");

    FPreviewGraphFixture CancelFixture;
    const Stoner::Core::TSharedPtr<FOutputTerminalProbe> CancelProbe =
        Stoner::Core::MakeShared<FOutputTerminalProbe>();
    CancelProbe->PreviewSubmitResult = ERHIResult::Unsupported;
    CancelProbe->PreviewReleaseResult = ERHIResult::NotReady;
    FOutputTransformExecutionBindings CancelBindings;
    CancelBindings.SceneColorExternalToken = 7007;
    CancelBindings.PreviewFrameExecutor = CancelProbe;
    FOutputTransformPreviewTicket CancelTicket;
    (void)FOutputTransformExecutor().RecordPreview(
        CancelFixture.Plan, CancelFixture.Graph,
        CancelFixture.Declaration, CancelBindings, CancelTicket);
    const FOutputTransformPreviewResult CancelPending =
        FOutputTransformExecutor().SubmitPreview(CancelTicket);
    const FOutputTransformPreviewResult CancelRetryPending =
        FOutputTransformExecutor().RetirePreview(CancelTicket);
    CancelProbe->PreviewReleaseResult = ERHIResult::Success;
    const FOutputTransformPreviewResult Cancelled =
        FOutputTransformExecutor().RetirePreview(CancelTicket);
    Record(Result, CancelPending.Result == EOutputTransformResult::Unsupported &&
            !CancelPending.bRetired && CancelPending.OutstandingOwnerCount == 1 &&
            !CancelRetryPending.bRetired && CancelRetryPending.OutstandingOwnerCount == 1 &&
            Cancelled.bRetired && Cancelled.OutstandingOwnerCount == 0 &&
            CancelProbe->PreviewReleaseCount == 3,
        "Unsubmitted preview cancellation retries after a nonblocking NotReady release");

    FPreviewGraphFixture PresentationFixture;
    const Stoner::Core::TSharedPtr<FOutputTerminalProbe> PresentationProbe =
        Stoner::Core::MakeShared<FOutputTerminalProbe>();
    PresentationProbe->PreviewRetireRemainingOwners = 1;
    FOutputTransformExecutionBindings PresentationBindings;
    PresentationBindings.SceneColorExternalToken = 7008;
    PresentationBindings.PreviewFrameExecutor = PresentationProbe;
    FOutputTransformPreviewTicket PresentationTicket;
    (void)FOutputTransformExecutor().RecordPreview(
        PresentationFixture.Plan, PresentationFixture.Graph,
        PresentationFixture.Declaration, PresentationBindings,
        PresentationTicket);
    (void)FOutputTransformExecutor().SubmitPreview(PresentationTicket);
    (void)FOutputTransformExecutor().PollPreview(PresentationTicket);
    const FOutputTransformPreviewResult PresentationRetired =
        FOutputTransformExecutor().RetirePreview(PresentationTicket);
    Record(Result, PresentationRetired.bRetired &&
            PresentationRetired.OutstandingOwnerCount == 1 &&
            !PresentationTicket.IsValid() && PresentationProbe->PreviewRetireCount == 1,
        "Preview retire releases the render ticket while retaining independent presentation-owner diagnostics");

    FPreviewGraphFixture RetireRetryFixture;
    const auto RetireRetryProbe = Stoner::Core::MakeShared<FOutputTerminalProbe>();
    FOutputTransformExecutionBindings RetireRetryBindings;
    RetireRetryBindings.SceneColorExternalToken = 7009;
    RetireRetryBindings.PreviewFrameExecutor = RetireRetryProbe;
    FOutputTransformPreviewTicket RetireRetryTicket;
    (void)FOutputTransformExecutor().RecordPreview(RetireRetryFixture.Plan,
        RetireRetryFixture.Graph, RetireRetryFixture.Declaration,
        RetireRetryBindings, RetireRetryTicket);
    (void)FOutputTransformExecutor().SubmitPreview(RetireRetryTicket);
    (void)FOutputTransformExecutor().PollPreview(RetireRetryTicket);
    RetireRetryProbe->PreviewRetireResult = ERHIResult::NotReady;
    const auto RetirePending = FOutputTransformExecutor().RetirePreview(RetireRetryTicket);
    RetireRetryProbe->PreviewRetireResult = ERHIResult::Success;
    const auto RetireComplete = FOutputTransformExecutor().RetirePreview(RetireRetryTicket);
    Record(Result, !RetirePending.bRetired && RetirePending.bRenderCompleted &&
            RetirePending.NativeResult == ERHIResult::NotReady &&
            RetirePending.OutstandingOwnerCount == 1 &&
            RetireComplete.bRetired &&
            RetireComplete.Result == EOutputTransformResult::Success &&
            RetireComplete.NativeResult == ERHIResult::Success,
        "Native retire NotReady retains the completed ticket and does not poison a successful retry");

    FPreviewGraphFixture ReleaseFailureFixture;
    const auto ReleaseFailureProbe = Stoner::Core::MakeShared<FOutputTerminalProbe>();
    FOutputTransformExecutionBindings ReleaseFailureBindings;
    ReleaseFailureBindings.SceneColorExternalToken = 7010;
    ReleaseFailureBindings.PreviewFrameExecutor = ReleaseFailureProbe;
    FOutputTransformPreviewTicket ReleaseFailureTicket;
    (void)FOutputTransformExecutor().RecordPreview(ReleaseFailureFixture.Plan,
        ReleaseFailureFixture.Graph, ReleaseFailureFixture.Declaration,
        ReleaseFailureBindings, ReleaseFailureTicket);
    ReleaseFailureProbe->PreviewReleaseResult = ERHIResult::Failed;
    const auto ReleaseFailed = FOutputTransformExecutor().RetirePreview(ReleaseFailureTicket);
    ReleaseFailureProbe->PreviewReleaseResult = ERHIResult::Success;
    const auto ReleaseRecovered = FOutputTransformExecutor().RetirePreview(ReleaseFailureTicket);
    Record(Result, !ReleaseFailed.bRetired && ReleaseFailed.OutstandingOwnerCount == 1 &&
            ReleaseFailed.Result == EOutputTransformResult::TerminalFailed &&
            ReleaseRecovered.bRetired &&
            ReleaseRecovered.Result == EOutputTransformResult::TerminalFailed &&
            ReleaseRecovered.NativeResult == ERHIResult::Failed,
        "Successful cancellation retry preserves the earlier native cleanup failure");

    FPreviewGraphFixture PendingAcquireFixture;
    const auto PendingAcquireProbe = Stoner::Core::MakeShared<FOutputTerminalProbe>(
        FOutputTerminalProbe::EFailure::AcquirePaused);
    PendingAcquireProbe->Owners = 1;
    FOutputTransformExecutionBindings PendingAcquireBindings;
    PendingAcquireBindings.SceneColorExternalToken = 7011;
    PendingAcquireBindings.PreviewFrameExecutor = PendingAcquireProbe;
    FOutputTransformPreviewTicket PendingAcquireTicket;
    const auto PendingAcquire = FOutputTransformExecutor().RecordPreview(
        PendingAcquireFixture.Plan, PendingAcquireFixture.Graph,
        PendingAcquireFixture.Declaration, PendingAcquireBindings, PendingAcquireTicket);
    Record(Result, !PendingAcquireTicket.IsValid() &&
            PendingAcquire.NativeResult == ERHIResult::NotReady &&
            PendingAcquire.FrameToken == PendingAcquireFixture.Plan.FrameToken &&
            PendingAcquire.Execution.FrameToken == PendingAcquire.FrameToken &&
            PendingAcquire.OutstandingOwnerCount == 1 &&
            PendingAcquireProbe->PreviewReleaseCount == 0,
        "Pending acquire retains frame attribution and independently owned native acquisition diagnostics");
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
    TestAsynchronousPreviewLifecycle(Result);
    return Result;
}
