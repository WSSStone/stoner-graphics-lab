#include "RendererPostProcessGraphTests.h"

#include "Renderer/FHDRPostProcessPipeline.h"
#include "Renderer/FOutputTransformExecutor.h"
#include "Renderer/FRenderGraph.h"

#include <iostream>
#include <string_view>

namespace
{

using namespace Stoner::Renderer;
using namespace Stoner::RHI;

void Record(FRendererPostProcessGraphTestResult& Result, bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FHDRSceneColorHandoff AddSceneColor(FRenderGraph& Graph)
{
    FRenderGraphResourceDesc Desc = FRenderGraphResourceDesc::TypedTexture2D(
        "SceneColor", 128, 72, ERHIFormat::R16G16B16A16_Float,
        ERHISampleCount::One,
        ERHITextureUsage::ColorAttachment | ERHITextureUsage::Sampled,
        ERenderGraphColorDomain::SceneLinearRec709D65);
    Desc.Ownership = ERenderGraphResourceOwnership::Imported;
    Desc.InitialState = ERenderGraphResourceState::External;
    const FRenderGraphResourceHandle Resource =
        Graph.CreateBuilder().ImportResource(Desc);
    FHDRSceneColorHandoffDesc HandoffDesc;
    HandoffDesc.SceneColorId = 400;
    HandoffDesc.ViewId = 40;
    HandoffDesc.FrameToken = 41;
    HandoffDesc.Width = 128;
    HandoffDesc.Height = 72;
    FHDRSceneColorHandoff Handoff =
        FHDRSceneColorHandoff::Declare(HandoffDesc);
    (void)Handoff.BindProducer(Resource);
    (void)Handoff.MarkProduced();
    return Handoff;
}

FPostProcessOperationDesc MakeInsertion(const char* Id,
    EPostProcessInsertionPoint Point, Stoner::Core::int32 Order)
{
    FPostProcessOperationDesc Operation;
    Operation.OperationId = Id;
    Operation.StrategyVersion = "Test.Insertion.v1";
    Operation.InsertionPoint = Point;
    Operation.OrderKey = Order;
    Operation.InputDomain = Point == EPostProcessInsertionPoint::PreTonemap
        ? ERenderGraphColorDomain::SceneLinearRec709D65
        : ERenderGraphColorDomain::DisplayLinearRec709D65;
    Operation.OutputDomain = Operation.InputDomain;
    return Operation;
}

void TestTypedEmptyInsertionSubgraph(
    FRendererPostProcessGraphTestResult& Result)
{
    FRenderGraph Graph("TypedOutputSubgraph");
    const FHDRSceneColorHandoff Handoff = AddSceneColor(Graph);
    const FOutputTransformPrepareResult Prepared =
        FHDRPostProcessPipeline().Prepare(Handoff, {});
    const FOutputTransformGraphDeclaration Declaration =
        FHDRPostProcessPipeline().DeclareGraph(Graph, Prepared.Plan);
    Record(Result, Declaration.IsValid() && Graph.GetResources().size() == 4 &&
            Graph.GetPasses().size() == 4 && Graph.GetOutputs().size() == 1,
        "Empty insertion output subgraph declares four typed images and one formal output");
    Record(Result, Declaration.FullscreenPassCount == 3 &&
            Declaration.FullImageVisitCount == 3 &&
            Declaration.GpuReadbackCopyCount == 0 &&
            Declaration.CpuReadbackInitiationCount == 0,
        "Empty insertion path is bounded to three fullscreen visits and no readback");

    const FRenderGraphResourceRecord* Exposure =
        Graph.FindResource(Declaration.ExposedSceneColor);
    const FRenderGraphResourceRecord* ToneMapped =
        Graph.FindResource(Declaration.ToneOrViewedColor);
    const FRenderGraphResourceRecord* Formal =
        Graph.FindResource(Declaration.FormalOutput);
    Record(Result, Exposure && ToneMapped && Formal &&
            Exposure->Desc.Texture.ColorDomain ==
                ERenderGraphColorDomain::SceneLinearRec709D65 &&
            ToneMapped->Desc.Texture.ColorDomain ==
                ERenderGraphColorDomain::DisplayLinearRec709D65 &&
            Formal->Desc.Texture.ColorDomain ==
                ERenderGraphColorDomain::EncodedSrgb,
        "Output resources preserve scene-linear display-linear and encoded domains");

    Record(Result, Graph.Compile() == ERenderGraphResult::Success &&
            Graph.GetCompiledGraph().ScheduledPasses.size() == 4,
        "Typed output subgraph compiles without culling presentation dependencies");
    Stoner::Core::TArray<Stoner::Core::FString> ScheduledNames;
    (void)Graph.GetCompiledGraph().VisitSchedule(
        [&Graph, &ScheduledNames](const FRenderGraphScheduleEvent& Event) {
            if (Event.Kind == ERenderGraphScheduleEventKind::Pass)
            {
                ScheduledNames.push_back(
                    Graph.GetPasses()[Event.PassIndex].Desc.Name);
            }
            return ERenderGraphResult::Success;
        });
    Record(Result, ScheduledNames.size() == 4 &&
            ScheduledNames[0] == "ManualExposure" &&
            ScheduledNames[1] == "SDRToneMap" &&
            ScheduledNames[2] == "OutputDeviceTransform" &&
            ScheduledNames[3] == "Presentation",
        "Compiled schedule preserves exposure tone output-device and presentation order");
}

void TestOptionalReadbackAndPresentationOnlyExecution(
    FRendererPostProcessGraphTestResult& Result)
{
    FRenderGraph Graph("ReadbackOutputSubgraph");
    const FHDRSceneColorHandoff Handoff = AddSceneColor(Graph);
    FOutputTransformSettings Settings;
    Settings.bRequireReadback = true;
    const FOutputTransformPrepareResult Prepared =
        FHDRPostProcessPipeline().Prepare(Handoff, Settings);
    const FOutputTransformGraphDeclaration Declaration =
        FHDRPostProcessPipeline().DeclareGraph(Graph, Prepared.Plan);
    Record(Result, Declaration.IsValid() &&
            Declaration.GpuReadbackCopyCount == 1 &&
            Declaration.ReadbackPass.IsValid() &&
            Declaration.PresentationPass.IsValid() &&
            Graph.GetResources().size() == 5 &&
            Graph.GetPasses().size() == 5,
        "Readback plus presentation adds one exact GPU copy and one bounded buffer");
    const FRenderGraphPassRecord* Readback =
        Graph.FindPass(Declaration.ReadbackPass);
    const FRenderGraphPassRecord* Presentation =
        Graph.FindPass(Declaration.PresentationPass);
    Record(Result, Readback && Presentation &&
            Readback->Desc.ExternalSideEffect ==
                ERenderGraphExternalSideEffect::Readback &&
            Presentation->Desc.ExternalSideEffect ==
                ERenderGraphExternalSideEffect::Presentation,
        "Readback and presentation are explicit externally observed graph effects");

    FRenderGraph PresentationGraph("PresentationOnly");
    const FHDRSceneColorHandoff PresentationHandoff =
        AddSceneColor(PresentationGraph);
    const FOutputTransformPrepareResult PresentationPrepared =
        FHDRPostProcessPipeline().Prepare(PresentationHandoff, {});
    const FOutputTransformGraphDeclaration PresentationDeclaration =
        FHDRPostProcessPipeline().DeclareGraph(
            PresentationGraph, PresentationPrepared.Plan);
    (void)PresentationGraph.Compile();
    FOutputTransformExecutionBindings Bindings;
    Bindings.SceneColorExternalToken = 700;
    const FOutputTransformExecutionResult Execution =
        FOutputTransformExecutor().Execute(PresentationPrepared.Plan,
            PresentationGraph, PresentationDeclaration, Bindings);
    Record(Result, Execution.Succeeded() &&
            Execution.bFormalOutputPublished &&
            Execution.CpuReadbackInitiationCount == 0 &&
            Execution.ExecutedFullscreenPassCount == 3 &&
            Execution.ExecutedFullImageVisitCount == 3,
        "Presentation-only execution publishes once without initiating CPU readback");
}

void TestHDRViewingGraphAndExactReadbackFootprint(
    FRendererPostProcessGraphTestResult& Result)
{
    FRenderGraph Graph("HDRViewingOutputSubgraph");
    const FHDRSceneColorHandoff Handoff = AddSceneColor(Graph);
    FOutputTransformSettings Settings;
    Settings.DynamicRange = EOutputDynamicRange::HDR;
    Settings.OutputDeviceProfileId = "Hdr.Linear.1000.v1";
    Settings.PreferredNativeEncoding =
        ERHIPresentationNativeEncoding::ScRgb80;
    Settings.bRequireReadback = true;
    const FOutputTransformPrepareResult Prepared =
        FHDRPostProcessPipeline().Prepare(Handoff, Settings);
    const FOutputTransformGraphDeclaration Declaration =
        FHDRPostProcessPipeline().DeclareGraph(Graph, Prepared.Plan);
    const FRenderGraphResourceRecord* Readback =
        Graph.FindResource(Declaration.ReadbackBuffer);
    Record(Result, Declaration.IsValid() &&
            Declaration.OrderedPasses.size() == 5 &&
            Graph.FindPass(Declaration.OrderedPasses[1]) &&
            Graph.FindPass(Declaration.OrderedPasses[1])->Desc.Name ==
                "HDRViewingTransform",
        "HDR output graph selects the viewing-transform pass");
    Record(Result, Readback &&
            Readback->Desc.SizeInBytes == 128U * 72U * 8U,
        "FP16 formal readback uses the exact output-format footprint");
}

void TestOrderedInsertionPlacementAndResources(
    FRendererPostProcessGraphTestResult& Result)
{
    FRenderGraph Graph("InsertedOutputSubgraph");
    const FHDRSceneColorHandoff Handoff = AddSceneColor(Graph);
    FOutputTransformSettings Settings;
    FPostProcessOperationDesc PreLater = MakeInsertion(
        "Pre.ColorAdjust", EPostProcessInsertionPoint::PreTonemap, 20);
    PreLater.DependsOn = {"Pre.Denoise"};
    PreLater.Reads = {"InputColor", "DenoisedMoments"};
    FPostProcessOperationDesc PreEarlier = MakeInsertion(
        "Pre.Denoise", EPostProcessInsertionPoint::PreTonemap, 10);
    PreEarlier.Writes = {"OutputColor", "DenoisedMoments"};
    (void)Settings.PreTonemapOperations.Add(PreLater);
    (void)Settings.PreTonemapOperations.Add(PreEarlier);
    (void)Settings.PostTonemapOperations.Add(MakeInsertion(
        "Post.DisplayFilter", EPostProcessInsertionPoint::PostTonemap, 5));

    const FOutputTransformPrepareResult Prepared =
        FHDRPostProcessPipeline().Prepare(Handoff, Settings);
    Record(Result, Prepared.Succeeded() && Prepared.Plan.Stages.size() == 8 &&
            Prepared.Plan.Stages[2].Name == "Pre.Denoise" &&
            Prepared.Plan.Stages[3].Name == "Pre.ColorAdjust" &&
            Prepared.Plan.Stages[4].Kind ==
                EOutputTransformStageKind::SDRToneMap &&
            Prepared.Plan.Stages[5].Name == "Post.DisplayFilter" &&
            Prepared.Plan.Stages[6].Kind ==
                EOutputTransformStageKind::OutputDeviceTransform &&
            Prepared.Plan.InsertionDiagnostics.size() == 3 &&
            Prepared.Plan.InsertionDiagnostics[0].IsValid() &&
            Prepared.Plan.InsertionDiagnostics[0].OrderKey == 10 &&
            Prepared.Plan.InsertionDiagnostics[2].InsertionPoint ==
                "PostTonemap",
        "Prepared plan places pre and post composites around the tone-map stage");

    const FOutputTransformGraphDeclaration Declaration =
        FHDRPostProcessPipeline().DeclareGraph(Graph, Prepared.Plan);
    Record(Result, Declaration.IsValid() &&
            Declaration.PreTonemapOutputs.size() == 2 &&
            Declaration.PostTonemapOutputs.size() == 1 &&
            Declaration.InsertionPasses.size() == 3 &&
            Declaration.InsertionResources.size() == 4 &&
            Declaration.FullscreenPassCount == 6 &&
            Declaration.FullImageVisitCount == 6,
        "Graph declares chained insertion outputs and the auxiliary hazard resource");

    Record(Result, Graph.Compile() == ERenderGraphResult::Success,
        "Inserted output subgraph compiles with explicit resources and dependencies");
    Stoner::Core::TArray<Stoner::Core::FString> Names;
    (void)Graph.GetCompiledGraph().VisitSchedule(
        [&Graph, &Names](const FRenderGraphScheduleEvent& Event)
        {
            if (Event.Kind == ERenderGraphScheduleEventKind::Pass)
                Names.push_back(Graph.GetPasses()[Event.PassIndex].Desc.Name);
            return ERenderGraphResult::Success;
        });
    Record(Result, Names.size() == 7 &&
            Names[0] == "ManualExposure" &&
            Names[1] == "Pre.Denoise" &&
            Names[2] == "Pre.ColorAdjust" &&
            Names[3] == "SDRToneMap" &&
            Names[4] == "Post.DisplayFilter" &&
            Names[5] == "OutputDeviceTransform" &&
            Names[6] == "Presentation",
        "Compiled graph preserves the canonical insertion placement exactly");
}

void TestInsertionHazardExternalOutputAndEmptyEquivalence(
    FRendererPostProcessGraphTestResult& Result)
{
    FRenderGraph HazardGraph("InsertionHazard");
    const FHDRSceneColorHandoff HazardHandoff = AddSceneColor(HazardGraph);
    FOutputTransformSettings HazardSettings;
    FPostProcessOperationDesc Hazard = MakeInsertion(
        "Pre.Hazard", EPostProcessInsertionPoint::PreTonemap, 1);
    Hazard.Reads = {"InputColor", "NeverWritten"};
    (void)HazardSettings.PreTonemapOperations.Add(Hazard);
    Record(Result, !FHDRPostProcessPipeline().Prepare(
            HazardHandoff, HazardSettings).Succeeded(),
        "Read-before-write insertion hazard fails before graph declaration");

    FOutputTransformSettings ExternalSettings;
    FPostProcessOperationDesc External = MakeInsertion(
        "Post.External", EPostProcessInsertionPoint::PostTonemap, 1);
    External.bExternalOutput = true;
    (void)ExternalSettings.PostTonemapOperations.Add(External);
    Record(Result, !FHDRPostProcessPipeline().Prepare(
            HazardHandoff, ExternalSettings).Succeeded(),
        "Insertion cannot create a competing externally observed output");

    FRenderGraph FirstGraph("EmptyInsertionFirst");
    FRenderGraph SecondGraph("EmptyInsertionSecond");
    const FOutputTransformPrepareResult First =
        FHDRPostProcessPipeline().Prepare(AddSceneColor(FirstGraph), {});
    FOutputTransformSettings ExplicitEmpty;
    const FOutputTransformPrepareResult Second =
        FHDRPostProcessPipeline().Prepare(
            AddSceneColor(SecondGraph), ExplicitEmpty);
    Record(Result, First.Succeeded() && Second.Succeeded() &&
            First.Plan.PlanFingerprint == Second.Plan.PlanFingerprint &&
            First.Plan.FormalOutputId == Second.Plan.FormalOutputId &&
            First.Plan.Stages.size() == Second.Plan.Stages.size(),
        "Canonical empty composites preserve the effective formal plan");
}

} // namespace

FRendererPostProcessGraphTestResult RunRendererPostProcessGraphTests()
{
    FRendererPostProcessGraphTestResult Result;
    TestTypedEmptyInsertionSubgraph(Result);
    TestOptionalReadbackAndPresentationOnlyExecution(Result);
    TestHDRViewingGraphAndExactReadbackFootprint(Result);
    TestOrderedInsertionPlacementAndResources(Result);
    TestInsertionHazardExternalOutputAndEmptyEquivalence(Result);
    return Result;
}
