#include "Renderer/FHDRPostProcessPipeline.h"
#include "FUICompositionExecutor.h"
#include "VulkanRHI/FVulkanDevice.h"
#include "VulkanRHI/FVulkanCommandBuffer.h"
#include <fstream>
#include <iterator>
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
    for (const int Visibility : {0,1,2})
        for (const auto Mode : {EOutputTransformDebugBypassMode::BoundedVisualization,
                EOutputTransformDebugBypassMode::HDRPreservingReadback})
        {
            const bool Visible=Visibility!=0;
            UI.bDiagnosticWidgetVisible=Visibility==1;
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
            const bool Widget=Visible && UI.bDiagnosticWidgetVisible && Mode==EOutputTransformDebugBypassMode::BoundedVisualization;
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
    UI.bDiagnosticWidgetVisible=true;
    FRenderGraph Feedback("feedback rejected");
    FOutputTransformSettings Settings;
    Settings.DiagnosticBypass.Mode=EOutputTransformDebugBypassMode::BoundedVisualization;
    Settings.DiagnosticBypass.StageName="OutputDeviceTransform";
    auto Prepared=Pipeline.Prepare(Scene(Feedback),Settings,&UI);
    Prepared.Plan.ExecutionPurpose=EFrameExecutionPurpose::InteractivePreview;
    Prepared.Plan.ReadbackSelection=EFrameReadbackSelection::None;
    Check(!Prepared.Plan.IsValid(),"same-frame UI cannot sample its own final output transfer");
    auto Device=Stoner::Core::MakeShared<Stoner::Backend::Vulkan::FVulkanDevice>();
    Stoner::Backend::Vulkan::FVulkanInstanceDesc DeviceDesc;
    DeviceDesc.RuntimeMode=Stoner::Backend::Vulkan::EVulkanInstanceRuntimeMode::DeterministicFallback;
    if (Device->Initialize(DeviceDesc)!=ERHIResult::Success) return Failed+1;
    {
        Stoner::Core::TArray<FRHIShaderModuleDesc> Shaders;
        for (const auto Stage : {ERHIShaderStage::Vertex,ERHIShaderStage::Fragment})
        {
            FRHIShaderModuleDesc Shader; Shader.Stage=Stage; Shader.EntryPoint="main";
            const char* Path=Stage==ERHIShaderStage::Vertex ? "Content/Shaders/PostProcess/Fullscreen.vert.spv" : "Content/Shaders/UI/UIDiagnostic.frag.spv";
            std::ifstream Input(Path,std::ios::binary);
            Shader.Payload.Bytes.assign(std::istreambuf_iterator<char>(Input),{});
            Shader.Payload.Format=ERHIShaderPayloadFormat::SPIRV; Shader.Payload.PayloadIdentity=Path;
            Shader.Payload.PayloadDigest=ComputeRHISha256(Shader.Payload.Bytes); Shader.Payload.TargetProfile="vulkan-1.3";
            if (Stage==ERHIShaderStage::Fragment) Shader.InterfaceMetadata.Bindings={
                {0,0,ERHIDescriptorType::CombinedTextureSampler,1,ERHIShaderStageFlags::Fragment},
                {0,1,ERHIDescriptorType::UniformBuffer,1,ERHIShaderStageFlags::Fragment}};
            Shaders.push_back(std::move(Shader));
        }
        FRHITextureDesc Desc; Desc.Width=2048; Desc.Height=512; Desc.Format=ERHIFormat::R16G16B16A16_Float;
        Desc.Usage=ERHITextureUsage::Sampled | ERHITextureUsage::ColorAttachment;
        auto Source=Device->CreateTexture(Desc).Object;
        FResolvedOutputTransformDebugBypass Selection;
        Selection.SourceStageId=1; Selection.SourceStageName="SceneColorHandoff";
        Selection.SourceDomain=ERenderGraphColorDomain::SceneLinearRec709D65;
        Selection.Mode=EOutputTransformDebugBypassMode::BoundedVisualization;
        Selection.VisualizationMinimum=-2; Selection.VisualizationMaximum=6;
        FUIDiagnosticFrame Frame;
        Check(FUICompositionExecutor::PrepareDiagnostic(Device,Source,Selection,Shaders,1024*256*4-1,Frame)==ERHIResult::NotReady &&
            !Frame.GetOutput(),"diagnostic attachment budget rejects before publishing a target");
        Check(FUICompositionExecutor::PrepareDiagnostic(Device,Source,Selection,Shaders,1024*256*4,Frame)==ERHIResult::Success &&
            Frame.CanRecord() && Frame.GetOutput()->GetDesc().Width==1024 && Frame.GetOutput()->GetDesc().Height==256 &&
            !HasRHIFlag(Frame.GetOutput()->GetUsage(),ERHITextureUsage::CopySource),
            "diagnostic producer prepares actual RHI resources at the exact bounded footprint");
        const auto Prior=Frame.GetOutput(); Selection.Mode=EOutputTransformDebugBypassMode::HDRPreservingReadback;
        Check(FUICompositionExecutor::PrepareDiagnostic(Device,Source,Selection,Shaders,1024*256*4,Frame)==ERHIResult::InvalidState &&
            Frame.GetOutput()==Prior,"numeric capture selection cannot replace a GPU widget producer");
        auto Command=Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
        Check(FUICompositionExecutor::RecordDiagnostic(Frame,Command)==ERHIResult::InvalidState && Frame.CanRecord(),
            "an idle command cannot consume a prepared diagnostic producer");
        (void)Command->Begin();
        FRHIResourceBarrierDesc Transition; Transition.Texture=Source; Transition.After=ERHIResourceLayout::ShaderReadOnly;
        (void)Command->RecordLayoutTransition(Transition);
        const auto Status=FUICompositionExecutor::RecordDiagnostic(Frame,Command);
        const auto Native=std::dynamic_pointer_cast<Stoner::Backend::Vulkan::FVulkanCommandBuffer>(Command);
        bool Copies=false;
        for (const auto& C : Native->GetRecordedCommands()) Copies |= C.Type==ERHISymbolicCommandType::BufferToTextureCopy ||
            C.Type==ERHISymbolicCommandType::TextureToBufferCopy;
        Check(Status==ERHIResult::Success && !Copies && !Frame.CanRecord() &&
            FUICompositionExecutor::RecordDiagnostic(Frame,Command)==ERHIResult::InvalidState,
            "diagnostic producer records one bounded raster pass without copies and cannot be replayed");
        Selection.Mode=EOutputTransformDebugBypassMode::BoundedVisualization;
        Check(FUICompositionExecutor::PrepareDiagnostic(Device,Source,Selection,Shaders,1024*256*4,Frame)==ERHIResult::InvalidState &&
            Frame.GetOutput()==Prior,"a recorded diagnostic producer cannot be overwritten before its owner retires");
        (void)Command->End(); (void)Command->Reset();
    }
    (void)Device->Shutdown();
    return Failed;
}
