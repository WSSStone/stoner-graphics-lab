#include "FProductionContentDeferredExecution.h"

#include "Renderer/FShaderAssetConversion.h"
#include "RHI/FRHIBufferUploadDesc.h"
#include "RHI/IRHIBuffer.h"
#include "RHI/IRHIFramebuffer.h"
#include "RHI/IRHIGraphicsPipeline.h"
#include "RHI/IRHIDescriptorSet.h"
#include "RHI/IRHIDevice.h"
#include "RHI/IRHIPipelineLayout.h"
#include "RHI/IRHIRenderPass.h"
#include "RHI/IRHISampler.h"
#include "RHI/IRHIShaderModule.h"
#include "RHI/IRHITexture.h"

#include <algorithm>
#include <array>
#include <map>
#include <set>

namespace Stoner::Demo
{
namespace
{

using namespace Stoner::Asset;
using namespace Stoner::Core;
using namespace Stoner::Renderer;
using namespace Stoner::RHI;

void Fail(FString* OutReason, const char* Reason)
{
    if (OutReason) *OutReason = Reason;
}

template <typename T>
void InvalidateReverse(TArray<TSharedPtr<T>>& Resources) noexcept
{
    for (auto It = Resources.rbegin(); It != Resources.rend(); ++It)
        if (*It) (void)(*It)->Invalidate();
    Resources.clear();
}

class FPayloadLookup final : public IShaderPayloadLookup
{
public:
    explicit FPayloadLookup(
        const TArray<TSharedPtr<const FShaderPayloadAsset>>& Payloads)
    {
        for (const auto& Payload : Payloads)
            if (Payload) Payloads_.try_emplace(Payload->GetId(), Payload);
    }

    TSharedPtr<const FShaderPayloadAsset> Find(
        const FAssetId& Id) const override
    {
        const auto Found = Payloads_.find(Id);
        return Found == Payloads_.end() ? nullptr : Found->second;
    }

private:
    std::map<FAssetId, TSharedPtr<const FShaderPayloadAsset>> Payloads_;
};

EShaderBackendFamily ShaderBackend(EAssetGraphicsBackend Backend)
{
    return Backend == EAssetGraphicsBackend::Metal
        ? EShaderBackendFamily::Metal : EShaderBackendFamily::Vulkan;
}

FShaderTargetRequest MakeShaderTarget(
    const FAssetTargetProfileEvidence& Evidence)
{
    FShaderTargetRequest Result;
    Result.Backend = ShaderBackend(Evidence.Profile.GraphicsBackend);
    Result.CpuArchitecture = Evidence.Profile.CpuArchitecture;
    for (const auto& Choice : Evidence.Profile.ShaderPayloadChoices)
    {
        if (Choice.Backend == Evidence.Profile.GraphicsBackend &&
            std::find(Result.AcceptableProfiles.begin(),
                Result.AcceptableProfiles.end(), Choice.Profile) ==
                Result.AcceptableProfiles.end())
            Result.AcceptableProfiles.push_back(Choice.Profile);
    }
    return Result;
}

const FShaderAsset* FindProgram(
    const TArray<TSharedPtr<const FShaderAsset>>& Programs,
    const char* Leaf)
{
    const FString Expected(
        std::string("Engine/Shaders/Deferred/") + Leaf);
    const auto Found = std::find_if(
        Programs.begin(), Programs.end(),
        [&Expected](const auto& Program)
        {
            return Program &&
                Program->GetDesc().Id.GetLogicalPath() == Expected;
        });
    return Found == Programs.end() ? nullptr : Found->get();
}

bool BuildLayout(
    const TArray<FRHIShaderModuleDesc>& Modules,
    FRHIPipelineLayoutDesc& OutLayout)
{
    FRHIPipelineLayoutDesc Candidate;
    for (const auto& Module : Modules)
    {
        for (const auto& Required : Module.InterfaceMetadata.Bindings)
        {
            auto Found = std::find_if(
                Candidate.Bindings.begin(), Candidate.Bindings.end(),
                [&Required](const auto& Existing)
                {
                    return Existing.SetIndex == Required.SetIndex &&
                        Existing.BindingSlot == Required.BindingSlot;
                });
            if (Found == Candidate.Bindings.end())
                Candidate.Bindings.push_back({
                    Required.SetIndex, Required.BindingSlot,
                    Required.DescriptorType, Required.ArrayCount,
                    Required.Visibility});
            else if (Found->DescriptorType == Required.DescriptorType &&
                     Found->ArrayCount == Required.ArrayCount)
                Found->Visibility |= Required.Visibility;
            else
                return false;
        }
        for (const auto& Required : Module.InterfaceMetadata.ConstantRanges)
        {
            auto Found = std::find_if(
                Candidate.ConstantRanges.begin(),
                Candidate.ConstantRanges.end(),
                [&Required](const auto& Existing)
                {
                    return Existing.OffsetBytes == Required.OffsetBytes &&
                        Existing.SizeBytes == Required.SizeBytes;
                });
            if (Found == Candidate.ConstantRanges.end())
                Candidate.ConstantRanges.push_back(Required);
            else
                Found->Visibility |= Required.Visibility;
        }
    }
    std::sort(Candidate.Bindings.begin(), Candidate.Bindings.end(),
        [](const auto& Left, const auto& Right)
        {
            return Left.SetIndex != Right.SetIndex
                ? Left.SetIndex < Right.SetIndex
                : Left.BindingSlot < Right.BindingSlot;
        });
    if (!IsValidRHIPipelineLayoutDesc(Candidate)) return false;
    OutLayout = std::move(Candidate);
    return true;
}

struct FProgramResources
{
    TArray<TSharedPtr<IRHIShaderModule>> Modules;
    TSharedPtr<IRHIPipelineLayout> Layout;
};

bool CreateProgram(
    IRHIDevice& Device,
    const FShaderAsset& Program,
    const FShaderTargetRequest& Target,
    const FPayloadLookup& Lookup,
    FProductionContentDeferredExecutionResources& Owner,
    FProgramResources& Out)
{
    FSelectedShaderProgram Selected;
    FShaderAssetSnapshot Snapshot;
    if (SelectShaderProgram(Program, Target, Lookup, Selected) !=
            EAssetResult::Success ||
        ConvertShaderAsset({&Selected}, Snapshot) !=
            EMaterialResult::Success ||
        Snapshot.ModuleDescriptions.size() != 2)
        return false;

    FRHIPipelineLayoutDesc LayoutDesc;
    if (!BuildLayout(Snapshot.ModuleDescriptions, LayoutDesc)) return false;
    FProgramResources Candidate;
    for (const auto& Desc : Snapshot.ModuleDescriptions)
    {
        auto Module = Device.CreateShaderModule(Desc);
        if (!Module.Succeeded()) return false;
        Candidate.Modules.push_back(Module.Object);
        Owner.OwnedShaders.push_back(std::move(Module.Object));
    }
    auto Layout = Device.CreatePipelineLayout(LayoutDesc);
    if (!Layout.Succeeded()) return false;
    Candidate.Layout = Layout.Object;
    Owner.OwnedLayouts.push_back(std::move(Layout.Object));
    Out = std::move(Candidate);
    return true;
}

TSharedPtr<IRHIBuffer> FindFrameBuffer(
    const FStaticModelRenderSnapshot& Snapshot)
{
    if (Snapshot.GetDrawResources().empty()) return {};
    for (const auto& Binding :
         Snapshot.GetDrawResources().front().BufferBindings)
        if (Binding.SetIndex == 0 && Binding.BindingSlot == 0)
            return Binding.Buffer;
    return {};
}

bool CreateUploadedBuffer(
    IRHIDevice& Device,
    const void* Bytes,
    uint64 ByteCount,
    ERHIBufferUsage Usage,
    FProductionContentDeferredExecutionResources& Owner,
    TSharedPtr<IRHIBuffer>& Out)
{
    if (!Bytes || ByteCount == 0) return false;
    auto Buffer = Device.CreateBuffer({
        ByteCount, Usage | ERHIBufferUsage::CopyDestination,
        ERHIMemoryAccess::DeviceLocal});
    if (!Buffer.Succeeded() ||
        Device.UploadBuffer(Buffer.Object, {0, Bytes, ByteCount}) !=
            ERHIResult::Success)
        return false;
    Out = Buffer.Object;
    Owner.OwnedBuffers.push_back(std::move(Buffer.Object));
    return true;
}

bool CreateTexture(
    IRHIDevice& Device,
    uint32 Width,
    uint32 Height,
    ERHIFormat Format,
    ERHITextureUsage Usage,
    FProductionContentDeferredExecutionResources& Owner,
    TSharedPtr<IRHITexture>& Out)
{
    FRHITextureDesc Desc;
    Desc.Width = Width;
    Desc.Height = Height;
    Desc.Format = Format;
    Desc.Usage = Usage;
    auto Texture = Device.CreateTexture(Desc);
    if (!Texture.Succeeded()) return false;
    Out = Texture.Object;
    Owner.OwnedTextures.push_back(std::move(Texture.Object));
    return true;
}

bool CreatePass(
    IRHIDevice& Device,
    TArray<FRHIRenderPassAttachmentDesc> Attachments,
    TArray<FRHIFramebufferAttachment> Targets,
    uint32 Width,
    uint32 Height,
    FProductionContentDeferredExecutionResources& Owner,
    TSharedPtr<IRHIRenderPass>& OutPass,
    TSharedPtr<IRHIFramebuffer>& OutFramebuffer)
{
    auto Pass = Device.CreateRenderPass({std::move(Attachments)});
    if (!Pass.Succeeded()) return false;
    FRHIFramebufferDesc Desc;
    Desc.RenderPass = Pass.Object;
    Desc.Attachments = std::move(Targets);
    Desc.Width = Width;
    Desc.Height = Height;
    auto Framebuffer = Device.CreateFramebuffer(Desc);
    if (!Framebuffer.Succeeded()) return false;
    OutPass = Pass.Object;
    OutFramebuffer = Framebuffer.Object;
    Owner.OwnedRenderPasses.push_back(std::move(Pass.Object));
    Owner.OwnedFramebuffers.push_back(std::move(Framebuffer.Object));
    return true;
}

bool CreatePipeline(
    IRHIDevice& Device,
    const FProgramResources& Program,
    const FDeferredVertexLayoutContract& VertexLayout,
    TArray<ERHIFormat> ColorFormats,
    ERHIFormat DepthFormat,
    bool bAdditive,
    ERHICullMode CullMode,
    const char* Name,
    FProductionContentDeferredExecutionResources& Owner,
    TSharedPtr<IRHIGraphicsPipeline>& Out)
{
    FRHIGraphicsPipelineDesc Desc;
    Desc.ShaderModules = Program.Modules;
    Desc.PipelineLayout = Program.Layout;
    Desc.VertexInput.Stride = VertexLayout.Stride;
    Desc.VertexInput.Attributes = VertexLayout.Attributes;
    Desc.Rasterizer.CullMode = CullMode;
    Desc.Blend.bEnabled = bAdditive;
    if (bAdditive)
    {
        Desc.Blend.SourceColor = ERHIBlendFactor::One;
        Desc.Blend.DestinationColor = ERHIBlendFactor::One;
    }
    Desc.DepthStencil.bDepthTestEnabled = DepthFormat != ERHIFormat::Unknown;
    Desc.DepthStencil.bDepthWriteEnabled = DepthFormat != ERHIFormat::Unknown;
    Desc.RenderTargets.ColorFormats = std::move(ColorFormats);
    Desc.RenderTargets.DepthStencilFormat = DepthFormat;
    Desc.RenderTargets.SampleCount = ERHISampleCount::One;
    Desc.RuntimeMode = Device.GetRuntimeSnapshot().ObjectMode;
    Desc.CompatibilitySummary = Name;
    auto Pipeline = Device.CreateGraphicsPipeline(Desc);
    if (!Pipeline.Succeeded()) return false;
    Out = Pipeline.Object;
    Owner.OwnedPipelines.push_back(std::move(Pipeline.Object));
    return true;
}

bool CreateDescriptors(
    IRHIDevice& Device,
    const TSharedPtr<IRHIPipelineLayout>& Layout,
    const TSharedPtr<IRHIBuffer>& Frame,
    const TSharedPtr<IRHIBuffer>& Lights,
    const std::array<TSharedPtr<IRHITexture>, 5>& Textures,
    const TSharedPtr<IRHISampler>& Sampler,
    FProductionContentDeferredExecutionResources& Owner,
    TArray<TSharedPtr<IRHIDescriptorSet>>& Out)
{
    if (!Layout) return false;
    std::set<uint32> Sets;
    for (const auto& Binding : Layout->GetDesc().Bindings)
        Sets.insert(Binding.SetIndex);
    for (const uint32 Set : Sets)
    {
        auto Created = Device.CreateDescriptorSet(Layout, Set);
        if (!Created.Succeeded()) return false;
        for (const auto& Binding : Layout->GetDesc().Bindings)
        {
            if (Binding.SetIndex != Set || Binding.ArrayCount != 1)
                continue;
            ERHIResult Result = ERHIResult::InvalidState;
            if (Set == 0 && Binding.BindingSlot == 0 && Frame)
                Result = Created.Object->UpdateBuffer(0, 0, Frame);
            else if (Set == 3 && Binding.BindingSlot == 0 && Lights)
                Result = Created.Object->UpdateBuffer(0, 0, Lights);
            else if (Set == 2 && Binding.BindingSlot < Textures.size() &&
                     Textures[Binding.BindingSlot] && Sampler)
                Result = Created.Object->UpdateCombinedTextureSampler(
                    Binding.BindingSlot, 0,
                    Textures[Binding.BindingSlot], Sampler);
            else
                return false;
            if (Result != ERHIResult::Success) return false;
        }
        Out.push_back(Created.Object);
        Owner.OwnedDescriptorSets.push_back(std::move(Created.Object));
    }
    return true;
}

bool AddReadback(
    IRHIDevice& Device,
    const char* Name,
    const TSharedPtr<IRHITexture>& Texture,
    uint32 Width,
    uint32 Height,
    FProductionContentDeferredExecutionResources& Owner)
{
    FRHITextureBufferCopyRegion Region;
    Region.Width = Width;
    Region.Height = Height;
    uint64 Bytes = 0;
    if (!Texture || !TryGetRHITextureBufferCopyByteSize(
            Region, Texture->GetFormat(), Bytes))
        return false;
    auto Buffer = Device.CreateBuffer({
        Bytes, ERHIBufferUsage::CopyDestination,
        ERHIMemoryAccess::HostVisible});
    if (!Buffer.Succeeded()) return false;
    Owner.Bindings.Readbacks.push_back({Name, Texture, Buffer.Object, Region});
    Owner.OwnedBuffers.push_back(std::move(Buffer.Object));
    return true;
}

} // namespace

bool FProductionContentDeferredExecutionResources::IsValid() const noexcept
{
    return Plan.IsValid() && Graph.bValid && Bindings.CommandBuffer &&
        Bindings.BaseColorAO && Bindings.NormalRoughness &&
        Bindings.EmissiveMetallic && Bindings.Depth &&
        Bindings.LightingAccumulation && Bindings.FinalOutput &&
        !Bindings.SurfaceDraws.empty() && Bindings.FullscreenVertexBuffer &&
        Bindings.SphereVertexBuffer && Bindings.SphereIndexBuffer &&
        Bindings.ConeVertexBuffer && Bindings.ConeIndexBuffer &&
        Bindings.Readbacks.size() == 6;
}

void FProductionContentDeferredExecutionResources::Release() noexcept
{
    Bindings = {};
    InvalidateReverse(OwnedDescriptorSets);
    InvalidateReverse(OwnedFramebuffers);
    InvalidateReverse(OwnedRenderPasses);
    InvalidateReverse(OwnedPipelines);
    InvalidateReverse(OwnedLayouts);
    InvalidateReverse(OwnedShaders);
    InvalidateReverse(OwnedSamplers);
    InvalidateReverse(OwnedTextures);
    InvalidateReverse(OwnedBuffers);
    Plan = {};
    Graph = {};
}

ERHIResult FProductionContentDeferredExecutionBuilder::Build(
    const TSharedPtr<IRHIDevice>& Device,
    const FStaticModelRenderSnapshot& Snapshot,
    const FProductionContentComposition& Composition,
    const TArray<TSharedPtr<const FShaderAsset>>& RenderShaders,
    const TArray<TSharedPtr<const FShaderPayloadAsset>>& RenderShaderPayloads,
    const FAssetTargetProfileEvidence& TargetEvidence,
    FProductionContentDeferredExecutionResources& OutResources,
    FString* OutReason)
{
    OutResources = {};
    if (OutReason) OutReason->Clear();
    if (!Device || !Device->IsActive() ||
        TargetEvidence.Validate() != EAssetResult::Success)
    {
        Fail(OutReason, "invalid Deferred production execution input");
        return ERHIResult::InvalidState;
    }
    const auto* Directional = FindProgram(RenderShaders, "DirectionalLight");
    const auto* Point = FindProgram(RenderShaders, "PointLight");
    const auto* Spot = FindProgram(RenderShaders, "SpotLight");
    const auto* CompositionProgram = FindProgram(RenderShaders, "Composition");
    const FShaderTargetRequest Target = MakeShaderTarget(TargetEvidence);
    if (!Directional || !Point || !Spot || !CompositionProgram ||
        Target.AcceptableProfiles.empty())
    {
        Fail(OutReason, "strict Deferred shader closure is incomplete");
        return ERHIResult::InvalidState;
    }

    FProductionContentDeferredExecutionResources Candidate;
    FDeferredRendererConfiguration RendererConfig;
    RendererConfig.bEnableValidationReadback = true;
    if (FDeferredRenderer(RendererConfig).PrepareFrame(
            Composition.DeferredInputs, Candidate.Plan) !=
            EDeferredResult::Success)
    {
        Fail(OutReason, "Deferred production frame planning failed");
        return ERHIResult::InvalidState;
    }
    Candidate.Graph = BuildDeferredRenderGraphDeclaration(Candidate.Plan);
    if (!Candidate.Graph.bValid ||
        !BindProductionDeferredDraws(
            Snapshot, Candidate.Plan, Candidate.Bindings, OutReason) ||
        !UploadProductionDeferredUniforms(
            *Device, Snapshot, Candidate.Plan, OutReason))
        return ERHIResult::InvalidState;

    FPayloadLookup Lookup(RenderShaderPayloads);
    FProgramResources DirectionalResources;
    FProgramResources PointResources;
    FProgramResources SpotResources;
    FProgramResources CompositionResources;
    if (!CreateProgram(*Device, *Directional, Target, Lookup, Candidate,
            DirectionalResources) ||
        !CreateProgram(*Device, *Point, Target, Lookup, Candidate,
            PointResources) ||
        !CreateProgram(*Device, *Spot, Target, Lookup, Candidate,
            SpotResources) ||
        !CreateProgram(*Device, *CompositionProgram, Target, Lookup, Candidate,
            CompositionResources))
    {
        Fail(OutReason, "Deferred strict shader realization failed");
        return ERHIResult::InvalidState;
    }

    const uint32 Width = Candidate.Plan.SurfaceLayout.Extent.Width;
    const uint32 Height = Candidate.Plan.SurfaceLayout.Extent.Height;
    const ERHITextureUsage GBufferUsage =
        ERHITextureUsage::ColorAttachment | ERHITextureUsage::Sampled |
        ERHITextureUsage::CopySource;
    const ERHITextureUsage DepthUsage =
        ERHITextureUsage::DepthStencilAttachment | ERHITextureUsage::Sampled |
        ERHITextureUsage::CopySource;
    if (!CreateTexture(*Device, Width, Height, ERHIFormat::R8G8B8A8_UNorm,
            GBufferUsage, Candidate, Candidate.Bindings.BaseColorAO) ||
        !CreateTexture(*Device, Width, Height,
            ERHIFormat::R16G16B16A16_Float, GBufferUsage, Candidate,
            Candidate.Bindings.NormalRoughness) ||
        !CreateTexture(*Device, Width, Height,
            ERHIFormat::R16G16B16A16_Float, GBufferUsage, Candidate,
            Candidate.Bindings.EmissiveMetallic) ||
        !CreateTexture(*Device, Width, Height, ERHIFormat::D32_Float,
            DepthUsage, Candidate, Candidate.Bindings.Depth) ||
        !CreateTexture(*Device, Width, Height,
            ERHIFormat::R16G16B16A16_Float, GBufferUsage, Candidate,
            Candidate.Bindings.LightingAccumulation) ||
        !CreateTexture(*Device, Width, Height, Candidate.Plan.Output.Format,
            ERHITextureUsage::ColorAttachment | ERHITextureUsage::CopySource,
            Candidate, Candidate.Bindings.FinalOutput))
    {
        Fail(OutReason, "Deferred attachment creation failed");
        return ERHIResult::Failed;
    }

    auto Sampler = Device->CreateSampler({});
    if (!Sampler.Succeeded())
    {
        Fail(OutReason, "Deferred GBuffer sampler creation failed");
        return ERHIResult::Failed;
    }
    const auto SharedSampler = Sampler.Object;
    Candidate.OwnedSamplers.push_back(std::move(Sampler.Object));

    TArray<FDeferredLightUniform> Lights;
    Lights.reserve(Candidate.Plan.Lights.Accepted.size());
    for (const auto& Light : Candidate.Plan.Lights.Accepted)
        Lights.push_back(BuildDeferredLightUniform(Light));
    TSharedPtr<IRHIBuffer> LightBuffer;
    if (Lights.empty() || !CreateUploadedBuffer(
            *Device, Lights.data(), Lights.size() * sizeof(Lights.front()),
            ERHIBufferUsage::Storage, Candidate, LightBuffer))
    {
        Fail(OutReason, "Deferred light buffer upload failed");
        return ERHIResult::Failed;
    }

    constexpr float FullscreenVertices[] = {-1.0f, -1.0f, 3.0f, -1.0f,
        -1.0f, 3.0f};
    constexpr float SphereVertices[] = {
        0, 0, 1, 1, 0, 0, 0, 1, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1};
    constexpr uint16 SphereIndices[] = {
        0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1,
        5, 2, 1, 5, 3, 2, 5, 4, 3, 5, 1, 4};
    constexpr float ConeVertices[] = {
        0, 0, 1, 1, 0, -1, 0, 1, -1, -1, 0, -1, 0, -1, -1};
    constexpr uint16 ConeIndices[] = {
        0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1,
        1, 4, 3, 1, 3, 2};
    if (!CreateUploadedBuffer(*Device, FullscreenVertices,
            sizeof(FullscreenVertices), ERHIBufferUsage::Vertex, Candidate,
            Candidate.Bindings.FullscreenVertexBuffer) ||
        !CreateUploadedBuffer(*Device, SphereVertices, sizeof(SphereVertices),
            ERHIBufferUsage::Vertex, Candidate,
            Candidate.Bindings.SphereVertexBuffer) ||
        !CreateUploadedBuffer(*Device, SphereIndices, sizeof(SphereIndices),
            ERHIBufferUsage::Index, Candidate,
            Candidate.Bindings.SphereIndexBuffer) ||
        !CreateUploadedBuffer(*Device, ConeVertices, sizeof(ConeVertices),
            ERHIBufferUsage::Vertex, Candidate,
            Candidate.Bindings.ConeVertexBuffer) ||
        !CreateUploadedBuffer(*Device, ConeIndices, sizeof(ConeIndices),
            ERHIBufferUsage::Index, Candidate,
            Candidate.Bindings.ConeIndexBuffer))
    {
        Fail(OutReason, "Deferred support geometry upload failed");
        return ERHIResult::Failed;
    }
    Candidate.Bindings.SphereIndexCount = std::size(SphereIndices);
    Candidate.Bindings.ConeIndexCount = std::size(ConeIndices);

    const auto ColorClear = ERHIAttachmentLoadOp::Clear;
    const auto ColorLoad = ERHIAttachmentLoadOp::Load;
    const auto Store = ERHIAttachmentStoreOp::Store;
    if (!CreatePass(*Device,
            {
                {ERHIAttachmentRole::Color, ERHIFormat::R8G8B8A8_UNorm,
                    ERHISampleCount::One, ColorClear, Store},
                {ERHIAttachmentRole::Color, ERHIFormat::R16G16B16A16_Float,
                    ERHISampleCount::One, ColorClear, Store},
                {ERHIAttachmentRole::Color, ERHIFormat::R16G16B16A16_Float,
                    ERHISampleCount::One, ColorClear, Store},
                {ERHIAttachmentRole::DepthStencil, ERHIFormat::D32_Float,
                    ERHISampleCount::One, ColorClear, Store}},
            {{Candidate.Bindings.BaseColorAO, 0, 0},
             {Candidate.Bindings.NormalRoughness, 0, 0},
             {Candidate.Bindings.EmissiveMetallic, 0, 0},
             {Candidate.Bindings.Depth, 0, 0}},
            Width, Height, Candidate, Candidate.Bindings.Surface.RenderPass,
            Candidate.Bindings.Surface.Framebuffer) ||
        !CreatePass(*Device,
            {{ERHIAttachmentRole::Color,
                ERHIFormat::R16G16B16A16_Float, ERHISampleCount::One,
                ColorClear, Store}},
            {{Candidate.Bindings.LightingAccumulation, 0, 0}}, Width, Height,
            Candidate, Candidate.Bindings.Directional.RenderPass,
            Candidate.Bindings.Directional.Framebuffer) ||
        !CreatePass(*Device,
            {{ERHIAttachmentRole::Color,
                ERHIFormat::R16G16B16A16_Float, ERHISampleCount::One,
                ColorLoad, Store}},
            {{Candidate.Bindings.LightingAccumulation, 0, 0}}, Width, Height,
            Candidate, Candidate.Bindings.PointOutside.RenderPass,
            Candidate.Bindings.PointOutside.Framebuffer) ||
        !CreatePass(*Device,
            {{ERHIAttachmentRole::Color, Candidate.Plan.Output.Format,
                ERHISampleCount::One, ColorClear, Store}},
            {{Candidate.Bindings.FinalOutput, 0, 0}}, Width, Height,
            Candidate, Candidate.Bindings.Composition.RenderPass,
            Candidate.Bindings.Composition.Framebuffer))
    {
        Fail(OutReason, "Deferred render target scope creation failed");
        return ERHIResult::Failed;
    }
    Candidate.Bindings.Surface.Pipeline =
        Candidate.Bindings.SurfaceDraws.front().Pipeline;
    Candidate.Bindings.PointInside.RenderPass =
        Candidate.Bindings.PointOutside.RenderPass;
    Candidate.Bindings.PointInside.Framebuffer =
        Candidate.Bindings.PointOutside.Framebuffer;
    Candidate.Bindings.SpotOutside.RenderPass =
        Candidate.Bindings.PointOutside.RenderPass;
    Candidate.Bindings.SpotOutside.Framebuffer =
        Candidate.Bindings.PointOutside.Framebuffer;
    Candidate.Bindings.SpotInside.RenderPass =
        Candidate.Bindings.PointOutside.RenderPass;
    Candidate.Bindings.SpotInside.Framebuffer =
        Candidate.Bindings.PointOutside.Framebuffer;
    Candidate.Bindings.Transparency = Candidate.Bindings.Composition;

    if (!CreatePipeline(*Device, DirectionalResources,
            GetDeferredFullscreenVertexLayout(),
            {ERHIFormat::R16G16B16A16_Float}, ERHIFormat::Unknown, false,
            ERHICullMode::None, "production-deferred-directional", Candidate,
            Candidate.Bindings.Directional.Pipeline) ||
        !CreatePipeline(*Device, PointResources,
            GetDeferredVolumeVertexLayout(),
            {ERHIFormat::R16G16B16A16_Float}, ERHIFormat::Unknown, true,
            ERHICullMode::Back, "production-deferred-point-outside", Candidate,
            Candidate.Bindings.PointOutside.Pipeline) ||
        !CreatePipeline(*Device, PointResources,
            GetDeferredVolumeVertexLayout(),
            {ERHIFormat::R16G16B16A16_Float}, ERHIFormat::Unknown, true,
            ERHICullMode::Front, "production-deferred-point-inside", Candidate,
            Candidate.Bindings.PointInside.Pipeline) ||
        !CreatePipeline(*Device, SpotResources,
            GetDeferredVolumeVertexLayout(),
            {ERHIFormat::R16G16B16A16_Float}, ERHIFormat::Unknown, true,
            ERHICullMode::Back, "production-deferred-spot-outside", Candidate,
            Candidate.Bindings.SpotOutside.Pipeline) ||
        !CreatePipeline(*Device, SpotResources,
            GetDeferredVolumeVertexLayout(),
            {ERHIFormat::R16G16B16A16_Float}, ERHIFormat::Unknown, true,
            ERHICullMode::Front, "production-deferred-spot-inside", Candidate,
            Candidate.Bindings.SpotInside.Pipeline) ||
        !CreatePipeline(*Device, CompositionResources,
            GetDeferredFullscreenVertexLayout(),
            {Candidate.Plan.Output.Format}, ERHIFormat::Unknown, false,
            ERHICullMode::None, "production-deferred-composition", Candidate,
            Candidate.Bindings.Composition.Pipeline))
    {
        Fail(OutReason, "Deferred stage pipeline creation failed");
        return ERHIResult::Failed;
    }
    Candidate.Bindings.Transparency.Pipeline =
        Candidate.Bindings.Composition.Pipeline;

    const auto FrameBuffer = FindFrameBuffer(Snapshot);
    const std::array<TSharedPtr<IRHITexture>, 5> GBufferTextures = {
        Candidate.Bindings.BaseColorAO,
        Candidate.Bindings.NormalRoughness,
        Candidate.Bindings.EmissiveMetallic,
        Candidate.Bindings.Depth,
        Candidate.Bindings.LightingAccumulation};
    if (!CreateDescriptors(*Device, DirectionalResources.Layout, FrameBuffer,
            LightBuffer, GBufferTextures, SharedSampler, Candidate,
            Candidate.Bindings.Directional.DescriptorSets) ||
        !CreateDescriptors(*Device, PointResources.Layout, FrameBuffer,
            LightBuffer, GBufferTextures, SharedSampler, Candidate,
            Candidate.Bindings.PointOutside.DescriptorSets) ||
        !CreateDescriptors(*Device, PointResources.Layout, FrameBuffer,
            LightBuffer, GBufferTextures, SharedSampler, Candidate,
            Candidate.Bindings.PointInside.DescriptorSets) ||
        !CreateDescriptors(*Device, SpotResources.Layout, FrameBuffer,
            LightBuffer, GBufferTextures, SharedSampler, Candidate,
            Candidate.Bindings.SpotOutside.DescriptorSets) ||
        !CreateDescriptors(*Device, SpotResources.Layout, FrameBuffer,
            LightBuffer, GBufferTextures, SharedSampler, Candidate,
            Candidate.Bindings.SpotInside.DescriptorSets) ||
        !CreateDescriptors(*Device, CompositionResources.Layout, FrameBuffer,
            LightBuffer, GBufferTextures, SharedSampler, Candidate,
            Candidate.Bindings.Composition.DescriptorSets))
    {
        Fail(OutReason, "Deferred stage descriptor creation failed");
        return ERHIResult::Failed;
    }
    Candidate.Bindings.Transparency.DescriptorSets =
        Candidate.Bindings.Composition.DescriptorSets;

    if (!AddReadback(*Device, "BaseColorAO", Candidate.Bindings.BaseColorAO,
            Width, Height, Candidate) ||
        !AddReadback(*Device, "NormalRoughness",
            Candidate.Bindings.NormalRoughness, Width, Height, Candidate) ||
        !AddReadback(*Device, "EmissiveMetallic",
            Candidate.Bindings.EmissiveMetallic, Width, Height, Candidate) ||
        !AddReadback(*Device, "Depth", Candidate.Bindings.Depth,
            Width, Height, Candidate) ||
        !AddReadback(*Device, "LightingAccumulation",
            Candidate.Bindings.LightingAccumulation, Width, Height,
            Candidate) ||
        !AddReadback(*Device, "FinalOutput", Candidate.Bindings.FinalOutput,
            Width, Height, Candidate))
    {
        Fail(OutReason, "Deferred readback allocation failed");
        return ERHIResult::Failed;
    }
    auto Commands = Device->CreateCommandBuffer(ERHIQueueType::Graphics);
    if (!Commands.Succeeded())
    {
        Fail(OutReason, "Deferred graphics command buffer creation failed");
        return Commands.Result;
    }
    Candidate.Bindings.CommandBuffer = std::move(Commands.Object);
    if (!Candidate.IsValid())
    {
        Fail(OutReason, "Deferred execution resources are incomplete");
        return ERHIResult::InvalidState;
    }
    OutResources = std::move(Candidate);
    return ERHIResult::Success;
}

} // namespace Stoner::Demo
