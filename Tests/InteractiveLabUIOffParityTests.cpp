#include "FInteractiveLabShaders.h"
#include "FLabCaptureQueue.h"
#include "FProductionContentDeferredExecution.h"
#include "FUICompositionExecutor.h"
#include "MetalRHI/FMetalDeviceFactory.h"
#include "VulkanRHI/FVulkanDevice.h"
#include "RHI/RHIMinimal.h"
#include <algorithm>
#include <chrono>
#include "Renderer/FHDRPostProcessPipeline.h"
#include "Renderer/FForwardRenderer.h"
#include <iostream>

namespace
{
using namespace Stoner::Renderer;
using namespace Stoner::RHI;
// Keep the offscreen fixture finite while observing slow software execution.
// This does not alter live-loop deadlines or hardware-device wait budgets.
ERHIResult WaitForNativeParity(IRHIFence& Fence, const IRHIDevice& Device)
{
    const auto Runtime = Device.GetRuntimeSnapshot();
    const Stoner::Core::uint64 Budget = Runtime.bSoftwareDevice ? 120'000'000 : 5'000'000;
    const auto Start = std::chrono::steady_clock::now();
    const auto Result = Fence.Wait(Budget);
    const auto Elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - Start).count();
    std::cout << "[INFO] parity fence adapter=" << Runtime.AdapterName.CStr()
        << " software=" << Runtime.bSoftwareDevice << " budget-us=" << Budget
        << " elapsed-ms=" << Elapsed << " result=" << static_cast<int>(Result) << std::endl;
    return Result;
}
FForwardFramePlan ForwardFixture()
{
    using namespace Stoner::Core;
    FForwardFrameInputs Inputs;
    Inputs.View.ViewName="ParityCamera";
    Inputs.View.ViewMatrix=FMatrix4x4::Translation({-2,-3,-4});
    Inputs.View.ViewProjectionMatrix=FMatrix4x4::Scale({0.5f,0.75f,1})*Inputs.View.ViewMatrix;
    Inputs.View.CameraPosition={2,3,4};
    Inputs.View.Viewport.Extent={64,32};
    Inputs.Output.ColorTargetName="ParityScene";
    Inputs.Output.DepthTargetName="ParityDepth";
    Inputs.Output.Extent={64,32};
    FMeshDrawCandidate Draw;
    Draw.ObjectId=Draw.MeshId=1; Draw.DebugName="ParityMesh";
    Draw.WorldPosition={7,3,4};
    auto& Material=Draw.MaterialBinding;
    Material.MaterialId=1; Material.MaterialName="ParityMaterial";
    Material.bHasMaterialBinding=Material.bHasShaderBinding=true;
    Material.SurfaceInputs={true,true,true,true,true,true,true,{}};
    Inputs.DrawCandidates.push_back(Draw);
    FForwardFramePlan Plan;
    FForwardRenderer Renderer;
    (void)Renderer.PrepareFrame(Inputs,Plan);
    return Plan;
}
FHDRSceneColorHandoff Handoff(FRenderGraph& Graph,EHDRSceneColorProducer Producer,
    const FForwardFramePlan& Forward)
{
    auto Desc=FRenderGraphResourceDesc::TypedTexture2D("Scene",64,32,ERHIFormat::R16G16B16A16_Float,
        ERHISampleCount::One,ERHITextureUsage::Sampled | ERHITextureUsage::ColorAttachment,
        ERenderGraphColorDomain::SceneLinearRec709D65);
    Desc.Ownership=ERenderGraphResourceOwnership::Imported;
    Desc.InitialState=ERenderGraphResourceState::External;
    FHDRSceneColorHandoffDesc Identity;
    Identity.SceneColorId=Identity.ViewId=Identity.FrameToken=1;
    Identity.Width=64; Identity.Height=32; Identity.Producer=Producer;
    auto Scene=Producer==EHDRSceneColorProducer::Forward
        ? Forward.SceneColorHandoff : FHDRSceneColorHandoff::Declare(Identity);
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
    const auto Forward=ForwardFixture();
    Check(Forward.IsValid() && Forward.AcceptedOpaqueDraws.size()==1 &&
        Forward.AcceptedOpaqueDraws[0].GetCameraSpaceDepth()==5 &&
        Forward.PassOrder.size()==3,
        "bounded Forward fixture preserves nonidentity camera depth and three scene passes");
    const auto SceneDeclaration=Forward.GraphDeclaration.Dump();
    for (auto Producer : {EHDRSceneColorProducer::Forward,EHDRSceneColorProducer::Deferred})
    {
        FRenderGraph FormalGraph("UI-off formal"),PreviewGraph("UI-off preview");
        FOutputTransformSettings Settings; Settings.ManualExposureStops=3;
        auto FormalSettings=Settings; FormalSettings.bRequireReadback=true;
        auto Formal=Pipeline.Prepare(Handoff(FormalGraph,Producer,Forward),FormalSettings);
        auto Preview=Pipeline.Prepare(Handoff(PreviewGraph,Producer,Forward),Settings);
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
    FRenderGraph UIGraph("Forward terminal UI");
    FUICompositionSettings UI;
    UI.OutputProfileId="Sdr.sRGB.v1";
    UI.BlendDomain=ERenderGraphColorDomain::DisplayLinearRec709D65;
    UI.UIReferenceWhiteNits=UI.NativePackingWhiteNits=100;
    UI.DisplayGeneration=1;
    auto WithUI=Pipeline.Prepare(Handoff(UIGraph,EHDRSceneColorProducer::Forward,Forward),{},&UI);
    WithUI.Plan.ExecutionPurpose=EFrameExecutionPurpose::InteractivePreview;
    WithUI.Plan.ReadbackSelection=EFrameReadbackSelection::None;
    const auto UIDeclaration=Pipeline.DeclareGraph(UIGraph,WithUI.Plan);
    Check(WithUI.Succeeded() && UIGraph.Compile()==ERenderGraphResult::Success &&
        Pipeline.ValidateOutputGraph(UIGraph,WithUI.Plan,UIDeclaration) &&
        UIDeclaration.UIComposite.IsValid() && UIDeclaration.UIPass.IsValid() &&
        UIDeclaration.FullscreenPassCount==4 && UIDeclaration.GpuReadbackCopyCount==0 &&
        WithUI.Plan.SceneColor.GetSceneColorId()==Forward.SceneColorHandoff.GetSceneColorId(),
        "real Forward handoff accepts the shared terminal UI stage without changing scene identity");
    Check(Forward.GraphDeclaration.Dump()==SceneDeclaration &&
        Forward.ViewData.ViewMatrix==Stoner::Core::FMatrix4x4::Translation({-2,-3,-4}) &&
        Forward.ViewData.ViewProjectionMatrix==Stoner::Core::FMatrix4x4::Scale({0.5f,0.75f,1})*
            Stoner::Core::FMatrix4x4::Translation({-2,-3,-4}),
        "output preparation preserves Forward scene declarations and exact camera matrices");
    return Failed;
}


static int RunDeferredNativeUIOffParity(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIDevice>& Device,
    const Stoner::Demo::FProductionContentLoadedClosure& Closure,
    const Stoner::Asset::FAssetTargetProfileEvidence& Target);

// Native compatibility evidence only: the production Deferred path remains
// authoritative. Reuse its cooked output-stage resources for a bounded Forward
// scene, with an explicit test readback after ordinary GPU composition.
int RunInteractiveLabForwardNativeParity(const Stoner::Demo::FProductionContentLoadedClosure& Closure,
    const Stoner::Asset::FAssetTargetProfileEvidence& Target,
    const Stoner::Demo::FInteractiveLabShaders& Shaders)
{
    using namespace Stoner;
    using namespace Stoner::Core;
    using namespace Stoner::Demo;
    int Failed=0;
    const auto Check=[&](bool OK,const char* Name) {
        std::cout<<(OK ? "[PASS] " : "[FAIL] ")<<Name<<'\n';
        if (!OK) ++Failed;
        return OK;
    };
    const bool Metal=Target.Profile.GraphicsBackend==Asset::EAssetGraphicsBackend::Metal;
    TSharedPtr<IRHIDevice> Device;
    if (Metal) Device=Backend::Metal::CreateMetalDevice().Device;
    else
    {
        auto Vulkan=MakeShared<Backend::Vulkan::FVulkanDevice>();
        Backend::Vulkan::FVulkanInstanceDesc Init;
        Init.RuntimeMode=Backend::Vulkan::EVulkanInstanceRuntimeMode::DeterministicFallback;
        if (Vulkan->Initialize(Init)==ERHIResult::Success &&
            Vulkan->EnableNativeShaderRuntime()==ERHIResult::Success)
        {
            Vulkan->ConfigureDescriptorPoolCapacity(128);
            Device=Vulkan;
        }
    }
    if (!Check(Device && Device->GetRuntimeSnapshot().NativeOperations.bAvailable,
        "Forward parity requires a real native device")) return Failed;
    FStaticModelRealizationRequest Request;
    Request.Device=Device; Request.Model=Closure.Model; Request.Dependencies=Closure.Dependencies;
    Request.TargetEvidence=MakeShared<const Asset::FAssetTargetProfileEvidence>(Target);
    for (const auto& Texture : Request.Dependencies.Textures)
        Request.TextureTargetProfiles.push_back({Texture->GetId(),FTextureTargetProfile::DesktopDefault(Texture->GetInfo())});
    Request.RenderTargets.ColorFormats={ERHIFormat::R16G16B16A16_Float,
        ERHIFormat::R16G16B16A16_Float,ERHIFormat::R16G16B16A16_Float};
    Request.RenderTargets.DepthStencilFormat=ERHIFormat::D32_Float;
    TSharedPtr<const FStaticModelRenderSnapshot> Snapshot;
    FStaticModelRealizationInspection Inspection;
    if (!Check(FStaticModelRealizer::Realize(Request,Snapshot,Inspection)==ERHIResult::Success && Snapshot,
        "Forward parity realizes the strict-cooked Lantern mesh and materials")) return Failed;
    FProductionContentComposition Composition;
    FProductionContentCompositionConfig Config;
    Config.WorkloadRevision="production-content-lantern-v2"; Config.Width=64; Config.Height=32;
    FString Reason;
    if (!Check(FProductionContentCompositionBuilder::Build(Snapshot,Config,Composition,&Reason),
        "Forward parity uses the bounded production camera and scene")) return Failed;
    FProductionContentDeferredExecutionResources Output;
    FOutputTransformSettings Settings; Settings.bRequireReadback=true;
    if (!Check(FProductionContentDeferredExecutionBuilder::Build(Device,*Snapshot,Composition,
        Closure.RenderShaders,Closure.RenderShaderPayloads,Target,Settings,Output,&Reason)==ERHIResult::Success,
        "Forward parity reuses the strict-cooked shared output chain"))
    { std::cout<<Reason.CStr()<<'\n'; return Failed; }
    FForwardFramePlan Plan;
    FForwardFrameExecutionBindings Bindings;
    if (!Check(FForwardRenderer().PrepareFrame(Composition.ForwardInputs,Plan)==EForwardResult::Success &&
        PrepareProductionForwardSmoke(*Device,*Snapshot,Plan,Output.Plan,Bindings,&Reason),
        "Forward parity prepares existing native aggregate smoke bindings")) return Failed;
    // The fixture performs one final explicit readback, not the legacy raw
    // scene-color copy. Ordinary scene/UI recording must remain readback-free.
    Bindings.ReadbackBuffer.reset();
    auto Queue=Device->CreateCommandQueue(ERHIQueueType::Graphics).Object;
    auto SceneFence=Device->CreateFence(false).Object;
    const auto Before=Device->GetRuntimeSnapshot().NativeOperations;
    if (!Check(Queue && SceneFence && FForwardFrameExecutor().Execute(Plan,Bindings).Succeeded() &&
        Queue->SubmitDeferred(Bindings.CommandBuffer,{}, {},SceneFence)==ERHIResult::Success &&
        WaitForNativeParity(*SceneFence,*Device)==ERHIResult::Success,
        "Forward parity executes the actual cooked mesh on the GPU")) return Failed;
    auto& Stages=Output.Bindings.OutputTransformStages;
    Stages[0].Input=Bindings.OutputTexture;
    if (!Check(Stages[0].Stage.DescriptorSets[0]->UpdateCombinedTextureSampler(
        0,0,Bindings.OutputTexture,Output.OutputSampler)==ERHIResult::Success,
        "Forward scene feeds the same exposure and tone-map shaders")) return Failed;
    FUITextureRegistry Registry(Device); Registry.BeginEligibleFrame(1,true);
    FUITextureRequest Texture; Texture.RequestId=Texture.LogicalSlot=1;
    Texture.Width=Texture.Height=1; Texture.Format=ERHIFormat::R8G8B8A8_UNorm;
    Texture.ColorDomain=EUITextureColorDomain::AlphaCoverage; Texture.PixelBytes={255,255,255,255};
    const auto Registered=Registry.Prepare(Texture);
    const auto Lease=Registry.Acquire(Registered.TextureId);
    TArray<uint8> Baseline;
    for (int Mode=0;Mode<3;++Mode)
    {
        // 0 hidden, 1 visible empty, 2 a white 8x8 widget. Neither hidden nor
        // empty may allocate a UI target; unchanged pixels compare byte-exactly.
        FUIDrawSnapshot Draw(1,1,1,1);
        const FUIVertex Vertices[]={{{0,0},{0,0},0xffffffff},{{8,0},{1,0},0xffffffff},
            {{0,8},{0,1},0xffffffff},{{8,8},{1,1},0xffffffff}};
        const uint32 Indices[]={0,1,2,2,1,3};
        FUIDrawCommand Item; Item.IndexCount=6; Item.TextureId=Registered.TextureId; Item.ClipRect={0,0,8,8};
        bool Packet=Draw.SetDisplay({0,0},{64,32},{1,1});
        if (Mode==2) Packet=Packet && Draw.SetVertices(Vertices) && Draw.SetIndices(Indices) &&
            Draw.SetCommands({&Item,1}) && Draw.SetTextureLeases({&Lease,1});
        Packet=Packet && Draw.Publish();
        FUICompositionSettings UISettings;
        UISettings.OutputProfileId="Sdr.sRGB.v1";
        UISettings.BlendDomain=ERenderGraphColorDomain::DisplayLinearRec709D65;
        UISettings.UIReferenceWhiteNits=UISettings.NativePackingWhiteNits=100; UISettings.DisplayGeneration=1;
        FUICompositionFrame UI;
        const auto Tone=Stages[1].Output;
        if (Mode && !Check(Packet && FUICompositionExecutor::Prepare(Device,Draw,{1,1,1,0,64,32,{}},
            UISettings,Registry,Tone,Shaders.Draw.ModuleDescriptions,Shaders.Copy.ModuleDescriptions,UI)==ERHIResult::Success &&
            (Mode==2 ? UI.HasDraws() && UI.GetOutput()!=Tone : !UI.HasDraws() && UI.GetOutput()==Tone),
            "Forward empty UI omits targets while nonempty UI prepares terminal composition")) return Failed;
        auto Terminal=Mode==2 ? UI.GetOutput() : Tone;
        if (!Check(Stages[2].Stage.DescriptorSets[0]->UpdateCombinedTextureSampler(0,0,Terminal,Output.OutputSampler)==ERHIResult::Success,
            "Forward sole output transfer samples the selected terminal source")) return Failed;
        auto Command=Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
        auto Fence=Device->CreateFence(false).Object;
        bool OK=Command && Fence && Command->Begin()==ERHIResult::Success;
        const auto Transition=[&](const TSharedPtr<IRHITexture>& Tex,ERHIResourceLayout From,ERHIResourceLayout To) {
            FRHIResourceBarrierDesc B; B.Texture=Tex; B.Before=From; B.After=To;
            return Command->RecordLayoutTransition(B)==ERHIResult::Success;
        };
        FUITextureSubmission Submission;
        for (size_t StageIndex=0;StageIndex<Stages.size() && OK;++StageIndex)
        {
            const auto& Stage=Stages[StageIndex];
            if (StageIndex==2 && Mode==2)
                OK=Transition(Tone,ERHIResourceLayout::ColorAttachment,ERHIResourceLayout::ShaderReadOnly) &&
                    FUICompositionExecutor::Record(UI,Registry,Command,Submission)==ERHIResult::Success;
            else if (StageIndex!=0 || Mode==0)
                OK=Transition(StageIndex==2 ? Tone : Stage.Input,
                    ERHIResourceLayout::ColorAttachment,
                    ERHIResourceLayout::ShaderReadOnly);
            OK=OK && Transition(Stage.Output,ERHIResourceLayout::Undefined,ERHIResourceLayout::ColorAttachment) &&
                Command->BeginRenderPass(Stage.Stage.RenderPass,Stage.Stage.Framebuffer)==ERHIResult::Success &&
                Command->SetViewport({0,0,64,32,0,1})==ERHIResult::Success &&
                Command->SetScissor({0,0,64,32})==ERHIResult::Success &&
                Command->BindGraphicsPipeline(Stage.Stage.Pipeline)==ERHIResult::Success;
            for (const auto& Set : Stage.Stage.DescriptorSets) OK=OK && Command->BindDescriptorSet(Set)==ERHIResult::Success;
            OK=OK && Command->BindVertexBuffer(Output.Bindings.FullscreenVertexBuffer)==ERHIResult::Success &&
                Command->RecordDraw(3,1)==ERHIResult::Success && Command->EndRenderPass()==ERHIResult::Success;
            if (!OK) std::cout << "[INFO] Forward terminal recording failed mode=" << Mode
                << " stage=" << StageIndex << '\n';
        }
        const auto EndResult=OK ? Command->End() : ERHIResult::InvalidState;
        const auto SubmitResult=EndResult==ERHIResult::Success
            ? Queue->SubmitDeferred(Command,{}, {},Fence) : ERHIResult::InvalidState;
        OK=OK && EndResult==ERHIResult::Success && SubmitResult==ERHIResult::Success;
        if (OK && Mode==2) OK=Submission.Commit(Fence)==ERHIResult::Success;
        const auto WaitResult=OK ? WaitForNativeParity(*Fence,*Device) : ERHIResult::InvalidState;
        if (!OK || WaitResult!=ERHIResult::Success)
            std::cout << "[INFO] Forward terminal mode=" << Mode << " end=" << static_cast<int>(EndResult)
                << " submit=" << static_cast<int>(SubmitResult) << " wait=" << static_cast<int>(WaitResult) << '\n';
        if (!Check(OK && WaitResult==ERHIResult::Success,"Forward terminal chain completes native GPU submission")) return Failed;
        Registry.Poll();
        const auto After=Device->GetRuntimeSnapshot().NativeOperations;
        Check(After.ImageReadbackCopyCount==Before.ImageReadbackCopyCount+static_cast<uint64>(Mode) &&
            After.QueueIdleCallCount==Before.QueueIdleCallCount && After.DeviceIdleCallCount==Before.DeviceIdleCallCount,
            "Forward/UI rendering adds no readback or idle operation before the explicit fixture capture");
        auto ReadCommand=Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
        auto ReadFence=Device->CreateFence(false).Object;
        auto Buffer=Device->CreateBuffer({8192,ERHIBufferUsage::CopyDestination,ERHIMemoryAccess::HostVisible}).Object;
        FRHIResourceBarrierDesc Barrier; Barrier.Texture=Stages.back().Output;
        Barrier.Before=ERHIResourceLayout::ColorAttachment; Barrier.After=ERHIResourceLayout::CopySource;
        FRHITextureBufferCopyRegion Region; Region.Width=64; Region.Height=32; Region.DestinationRowLengthTexels=64;
        TArray<uint8> Pixels;
        OK=ReadCommand && ReadFence && Buffer && ReadCommand->Begin()==ERHIResult::Success &&
            ReadCommand->RecordLayoutTransition(Barrier)==ERHIResult::Success &&
            ReadCommand->RecordTextureToBufferCopy(Stages.back().Output,Buffer,Region)==ERHIResult::Success &&
            ReadCommand->End()==ERHIResult::Success && Queue->SubmitDeferred(ReadCommand,{}, {},ReadFence)==ERHIResult::Success &&
            WaitForNativeParity(*ReadFence,*Device)==ERHIResult::Success;
        if (OK) OK=(Metal ? Backend::Metal::ReadMetalBufferForValidation(Device,Buffer,0,8192,Pixels) :
            std::dynamic_pointer_cast<Backend::Vulkan::FVulkanDevice>(Device)->ReadbackBufferForTesting(Buffer,0,8192,Pixels))==ERHIResult::Success;
        if (!Check(OK && Pixels.size()==8192,"Forward fixture captures exact 64x32 encoded output")) return Failed;
        if (Mode==0)
        {
            Baseline=Pixels;
            bool NonBlack=false;
            for (size_t I=0;I<Pixels.size();I+=4) NonBlack|=Pixels[I]!=0 || Pixels[I+1]!=0 || Pixels[I+2]!=0;
            Check(NonBlack,"Forward UI-off native scene contains rendered color");
        }
        else
        {
            bool Match=true;
            for (uint32 Y=0;Y<32;++Y) for (uint32 X=0;X<64;++X) for (uint32 C=0;C<4;++C)
            {
                const size_t Offset=(Y*64+X)*4+C;
                const uint8 Expected=Mode==2 && X<8 && Y<8 ? 255 : Baseline[Offset];
                Match &= Pixels[Offset]==Expected;
            }
            Check(Match,"Forward empty UI preserves every pixel; terminal widget changes only its exact 8x8 region");
        }
    }
    Failed+=RunDeferredNativeUIOffParity(Device,Closure,Target);
    return Failed;
}

static int RunDeferredNativeUIOffParity(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIDevice>& Device,
    const Stoner::Demo::FProductionContentLoadedClosure& Closure,
    const Stoner::Asset::FAssetTargetProfileEvidence& Target)
{
    using namespace Stoner;
    using namespace Stoner::Core;
    using namespace Stoner::Demo;
    int Failed=0;
    const auto Check=[&](bool OK,const char* Name) {
        std::cout<<(OK ? "[PASS] " : "[FAIL] ")<<Name<<'\n';
        if (!OK) ++Failed;
        return OK;
    };
    FStaticModelRealizationRequest Request;
    Request.Device=Device; Request.Model=Closure.Model; Request.Dependencies=Closure.Dependencies;
    Request.TargetEvidence=MakeShared<const Asset::FAssetTargetProfileEvidence>(Target);
    for (const auto& Texture : Request.Dependencies.Textures)
        Request.TextureTargetProfiles.push_back({Texture->GetId(),FTextureTargetProfile::DesktopDefault(Texture->GetInfo())});
    Request.RenderTargets.ColorFormats={ERHIFormat::R8G8B8A8_UNorm,
        ERHIFormat::R16G16B16A16_Float,ERHIFormat::R16G16B16A16_Float};
    Request.RenderTargets.DepthStencilFormat=ERHIFormat::D32_Float;
    TSharedPtr<const FStaticModelRenderSnapshot> Snapshot;
    FStaticModelRealizationInspection Inspection;
    if (!Check(FStaticModelRealizer::Realize(Request,Snapshot,Inspection)==ERHIResult::Success && Snapshot,
        "UI-off scene parity realizes the production Deferred formats")) return Failed;
    FProductionContentComposition Composition;
    FProductionContentCompositionConfig Config;
    Config.WorkloadRevision="production-content-lantern-v2"; Config.Width=64; Config.Height=32;
    FString Reason;
    if (!Check(FProductionContentCompositionBuilder::Build(Snapshot,Config,Composition,&Reason),
        "UI-off scene parity freezes the exact production camera and dimensions")) return Failed;
    FOutputTransformSettings Settings; Settings.ManualExposureStops=3; Settings.bRequireReadback=true;
    FProductionContentDeferredExecutionResources Formal,Preview;
    if (!Check(FProductionContentDeferredExecutionBuilder::Build(Device,*Snapshot,Composition,
        Closure.RenderShaders,Closure.RenderShaderPayloads,Target,Settings,Formal,&Reason)==ERHIResult::Success,
        "UI-off scene parity builds the unchanged formal path")) return Failed;
    auto TargetDesc=Formal.Bindings.FormalOutput->GetDesc();
    TargetDesc.Usage=ERHITextureUsage::ColorAttachment | ERHITextureUsage::Present | ERHITextureUsage::CopySource;
    auto Borrowed=Device->CreateTexture(TargetDesc).Object;
    FProductionContentDeferredExecutionBuildOptions Options;
    Options.ExecutionPurpose=EFrameExecutionPurpose::InteractivePreview;
    Options.ReadbackSelection=EFrameReadbackSelection::None; Options.SceneLease=Snapshot;
    Options.BorrowedFinalOutput=Borrowed;
    auto PreviewSettings=Settings; PreviewSettings.bRequireReadback=false; PreviewSettings.bRequirePresentation=true;
    if (!Check(Borrowed && FProductionContentDeferredExecutionBuilder::Build(Device,*Snapshot,Composition,
        Closure.RenderShaders,Closure.RenderShaderPayloads,Target,PreviewSettings,Preview,&Reason,Options)==ERHIResult::Success,
        "UI-off scene parity builds real preview resources with slot-local uniforms"))
    { std::cout<<Reason.CStr()<<'\n'; return Failed; }
    Check(Formal.Plan.View.View==Preview.Plan.View.View && Formal.Plan.View.Projection==Preview.Plan.View.Projection &&
        Formal.Plan.View.ViewProjection==Preview.Plan.View.ViewProjection &&
        Formal.Plan.View.InverseViewProjection==Preview.Plan.View.InverseViewProjection &&
        Formal.Plan.View.Extent.Width==64 && Formal.Plan.View.Extent.Height==32 &&
        Preview.Plan.View.Extent.Width==64 && Preview.Plan.View.Extent.Height==32 &&
        !Preview.OutputTransformPlan.TerminalUI && Preview.Bindings.OutputTransformStages.size()==3 &&
        Preview.Bindings.Readbacks.empty(),
        "native formal/preview plans retain exact camera matrices and omit UI/readback resources");
    auto Queue=Device->CreateCommandQueue(ERHIQueueType::Graphics).Object;
    const auto Read=[&](const TSharedPtr<IRHIBuffer>& Buffer,uint64 Size,TArray<uint8>& Pixels) {
        return Target.Profile.GraphicsBackend==Asset::EAssetGraphicsBackend::Metal
            ? Backend::Metal::ReadMetalBufferForValidation(Device,Buffer,0,Size,Pixels)
            : std::dynamic_pointer_cast<Backend::Vulkan::FVulkanDevice>(Device)->ReadbackBufferForTesting(Buffer,0,Size,Pixels);
    };
    TArray<uint8> Baseline;
    for (auto* Resources : {&Formal,&Preview})
    {
        const bool IsPreview=Resources==&Preview;
        auto Bindings=Resources->Bindings;
        // Offscreen comparison exercises preview scene/uniform/output recording;
        // actual acquire/presentation remains the separate native session suite.
        Bindings.bTransitionFinalOutputToPresent=false;
        auto Fence=Device->CreateFence(false).Object;
        const auto Before=Device->GetRuntimeSnapshot().NativeOperations;
        if (!Check(Queue && Fence && FDeferredFrameExecutor().Execute(Resources->Plan,Resources->Graph,Bindings).Succeeded() &&
            Queue->SubmitDeferred(Bindings.CommandBuffer,{}, {},Fence)==ERHIResult::Success && WaitForNativeParity(*Fence,*Device)==ERHIResult::Success,
            "UI-off formal and preview execute the complete native Deferred scene and output chain")) return Failed;
        const auto After=Device->GetRuntimeSnapshot().NativeOperations;
        Check(After.ImageReadbackCopyCount-Before.ImageReadbackCopyCount==(IsPreview ? 0U : 6U) &&
            After.QueueIdleCallCount==Before.QueueIdleCallCount && After.DeviceIdleCallCount==Before.DeviceIdleCallCount,
            "native preview scene has zero implicit readbacks while formal retains all six probes");
        const auto It=std::find_if(Formal.Bindings.Readbacks.begin(),Formal.Bindings.Readbacks.end(),
            [](const auto& Binding) { return Binding.Name==FString("FinalOutput"); });
        if (!Check(It!=Formal.Bindings.Readbacks.end() && TargetDesc.Format==ERHIFormat::R8G8B8A8_UNorm &&
            It->Region.Width==64 && It->Region.Height==32,
            "UI-off comparison requires the exact encoded format and unmodified capture extent")) return Failed;
        uint64 Size=0;
        if (!Check(TryGetRHITextureBufferCopyByteSize(It->Region,TargetDesc.Format,Size) && Size==8192,
            "UI-off comparison uses the original exact capture footprint")) return Failed;
        if (IsPreview)
        {
            auto Command=Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
            auto ReadFence=Device->CreateFence(false).Object;
            FRHIResourceBarrierDesc Barrier; Barrier.Texture=Borrowed;
            Barrier.Before=ERHIResourceLayout::ColorAttachment; Barrier.After=ERHIResourceLayout::CopySource;
            if (!Check(Command && ReadFence && Command->Begin()==ERHIResult::Success &&
                Command->RecordLayoutTransition(Barrier)==ERHIResult::Success &&
                Command->RecordTextureToBufferCopy(Borrowed,It->Destination,It->Region)==ERHIResult::Success &&
                Command->End()==ERHIResult::Success && Queue->SubmitDeferred(Command,{}, {},ReadFence)==ERHIResult::Success &&
                WaitForNativeParity(*ReadFence,*Device)==ERHIResult::Success,
                "preview comparison explicitly captures only after ordinary rendering completes")) return Failed;
        }
        TArray<uint8> Pixels;
        if (!Check(Read(It->Destination,Size,Pixels)==ERHIResult::Success && Pixels.size()==Size,
            "native UI-off pixels are read without crop, resize or alignment changes")) return Failed;
        if (IsPreview) Check(Pixels==Baseline,"every native Deferred UI-off preview pixel equals the formal path at +3 EV");
        else
        {
            Baseline=Pixels;
            bool NonBlack=false;
            for (size_t I=0;I<Pixels.size();I+=4) NonBlack|=Pixels[I]!=0 || Pixels[I+1]!=0 || Pixels[I+2]!=0;
            Check(NonBlack,"formal UI-off comparison contains actual rendered scene pixels");
        }
    }
    // Real native copy is inserted in the same command as the selected preview.
    // The test alone waits for completion; the capture queue only polls it.
    for (const bool Numeric : {false,true})
    {
        FLabCaptureQueue Captures;
        FLabCaptureRequest Capture;
        Capture.RequestId=1;
        Capture.Target.SettingsGeneration=Capture.Target.DisplayGeneration=Capture.Target.OutputGeneration=1;
        Capture.Target.Width=64; Capture.Target.Height=32;
        Capture.Target.Format=Numeric ? ERHIFormat::R16G16B16A16_Float : TargetDesc.Format;
        Capture.Target.OutputProfile=Preview.OutputTransformPlan.ResolvedSettings.OutputDeviceProfileId;
        Capture.Target.Stage=Numeric ? "ManualExposure" : "FinalOutput";
        Capture.Target.Purpose=Numeric ? ELabCapturePurpose::HDRNumeric : ELabCapturePurpose::SDRPreview;
        const FLabCaptureFrame Frame{100,Capture.Target,EFrameExecutionPurpose::InteractivePreview,true};
        auto Bindings=Preview.Bindings;
        Bindings.bTransitionFinalOutputToPresent=false;
        if (!Check(Bindings.CommandBuffer->Reset()==ERHIResult::Success &&
            Captures.Request(Capture,100)==ELabCaptureStatus::Pending,
            "native explicit capture starts with an idle command and bounded request")) return Failed;
        const auto Before=Device->GetRuntimeSnapshot().NativeOperations;
        FLabCapturePrepared Prepared;
        const auto Execution=FDeferredFrameExecutor().Execute(Preview.Plan,Preview.Graph,Bindings,[&] {
            FDeferredReadbackBinding Copy;
            if (Captures.PrepareNext(Frame,Device,Bindings.CommandBuffer,101,Prepared)==ELabCaptureStatus::Success)
            {
                Copy.Name=Capture.Target.Stage;
                Copy.Source=Numeric ? Bindings.OutputTransformStages[0].Output : Bindings.FormalOutput;
                Copy.Destination=Prepared.Staging;
                Copy.Region=Prepared.Region;
            }
            return Copy;
        });
        auto Fence=Device->CreateFence(false).Object;
        if (!Check(Execution.Succeeded() && Prepared.Staging && Fence &&
            Queue->SubmitDeferred(Bindings.CommandBuffer,{}, {},Fence)==ERHIResult::Success &&
            Captures.Submit(1,100,Fence),
            "native explicit capture shares the selected render command and completion fence")) return Failed;
        Prepared={};
        if (!Check(WaitForNativeParity(*Fence,*Device)==ERHIResult::Success,
            "native capture fixture observes actual GPU completion")) return Failed;
        Captures.Poll(102);
        if (!Check(Bindings.CommandBuffer->Reset()==ERHIResult::Success,
            "native capture releases command references before consumer readback")) return Failed;
        TArray<uint8> Pixels;
        FLabCaptureCompletion Completion;
        unsigned Reads=0;
        const auto Consumed=Captures.ProcessOne(1,103,[&](const auto&,const auto& Buffer,const auto& Region) {
            ++Reads;
            uint64 Bytes=0;
            return TryGetRHITextureBufferCopyByteSize(Region,Capture.Target.Format,Bytes) &&
                Read(Buffer,Bytes,Pixels)==ERHIResult::Success && Pixels.size()==Bytes;
        },Completion);
        const auto After=Device->GetRuntimeSnapshot().NativeOperations;
        Check(Consumed && Reads==1 && Completion.Status==ELabCaptureStatus::Success &&
            Completion.FrameToken==100 && Completion.Request.Target==Capture.Target &&
            Captures.GetStatistics().Requests==0 && Captures.GetStatistics().StagingBytes==0 &&
            After.ImageReadbackCopyCount-Before.ImageReadbackCopyCount==1 &&
            After.QueueIdleCallCount==Before.QueueIdleCallCount && After.DeviceIdleCallCount==Before.DeviceIdleCallCount,
            "explicit native capture copies once, preserves identity, drains staging and performs no idle");
        Check(Numeric ? Pixels.size()==64*32*8 : Pixels==Baseline,
            "native capture preserves float16 numeric bytes or exact formal-equivalent SDR pixels");
    }
    return Failed;
}
