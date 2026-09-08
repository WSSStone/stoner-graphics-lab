#include "Renderer/FHDRPostProcessPipeline.h"
#include <algorithm>
#include <iostream>
#include <string_view>

namespace
{
using namespace Stoner::Renderer;
using namespace Stoner::RHI;
FHDRSceneColorHandoff Scene(FRenderGraph& Graph)
{
    auto Desc=FRenderGraphResourceDesc::TypedTexture2D("Scene",2048,512,ERHIFormat::R16G16B16A16_Float,
        ERHISampleCount::One,ERHITextureUsage::Sampled | ERHITextureUsage::ColorAttachment,
        ERenderGraphColorDomain::SceneLinearRec709D65);
    Desc.Ownership=ERenderGraphResourceOwnership::Imported;
    Desc.InitialState=ERenderGraphResourceState::External;
    FHDRSceneColorHandoffDesc H;
    H.SceneColorId=1; H.ViewId=1; H.FrameToken=1; H.Width=2048; H.Height=512;
    auto Result=FHDRSceneColorHandoff::Declare(H);
    (void)Result.BindProducer(Graph.CreateBuilder().ImportResource(Desc));
    (void)Result.MarkProduced(); return Result;
}
}
int RunInteractiveLabDebugTests()
{
    int Failed=0;
    auto Check=[&](bool OK,const char* Name) {
        std::cout<<(OK ? "[PASS] " : "[FAIL] ")<<Name<<'\n'; if (!OK) ++Failed;
    };
    FHDRPostProcessPipeline Pipeline;
    FUICompositionSettings UI;
    UI.OutputProfileId="Sdr.sRGB.v1"; UI.BlendDomain=ERenderGraphColorDomain::DisplayLinearRec709D65;
    UI.UIReferenceWhiteNits=UI.NativePackingWhiteNits=100; UI.DisplayGeneration=1;
    for (const auto* StageName : {"ManualExposure","SDRToneMap"})
    for (const bool Visible : {false,true})
        for (const auto Mode : {EOutputTransformDebugBypassMode::BoundedVisualization,
                EOutputTransformDebugBypassMode::HDRPreservingReadback})
        {
            FRenderGraph Graph("preview diagnostic");
            FOutputTransformSettings Settings;
            Settings.DiagnosticBypass.Mode=Mode; Settings.DiagnosticBypass.StageName=StageName;
            Settings.DiagnosticBypass.VisualizationMinimum=2; Settings.DiagnosticBypass.VisualizationMaximum=8;
            auto Prepared=Pipeline.Prepare(Scene(Graph),Settings,Visible ? &UI : nullptr);
            Prepared.Plan.ExecutionPurpose=EFrameExecutionPurpose::InteractivePreview;
            Prepared.Plan.ReadbackSelection=EFrameReadbackSelection::None;
            const auto Declaration=Pipeline.DeclareGraph(Graph,Prepared.Plan);
            const bool Compiled=Graph.Compile()==ERenderGraphResult::Success;
            Check(Prepared.Succeeded() && Compiled && Declaration.IsValid() &&
                Pipeline.ValidateOutputGraph(Graph,Prepared.Plan,Declaration),"preview diagnostic plan and graph validate");
            Check(Declaration.DiagnosticReadbackCopyCount==0 && !Declaration.DiagnosticReadbackBuffer.IsValid() &&
                !Declaration.DiagnosticReadbackPass.IsValid() && Declaration.GpuReadbackCopyCount==0,
                "selecting a preview diagnostic never schedules numeric or formal readback");
            const bool Widget=Visible && Mode==EOutputTransformDebugBypassMode::BoundedVisualization;
            Check(Declaration.DiagnosticOutput.IsValid()==Widget && Declaration.DiagnosticFullscreenPassCount==(Widget ? 1u : 0u),
                "hidden UI and numeric selection retain settings without allocating a visualization target");
            Check(Prepared.Plan.DiagnosticBypass.SourceStageName==StageName &&
                Prepared.Plan.DiagnosticBypass.SourceDomain==(std::string_view(StageName)=="ManualExposure" ?
                    ERenderGraphColorDomain::SceneLinearRec709D65 : ERenderGraphColorDomain::DisplayLinearRec709D65) &&
                Prepared.Plan.DiagnosticBypass.VisualizationMinimum==2 && Prepared.Plan.DiagnosticBypass.VisualizationMaximum==8,
                "non-default diagnostic stage domain and range survive preview planning");
            if (Widget)
            {
                const auto* Target=Graph.FindResource(Declaration.DiagnosticOutput);
                Check(Target && Target->Desc.Width==1024 && Target->Desc.Height==256 &&
                    Target->Desc.Texture.Format==ERHIFormat::R8G8B8A8_sRGB &&
                    !HasRHIFlag(Target->Desc.Texture.Usage,ERHITextureUsage::CopySource),
                    "widget target preserves aspect within 1024 pixels and uses sampled sRGB without CopySource");
                const auto& Edges=Graph.GetCompiledGraph().DependencyEdges;
                Check(std::any_of(Edges.begin(),Edges.end(),[&](const auto& E) {
                    return E.FromPassIndex==Declaration.DiagnosticVisualizationPass.Index &&
                        E.ToPassIndex==Declaration.UIPass.Index; }),"diagnostic producer is an actual dependency of terminal UI sampling");
            }
        }
    FRenderGraph Feedback("feedback rejected");
    FOutputTransformSettings Settings;
    Settings.DiagnosticBypass.Mode=EOutputTransformDebugBypassMode::BoundedVisualization;
    Settings.DiagnosticBypass.StageName="OutputDeviceTransform";
    auto Prepared=Pipeline.Prepare(Scene(Feedback),Settings,&UI);
    Prepared.Plan.ExecutionPurpose=EFrameExecutionPurpose::InteractivePreview;
    Prepared.Plan.ReadbackSelection=EFrameReadbackSelection::None;
    Check(!Prepared.Plan.IsValid(),"same-frame UI cannot sample its own final output transfer");
    return Failed;
}
