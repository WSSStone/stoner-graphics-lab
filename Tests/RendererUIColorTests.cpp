#include "Renderer/FUICompositionSettings.h"
#include "Renderer/FHDRPostProcessPipeline.h"
#include <iostream>
#include <algorithm>
#include <limits>

int RunRendererUIColorTests()
{
    using namespace Stoner::Renderer;
    int Failed=0;
    const auto Check=[&](bool Value,const char* Name) {
        std::cout<<(Value ? "[PASS] " : "[FAIL] ")<<Name<<'\n';
        if (!Value) ++Failed;
    };
    const FOutputTransformSettingsValidator Validator;
    for (const auto& Profile : Validator.GetProfiles())
    {
        FUICompositionSettings Settings;
        Settings.OutputProfileId=Profile.ProfileId;
        Settings.BlendDomain=Profile.DisplayLinearDomain;
        Settings.DisplayGeneration=7;
        const bool EDR=Profile.MetadataPolicy==EOutputMetadataPolicy::EDRState;
        Settings.UIReferenceWhiteNits=Settings.NativePackingWhiteNits=EDR ? 160.0f : Profile.ReferenceWhiteNits;
        Check(Settings.UIWhiteMultiplier==1 && Settings.IsValid(),
            "UI profile accepts default unit multiplier and its resolved reference white");
        for (float Multiplier : {0.25f,2.0f})
        {
            Settings.UIWhiteMultiplier=Multiplier;
            Check(Settings.IsValid(),"UI profile accepts both inclusive brightness limits");
        }
        for (float Multiplier : {0.249f,2.001f,std::numeric_limits<float>::infinity(),
                std::numeric_limits<float>::quiet_NaN()})
        {
            Settings.UIWhiteMultiplier=Multiplier;
            Check(!Settings.IsValid(),"UI profile rejects out-of-range or non-finite brightness");
        }
        Settings.UIWhiteMultiplier=1;
        Settings.NativePackingWhiteNits+=1;
        Check(!Settings.IsValid(),"UI profile rejects inconsistent composition and native packing whites");
        Settings.NativePackingWhiteNits=Settings.UIReferenceWhiteNits;
        Settings.DisplayGeneration=0;
        Check(!Settings.IsValid(),"UI reference white requires a nonzero display generation");
        Settings.DisplayGeneration=7;
        Settings.BlendDomain=ERenderGraphColorDomain::SceneLinearRec709D65;
        Check(!Settings.IsValid(),"UI cannot compose in a scene-linear domain");
    }
    for (float Exposure : {-16.0f,0.0f,16.0f})
    {
        using namespace Stoner::RHI;
        FRenderGraph Graph("UI exposure ordering");
        auto Resource=FRenderGraphResourceDesc::TypedTexture2D("Scene",16,16,ERHIFormat::R16G16B16A16_Float,
            ERHISampleCount::One,ERHITextureUsage::Sampled | ERHITextureUsage::ColorAttachment,
            ERenderGraphColorDomain::SceneLinearRec709D65);
        Resource.Ownership=ERenderGraphResourceOwnership::Imported;
        Resource.InitialState=ERenderGraphResourceState::External;
        FHDRSceneColorHandoffDesc Desc;
        Desc.SceneColorId=Desc.ViewId=Desc.FrameToken=1; Desc.Width=Desc.Height=16;
        auto Scene=FHDRSceneColorHandoff::Declare(Desc);
        (void)Scene.BindProducer(Graph.CreateBuilder().ImportResource(Resource));
        (void)Scene.MarkProduced();
        FOutputTransformSettings Output; Output.ManualExposureStops=Exposure;
        FUICompositionSettings UI;
        UI.OutputProfileId="Sdr.sRGB.v1"; UI.BlendDomain=ERenderGraphColorDomain::DisplayLinearRec709D65;
        UI.UIReferenceWhiteNits=UI.NativePackingWhiteNits=100; UI.DisplayGeneration=9; UI.UIWhiteMultiplier=0.5f;
        FHDRPostProcessPipeline Pipeline;
        auto Prepared=Pipeline.Prepare(Scene,Output,&UI);
        Prepared.Plan.ExecutionPurpose=EFrameExecutionPurpose::InteractivePreview;
        Prepared.Plan.ReadbackSelection=EFrameReadbackSelection::None;
        const auto Declaration=Pipeline.DeclareGraph(Graph,Prepared.Plan);
        const auto& Stages=Prepared.Plan.Stages;
        Check(Prepared.Succeeded() && Graph.Compile()==ERenderGraphResult::Success &&
            Pipeline.ValidateOutputGraph(Graph,Prepared.Plan,Declaration) &&
            Prepared.Plan.ResolvedSettings.ManualExposureStops==Exposure &&
            Prepared.Plan.TerminalUI && Prepared.Plan.TerminalUI->UIWhiteMultiplier==0.5f &&
            Prepared.Plan.TerminalUI->DisplayGeneration==9,
            "scene exposure extremes preserve the exact terminal UI white snapshot");
        const auto UIStage=std::find_if(Stages.begin(),Stages.end(),[](const auto& Stage) {
            return Stage.Kind==EOutputTransformStageKind::TerminalUI;
        });
        Check(UIStage!=Stages.end() && UIStage+1!=Stages.end() &&
            (UIStage+1)->Kind==EOutputTransformStageKind::OutputDeviceTransform &&
            std::all_of(UIStage+2,Stages.end(),[](const auto& Stage) {
                return Stage.Kind==EOutputTransformStageKind::FormalReadback || Stage.Kind==EOutputTransformStageKind::Presentation;
            }),
            "UI remains after all scene stages and before the sole output transfer at every exposure");
    }
    return Failed;
}
