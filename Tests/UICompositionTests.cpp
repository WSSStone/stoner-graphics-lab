#include "FUICompositionExecutor.h"
#include "UINativeRasterFixture.h"
#include "Renderer/FUIRenderSession.h"
#include "FInteractiveLabShaders.h"
#include "VulkanRHI/FVulkanDevice.h"
#include "MetalRHI/FMetalDeviceFactory.h"
#include <iostream>
#include <cstdlib>
#include <cmath>

int RunUICompositionPreparationTests(const Stoner::Demo::FInteractiveLabShaders& Shaders, bool RequireNative)
{
    using namespace Stoner;
    using namespace Stoner::Renderer;
    using namespace Stoner::RHI;
    int Failed = 0;
    const auto Check = [&](bool OK, const char* Name)
    { std::cout << (OK ? "[PASS] " : "[FAIL] ") << Name << '\n'; if (!OK) ++Failed; return OK; };
    const bool Native = RequireNative || std::getenv("STONER_REQUIRE_UI_COMPOSITION") != nullptr;
    const bool Metal = Shaders.Draw.SelectedTarget.Backend == Asset::EShaderBackendFamily::Metal;
    Core::TSharedPtr<IRHIDevice> Device;
    if (Metal) Device = Backend::Metal::CreateMetalDevice().Device;
    else
    {
        auto Vulkan = Core::MakeShared<Backend::Vulkan::FVulkanDevice>();
        Backend::Vulkan::FVulkanInstanceDesc Init;
        Init.RuntimeMode = Backend::Vulkan::EVulkanInstanceRuntimeMode::DeterministicFallback;
        if (Vulkan->Initialize(Init) == ERHIResult::Success &&
            (!Native || Vulkan->EnableNativeShaderRuntime() == ERHIResult::Success)) Device = Vulkan;
    }
    if (Native && (!Device || !Device->IsActive()))
    { std::cout << "[UNSUPPORTED] native UI device unavailable; requested native validation cannot pass\n"; return 1; }
    if (!Check(Device && Device->IsActive(), "UI composition fixture initializes")) return Failed;
    {
        FUITextureRegistry Registry(Device);
        Registry.BeginEligibleFrame(1, true);
        FUITextureRequest Request;
        Request.RequestId = 1; Request.LogicalSlot = 1; Request.Width = Request.Height = 1;
        Request.Format = ERHIFormat::R8G8B8A8_UNorm;
        Request.ColorDomain = EUITextureColorDomain::AlphaCoverage;
        Request.PixelBytes = {255,255,255,128};
        const auto Texture = Registry.Prepare(Request);
        const FUITextureLease Lease = Registry.Acquire(Texture.TextureId);
        FUIDrawSnapshot Draw(1,1,1,1);
        const FUIVertex Vertices[] = {{{0,0},{0,0},0xffffffff},{{16,0},{1,0},0xffffffff},{{0,16},{0,1},0xffffffff}};
        const Core::uint32 Indices[] = {0,1,2};
        FUIDrawCommand Item; Item.IndexCount = 3; Item.TextureId = Texture.TextureId; Item.ClipRect = {0,0,16,16};
        const bool Packet = Draw.SetDisplay({0,0},{16,16},{1,1}) && Draw.SetVertices(Vertices) &&
            Draw.SetIndices(Indices) && Draw.SetCommands({&Item,1}) && Draw.SetTextureLeases({&Lease,1}) && Draw.Publish();
        FUIDrawValidationContext Context{1,1,1,0,16,16,{}};
        FUICompositionSettings Settings;
        Settings.OutputProfileId = "Sdr.sRGB.v1";
        Settings.BlendDomain = ERenderGraphColorDomain::DisplayLinearRec709D65;
        Settings.UIReferenceWhiteNits = Settings.NativePackingWhiteNits = 100;
        Settings.DisplayGeneration = 1;
        FRHITextureDesc Desc; Desc.Width = Desc.Height = 16;
        Desc.Format = ERHIFormat::R16G16B16A16_Float;
        Desc.Usage = ERHITextureUsage::Sampled | ERHITextureUsage::ColorAttachment;
        const auto Scene = Device->CreateTexture(Desc).Object;
        FUICompositionFrame Frame;
        if (Check(Packet && Scene && FUICompositionExecutor::Prepare(Device,Draw,Context,Settings,Registry,Scene,
            Shaders.Draw.ModuleDescriptions,Shaders.Copy.ModuleDescriptions,Frame) == ERHIResult::Success &&
            Frame.HasDraws() && Frame.GetOutput() && Frame.GetOutput() != Scene,
            "composition preflight prepares a distinct target and complete leased resources"))
        {
            const auto Original = Frame.GetOutput();
            auto Bad = Context; Bad.DisplayGeneration = 2;
            Check(FUICompositionExecutor::Prepare(Device,Draw,Bad,Settings,Registry,Scene,
                Shaders.Draw.ModuleDescriptions,Shaders.Copy.ModuleDescriptions,Frame) != ERHIResult::Success &&
                Frame.GetOutput() == Original, "stale UI preflight preserves the existing prepared frame");
            Check(FUICompositionExecutor::Prepare(Device,Draw,Context,Settings,Registry,Scene,{},
                Shaders.Copy.ModuleDescriptions,Frame) != ERHIResult::Success && Frame.GetOutput() == Original,
                "missing draw shader cannot publish a partial composition frame");
            FUITextureRegistry OtherRegistry(Device);
            Check(FUICompositionExecutor::Prepare(Device,Draw,Context,Settings,OtherRegistry,Scene,
                Shaders.Draw.ModuleDescriptions,Shaders.Copy.ModuleDescriptions,Frame) != ERHIResult::Success &&
                Frame.GetOutput() == Original, "foreign-registry texture leases cannot pass composition preflight");
            auto Command = Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
            FUITextureSubmission Submission;
            bool Begun = Command && Command->Begin() == ERHIResult::Success;
            auto ScenePass = Device->CreateRenderPass({{{ERHIAttachmentRole::Color,Desc.Format,
                ERHISampleCount::One,ERHIAttachmentLoadOp::Clear,ERHIAttachmentStoreOp::Store}}}).Object;
            FRHIFramebufferDesc SceneFramebufferDesc;
            SceneFramebufferDesc.RenderPass = ScenePass; SceneFramebufferDesc.Attachments = {{Scene,0,0}};
            SceneFramebufferDesc.Width = SceneFramebufferDesc.Height = 16;
            auto SceneFramebuffer = Device->CreateFramebuffer(SceneFramebufferDesc).Object;
            FRHIResourceBarrierDesc SceneTransition;
            SceneTransition.Texture = Scene; SceneTransition.After = ERHIResourceLayout::ColorAttachment;
            Begun = Begun && ScenePass && SceneFramebuffer &&
                Command->RecordLayoutTransition(SceneTransition) == ERHIResult::Success &&
                Command->BeginRenderPass(ScenePass,SceneFramebuffer,{{{0.25f,0.125f,0.0625f,0.25f}}}) == ERHIResult::Success &&
                Command->EndRenderPass() == ERHIResult::Success;
            SceneTransition.Before = ERHIResourceLayout::ColorAttachment;
            SceneTransition.After = ERHIResourceLayout::ShaderReadOnly;
            Begun = Begun && Command->RecordLayoutTransition(SceneTransition) == ERHIResult::Success;
            const auto Before = Device->GetRuntimeSnapshot().NativeOperations;
            const bool Recorded = Check(Begun &&
                FUICompositionExecutor::Record(Frame,Registry,Command,Submission) == ERHIResult::Success &&
                Command->End() == ERHIResult::Success, "composition records upload, scene copy and indexed UI in one command stream");
            Check(FUICompositionExecutor::Record(Frame,Registry,Command,Submission) != ERHIResult::Success,
                "a prepared composition frame cannot record twice");
            if (Native && Recorded)
            {
                auto Queue = Device->CreateCommandQueue(ERHIQueueType::Graphics).Object;
                auto Fence = Device->CreateFence(false).Object;
                const bool Submitted = Queue && Fence && Queue->SubmitDeferred(Command,{}, {},Fence) == ERHIResult::Success &&
                    Submission.Commit(Fence) == ERHIResult::Success;
                const auto After = Device->GetRuntimeSnapshot().NativeOperations;
                Check(Submitted && Before.bAvailable && After.bAvailable &&
                    After.ImageReadbackCopyCount == Before.ImageReadbackCopyCount &&
                    After.ReadbackMapCount == Before.ReadbackMapCount && After.ReadbackWaitCount == Before.ReadbackWaitCount &&
                    After.FenceWaitCallCount == Before.FenceWaitCallCount &&
                    After.QueueIdleCallCount == Before.QueueIdleCallCount && After.DeviceIdleCallCount == Before.DeviceIdleCallCount,
                    "native UI composition submits without synchronous waits or image readbacks");
                const bool Completed = Submitted && Fence->Wait(5000000) == ERHIResult::Success;
                if (Check(Completed,"native composition render completion is observed with a bounded validation wait"))
                {
                    Registry.Poll();
                    auto ReadCommand = Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
                    auto ReadFence = Device->CreateFence(false).Object;
                    auto Readback = Device->CreateBuffer({4096,ERHIBufferUsage::CopyDestination,ERHIMemoryAccess::HostVisible}).Object;
                    FRHIResourceBarrierDesc Transition;
                    Transition.Texture = Frame.GetOutput(); Transition.Before = ERHIResourceLayout::ShaderReadOnly;
                    Transition.After = ERHIResourceLayout::CopySource;
                    FRHITextureBufferCopyRegion Region; Region.Width = Region.Height = 16;
                    Region.DestinationRowLengthTexels = 32;
                    const bool Read = ReadCommand && ReadFence && Readback && ReadCommand->Begin() == ERHIResult::Success &&
                        ReadCommand->RecordLayoutTransition(Transition) == ERHIResult::Success &&
                        ReadCommand->RecordTextureToBufferCopy(Frame.GetOutput(),Readback,Region) == ERHIResult::Success &&
                        ReadCommand->End() == ERHIResult::Success && Queue->SubmitDeferred(ReadCommand,{}, {},ReadFence) == ERHIResult::Success &&
                        ReadFence->Wait(5000000) == ERHIResult::Success;
                    Core::TArray<Core::uint8> Bytes;
                    const auto ReadResult = !Read ? ERHIResult::Failed : Metal
                        ? Backend::Metal::ReadMetalBufferForValidation(Device,Readback,0,4096,Bytes)
                        : std::dynamic_pointer_cast<Backend::Vulkan::FVulkanDevice>(Device)->ReadbackBufferForTesting(Readback,0,4096,Bytes);
                    if (Check(ReadResult == ERHIResult::Success && Bytes.size() == 4096,"independent composition readback completes"))
                    {
                        const auto Half = [&](int X,int Y,int Channel)
                        {
                            const auto Offset = Y*256+X*8+Channel*2;
                            const unsigned int V = Bytes[Offset] | (static_cast<unsigned int>(Bytes[Offset+1])<<8);
                            const int Exp = (V>>10)&31;
                            return (V&32768 ? -1.0f : 1.0f) * std::ldexp(static_cast<float>((V&1023)+(Exp ? 1024 : 0)),Exp ? Exp-25 : -24);
                        };
                        bool Alpha = true;
                        for (int Y=0;Y<16;++Y) for (int X=0;X<16;++X) Alpha &= Half(X,Y,3) == 1.0f;
                        Check(Alpha,"scene copy and RGB-only indexed blending preserve alpha one across every pixel");
                        const float Coverage = 128.0f/255.0f;
                        const float SceneRGB[] = {0.25f,0.125f,0.0625f};
                        bool Colors = true;
                        for (int C=0;C<3;++C)
                            Colors &= std::abs(Half(2,2,C)-(Coverage+SceneRGB[C]*(1-Coverage))) < 0.004f &&
                                std::abs(Half(14,14,C)-SceneRGB[C]) < 0.004f;
                        Check(Colors,"native UI coverage blends in linear RGB while uncovered scene pixels remain unchanged");
                    }
                }
            }
            Command.reset(); // discard command before cancellation of upload reservations
        }
        {
            FUIRenderSession Session(Device,7);
            Session.BeginEligibleFrame(1,true);
            const auto PreparedTexture = Session.PrepareTexture(Request);
            const auto SessionLease = Session.AcquireTexture(PreparedTexture.TextureId);
            const auto MakePacket = [&](Core::uint64 Id, bool Diagnostic = false)
            {
                FUIDrawSnapshot PacketValue(7,Id,1,1);
                auto CommandValue = Item; CommandValue.TextureId = PreparedTexture.TextureId;
                CommandValue.bDiagnosticWidget = Diagnostic;
                (void)PacketValue.SetDisplay({0,0},{16,16},{1,1}); (void)PacketValue.SetVertices(Vertices);
                (void)PacketValue.SetIndices(Indices); (void)PacketValue.SetCommands({&CommandValue,1});
                (void)PacketValue.SetTextureLeases({&SessionLease,1}); (void)PacketValue.Publish();
                return PacketValue;
            };
            Core::TSharedPtr<FUIRenderFrame> First,Second,Third;
            const auto Prepare = [&](Core::uint64 Id,Core::TSharedPtr<FUIRenderFrame>& Out)
            { return Session.PrepareFrame(MakePacket(Id),Settings,1,0,Scene,
                Shaders.Draw.ModuleDescriptions,Shaders.Copy.ModuleDescriptions,Out); };
            Check(Prepare(1,First) == ERHIResult::Success && First && First->HasDraws() &&
                First->GetInput() == Scene && First->GetOutput() != Scene,
                "public Renderer UI session prepares an opaque frame with private texture ownership");
            Check(Prepare(1,Third) != ERHIResult::Success && !Third,
                "Renderer UI session rejects replay of an already prepared frame identity");
            Check(Prepare(2,Second) == ERHIResult::Success && Prepare(3,Third) == ERHIResult::NotReady && !Third,
                "Renderer UI session bounds outstanding prepared frames to two");
            if (First)
            {
                Check(First->ReleaseCompleted() == ERHIResult::InvalidState &&
                    First->Commit(Device->CreateFence(false).Object) == ERHIResult::InvalidState,
                    "unrecorded UI frame cannot claim submission or completion");
                Check(First->CancelAfterCommandDiscard() == ERHIResult::Success && !First->GetOutput() &&
                    Prepare(3,Third) == ERHIResult::Success,
                    "cancelled UI frame releases its bounded preparation slot");
            }
            if (Second) (void)Second->CancelAfterCommandDiscard();
            if (Third) (void)Third->CancelAfterCommandDiscard();
            Session.BeginEligibleFrame(140,true);
            auto Graph = Core::MakeShared<FRenderGraph>("session diagnostic");
            auto Builder = Graph->CreateBuilder();
            const auto Resource = Builder.CreateTexture("widget",16,16,ERHIFormat::R8G8B8A8_sRGB,
                ERHISampleCount::One,ERHITextureUsage::Sampled | ERHITextureUsage::ColorAttachment,
                ERenderGraphColorDomain::EncodedSrgb);
            auto ProducerDesc = FRenderGraphPassDesc::Make("diagnostic",ERenderGraphPassType::Graphics);
            ProducerDesc.Accesses.push_back({Resource,ERenderGraphAccessType::Write,ERenderGraphResourceState::Write});
            const auto Producer = Builder.AddPass(ProducerDesc);
            auto ConsumerDesc = FRenderGraphPassDesc::Make("terminal UI",ERenderGraphPassType::Graphics);
            ConsumerDesc.bPreserveForSideEffects=true;
            ConsumerDesc.Accesses.push_back({Resource,ERenderGraphAccessType::Read,ERenderGraphResourceState::Read});
            const auto Consumer = Builder.AddPass(ConsumerDesc);
            Check(Graph->Compile()==ERenderGraphResult::Success,"session diagnostic graph compiles producer before UI");
            FUIDiagnosticRenderInput Diagnostic;
            Diagnostic.Source=Scene; Diagnostic.Graph=Graph; Diagnostic.Resource=Resource;
            Diagnostic.Producer=Producer; Diagnostic.Consumer=Consumer;
            Diagnostic.Selection.SourceStageId=1; Diagnostic.Selection.SourceStageName="SDRToneMap";
            Diagnostic.Selection.SourceDomain=ERenderGraphColorDomain::DisplayLinearRec709D65;
            Diagnostic.Selection.ReferenceWhiteNits=100; Diagnostic.Selection.TargetPeakNits=100;
            Diagnostic.Selection.Mode=EOutputTransformDebugBypassMode::BoundedVisualization;
            Diagnostic.Selection.VisualizationMinimum=0; Diagnostic.Selection.VisualizationMaximum=1;
            Diagnostic.Shaders=Shaders.Diagnostic.ModuleDescriptions;
            Diagnostic.RemainingAttachmentBytes=16*16*4-1;
            Core::TSharedPtr<FUIRenderFrame> Widget;
            const auto PrepareWidget = [&]() { return Session.PrepareFrame(MakePacket(4,true),Settings,1,0,Scene,
                Shaders.Draw.ModuleDescriptions,Shaders.Copy.ModuleDescriptions,Widget,&Diagnostic); };
            Check(PrepareWidget()==ERHIResult::NotReady && !Widget,
                "session diagnostic budget failure leaves the frame slot available");
            Diagnostic.RemainingAttachmentBytes=16*16*4;
            Check(PrepareWidget()==ERHIResult::Success && Widget && Widget->HasDiagnostic() &&
                Widget->GetDiagnosticAttachmentBytes()==16*16*4 && Widget->CanRecord(),
                "session retains a bounded diagnostic producer and resolves its UI snapshot");
            if (Widget)
            {
                auto WidgetCommand=Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
                Check(Widget->Record(WidgetCommand)==ERHIResult::InvalidState && Widget->CanRecord(),
                    "idle command rejection preserves diagnostic frame recording eligibility");
                (void)WidgetCommand->Begin();
                Check(Widget->Record(WidgetCommand)==ERHIResult::Success && !Widget->CanRecord(),
                    "session records diagnostic production before UI sampling in one command");
                WidgetCommand.reset();
                Check(Widget->CancelAfterCommandDiscard()==ERHIResult::Success && !Widget->HasDiagnostic() &&
                    Widget->GetDiagnosticAttachmentBytes()==0,
                    "discarded diagnostic frame releases producer and registry ownership together");
            }

        }
        FUIDrawSnapshot Clipped(1,2,1,1);
        Item.ClipRect = {-20,-20,-10,-10};
        const bool ClippedPacket = Clipped.SetDisplay({0,0},{16,16},{1,1}) && Clipped.SetVertices(Vertices) &&
            Clipped.SetIndices(Indices) && Clipped.SetCommands({&Item,1}) && Clipped.SetTextureLeases({&Lease,1}) && Clipped.Publish();
        FUICompositionFrame ClippedFrame;
        Check(ClippedPacket && FUICompositionExecutor::Prepare(Device,Clipped,Context,Settings,Registry,Scene,{}, {},ClippedFrame) ==
            ERHIResult::Success && !ClippedFrame.HasDraws() && ClippedFrame.GetOutput() == Scene,
            "fully clipped UI omits shader preparation and the composition target");
        FUIDrawSnapshot Empty(1,2,1,1);
        (void)Empty.SetDisplay({0,0},{16,16},{1,1}); (void)Empty.Publish();
        FUICompositionFrame NoUI;
        Check(FUICompositionExecutor::Prepare(Device,Empty,Context,Settings,Registry,Scene,{}, {},NoUI) == ERHIResult::Success &&
            !NoUI.HasDraws() && NoUI.GetOutput() == Scene, "empty UI retains scene output without shader or composition-target preparation");
    }
    if (Native) Failed += Metal ? RunMetalUINativeTests(Device,Shaders.Draw.ModuleDescriptions,Shaders.Copy.ModuleDescriptions,Shaders.Diagnostic.ModuleDescriptions)
        : RunVulkanUINativeTests(Device,Shaders.Draw.ModuleDescriptions,Shaders.Copy.ModuleDescriptions,Shaders.Diagnostic.ModuleDescriptions);
    (void)Device->Shutdown();
    return Failed;
}
