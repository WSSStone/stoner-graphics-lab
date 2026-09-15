#include "FUICompositionExecutor.h"
#include "RHI/FRHIBufferUploadDesc.h"
#include "RHI/IRHIBuffer.h"
#include "RHI/IRHIDescriptorSet.h"
#include "RHI/IRHIFramebuffer.h"
#include "RHI/IRHIGraphicsPipeline.h"
#include "RHI/IRHIPipelineLayout.h"
#include "RHI/IRHIRenderPass.h"
#include "RHI/IRHISampler.h"
#include "RHI/IRHIShaderModule.h"
#include "RHI/IRHITexture.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <new>

namespace Stoner::Renderer
{
using namespace Stoner::Core;
using namespace Stoner::RHI;
namespace
{
struct FPass
{
    TArray<TSharedPtr<IRHIShaderModule>> Modules;
    TSharedPtr<IRHIPipelineLayout> Layout;
    TSharedPtr<IRHIGraphicsPipeline> Pipeline;
    TSharedPtr<IRHIRenderPass> Pass;
    TSharedPtr<IRHIFramebuffer> Framebuffer;
};
bool MakePass(IRHIDevice& Device, std::span<const FRHIShaderModuleDesc> Shaders,
    const TSharedPtr<IRHITexture>& Target, bool Draw, FPass& Out, bool Diagnostic = false)
{
    if (Shaders.size() != 2) return false;
    bool Vertex = false, Fragment = false;
    for (const auto& Desc : Shaders)
    {
        if (Desc.Stage == ERHIShaderStage::Vertex && !Vertex) Vertex = true;
        else if (Desc.Stage == ERHIShaderStage::Fragment && !Fragment) Fragment = true;
        else return false;
        auto Module = Device.CreateShaderModule(Desc);
        if (!Module.Succeeded()) return false;
        Out.Modules.push_back(std::move(Module.Object));
    }
    FRHIPipelineLayoutDesc Layout;
    Layout.Bindings.push_back({0,0,ERHIDescriptorType::CombinedTextureSampler,1,ERHIShaderStageFlags::Fragment});
    if (Draw || Diagnostic) Layout.Bindings.push_back({0,1,ERHIDescriptorType::UniformBuffer,1,
        Draw ? ERHIShaderStageFlags::Vertex | ERHIShaderStageFlags::Fragment : ERHIShaderStageFlags::Fragment});
    auto CreatedLayout = Device.CreatePipelineLayout(Layout);
    if (!CreatedLayout.Succeeded()) return false;
    Out.Layout = std::move(CreatedLayout.Object);
    FRHIGraphicsPipelineDesc Pipeline;
    Pipeline.ShaderModules = Out.Modules; Pipeline.PipelineLayout = Out.Layout;
    Pipeline.VertexInput = Draw ? FRHIVertexInputDesc{20,{{0,ERHIFormat::R32G32_Float,0},
        {1,ERHIFormat::R32G32_Float,8},{2,ERHIFormat::R8G8B8A8_UNorm,16}}}
        : FRHIVertexInputDesc{8,{{0,ERHIFormat::R32G32_Float,0}}};
    Pipeline.Rasterizer.CullMode = ERHICullMode::None;
    Pipeline.Blend.bEnabled = Draw;
    Pipeline.Blend.SourceColor = Draw ? ERHIBlendFactor::SourceAlpha : ERHIBlendFactor::One;
    Pipeline.Blend.DestinationColor = Draw ? ERHIBlendFactor::OneMinusSourceAlpha : ERHIBlendFactor::Zero;
    Pipeline.Blend.ColorWriteMask = Draw ? ERHIColorWriteMask::RGB : ERHIColorWriteMask::RGBA;
    Pipeline.RenderTargets.ColorFormats = {Target->GetFormat()};
    Pipeline.RuntimeMode = Device.GetRuntimeSnapshot().ObjectMode;
    Pipeline.CompatibilitySummary = Draw ? "Lab.UIDraw" : Diagnostic ? "Lab.UIDiagnostic" : "Lab.UICopy";
    auto CreatedPipeline = Device.CreateGraphicsPipeline(Pipeline);
    if (!CreatedPipeline.Succeeded()) return false;
    Out.Pipeline = std::move(CreatedPipeline.Object);
    auto Pass = Device.CreateRenderPass({{{ERHIAttachmentRole::Color,Target->GetFormat(),
        ERHISampleCount::One, Draw ? ERHIAttachmentLoadOp::Load : ERHIAttachmentLoadOp::Clear,
        ERHIAttachmentStoreOp::Store}}});
    if (!Pass.Succeeded()) return false;
    Out.Pass = std::move(Pass.Object);
    FRHIFramebufferDesc Framebuffer;
    Framebuffer.RenderPass = Out.Pass; Framebuffer.Attachments = {{Target,0,0}};
    Framebuffer.Width = Target->GetDesc().Width; Framebuffer.Height = Target->GetDesc().Height;
    auto CreatedFramebuffer = Device.CreateFramebuffer(Framebuffer);
    if (!CreatedFramebuffer.Succeeded()) return false;
    Out.Framebuffer = std::move(CreatedFramebuffer.Object);
    return true;
}
TSharedPtr<IRHIBuffer> Buffer(IRHIDevice& Device, const void* Data, uint64 Size, ERHIBufferUsage Usage)
{
    auto Created = Device.CreateBuffer({Size,Usage,ERHIMemoryAccess::HostVisible});
    if (!Created.Succeeded() || Device.UploadBuffer(Created.Object,{0,Data,Size}) != ERHIResult::Success) return {};
    return Created.Object;
}
}
struct FUIDiagnosticFrame::FImpl
{
    TSharedPtr<IRHITexture> Source, Output;
    FPass Pass;
    TSharedPtr<IRHIBuffer> Fullscreen, Parameters;
    TSharedPtr<IRHISampler> Sampler;
    TSharedPtr<IRHIDescriptorSet> Set;
    bool bAttempted = false;
};
TSharedPtr<IRHITexture> FUIDiagnosticFrame::GetOutput() const noexcept { return Impl ? Impl->Output : nullptr; }
bool FUIDiagnosticFrame::CanRecord() const noexcept
{
    return Impl && !Impl->bAttempted && Impl->Source->GetLifecycleState()==ERHIResourceLifecycleState::Valid &&
        Impl->Output->GetLifecycleState()==ERHIResourceLifecycleState::Valid;
}
ERHIResult FUICompositionExecutor::PrepareDiagnostic(const TSharedPtr<IRHIDevice>& Device,
    const TSharedPtr<IRHITexture>& Source, const FResolvedOutputTransformDebugBypass& Selection,
    std::span<const FRHIShaderModuleDesc> Shaders, uint64 RemainingBytes, FUIDiagnosticFrame& OutFrame)
{
    if (OutFrame.Impl && OutFrame.Impl->bAttempted) return ERHIResult::InvalidState;
    if (!Device || !Device->IsActive() || !Source || Source->GetLifecycleState()!=ERHIResourceLifecycleState::Valid ||
        !Selection.IsValid() || Selection.Mode!=EOutputTransformDebugBypassMode::BoundedVisualization ||
        (Selection.SourceDomain!=ERenderGraphColorDomain::SceneLinearRec709D65 &&
         Selection.SourceDomain!=ERenderGraphColorDomain::DisplayLinearRec709D65 &&
         Selection.SourceDomain!=ERenderGraphColorDomain::DisplayLinearRec2020D65)) return ERHIResult::InvalidState;
    const auto& Desc=Source->GetDesc();
    if (Desc.Format!=ERHIFormat::R16G16B16A16_Float || Desc.Dimension!=ERHITextureDimension::Texture2D ||
        Desc.Width==0 || Desc.Height==0 || Desc.MipLevels!=1 || Desc.ArrayLayers!=1 || Desc.Depth!=1 ||
        Desc.SampleCount!=ERHISampleCount::One || !HasRHIFlag(Desc.Usage,ERHITextureUsage::Sampled)) return ERHIResult::InvalidState;
    const uint64 Largest=std::max<uint64>(1024,std::max(Desc.Width,Desc.Height));
    const auto Width=static_cast<uint32>(std::max<uint64>(1,uint64{Desc.Width}*1024/Largest));
    const auto Height=static_cast<uint32>(std::max<uint64>(1,uint64{Desc.Height}*1024/Largest));
    if (uint64{Width}*Height*4>RemainingBytes) return ERHIResult::NotReady;
    try
    {
        auto Candidate=MakeShared<FUIDiagnosticFrame::FImpl>(); Candidate->Source=Source;
        FRHITextureDesc Target; Target.Width=Width; Target.Height=Height; Target.Format=ERHIFormat::R8G8B8A8_sRGB;
        Target.Usage=ERHITextureUsage::ColorAttachment | ERHITextureUsage::Sampled;
        auto Output=Device->CreateTexture(Target); if (!Output.Succeeded()) return Output.Result;
        Candidate->Output=std::move(Output.Object);
        if (!MakePass(*Device,Shaders,Candidate->Output,false,Candidate->Pass,true)) return ERHIResult::Unavailable;
        constexpr float Vertices[]={-1,-1,3,-1,-1,3};
        const std::array<float,4> Parameters{Selection.VisualizationMinimum,Selection.VisualizationMaximum,0,0};
        Candidate->Fullscreen=Buffer(*Device,Vertices,sizeof(Vertices),ERHIBufferUsage::Vertex);
        Candidate->Parameters=Buffer(*Device,Parameters.data(),sizeof(Parameters),ERHIBufferUsage::Uniform);
        FRHISamplerDesc Sampling; Sampling.MinFilter=Sampling.MagFilter=ERHISamplerFilter::Linear;
        Sampling.MipFilter=ERHISamplerMipFilter::None;
        Sampling.AddressU=Sampling.AddressV=Sampling.AddressW=ERHISamplerAddressMode::ClampToEdge;
        auto Sampler=Device->CreateSampler(Sampling); auto Set=Device->CreateDescriptorSet(Candidate->Pass.Layout,0);
        if (!Candidate->Fullscreen || !Candidate->Parameters || !Sampler.Succeeded() || !Set.Succeeded()) return ERHIResult::Unavailable;
        Candidate->Sampler=std::move(Sampler.Object); Candidate->Set=std::move(Set.Object);
        auto Status=Candidate->Set->UpdateCombinedTextureSampler(0,0,Source,Candidate->Sampler);
        if (Status==ERHIResult::Success) Status=Candidate->Set->UpdateBuffer(1,0,Candidate->Parameters);
        if (Status!=ERHIResult::Success) return Status;
        OutFrame.Impl=std::move(Candidate); return ERHIResult::Success;
    }
    catch (const std::bad_alloc&) { return ERHIResult::Unavailable; }
}
ERHIResult FUICompositionExecutor::RecordDiagnostic(FUIDiagnosticFrame& Frame, const TSharedPtr<IRHICommandBuffer>& Command)
{
    if (!Command || Command->GetState()!=ERHICommandBufferState::Recording || !Frame.CanRecord()) return ERHIResult::InvalidState;
    auto& S=*Frame.Impl; S.bAttempted=true;
    ERHIResult Status=ERHIResult::Success;
    const auto Step=[&](ERHIResult R) { Status=R; return R==ERHIResult::Success; };
    FRHIResourceBarrierDesc Transition; Transition.Texture=S.Output; Transition.After=ERHIResourceLayout::ColorAttachment;
    const auto& Desc=S.Output->GetDesc();
    if (!Step(Command->RecordLayoutTransition(Transition)) ||
        !Step(Command->BeginRenderPass(S.Pass.Pass,S.Pass.Framebuffer)) ||
        !Step(Command->SetViewport({0,0,float(Desc.Width),float(Desc.Height),0,1})) ||
        !Step(Command->SetScissor({0,0,Desc.Width,Desc.Height})) ||
        !Step(Command->BindGraphicsPipeline(S.Pass.Pipeline)) || !Step(Command->BindVertexBuffer(S.Fullscreen)) ||
        !Step(Command->BindDescriptorSet(S.Set)) || !Step(Command->RecordDraw(3)) || !Step(Command->EndRenderPass())) return Status;
    Transition.Before=ERHIResourceLayout::ColorAttachment; Transition.After=ERHIResourceLayout::ShaderReadOnly;
    return Command->RecordLayoutTransition(Transition);
}
struct FUICompositionFrame::FImpl
{
    TSharedPtr<IRHITexture> Scene, Output;
    TArray<FUITextureLease> TextureLeases;
    FUIGpuTextureContext GpuContext;
    TArray<FUIValidatedCommand> Commands;
    FPass Copy, Draw;
    TSharedPtr<IRHIBuffer> Vertices, Indices, Fullscreen, Uniform;
    TSharedPtr<IRHISampler> Sampler;
    TSharedPtr<IRHIDescriptorSet> CopySet;
    TArray<TSharedPtr<IRHIDescriptorSet>> TextureSets;
    bool bRecorded = false;
};
TSharedPtr<IRHITexture> FUICompositionFrame::GetOutput() const noexcept { return Impl ? Impl->Output : nullptr; }
bool FUICompositionFrame::HasDraws() const noexcept { return Impl && Impl->Output != Impl->Scene; }
uint64 FUICompositionFrame::GetRetainedDrawBytes() const noexcept
{
    // Geometry has already been uploaded. Only validated commands and texture
    // generation leases remain CPU-owned until render retirement.
    return Impl ? Impl->TextureLeases.capacity()*sizeof(FUITextureLease) +
        Impl->Commands.capacity()*sizeof(FUIValidatedCommand) : 0;
}
bool FUICompositionFrame::CanRecord(const FUITextureRegistry& Registry) const noexcept
{ return Impl && !Impl->bRecorded && (!HasDraws() ||
    Registry.CanRecordSubmission(Impl->TextureLeases,Impl->GpuContext.Graph ? &Impl->GpuContext : nullptr) == ERHIResult::Success); }
ERHIResult FUICompositionExecutor::Prepare(const TSharedPtr<IRHIDevice>& Device,
    const FUIDrawSnapshot& Snapshot, const FUIDrawValidationContext& Context,
    const FUICompositionSettings& Settings, FUITextureRegistry& Registry,
    const TSharedPtr<IRHITexture>& Scene, std::span<const FRHIShaderModuleDesc> DrawShaders,
    std::span<const FRHIShaderModuleDesc> CopyShaders, FUICompositionFrame& OutFrame,
    const FUIGpuTextureContext* GpuContext)
{
    if (!Device || !Device->IsActive() || !Scene || Scene->GetLifecycleState() != ERHIResourceLifecycleState::Valid ||
        !Settings.IsValid() || Settings.DisplayGeneration != Context.DisplayGeneration ||
        Scene->GetFormat() != ERHIFormat::R16G16B16A16_Float ||
        Scene->GetDesc().Dimension != ERHITextureDimension::Texture2D || Scene->GetDesc().ArrayLayers != 1 ||
        Scene->GetDesc().MipLevels != 1 || Scene->GetDesc().SampleCount != ERHISampleCount::One ||
        Scene->GetDesc().Width != Context.DrawableWidth || Scene->GetDesc().Height != Context.DrawableHeight ||
        !HasRHIFlag(Scene->GetUsage(),ERHITextureUsage::Sampled)) return ERHIResult::InvalidState;
    if (GpuContext && (GpuContext->FrameId!=Snapshot.GetFrameId() || GpuContext->SettingsRevision!=Context.SettingsRevision ||
        GpuContext->DisplayGeneration!=Context.DisplayGeneration)) return ERHIResult::InvalidState;
    try
    {
        auto OwnedContext = Context;
        OwnedContext.HasTextureLease = [&](FUITextureId Id)
        {
            const auto& Leases = Snapshot.GetTextureLeases();
            return std::any_of(Leases.begin(),Leases.end(),[&](const auto& Lease)
                { return Lease.GetId() == Id && Registry.ResolveTexture(Lease) != nullptr; });
        };
        auto Validated = FUIDrawValidator::Validate(Snapshot,OwnedContext);
        if (!Validated.bValid || Snapshot.GetTextureIds().size() != Snapshot.GetTextureLeases().size())
            return ERHIResult::InvalidState;
        const bool HasDraws = std::any_of(Validated.Commands.begin(),Validated.Commands.end(),
            [](const auto& C) { return C.Draw.Operation == EUIDrawOperation::Draw; });
        auto Candidate = MakeShared<FUICompositionFrame::FImpl>();
        Candidate->Scene = Candidate->Output = Scene;
        if (!HasDraws) { OutFrame.Impl = std::move(Candidate); return ERHIResult::Success; }
        Registry.Poll();
        const auto Available = Registry.CanRecordSubmission(Snapshot.GetTextureLeases(),GpuContext);
        if (Available != ERHIResult::Success) return Available;
        if (DrawShaders.size() != 2 || CopyShaders.size() != 2) return ERHIResult::InvalidState;
        Candidate->TextureLeases.assign(Snapshot.GetTextureLeases().begin(),Snapshot.GetTextureLeases().end());
        if (GpuContext) Candidate->GpuContext=*GpuContext;
        Candidate->Commands = std::move(Validated.Commands);
        FRHITextureDesc Target = Scene->GetDesc();
        Target.Usage = ERHITextureUsage::ColorAttachment | ERHITextureUsage::Sampled | ERHITextureUsage::CopySource;
        auto Created = Device->CreateTexture(Target);
        if (!Created.Succeeded()) return Created.Result;
        Candidate->Output = std::move(Created.Object);
        if (!MakePass(*Device,CopyShaders,Candidate->Output,false,Candidate->Copy) ||
            !MakePass(*Device,DrawShaders,Candidate->Output,true,Candidate->Draw)) return ERHIResult::Unavailable;
        FRHISamplerDesc Sampling;
        Sampling.MipFilter = ERHISamplerMipFilter::None;
        Sampling.AddressU = Sampling.AddressV = Sampling.AddressW = ERHISamplerAddressMode::ClampToEdge;
        auto Sampler = Device->CreateSampler(Sampling);
        if (!Sampler.Succeeded()) return Sampler.Result;
        Candidate->Sampler = std::move(Sampler.Object);
        TArray<uint8> VertexBytes(Snapshot.GetVertices().size() * 20);
        for (std::size_t I = 0; I < Snapshot.GetVertices().size(); ++I)
        {
            const auto& V = Snapshot.GetVertices()[I];
            const float PositionUV[] = {V.Position.X,V.Position.Y,V.UV.X,V.UV.Y};
            std::memcpy(VertexBytes.data()+I*20,PositionUV,sizeof(PositionUV));
            for (uint32 C = 0; C < 4; ++C) VertexBytes[I*20+16+C] = static_cast<uint8>(V.PackedRGBA8 >> (C*8));
        }
        Candidate->Vertices = Buffer(*Device,VertexBytes.data(),VertexBytes.size(),ERHIBufferUsage::Vertex);
        Candidate->Indices = Buffer(*Device,Snapshot.GetIndices().data(),Snapshot.GetIndices().size()*4,ERHIBufferUsage::Index);
        constexpr float Fullscreen[] = {-1,-1,3,-1,-1,3};
        Candidate->Fullscreen = Buffer(*Device,Fullscreen,sizeof(Fullscreen),ERHIBufferUsage::Vertex);
        const auto& Size = Snapshot.GetDisplaySize(); const auto& Position = Snapshot.GetDisplayPos();
        const auto* Profile = FOutputTransformSettingsValidator().FindProfile(Settings.OutputProfileId);
        const float White = Profile->DynamicRange == EOutputDynamicRange::SDR ? 1.0f : Settings.UIReferenceWhiteNits;
        const std::array<float,8> Parameters{2.0f/Size.X,2.0f/Size.Y,-1.0f-Position.X*2.0f/Size.X,
            -1.0f-Position.Y*2.0f/Size.Y,White*Settings.UIWhiteMultiplier,
            Settings.BlendDomain == ERenderGraphColorDomain::DisplayLinearRec2020D65 ? 1.0f : 0.0f,0,0};
        if (!std::all_of(Parameters.begin(),Parameters.end(),[](float V) { return std::isfinite(V); }))
            return ERHIResult::InvalidState;
        Candidate->Uniform = Buffer(*Device,Parameters.data(),sizeof(Parameters),ERHIBufferUsage::Uniform);
        if (!Candidate->Vertices || !Candidate->Indices || !Candidate->Fullscreen || !Candidate->Uniform)
            return ERHIResult::Unavailable;
        auto CopySet = Device->CreateDescriptorSet(Candidate->Copy.Layout,0);
        if (!CopySet.Succeeded() || CopySet.Object->UpdateCombinedTextureSampler(0,0,Scene,Candidate->Sampler) != ERHIResult::Success)
            return ERHIResult::Unavailable;
        Candidate->CopySet = std::move(CopySet.Object);
        for (const auto& Lease : Snapshot.GetTextureLeases())
        {
            auto Texture = Registry.ResolveTexture(Lease);
            if (!Texture || Texture->GetLifecycleState() != ERHIResourceLifecycleState::Valid) return ERHIResult::InvalidState;
            auto Set = Device->CreateDescriptorSet(Candidate->Draw.Layout,0);
            if (!Set.Succeeded() || Set.Object->UpdateCombinedTextureSampler(0,0,Texture,Candidate->Sampler) != ERHIResult::Success ||
                Set.Object->UpdateBuffer(1,0,Candidate->Uniform) != ERHIResult::Success) return ERHIResult::Unavailable;
            Candidate->TextureSets.push_back(std::move(Set.Object));
        }
        OutFrame.Impl = std::move(Candidate);
        return ERHIResult::Success;
    }
    catch (const std::bad_alloc&) { return ERHIResult::Unavailable; }
}
ERHIResult FUICompositionExecutor::Record(FUICompositionFrame& Frame, FUITextureRegistry& Registry,
    const TSharedPtr<IRHICommandBuffer>& Command, FUITextureSubmission& Submission)
{
    if (!Frame.Impl || Frame.Impl->bRecorded || !Command || Command->GetState() != ERHICommandBufferState::Recording)
        return ERHIResult::InvalidState;
    auto& S = *Frame.Impl;
    S.bRecorded = true;
    if (!Frame.HasDraws()) return ERHIResult::Success;
    auto Result = Registry.RecordSubmission(S.TextureLeases,Command,Submission,S.GpuContext.Graph ? &S.GpuContext : nullptr);
    if (Result != ERHIResult::Success) return Result;
    const auto Width = S.Output->GetDesc().Width, Height = S.Output->GetDesc().Height;
    const auto Step = [&](ERHIResult R) { if (Result == ERHIResult::Success) Result = R; return R == ERHIResult::Success; };
    FRHIResourceBarrierDesc Transition;
    Transition.Texture = S.Output; Transition.After = ERHIResourceLayout::ColorAttachment;
    if (!Step(Command->RecordLayoutTransition(Transition)) ||
        !Step(Command->BeginRenderPass(S.Copy.Pass,S.Copy.Framebuffer)) ||
        !Step(Command->SetViewport({0,0,static_cast<float>(Width),static_cast<float>(Height),0,1})) ||
        !Step(Command->SetScissor({0,0,Width,Height})) ||
        !Step(Command->BindGraphicsPipeline(S.Copy.Pipeline)) ||
        !Step(Command->BindVertexBuffer(S.Fullscreen)) || !Step(Command->BindDescriptorSet(S.CopySet)) ||
        !Step(Command->RecordDraw(3)) || !Step(Command->EndRenderPass())) return Result;
    // Preserve the color layout while ordering copy writes before load/blend.
    if (!Step(Command->RecordBarrier()) ||
        !Step(Command->BeginRenderPass(S.Draw.Pass,S.Draw.Framebuffer))) return Result;
    const auto BindState = [&]()
    {
        return Step(Command->SetViewport({0,0,static_cast<float>(Width),static_cast<float>(Height),0,1})) &&
            Step(Command->BindGraphicsPipeline(S.Draw.Pipeline)) && Step(Command->BindVertexBuffer(S.Vertices)) &&
            Step(Command->BindIndexBuffer(S.Indices,ERHIIndexType::UInt32));
    };
    if (!BindState()) return Result;
    for (const auto& C : S.Commands)
    {
        if (C.Draw.Operation == EUIDrawOperation::ResetState)
        { if (!BindState()) return Result; continue; }
        const auto& Leases = S.TextureLeases;
        const auto It = std::find_if(Leases.begin(),Leases.end(),[&](const auto& L) { return L.GetId() == C.Draw.TextureId; });
        if (It == Leases.end()) return ERHIResult::InvalidState;
        if (!Step(Command->SetScissor({C.ScissorX,C.ScissorY,C.ScissorWidth,C.ScissorHeight})) ||
            !Step(Command->BindDescriptorSet(S.TextureSets[static_cast<std::size_t>(It-Leases.begin())])) ||
            !Step(Command->RecordDrawIndexed({C.Draw.IndexCount,1,C.Draw.FirstIndex,C.Draw.BaseVertex,0}))) return Result;
    }
    if (!Step(Command->EndRenderPass())) return Result;
    Transition.Before = ERHIResourceLayout::ColorAttachment; Transition.After = ERHIResourceLayout::ShaderReadOnly;
    return Command->RecordLayoutTransition(Transition);
}
}
