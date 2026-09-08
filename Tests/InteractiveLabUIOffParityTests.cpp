#include "Renderer/FHDRPostProcessPipeline.h"
#include <iostream>

namespace
{
using namespace Stoner::Renderer;
using namespace Stoner::RHI;
FHDRSceneColorHandoff Handoff(FRenderGraph& Graph,EHDRSceneColorProducer Producer)
{
    auto Desc=FRenderGraphResourceDesc::TypedTexture2D("Scene",64,32,ERHIFormat::R16G16B16A16_Float,
        ERHISampleCount::One,ERHITextureUsage::Sampled | ERHITextureUsage::ColorAttachment,
        ERenderGraphColorDomain::SceneLinearRec709D65);
    Desc.Ownership=ERenderGraphResourceOwnership::Imported;
    Desc.InitialState=ERenderGraphResourceState::External;
    FHDRSceneColorHandoffDesc Identity;
    Identity.SceneColorId=Identity.ViewId=Identity.FrameToken=1;
    Identity.Width=64; Identity.Height=32; Identity.Producer=Producer;
    auto Scene=FHDRSceneColorHandoff::Declare(Identity);
    (void)Scene.BindProducer(Graph.CreateBuilder().ImportResource(Desc));
    (void)Scene.MarkProduced();
    return Scene;
}
}
int RunInteractiveLabUIOffParityTests()
{
    int Failed=0;
    const auto Check=[&](bool OK,const char* Name) {
        std::cout<<(OK ? "[PASS] " : "[FAIL] ")<<Name<<'\n';
        if (!OK) ++Failed;
    };
    const FHDRPostProcessPipeline Pipeline;
    for (auto Producer : {EHDRSceneColorProducer::Forward,EHDRSceneColorProducer::Deferred})
    {
        FRenderGraph FormalGraph("UI-off formal"),PreviewGraph("UI-off preview");
        FOutputTransformSettings Settings; Settings.ManualExposureStops=3;
        auto FormalSettings=Settings; FormalSettings.bRequireReadback=true;
        auto Formal=Pipeline.Prepare(Handoff(FormalGraph,Producer),FormalSettings);
        auto Preview=Pipeline.Prepare(Handoff(PreviewGraph,Producer),Settings);
        Preview.Plan.ExecutionPurpose=EFrameExecutionPurpose::InteractivePreview;
        Preview.Plan.ReadbackSelection=EFrameReadbackSelection::None;
        const auto F=Pipeline.DeclareGraph(FormalGraph,Formal.Plan);
        const auto P=Pipeline.DeclareGraph(PreviewGraph,Preview.Plan);
        Check(Formal.Succeeded() && Preview.Succeeded() &&
            FormalGraph.Compile()==ERenderGraphResult::Success && PreviewGraph.Compile()==ERenderGraphResult::Success &&
            Pipeline.ValidateOutputGraph(FormalGraph,Formal.Plan,F) && Pipeline.ValidateOutputGraph(PreviewGraph,Preview.Plan,P),
            "Forward and Deferred UI-off formal/preview graphs independently validate");
        Check(!F.UIComposite.IsValid() && !P.UIComposite.IsValid() && !F.UIPass.IsValid() && !P.UIPass.IsValid() &&
            F.FullscreenPassCount==3 && P.FullscreenPassCount==3 &&
            !P.DiagnosticOutput.IsValid() && !P.ReadbackBuffer.IsValid() && P.GpuReadbackCopyCount==0,
            "UI-off preview retains three output draws without UI targets or implicit readbacks");
        Check(Formal.Plan.OutputDesc.Width==64 && Formal.Plan.OutputDesc.Height==32 &&
            Preview.Plan.OutputDesc.Width==64 && Preview.Plan.OutputDesc.Height==32 &&
            Formal.Plan.OutputDesc.Format==Preview.Plan.OutputDesc.Format,
            "UI-off parity preserves exact dimensions and native output format");
        for (auto Stage : {EOutputTransformStageKind::ManualExposure,EOutputTransformStageKind::SDRToneMap,
                EOutputTransformStageKind::OutputDeviceTransform})
        {
            const auto A=Pipeline.BuildShaderParameterPayload(Formal.Plan.ResolvedSettings,Stage);
            const auto B=Pipeline.BuildShaderParameterPayload(Preview.Plan.ResolvedSettings,Stage);
            Check(A.IsValid() && B.IsValid() && A.Bytes==B.Bytes,
                "UI-off formal and preview use byte-identical color transform parameters");
        }
        Check(F.ReadbackBuffer.IsValid() && F.GpuReadbackCopyCount==1,
            "formal UI-off capture retains its required readback independently of preview");
    }
    return Failed;
}
