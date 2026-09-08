#include "UINativeRasterFixture.h"
#include "FUICompositionExecutor.h"
#include "RHI/IRHICommandQueue.h"
#include "RHI/IRHIFramebuffer.h"
#include "RHI/IRHIBuffer.h"
#include <cmath>
#include <iostream>

int RunUINativeRasterFixture(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIDevice>& Device,
    std::span<const Stoner::RHI::FRHIShaderModuleDesc> DrawShaders,
    std::span<const Stoner::RHI::FRHIShaderModuleDesc> CopyShaders,
    const FUINativeReadback& Readback)
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
        const FUITextureLease Leases[] = {Registry.Acquire(Color.TextureId),Registry.Acquire(Coverage.TextureId)};
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
        const bool Submitted = Recorded && Queue->SubmitDeferred(Command,{}, {},Fence) == ERHIResult::Success &&
            Submission.Commit(Fence) == ERHIResult::Success;
        const auto After = Device->GetRuntimeSnapshot().NativeOperations;
        Check(Submitted && After.FenceWaitCallCount == Before.FenceWaitCallCount &&
            After.QueueIdleCallCount == Before.QueueIdleCallCount && After.DeviceIdleCallCount == Before.DeviceIdleCallCount &&
            After.ImageReadbackCopyCount == Before.ImageReadbackCopyCount && After.ReadbackMapCount == Before.ReadbackMapCount &&
            After.ReadbackWaitCount == Before.ReadbackWaitCount,
            "offset/scissor UI submission introduces no synchronous wait or readback");
        if (!Submitted || Fence->Wait(5000000) != ERHIResult::Success)
        { Check(false,"native raster completion required"); Command.reset(); continue; }
        Registry.Poll();
        auto ReadCommand = Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
        const auto ReadFence = Device->CreateFence(false).Object;
        const uint64 Bytes = static_cast<uint64>(Extent)*256;
        const auto Buffer = Device->CreateBuffer({Bytes,ERHIBufferUsage::CopyDestination,ERHIMemoryAccess::HostVisible}).Object;
        FRHITextureBufferCopyRegion Region; Region.Width = Region.Height = Extent; Region.DestinationRowLengthTexels = 32;
        Transition.Texture = Frame.GetOutput(); Transition.Before = ERHIResourceLayout::ShaderReadOnly;
        Transition.After = ERHIResourceLayout::CopySource;
        TArray<uint8> Pixels;
        const bool Read = ReadCommand && ReadFence && Buffer && ReadCommand->Begin() == ERHIResult::Success &&
            ReadCommand->RecordLayoutTransition(Transition) == ERHIResult::Success &&
            ReadCommand->RecordTextureToBufferCopy(Frame.GetOutput(),Buffer,Region) == ERHIResult::Success &&
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
                if (static_cast<int>(X)<LeftEnd && static_cast<int>(Y)<LeftEnd)
                    Expected = Source[C]*AlphaValue + Background[C]*(1-AlphaValue);
                if (static_cast<int>(X)>=RightStart) Expected = Gray;
                const bool Match = std::isfinite(Actual) && std::abs(Actual-Expected) <= 0.002f+0.005f*std::abs(Expected);
                if (!Match && RGB) std::cout << "[INFO] first mismatch x=" << X << " y=" << Y << " channel=" << C
                    << " expected=" << Expected << " actual=" << Actual << '\n';
                RGB &= Match;
            }
            Check(RGB,"all pixels match sRGB-before-filtering, indexed offsets, state reset and signed scaled scissors");
            Check(Opaque,"RGB-only UI preserves copied alpha one at every native pixel");
        }
        ReadCommand.reset(); Command.reset();
    }
    return Failed;
}
