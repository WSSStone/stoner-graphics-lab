#include "UINativeRasterFixture.h"
#include "FUICompositionExecutor.h"
#include "RHI/IRHICommandQueue.h"
#include "RHI/IRHIFramebuffer.h"
#include "RHI/IRHIBuffer.h"
#include <cmath>
#include <cstdlib>
#include <iostream>

int RunUINativeRasterFixture(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIDevice>& Device,
    std::span<const Stoner::RHI::FRHIShaderModuleDesc> DrawShaders,
    std::span<const Stoner::RHI::FRHIShaderModuleDesc> CopyShaders,
    const FUINativeReadback& Readback,
    std::span<const Stoner::RHI::FRHIShaderModuleDesc> DiagnosticShaders)
{
    using namespace Stoner::Core;
    using namespace Stoner::RHI;
    using namespace Stoner::Renderer;
    int Failed = 0;
    const auto Check = [&](bool Value,const char* Name) {
        std::cout << (Value ? "[PASS] " : "[FAIL] ") << Name << '\n';
        if (!Value) ++Failed;
        return Value;
    };
    const auto Queue = Device->CreateCommandQueue(ERHIQueueType::Graphics).Object;
    if (!Check(Queue != nullptr,"native UI raster fixture creates its queue")) return Failed;
    if (DiagnosticShaders.size()!=2)
    {
        std::cout << "[SKIP] cooked diagnostic program is absent; no native diagnostic pass claimed\n";
        if (std::getenv("STONER_REQUIRE_UI_DIAGNOSTIC")) ++Failed;
    }
    else
    {
        const auto RunDiagnostic=[&]()
        {
            FRHITextureDesc Desc; Desc.Width=Desc.Height=8; Desc.Format=ERHIFormat::R16G16B16A16_Float;
            Desc.Usage=ERHITextureUsage::Sampled | ERHITextureUsage::ColorAttachment;
            auto Source=Device->CreateTexture(Desc).Object;
            FResolvedOutputTransformDebugBypass Selection;
            Selection.SourceStageId=1; Selection.SourceStageName="SceneColorHandoff";
            Selection.SourceDomain=ERenderGraphColorDomain::SceneLinearRec709D65;
            Selection.Mode=EOutputTransformDebugBypassMode::BoundedVisualization;
            Selection.VisualizationMinimum=-2; Selection.VisualizationMaximum=6;
            const auto Before=Device->GetRuntimeSnapshot().NativeOperations;
            FUIDiagnosticFrame Diagnostic;
            if (!Check(FUICompositionExecutor::PrepareDiagnostic(Device,Source,Selection,DiagnosticShaders,256,Diagnostic)==ERHIResult::Success,
                "native diagnostic target prepares from strict-cooked shader bytes")) return;
            FRenderGraph Graph("native widget"); auto Builder=Graph.CreateBuilder();
            const auto Resource=Builder.CreateTexture("widget",8,8,ERHIFormat::R8G8B8A8_sRGB,ERHISampleCount::One,
                Desc.Usage,ERenderGraphColorDomain::EncodedSrgb);
            auto P=FRenderGraphPassDesc::Make("diagnostic",ERenderGraphPassType::Graphics);
            P.Accesses.push_back({Resource,ERenderGraphAccessType::Write,ERenderGraphResourceState::Write});
            const auto Producer=Builder.AddPass(P);
            P=FRenderGraphPassDesc::Make("UI",ERenderGraphPassType::Graphics); P.bPreserveForSideEffects=true;
            P.Accesses.push_back({Resource,ERenderGraphAccessType::Read,ERenderGraphResourceState::Read});
            const auto Consumer=Builder.AddPass(P); (void)Builder.AddDependency(Producer,Consumer);
            (void)Graph.Compile();
            FUIGpuTextureContext Gpu{&Graph,Consumer,1,1,1};
            FUITextureRegistry Registry(Device); Registry.BeginEligibleFrame(1,true); FUITextureId Id;
            if (!Check(Registry.RegisterGpuTexture({1,{},Diagnostic.GetOutput(),Resource,Producer},Gpu,Id)==ERHIResult::Success,
                "native GPU diagnostic registers the exact producer and sampled texture generation")) return;
            auto Lease=Registry.Acquire(Id); FUIDrawSnapshot Snapshot(1,1,1,1);
            const FUIVertex Vertices[]={{{0,0},{0.5f,0.5f},0xffffffff},{{8,0},{0.5f,0.5f},0xffffffff},
                {{0,8},{0.5f,0.5f},0xffffffff},{{8,8},{0.5f,0.5f},0xffffffff}};
            const uint32 Indices[]={0,1,2,2,1,3}; FUIDrawCommand Draw; Draw.IndexCount=6; Draw.TextureId=Id; Draw.ClipRect={0,0,8,8};
            const bool Published=Snapshot.SetDisplay({0,0},{8,8},{1,1}) && Snapshot.SetVertices(Vertices) && Snapshot.SetIndices(Indices) &&
                Snapshot.SetCommands({&Draw,1}) && Snapshot.SetTextureLeases({&Lease,1}) && Snapshot.Publish();
            FUICompositionSettings Settings; Settings.OutputProfileId="Sdr.sRGB.v1";
            Settings.BlendDomain=ERenderGraphColorDomain::DisplayLinearRec709D65;
            Settings.UIReferenceWhiteNits=Settings.NativePackingWhiteNits=100; Settings.DisplayGeneration=1;
            FUICompositionFrame UI;
            if (!Check(Published && FUICompositionExecutor::Prepare(Device,Snapshot,{1,1,1,0,8,8,{}},Settings,Registry,Source,
                DrawShaders,CopyShaders,UI,&Gpu)==ERHIResult::Success,"native UI prepares a registered GPU diagnostic image")) return;
            auto Command=Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
            auto Fence=Device->CreateFence(false).Object;
            auto Pass=Device->CreateRenderPass({{{ERHIAttachmentRole::Color,Desc.Format,ERHISampleCount::One,
                ERHIAttachmentLoadOp::Clear,ERHIAttachmentStoreOp::Store}}}).Object;
            FRHIFramebufferDesc FB; FB.RenderPass=Pass; FB.Attachments={{Source,0,0}}; FB.Width=FB.Height=8;
            auto Target=Device->CreateFramebuffer(FB).Object;
            FRHIResourceBarrierDesc T; T.Texture=Source; T.After=ERHIResourceLayout::ColorAttachment;
            FUITextureSubmission Submission;
            bool Recorded=Command && Fence && Pass && Target && Command->Begin()==ERHIResult::Success &&
                Command->RecordLayoutTransition(T)==ERHIResult::Success &&
                Command->BeginRenderPass(Pass,Target,{{{-2,2,6,0.25f}}})==ERHIResult::Success && Command->EndRenderPass()==ERHIResult::Success;
            T.Before=ERHIResourceLayout::ColorAttachment; T.After=ERHIResourceLayout::ShaderReadOnly;
            Recorded=Recorded && Command->RecordLayoutTransition(T)==ERHIResult::Success &&
                FUICompositionExecutor::RecordDiagnostic(Diagnostic,Command)==ERHIResult::Success &&
                FUICompositionExecutor::Record(UI,Registry,Command,Submission)==ERHIResult::Success && Command->End()==ERHIResult::Success;
            const bool Submitted=Recorded && Queue->SubmitDeferred(Command,{}, {},Fence)==ERHIResult::Success;
            if (Submitted) (void)Submission.Commit(Fence);
            const auto After=Device->GetRuntimeSnapshot().NativeOperations;
            Check(Submitted && After.FenceWaitCallCount==Before.FenceWaitCallCount && After.ImageReadbackCopyCount==Before.ImageReadbackCopyCount && After.ReadbackMapCount==Before.ReadbackMapCount &&
                After.ReadbackWaitCount==Before.ReadbackWaitCount && After.QueueIdleCallCount==Before.QueueIdleCallCount &&
                After.DeviceIdleCallCount==Before.DeviceIdleCallCount,"native diagnostic and UI submit together with zero implicit readbacks or idle waits");
            if (!Submitted || Fence->Wait(5000000)!=ERHIResult::Success) { Check(false,"native diagnostic render completion required"); return; }
            Registry.Poll(); auto ReadCommand=Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
            auto ReadFence=Device->CreateFence(false).Object;
            auto Buffer=Device->CreateBuffer({2048,ERHIBufferUsage::CopyDestination,ERHIMemoryAccess::HostVisible}).Object;
            FRHITextureBufferCopyRegion Region; Region.Width=Region.Height=8; Region.DestinationRowLengthTexels=32;
            T.Texture=UI.GetOutput(); T.Before=ERHIResourceLayout::ShaderReadOnly; T.After=ERHIResourceLayout::CopySource;
            TArray<uint8> Pixels;
            const bool Read=ReadCommand && ReadFence && Buffer && ReadCommand->Begin()==ERHIResult::Success &&
                ReadCommand->RecordLayoutTransition(T)==ERHIResult::Success && ReadCommand->RecordTextureToBufferCopy(UI.GetOutput(),Buffer,Region)==ERHIResult::Success &&
                ReadCommand->End()==ERHIResult::Success && Queue->SubmitDeferred(ReadCommand,{}, {},ReadFence)==ERHIResult::Success &&
                ReadFence->Wait(5000000)==ERHIResult::Success && Readback(Buffer,2048,Pixels)==ERHIResult::Success && Pixels.size()==2048;
            bool Match=Read;
            if (Read) for (uint32 Y=0;Y<8;++Y) for (uint32 X=0;X<8;++X) for (uint32 C=0;C<4;++C)
            {
                const auto Offset=Y*256+X*8+C*2; const unsigned V=Pixels[Offset] | (unsigned(Pixels[Offset+1])<<8);
                const int Exp=(V>>10)&31; const float Actual=(V&32768 ? -1.0f : 1.0f)*std::ldexp(float((V&1023)+(Exp?1024:0)),Exp?Exp-25:-24);
                const float Expected[]={0,0.5f,1,1}; Match &= std::abs(Actual-Expected[C])<=0.007f;
            }
            Check(Match,"native diagnostic range mapping survives sRGB encode/sample and UI composition without double transfer");
            ReadCommand.reset(); Command.reset(); UI={}; Snapshot={}; Lease={}; Submission={};
            Check(Registry.RetireGpuTexture(Id)==ERHIResult::Success,"native diagnostic texture retires after render consumers release");
        };
        RunDiagnostic();
    }
    for (float Scale : {1.0f,1.5f,2.0f}) for (uint8 Alpha : {uint8{0},uint8{128},uint8{255}})
    {
        std::cout << "[INFO] UI raster scale=" << Scale << " alpha=" << static_cast<unsigned>(Alpha) << '\n';
        const uint32 Extent = static_cast<uint32>(16 * Scale);
        const auto Before = Device->GetRuntimeSnapshot().NativeOperations;
        FUITextureRegistry Registry(Device); Registry.BeginEligibleFrame(1,true);
        FUITextureRequest Request; Request.RequestId = 1; Request.LogicalSlot = 1;
        Request.Width = 2; Request.Height = 1; Request.Format = ERHIFormat::R8G8B8A8_sRGB;
        Request.ColorDomain = EUITextureColorDomain::SRGBRec709;
        Request.PixelBytes = {128,0,0,Alpha,0,0,255,Alpha};
        const auto Color = Registry.Prepare(Request);
        Request.RequestId = 2; Request.LogicalSlot = 2; Request.Width = 1;
        Request.Format = ERHIFormat::R8G8B8A8_UNorm; Request.ColorDomain = EUITextureColorDomain::AlphaCoverage;
        Request.PixelBytes = {255,255,255,255};
        const auto Coverage = Registry.Prepare(Request);
        FUITextureLease Leases[] = {Registry.Acquire(Color.TextureId),Registry.Acquire(Coverage.TextureId)};
        FUIDrawSnapshot Snapshot(1,1,1,1);
        // Unreferenced sentinels make ignoring either indexed offset observable.
        const FUIVertex Vertices[] = {{{999,999},{0,0},0},
            {{10,20},{0.5f,0.5f},0xff808080},{{26,20},{0.5f,0.5f},0xff808080},
            {{10,36},{0.5f,0.5f},0xff808080},{{26,36},{0.5f,0.5f},0xff808080}};
        const uint32 Indices[] = {999,999,999,0,1,2,2,1,3};
        FUIDrawCommand Left; Left.FirstIndex = 3; Left.BaseVertex = 1; Left.IndexCount = 6;
        Left.TextureId = Color.TextureId; Left.ClipRect = {9.25f,19.25f,18.2f,28.2f};
        auto Right = Left; Right.TextureId = Coverage.TextureId; Right.ClipRect = {18.2f,19.25f,27,37};
        FUIDrawCommand Reset; Reset.Operation = EUIDrawOperation::ResetState;
        const FUIDrawCommand Commands[] = {Left,Reset,Right};
        const bool Published = Snapshot.SetDisplay({10,20},{16,16},{Scale,Scale}) &&
            Snapshot.SetVertices(Vertices) && Snapshot.SetIndices(Indices) &&
            Snapshot.SetCommands(Commands) && Snapshot.SetTextureLeases(Leases) && Snapshot.Publish();
        FRHITextureDesc Desc; Desc.Width = Desc.Height = Extent;
        Desc.Format = ERHIFormat::R16G16B16A16_Float;
        Desc.Usage = ERHITextureUsage::ColorAttachment | ERHITextureUsage::Sampled;
        const auto Scene = Device->CreateTexture(Desc).Object;
        FUICompositionSettings Settings; Settings.OutputProfileId = "Sdr.sRGB.v1";
        Settings.BlendDomain = ERenderGraphColorDomain::DisplayLinearRec709D65;
        Settings.UIReferenceWhiteNits = Settings.NativePackingWhiteNits = 100; Settings.DisplayGeneration = 1;
        FUIDrawValidationContext Validation{1,1,1,0,Extent,Extent,{}};
        FUICompositionFrame Frame;
        if (!Check(Published && Scene && FUICompositionExecutor::Prepare(Device,Snapshot,Validation,Settings,
                Registry,Scene,DrawShaders,CopyShaders,Frame) == ERHIResult::Success,
                "native fixture prepares offset geometry and two real texture leases")) continue;
        auto Command = Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
        const auto Fence = Device->CreateFence(false).Object;
        const auto Pass = Device->CreateRenderPass({{{ERHIAttachmentRole::Color,Desc.Format,ERHISampleCount::One,
            ERHIAttachmentLoadOp::Clear,ERHIAttachmentStoreOp::Store}}}).Object;
        FRHIFramebufferDesc FB; FB.RenderPass = Pass; FB.Attachments = {{Scene,0,0}}; FB.Width = FB.Height = Extent;
        const auto Target = Device->CreateFramebuffer(FB).Object;
        FUITextureSubmission Submission;
        FRHIResourceBarrierDesc Transition; Transition.Texture = Scene; Transition.After = ERHIResourceLayout::ColorAttachment;
        bool Recorded = Command && Fence && Pass && Target && Command->Begin() == ERHIResult::Success &&
            Command->RecordLayoutTransition(Transition) == ERHIResult::Success &&
            Command->BeginRenderPass(Pass,Target,{{{0.125f,0.25f,0.5f,0.25f}}}) == ERHIResult::Success &&
            Command->EndRenderPass() == ERHIResult::Success;
        Transition.Before = ERHIResourceLayout::ColorAttachment; Transition.After = ERHIResourceLayout::ShaderReadOnly;
        Recorded = Recorded && Command->RecordLayoutTransition(Transition) == ERHIResult::Success &&
            FUICompositionExecutor::Record(Frame,Registry,Command,Submission) == ERHIResult::Success &&
            Command->End() == ERHIResult::Success;
        const bool ReplaceFont = Scale == 1.0f && Alpha == 128;
        FUICompositionFrame ReplacementFrame;
        TSharedPtr<IRHICommandBuffer> ReplacementCommand;
        TSharedPtr<IRHIFence> ReplacementFence;
        FUITextureSubmission ReplacementSubmission;
        bool ReplacementRecorded = false;
        FUITextureRequest DestroyOld;
        if (ReplaceFont && Recorded)
        {
            // Replace the alpha-coverage atlas while the old immutable draw is
            // already recorded. Submit both generations before any validation wait.
            Request.Operation = EUITextureOperation::Update; Request.RequestId = 3;
            Request.TextureId = Coverage.TextureId; Request.ExpectedGeneration = Coverage.TextureId.Generation;
            Request.PixelBytes = {255,255,255,64};
            const auto Updated = Registry.Prepare(Request);
            const auto NewLease = Registry.Acquire(Updated.TextureId);
            Check(Updated.Succeeded() && Updated.TextureId.Slot == Coverage.TextureId.Slot &&
                Updated.TextureId.Generation > Coverage.TextureId.Generation &&
                Registry.ResolveTexture(NewLease) != Registry.ResolveTexture(Leases[1]) &&
                Snapshot.GetTextureLeases()[1].GetId() == Coverage.TextureId,
                "recorded font snapshot preserves its exact old generation across atlas replacement");
            auto NewDraw = Right; NewDraw.TextureId = Updated.TextureId;
            FUIDrawSnapshot NewSnapshot(1,2,1,1);
            const bool NewPublished = NewSnapshot.SetDisplay({10,20},{16,16},{Scale,Scale}) &&
                NewSnapshot.SetVertices(Vertices) && NewSnapshot.SetIndices(Indices) &&
                NewSnapshot.SetCommands({&NewDraw,1}) && NewSnapshot.SetTextureLeases({&NewLease,1}) && NewSnapshot.Publish();
            ReplacementCommand = Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
            ReplacementFence = Device->CreateFence(false).Object;
            ReplacementRecorded = NewPublished && ReplacementCommand && ReplacementFence &&
                FUICompositionExecutor::Prepare(Device,NewSnapshot,Validation,Settings,Registry,Scene,
                    DrawShaders,CopyShaders,ReplacementFrame) == ERHIResult::Success &&
                ReplacementCommand->Begin() == ERHIResult::Success &&
                FUICompositionExecutor::Record(ReplacementFrame,Registry,ReplacementCommand,ReplacementSubmission) == ERHIResult::Success &&
                ReplacementCommand->End() == ERHIResult::Success;
            DestroyOld.Operation = EUITextureOperation::Destroy; DestroyOld.RequestId = 4;
            DestroyOld.TextureId = Coverage.TextureId; DestroyOld.ExpectedGeneration = Coverage.TextureId.Generation;
            Check(Registry.Prepare(DestroyOld).Result == ERHIResult::NotReady,
                "old atlas destruction cannot acknowledge while recorded draw leases remain");
        }
        const bool Submitted = Recorded && Queue->SubmitDeferred(Command,{}, {},Fence) == ERHIResult::Success &&
            Submission.Commit(Fence) == ERHIResult::Success;
        const bool ReplacementSubmitted = ReplaceFont && Submitted && ReplacementRecorded &&
            Queue->SubmitDeferred(ReplacementCommand,{}, {},ReplacementFence) == ERHIResult::Success &&
            ReplacementSubmission.Commit(ReplacementFence) == ERHIResult::Success;
        if (ReplaceFont) Check(ReplacementSubmitted,"both native atlas generations are submitted before validation waits");
        const auto After = Device->GetRuntimeSnapshot().NativeOperations;
        Check(Submitted && After.FenceWaitCallCount == Before.FenceWaitCallCount &&
            After.QueueIdleCallCount == Before.QueueIdleCallCount && After.DeviceIdleCallCount == Before.DeviceIdleCallCount &&
            After.ImageReadbackCopyCount == Before.ImageReadbackCopyCount && After.ReadbackMapCount == Before.ReadbackMapCount &&
            After.ReadbackWaitCount == Before.ReadbackWaitCount,
            "offset/scissor UI submission introduces no synchronous wait or readback");
        if (!Submitted || Fence->Wait(5000000) != ERHIResult::Success)
        { Check(false,"native raster completion required"); Command.reset(); continue; }
        if (ReplaceFont && (!ReplacementSubmitted || ReplacementFence->Wait(5000000) != ERHIResult::Success))
        { Check(false,"replacement atlas render completion required"); ReplacementCommand.reset(); Command.reset(); continue; }
        Registry.Poll();
        for (int OutputIndex = 0; OutputIndex < (ReplaceFont ? 2 : 1); ++OutputIndex)
        {
        auto ReadCommand = Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
        const auto ReadFence = Device->CreateFence(false).Object;
        const uint64 Bytes = static_cast<uint64>(Extent)*256;
        const auto Buffer = Device->CreateBuffer({Bytes,ERHIBufferUsage::CopyDestination,ERHIMemoryAccess::HostVisible}).Object;
        FRHITextureBufferCopyRegion Region; Region.Width = Region.Height = Extent; Region.DestinationRowLengthTexels = 32;
        const auto Output = OutputIndex == 0 ? Frame.GetOutput() : ReplacementFrame.GetOutput();
        Transition.Texture = Output; Transition.Before = ERHIResourceLayout::ShaderReadOnly;
        Transition.After = ERHIResourceLayout::CopySource;
        TArray<uint8> Pixels;
        const bool Read = ReadCommand && ReadFence && Buffer && ReadCommand->Begin() == ERHIResult::Success &&
            ReadCommand->RecordLayoutTransition(Transition) == ERHIResult::Success &&
            ReadCommand->RecordTextureToBufferCopy(Output,Buffer,Region) == ERHIResult::Success &&
            ReadCommand->End() == ERHIResult::Success && Queue->SubmitDeferred(ReadCommand,{}, {},ReadFence) == ERHIResult::Success &&
            ReadFence->Wait(5000000) == ERHIResult::Success && Readback(Buffer,Bytes,Pixels) == ERHIResult::Success && Pixels.size() == Bytes;
        if (Check(Read,"independent native raster readback completes"))
        {
            const float Gray = std::pow((128.0f/255.0f+0.055f)/1.055f,2.4f);
            const float AlphaValue = Alpha/255.0f;
            const float Source[] = {Gray*Gray*0.5f,0,Gray*0.5f};
            const float Background[] = {0.125f,0.25f,0.5f};
            const int LeftEnd = static_cast<int>(std::ceil(8.2f*Scale));
            const int RightStart = static_cast<int>(std::floor(8.2f*Scale));
            bool RGB = true, Opaque = true;
            for (uint32 Y=0;Y<Extent;++Y) for (uint32 X=0;X<Extent;++X) for (int C=0;C<4;++C)
            {
                const auto Offset = Y*256+X*8+static_cast<uint32>(C)*2;
                const unsigned V = Pixels[Offset] | (static_cast<unsigned>(Pixels[Offset+1])<<8);
                const int Exp = (V>>10)&31;
                const float Actual = (V&32768 ? -1.0f : 1.0f)*std::ldexp(static_cast<float>((V&1023)+(Exp?1024:0)),Exp?Exp-25:-24);
                if (C==3) { Opaque &= Actual == 1.0f; continue; }
                float Expected = Background[C];
                if (OutputIndex == 0 && static_cast<int>(X)<LeftEnd && static_cast<int>(Y)<LeftEnd)
                    Expected = Source[C]*AlphaValue + Background[C]*(1-AlphaValue);
                if (static_cast<int>(X)>=RightStart)
                    Expected = OutputIndex == 0 ? Gray : Gray*(64.0f/255.0f)+Background[C]*(191.0f/255.0f);
                const bool Match = std::isfinite(Actual) && std::abs(Actual-Expected) <= 0.002f+0.005f*std::abs(Expected);
                if (!Match && RGB) std::cout << "[INFO] first mismatch x=" << X << " y=" << Y << " channel=" << C
                    << " expected=" << Expected << " actual=" << Actual << '\n';
                RGB &= Match;
            }
            Check(RGB,OutputIndex == 0 ? "all pixels match sRGB-before-filtering, indexed offsets, state reset and signed scaled scissors"
                : "replacement atlas pixels use new coverage without changing the already-submitted old draw");
            Check(Opaque,"RGB-only UI preserves copied alpha one at every native pixel");
        }
        ReadCommand.reset();
        }
        ReplacementCommand.reset(); Command.reset();
        if (ReplaceFont)
        {
            Frame = {}; Snapshot = {}; Submission = {}; Leases[1] = {};
            Registry.Poll();
            Check(Registry.Prepare(DestroyOld).Result == ERHIResult::Success,
                "old atlas destruction acknowledges after native completion and final draw lease release");
        }
    }
    return Failed;
}
