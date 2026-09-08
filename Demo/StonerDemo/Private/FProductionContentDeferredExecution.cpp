#include "FProductionContentDeferredExecution.h"

#include "Renderer/FShaderAssetConversion.h"
#include "Renderer/FUIRenderSession.h"
#include "Renderer/FDeferredFrameUniformResources.h"
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
    const char* LogicalPath)
{
    const FString Expected(LogicalPath);
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
    TSharedPtr<IRHIBuffer>& Out, ERHIMemoryAccess Access = ERHIMemoryAccess::DeviceLocal)
{
    if (!Bytes || ByteCount == 0) return false;
    auto Buffer = Device.CreateBuffer({
        ByteCount, Usage | ERHIBufferUsage::CopyDestination,
        Access});
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

bool CreateOutputTransformDescriptors(
    IRHIDevice& Device,
    const TSharedPtr<IRHIPipelineLayout>& Layout,
    const TSharedPtr<IRHITexture>& Input,
    const TSharedPtr<IRHISampler>& Sampler,
    const TSharedPtr<IRHIBuffer>& Parameters,
    FProductionContentDeferredExecutionResources& Owner,
    TArray<TSharedPtr<IRHIDescriptorSet>>& Out)
{
    if (!Layout || !Input || !Sampler || !Parameters) return false;
    auto Created = Device.CreateDescriptorSet(Layout, 0);
    if (!Created.Succeeded() ||
        Created.Object->UpdateCombinedTextureSampler(
            0, 0, Input, Sampler) != ERHIResult::Success ||
        Created.Object->UpdateBuffer(1, 0, Parameters) != ERHIResult::Success)
        return false;
    Out.push_back(Created.Object);
    Owner.OwnedDescriptorSets.push_back(std::move(Created.Object));
    return true;
}

bool CreateOutputTransformStage(
    IRHIDevice& Device,
    const FProgramResources& Program,
    const TSharedPtr<IRHITexture>& Input,
    const TSharedPtr<IRHITexture>& Output,
    const TSharedPtr<IRHISampler>& Sampler,
    const FOutputTransformShaderParameterPayload& Parameters,
    const char* Name,
    FProductionContentDeferredExecutionResources& Owner)
{
    if (!Parameters.IsValid() || !Input || !Output) return false;
    FDeferredPostProcessStageBinding Stage;
    Stage.Name = Name;
    Stage.Input = Input;
    Stage.Output = Output;
    if (!CreatePass(Device,
            {{ERHIAttachmentRole::Color, Output->GetFormat(),
                ERHISampleCount::One, ERHIAttachmentLoadOp::Clear,
                ERHIAttachmentStoreOp::Store}},
            {{Output, 0, 0}}, Output->GetDesc().Width,
            Output->GetDesc().Height, Owner, Stage.Stage.RenderPass,
            Stage.Stage.Framebuffer) ||
        !CreatePipeline(Device, Program, GetDeferredFullscreenVertexLayout(),
            {Output->GetFormat()}, ERHIFormat::Unknown, false,
            ERHICullMode::None, Name, Owner, Stage.Stage.Pipeline))
        return false;
    TSharedPtr<IRHIBuffer> ParameterBuffer;
    if (!CreateUploadedBuffer(Device, Parameters.Bytes.data(),
            Parameters.Bytes.size(), ERHIBufferUsage::Uniform, Owner,
            ParameterBuffer, Owner.ExecutionPurpose == EFrameExecutionPurpose::InteractivePreview
                ? ERHIMemoryAccess::HostVisible : ERHIMemoryAccess::DeviceLocal) ||
        !CreateOutputTransformDescriptors(Device, Program.Layout, Input,
            Sampler, ParameterBuffer, Owner, Stage.Stage.DescriptorSets))
        return false;
    Owner.OutputParameterBuffers.push_back(ParameterBuffer);
    Owner.Bindings.OutputTransformStages.push_back(std::move(Stage));
    return true;
}

bool BuildOutputTransformPlan(
    const FProductionContentComposition& Composition,
    const FOutputTransformSettings& Settings,
    FOutputTransformPlan& OutPlan)
{
    FHDRSceneColorHandoffDesc Desc;
    Desc.SceneColorId = Composition.FrameToken;
    Desc.Producer = EHDRSceneColorProducer::Deferred;
    Desc.ViewId = Composition.FrameToken;
    Desc.FrameToken = Composition.FrameToken;
    Desc.Width = Composition.DeferredInputs.View.Extent.Width;
    Desc.Height = Composition.DeferredInputs.View.Extent.Height;
    auto Handoff = FHDRSceneColorHandoff::Declare(Desc);
    if (!Handoff.BindProducer({1, 0}) || !Handoff.MarkProduced()) return false;
    const auto Prepared = FHDRPostProcessPipeline().Prepare(Handoff, Settings);
    if (!Prepared.Succeeded()) return false;
    OutPlan = Prepared.Plan;
    return OutPlan.IsValid();
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

[[nodiscard]] bool IsBorrowedPreviewTarget(
    const TSharedPtr<IRHITexture>& Target,
    const FOutputTransformPlan& Plan) noexcept
{
    return Target &&
        Target->GetLifecycleState() == ERHIResourceLifecycleState::Valid &&
        Target->GetDesc().Dimension == ERHITextureDimension::Texture2D &&
        Target->GetFormat() == Plan.OutputDesc.Format &&
        Target->GetDesc().Width == Plan.OutputDesc.Width &&
        Target->GetDesc().Height == Plan.OutputDesc.Height &&
        Target->GetDesc().Depth == 1 &&
        Target->GetDesc().MipLevels == 1 &&
        Target->GetDesc().ArrayLayers == 1 &&
        Target->GetDesc().SampleCount == Plan.OutputDesc.SampleCount &&
        HasRHIFlag(Target->GetUsage(), ERHITextureUsage::ColorAttachment) &&
        HasRHIFlag(Target->GetUsage(), ERHITextureUsage::Present);
}

[[nodiscard]] bool IsOwnedTexture(
    const TArray<TSharedPtr<IRHITexture>>& Textures,
    const TSharedPtr<IRHITexture>& Target) noexcept
{
    return std::find(Textures.begin(), Textures.end(), Target) !=
        Textures.end();
}

[[nodiscard]] bool ValidateBuildOptions(
    const FProductionContentDeferredExecutionBuildOptions& Options,
    const FOutputTransformSettings& OutputSettings,
    const FProductionContentComposition& Composition,
    FString* OutReason)
{
    const bool bValidPurpose =
        IsValidFrameExecutionPurpose(Options.ExecutionPurpose);
    const bool bValidSelection =
        IsValidFrameReadbackSelection(Options.ReadbackSelection);
    const bool bFormal = Options.ExecutionPurpose ==
        EFrameExecutionPurpose::FormalValidation;
    const bool bPreview = Options.ExecutionPurpose ==
        EFrameExecutionPurpose::InteractivePreview;
    const auto& Extent = Composition.DeferredInputs.View.Extent;
    const bool bTargetShapeValid = !Options.BorrowedFinalOutput ||
        (Options.BorrowedFinalOutput->GetDesc().Width == Extent.Width &&
         Options.BorrowedFinalOutput->GetDesc().Height == Extent.Height);
    if (!bValidPurpose || !bValidSelection || (!bFormal && !bPreview) ||
        (bFormal && (Options.ReadbackSelection !=
                EFrameReadbackSelection::Formal ||
            !OutputSettings.bRequireReadback || Options.BorrowedFinalOutput)) ||
        (bPreview && (Options.ReadbackSelection !=
                EFrameReadbackSelection::None ||
            OutputSettings.bRequireReadback ||
            !OutputSettings.bRequirePresentation ||
            !Options.BorrowedFinalOutput || !bTargetShapeValid ||
            !Options.SceneLease)) )
    {
        Fail(OutReason,
            "Deferred execution purpose/readback selection does not match its resource contract");
        return false;
    }
    return true;
}

} // namespace

bool FProductionContentDeferredExecutionResources::IsValid() const noexcept
{
    const bool bFormal = ExecutionPurpose ==
        Renderer::EFrameExecutionPurpose::FormalValidation &&
        ReadbackSelection == Renderer::EFrameReadbackSelection::Formal;
    const bool bPreview = ExecutionPurpose ==
        Renderer::EFrameExecutionPurpose::InteractivePreview &&
        ReadbackSelection == Renderer::EFrameReadbackSelection::None;
    const bool bReadbacksValid = bFormal
        ? Bindings.Readbacks.size() == 6
        : bPreview && Bindings.Readbacks.empty();
    const bool bValidationPassValid = bFormal
        ? Plan.FindPass(Renderer::EDeferredPassStage::ValidationReadback) != nullptr
        : bPreview &&
            Plan.FindPass(Renderer::EDeferredPassStage::ValidationReadback) == nullptr;
    const bool bOutputTargetValid = bFormal
        ? IsOwnedTexture(OwnedTextures, Bindings.FormalOutput)
        : bPreview && !IsOwnedTexture(OwnedTextures, Bindings.FormalOutput) &&
            IsBorrowedPreviewTarget(Bindings.FormalOutput, OutputTransformPlan);
    return Plan.IsValid() && Graph.bValid && OutputTransformPlan.IsValid() &&
        Renderer::IsValidFrameExecutionPurpose(ExecutionPurpose) &&
        Renderer::IsValidFrameReadbackSelection(ReadbackSelection) &&
        ((bFormal && OutputTransformPlan.ResolvedSettings.bRequireReadback) ||
            (bPreview && !OutputTransformPlan.ResolvedSettings.bRequireReadback)) &&
        ExecutionPurpose == OutputTransformPlan.ExecutionPurpose &&
        ReadbackSelection == OutputTransformPlan.ReadbackSelection &&
        bValidationPassValid && bReadbacksValid && bOutputTargetValid &&
        (bFormal || (PreviewUniformResources &&
            PreviewUniformResources->IsValid())) &&
        Bindings.CommandBuffer &&
        Bindings.BaseColorAO && Bindings.NormalRoughness &&
        Bindings.EmissiveMetallic && Bindings.Depth &&
        Bindings.LightingAccumulation && Bindings.FinalOutput &&
        Bindings.FormalOutput && Bindings.OutputTransformStages.size() ==
            (OutputTransformPlan.TerminalUI ? 4U : 3U) &&
        !Bindings.SurfaceDraws.empty() && Bindings.FullscreenVertexBuffer &&
        Bindings.SphereVertexBuffer && Bindings.SphereIndexBuffer &&
        Bindings.ConeVertexBuffer && Bindings.ConeIndexBuffer &&
        (!bPreview || Bindings.bTransitionFinalOutputToPresent);
}

FDeferredFrameExecutionBindings
FProductionContentDeferredExecutionResources::BuildCycleBindings(
    bool bAuthoritativeReadbacks) const
{
    FDeferredFrameExecutionBindings Selected = Bindings;
    if (ExecutionPurpose == Renderer::EFrameExecutionPurpose::InteractivePreview)
    {
        Selected.Readbacks.clear();
    }
    else if (!bAuthoritativeReadbacks)
    {
        Selected.Readbacks.erase(std::remove_if(
            Selected.Readbacks.begin(), Selected.Readbacks.end(),
            [](const FDeferredReadbackBinding& Binding)
            {
                return Binding.Name != Core::FString("FinalOutput");
            }), Selected.Readbacks.end());
    }
    return Selected;
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
    OutputTransformPlan = {};
    PreviewUniformResources.reset();
    SceneLease.reset();
    OutputSettings = {};
    OutputParameterBuffers.clear();
    OutputSampler.reset();
    AttachmentBytes = 0;
}

ERHIResult FProductionContentDeferredExecutionBuilder::Build(
    const TSharedPtr<IRHIDevice>& Device,
    const FStaticModelRenderSnapshot& Snapshot,
    const FProductionContentComposition& Composition,
    const TArray<TSharedPtr<const FShaderAsset>>& RenderShaders,
    const TArray<TSharedPtr<const FShaderPayloadAsset>>& RenderShaderPayloads,
    const FAssetTargetProfileEvidence& TargetEvidence,
    const FOutputTransformSettings& OutputSettings,
    FProductionContentDeferredExecutionResources& OutResources,
    FString* OutReason,
    const FProductionContentDeferredExecutionBuildOptions& Options)
{
    OutResources = {};
    if (OutReason) OutReason->Clear();
    if (!ValidateBuildOptions(Options, OutputSettings, Composition,
            OutReason))
        return ERHIResult::InvalidState;
    if (!Device || !Device->IsActive() ||
        TargetEvidence.Validate() != EAssetResult::Success)
    {
        Fail(OutReason, "invalid Deferred production execution input");
        return ERHIResult::InvalidState;
    }
    if (Options.ExecutionPurpose == EFrameExecutionPurpose::InteractivePreview &&
        (!Options.SceneLease || Options.SceneLease.get() != &Snapshot))
    {
        Fail(OutReason,
            "preview requires a strong scene lease for the exact snapshot");
        return ERHIResult::InvalidState;
    }
    const auto* Directional = FindProgram(
        RenderShaders, "Engine/Shaders/Deferred/DirectionalLight");
    const auto* Point = FindProgram(
        RenderShaders, "Engine/Shaders/Deferred/PointLight");
    const auto* Spot = FindProgram(
        RenderShaders, "Engine/Shaders/Deferred/SpotLight");
    const auto* CompositionProgram = FindProgram(
        RenderShaders, "Engine/Shaders/Deferred/Composition");
    const auto* OutputTransformProgram = FindProgram(
        RenderShaders, "Engine/Shaders/PostProcess/OutputTransform");
    const FShaderTargetRequest Target = MakeShaderTarget(TargetEvidence);
    if (!Directional || !Point || !Spot || !CompositionProgram ||
        !OutputTransformProgram ||
        Target.AcceptableProfiles.empty())
    {
        Fail(OutReason, "strict Deferred shader closure is incomplete");
        return ERHIResult::InvalidState;
    }

    FProductionContentDeferredExecutionResources Candidate;
    FDeferredRendererConfiguration RendererConfig;
    RendererConfig.bEnableValidationReadback = Options.ExecutionPurpose ==
        EFrameExecutionPurpose::FormalValidation;
    if (FDeferredRenderer(RendererConfig).PrepareFrame(
            Composition.DeferredInputs, Candidate.Plan) !=
            EDeferredResult::Success)
    {
        Fail(OutReason, "Deferred production frame planning failed");
        return ERHIResult::InvalidState;
    }
    Candidate.Graph = BuildDeferredRenderGraphDeclaration(Candidate.Plan);
    if (!Candidate.Graph.bValid ||
        !BuildOutputTransformPlan(
            Composition, OutputSettings, Candidate.OutputTransformPlan))
        return ERHIResult::InvalidState;
    Candidate.ExecutionPurpose = Options.ExecutionPurpose;
    Candidate.ReadbackSelection = Options.ReadbackSelection;
    Candidate.SceneLease = Options.SceneLease;
    Candidate.OutputSettings = OutputSettings;
    Candidate.OutputTransformPlan.ExecutionPurpose =
        Options.ExecutionPurpose;
    Candidate.OutputTransformPlan.ReadbackSelection =
        Options.ReadbackSelection;
    if (Options.ExecutionPurpose == EFrameExecutionPurpose::InteractivePreview &&
        (!Options.BorrowedFinalOutput || !FHDRPostProcessPipeline().BindPreviewTargetFormat(
            Candidate.OutputTransformPlan, Options.BorrowedFinalOutput->GetFormat())))
    {
        Fail(OutReason, "borrowed preview target storage is incompatible with the output profile");
        return ERHIResult::Unsupported;
    }
    if (!Candidate.OutputTransformPlan.IsValid())
    {
        Fail(OutReason,
            "output transform plan does not match the requested execution purpose");
        return ERHIResult::InvalidState;
    }
    if (Options.ExecutionPurpose == EFrameExecutionPurpose::InteractivePreview &&
        !IsBorrowedPreviewTarget(Options.BorrowedFinalOutput,
            Candidate.OutputTransformPlan))
    {
        Fail(OutReason,
            "preview requires a valid borrowed ColorAttachment|Present output target");
        return ERHIResult::InvalidState;
    }
    if (Options.ExecutionPurpose == EFrameExecutionPurpose::InteractivePreview)
    {
        Candidate.PreviewUniformResources =
            Core::MakeShared<Renderer::FDeferredFrameUniformResources>();
        const ERHIResult UniformResult =
            Candidate.PreviewUniformResources->Initialize(
                Device, Options.SceneLease, Candidate.Plan, OutReason);
        if (UniformResult != ERHIResult::Success)
            return UniformResult;
        if (!Candidate.PreviewUniformResources->IsValid())
        {
            Fail(OutReason, "preview slot uniform resources are invalid");
            return ERHIResult::InvalidState;
        }
        Candidate.Bindings.SurfaceDraws =
            Candidate.PreviewUniformResources->GetSurfaceDraws();
    }
    else if (!BindProductionDeferredDraws(
                 Snapshot, Candidate.Plan, Candidate.Bindings, OutReason) ||
             !UploadProductionDeferredUniforms(
                 *Device, Snapshot, Candidate.Plan, OutReason))
        return ERHIResult::InvalidState;

    FPayloadLookup Lookup(RenderShaderPayloads);
    FProgramResources DirectionalResources;
    FProgramResources PointResources;
    FProgramResources SpotResources;
    FProgramResources CompositionResources;
    FProgramResources OutputTransformResources;
    if (!CreateProgram(*Device, *Directional, Target, Lookup, Candidate,
            DirectionalResources) ||
        !CreateProgram(*Device, *Point, Target, Lookup, Candidate,
            PointResources) ||
        !CreateProgram(*Device, *Spot, Target, Lookup, Candidate,
            SpotResources) ||
        !CreateProgram(*Device, *CompositionProgram, Target, Lookup, Candidate,
            CompositionResources) ||
        !CreateProgram(*Device, *OutputTransformProgram, Target, Lookup,
            Candidate, OutputTransformResources))
    {
        Fail(OutReason, "Deferred strict shader realization failed");
        return ERHIResult::InvalidState;
    }

    const uint32 Width = Candidate.Plan.SurfaceLayout.Extent.Width;
    const uint32 Height = Candidate.Plan.SurfaceLayout.Extent.Height;
    const ERHITextureUsage GBufferUsage =
        ERHITextureUsage::ColorAttachment | ERHITextureUsage::Sampled |
        (Options.ExecutionPurpose == EFrameExecutionPurpose::FormalValidation
            ? ERHITextureUsage::CopySource : ERHITextureUsage::None);
    const ERHITextureUsage DepthUsage =
        ERHITextureUsage::DepthStencilAttachment | ERHITextureUsage::Sampled |
        (Options.ExecutionPurpose == EFrameExecutionPurpose::FormalValidation
            ? ERHITextureUsage::CopySource : ERHITextureUsage::None);
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
            ERHITextureUsage::ColorAttachment | ERHITextureUsage::Sampled |
                (Options.ExecutionPurpose == EFrameExecutionPurpose::FormalValidation
                    ? ERHITextureUsage::CopySource : ERHITextureUsage::None),
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
    Candidate.OutputSampler = SharedSampler;
    Candidate.OwnedSamplers.push_back(std::move(Sampler.Object));

    const ERHITextureUsage IntermediateOutputUsage =
        ERHITextureUsage::ColorAttachment | ERHITextureUsage::Sampled;
    TSharedPtr<IRHITexture> ExposedSceneColor;
    TSharedPtr<IRHITexture> DisplayLinear;
    if (!CreateTexture(*Device, Width, Height,
            ERHIFormat::R16G16B16A16_Float, IntermediateOutputUsage,
            Candidate, ExposedSceneColor) ||
        !CreateTexture(*Device, Width, Height,
            ERHIFormat::R16G16B16A16_Float, IntermediateOutputUsage,
            Candidate, DisplayLinear))
    {
        Fail(OutReason, "formal output transform attachment creation failed");
        return ERHIResult::Failed;
    }
    if (Options.ExecutionPurpose == EFrameExecutionPurpose::InteractivePreview)
    {
        Candidate.Bindings.FormalOutput = Options.BorrowedFinalOutput;
    }
    else if (!CreateTexture(*Device, Width, Height,
            Candidate.OutputTransformPlan.OutputDesc.Format,
            ERHITextureUsage::ColorAttachment | ERHITextureUsage::CopySource,
            Candidate, Candidate.Bindings.FormalOutput))
    {
        Fail(OutReason, "formal output transform attachment creation failed");
        return ERHIResult::Failed;
    }
    Candidate.Bindings.bTransitionFinalOutputToPresent =
        Options.ExecutionPurpose == EFrameExecutionPurpose::InteractivePreview;

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

    const FHDRPostProcessPipeline OutputPipeline;
    const auto ExposureParameters = OutputPipeline.BuildShaderParameterPayload(
        Candidate.OutputTransformPlan.ResolvedSettings,
        EOutputTransformStageKind::ManualExposure);
    const auto ToneParameters = OutputPipeline.BuildShaderParameterPayload(
        Candidate.OutputTransformPlan.ResolvedSettings,
        Candidate.OutputTransformPlan.ResolvedSettings.DynamicRange ==
                EOutputDynamicRange::SDR
            ? EOutputTransformStageKind::SDRToneMap
            : EOutputTransformStageKind::HDRViewingTransform);
    const auto DeviceParameters = OutputPipeline.BuildShaderParameterPayload(
        Candidate.OutputTransformPlan.ResolvedSettings,
        EOutputTransformStageKind::OutputDeviceTransform);
    if (!CreateOutputTransformStage(*Device, OutputTransformResources,
            Candidate.Bindings.FinalOutput, ExposedSceneColor, SharedSampler,
            ExposureParameters, "production-output-manual-exposure",
            Candidate) ||
        !CreateOutputTransformStage(*Device, OutputTransformResources,
            ExposedSceneColor, DisplayLinear, SharedSampler, ToneParameters,
            "production-output-tone-map", Candidate) ||
        !CreateOutputTransformStage(*Device, OutputTransformResources,
            DisplayLinear, Candidate.Bindings.FormalOutput, SharedSampler,
            DeviceParameters, "production-output-device-transform",
            Candidate))
    {
        Fail(OutReason, "formal output transform realization failed");
        return ERHIResult::Failed;
    }

    const auto FrameBuffer =
        Candidate.PreviewUniformResources
            ? Candidate.PreviewUniformResources->GetFrameUniformBuffer()
            : FindFrameBuffer(Snapshot);
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

    if (Options.ExecutionPurpose == EFrameExecutionPurpose::FormalValidation &&
        (!AddReadback(*Device, "BaseColorAO", Candidate.Bindings.BaseColorAO,
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
            !AddReadback(*Device, "FinalOutput", Candidate.Bindings.FormalOutput,
                Width, Height, Candidate)))
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

ERHIResult FProductionContentDeferredExecutionBuilder::UpdatePreviewFrame(
    const TSharedPtr<IRHIDevice>& Device,
    const FStaticModelRenderSnapshot& Snapshot,
    const TSharedPtr<const FStaticModelRenderSnapshot>& SceneLease,
    const FProductionContentComposition& Composition,
    FProductionContentDeferredExecutionResources& InOutResources,
    FString* OutReason, const FOutputTransformSettings* NewOutputSettings)
{
    if (OutReason) OutReason->Clear();
    if (!Device || !Device->IsActive() || !SceneLease ||
        SceneLease.get() != &Snapshot ||
        InOutResources.ExecutionPurpose !=
            EFrameExecutionPurpose::InteractivePreview ||
        InOutResources.ReadbackSelection != EFrameReadbackSelection::None ||
        !InOutResources.PreviewUniformResources ||
        !InOutResources.PreviewUniformResources->IsValid())
    {
        Fail(OutReason, "invalid preview frame update ownership");
        return ERHIResult::InvalidState;
    }
    if (!InOutResources.Bindings.CommandBuffer ||
        InOutResources.Bindings.CommandBuffer->GetState() != ERHICommandBufferState::Idle ||
        InOutResources.OutputTransformPlan.TerminalUI)
    { Fail(OutReason,"preview settings update requires an idle slot without UI bindings"); return ERHIResult::InvalidState; }
    const auto& Settings = NewOutputSettings ? *NewOutputSettings : InOutResources.OutputSettings;
    if (Settings.OutputDeviceProfileId != InOutResources.OutputSettings.OutputDeviceProfileId ||
        Settings.PreferredNativeEncoding != InOutResources.OutputSettings.PreferredNativeEncoding ||
        Settings.NativeReferenceWhiteNits != InOutResources.OutputSettings.NativeReferenceWhiteNits ||
        Settings.DiagnosticBypass.Mode != EOutputTransformDebugBypassMode::Disabled || Settings.bRequireReadback ||
        !Settings.bRequirePresentation || (NewOutputSettings &&
            (!Settings.PreTonemapOperations.IsEmpty() || !Settings.PostTonemapOperations.IsEmpty())))
    { Fail(OutReason,"preview output mode or diagnostic change requires its transition path"); return ERHIResult::Unsupported; }
    const auto& Extent = Composition.DeferredInputs.View.Extent;
    if (Extent.Width == 0 || Extent.Height == 0 ||
        Composition.DeferredInputs.Output.Extent.Width != Extent.Width ||
        Composition.DeferredInputs.Output.Extent.Height != Extent.Height ||
        !InOutResources.Bindings.FormalOutput ||
        InOutResources.Bindings.FormalOutput->GetDesc().Width != Extent.Width ||
        InOutResources.Bindings.FormalOutput->GetDesc().Height != Extent.Height)
    {
        Fail(OutReason, "preview frame update extent does not match slot resources");
        return ERHIResult::InvalidState;
    }

    FDeferredRendererConfiguration RendererConfig;
    RendererConfig.bEnableValidationReadback = false;
    FDeferredFramePlan CandidatePlan;
    if (FDeferredRenderer(RendererConfig).PrepareFrame(
            Composition.DeferredInputs, CandidatePlan) !=
            EDeferredResult::Success)
    {
        Fail(OutReason, "preview frame planning failed");
        return ERHIResult::InvalidState;
    }
    if (CandidatePlan.Output.Format != InOutResources.Plan.Output.Format ||
        CandidatePlan.SurfaceLayout.Extent.Width !=
            InOutResources.Plan.SurfaceLayout.Extent.Width ||
        CandidatePlan.SurfaceLayout.Extent.Height !=
            InOutResources.Plan.SurfaceLayout.Extent.Height)
    {
        Fail(OutReason, "preview update would change persistent slot attachments");
        return ERHIResult::ResizeRequired;
    }
    FDeferredRenderGraphDeclaration CandidateGraph =
        BuildDeferredRenderGraphDeclaration(CandidatePlan);
    FOutputTransformPlan CandidateOutputPlan;
    if (!CandidateGraph.bValid || !BuildOutputTransformPlan(
            Composition, Settings, CandidateOutputPlan))
    {
        Fail(OutReason, "preview output transform update failed");
        return ERHIResult::InvalidState;
    }
    CandidateOutputPlan.ExecutionPurpose = EFrameExecutionPurpose::InteractivePreview;
    CandidateOutputPlan.ReadbackSelection = EFrameReadbackSelection::None;
    if (!FHDRPostProcessPipeline().BindPreviewTargetFormat(CandidateOutputPlan,
            InOutResources.Bindings.FormalOutput->GetFormat()) || !CandidateOutputPlan.IsValid() ||
        CandidateOutputPlan.OutputDesc.Format !=
            InOutResources.OutputTransformPlan.OutputDesc.Format)
    {
        Fail(OutReason, "preview output settings would change slot format");
        return ERHIResult::ResizeRequired;
    }
    if (NewOutputSettings)
    {
        const FHDRPostProcessPipeline Pipeline;
        const EOutputTransformStageKind Kinds[] = {EOutputTransformStageKind::ManualExposure,
            CandidateOutputPlan.ResolvedSettings.DynamicRange == EOutputDynamicRange::SDR
                ? EOutputTransformStageKind::SDRToneMap : EOutputTransformStageKind::HDRViewingTransform,
            EOutputTransformStageKind::OutputDeviceTransform};
        if (InOutResources.OutputParameterBuffers.size() != 3) return ERHIResult::InvalidState;
        std::array<FOutputTransformShaderParameterPayload,3> Payloads;
        for (std::size_t I = 0; I < 3; ++I)
        {
            Payloads[I] = Pipeline.BuildShaderParameterPayload(CandidateOutputPlan.ResolvedSettings,Kinds[I]);
            const auto& Buffer = InOutResources.OutputParameterBuffers[I];
            if (!Payloads[I].IsValid() || !Buffer || Buffer->GetDesc().MemoryAccess != ERHIMemoryAccess::HostVisible)
                return ERHIResult::InvalidState;
        }
        // A failed upload leaves this idle slot unrecordable; the caller must
        // cancel/retry it. Submitted slots and their parameters are untouched.
        for (std::size_t I = 0; I < 3; ++I)
        {
            const auto R = Device->UploadBuffer(InOutResources.OutputParameterBuffers[I],
                {0,Payloads[I].Bytes.data(),Payloads[I].Bytes.size()});
            if (R != ERHIResult::Success) return R;
        }
    }
    const ERHIResult UniformResult =
        InOutResources.PreviewUniformResources->Update(
            Device, CandidatePlan, OutReason);
    if (UniformResult != ERHIResult::Success)
        return UniformResult;

    InOutResources.OutputSettings = Settings;
    InOutResources.Plan = std::move(CandidatePlan);
    InOutResources.Graph = std::move(CandidateGraph);
    InOutResources.OutputTransformPlan = std::move(CandidateOutputPlan);
    InOutResources.Bindings.SurfaceDraws =
        InOutResources.PreviewUniformResources->GetSurfaceDraws();
    return ERHIResult::Success;
}

ERHIResult FProductionContentDeferredExecutionBuilder::BindPreviewUI(
    const TSharedPtr<FUIRenderFrame>& Frame,
    FProductionContentDeferredExecutionResources& Resources)
{
    auto& Stages = Resources.Bindings.OutputTransformStages;
    if (Resources.ExecutionPurpose != EFrameExecutionPurpose::InteractivePreview ||
        !Resources.Bindings.CommandBuffer ||
        Resources.Bindings.CommandBuffer->GetState() != ERHICommandBufferState::Idle ||
        !Resources.OutputSampler || (Stages.size() != 3 && Stages.size() != 4) ||
        (Stages.size() == 4 && (!Stages[2].UIFrame || Stages[2].Name != FString("TerminalUI"))) ||
        Stages.back().Stage.DescriptorSets.size() != 1 ||
        !Stages.back().Stage.DescriptorSets[0])
        return ERHIResult::InvalidState;
    const auto Scene = Stages.size() == 4 ? Stages[2].Input : Stages.back().Input;
    const bool HasUI = Frame && Frame->HasDraws();
    if (Frame && (!Frame->CanRecord() || Frame->GetInput() != Scene || !Frame->GetSettings()))
        return ERHIResult::InvalidState;
    try
    {
        auto Prepared = FHDRPostProcessPipeline().Prepare(Resources.OutputTransformPlan.SceneColor,
            Resources.OutputSettings, HasUI ? Frame->GetSettings() : nullptr);
        if (!Prepared.Succeeded()) return ERHIResult::InvalidState;
        auto& Plan = Prepared.Plan;
        Plan.ExecutionPurpose = Resources.ExecutionPurpose;
        Plan.ReadbackSelection = Resources.ReadbackSelection;
        if (!FHDRPostProcessPipeline().BindPreviewTargetFormat(Plan,
                Resources.Bindings.FormalOutput->GetFormat()) || !Plan.IsValid())
            return ERHIResult::InvalidState;
        auto Candidate = Stages;
        if (Candidate.size() == 4) Candidate.erase(Candidate.begin() + 2);
        const auto Input = HasUI ? Frame->GetOutput() : Scene;
        Candidate.back().Input = Input;
        if (HasUI)
        {
            FDeferredPostProcessStageBinding UI;
            UI.Name = "TerminalUI"; UI.Input = Scene; UI.Output = Input; UI.UIFrame = Frame;
            Candidate.insert(Candidate.end() - 1, std::move(UI));
        }
        // All allocations and plan validation precede the sole descriptor mutation.
        const auto Updated = Stages.back().Stage.DescriptorSets[0]->UpdateCombinedTextureSampler(
            0, 0, Input, Resources.OutputSampler);
        if (Updated != ERHIResult::Success) return Updated;
        Stages = std::move(Candidate);
        Resources.OutputTransformPlan = std::move(Plan);
        return ERHIResult::Success;
    }
    catch (const std::bad_alloc&) { return ERHIResult::Unavailable; }
}

ERHIResult FProductionContentDeferredExecutionBuilder::RebindPreviewOutput(
    const TSharedPtr<IRHIDevice>& Device,
    const TSharedPtr<IRHITexture>& BorrowedFinalOutput,
    FProductionContentDeferredExecutionResources& InOutResources,
    FString* OutReason)
{
    if (OutReason) OutReason->Clear();
    const bool bPreview = InOutResources.ExecutionPurpose ==
        EFrameExecutionPurpose::InteractivePreview &&
        InOutResources.ReadbackSelection == EFrameReadbackSelection::None;
    const auto* Stage = InOutResources.Bindings.OutputTransformStages.empty()
        ? nullptr : &InOutResources.Bindings.OutputTransformStages.back();
    if (!Device || !Device->IsActive() || !bPreview || !Stage ||
        !Stage->Stage.RenderPass || !BorrowedFinalOutput ||
        BorrowedFinalOutput->GetLifecycleState() !=
            ERHIResourceLifecycleState::Valid ||
        BorrowedFinalOutput->GetDesc().Dimension !=
            ERHITextureDimension::Texture2D ||
        BorrowedFinalOutput->GetDesc().Depth != 1 ||
        BorrowedFinalOutput->GetDesc().MipLevels != 1 ||
        BorrowedFinalOutput->GetDesc().ArrayLayers != 1 ||
        BorrowedFinalOutput->GetDesc().SampleCount != ERHISampleCount::One ||
        BorrowedFinalOutput->GetFormat() !=
            InOutResources.OutputTransformPlan.OutputDesc.Format ||
        BorrowedFinalOutput->GetDesc().Width !=
            InOutResources.OutputTransformPlan.OutputDesc.Width ||
        BorrowedFinalOutput->GetDesc().Height !=
            InOutResources.OutputTransformPlan.OutputDesc.Height ||
        !HasRHIFlag(BorrowedFinalOutput->GetUsage(),
            ERHITextureUsage::ColorAttachment) ||
        !HasRHIFlag(BorrowedFinalOutput->GetUsage(), ERHITextureUsage::Present))
    {
        Fail(OutReason, "borrowed preview output does not match the slot");
        return ERHIResult::InvalidState;
    }
    FRHIFramebufferDesc Desc;
    Desc.RenderPass = Stage->Stage.RenderPass;
    Desc.Attachments = {{BorrowedFinalOutput, 0, 0}};
    Desc.Width = InOutResources.OutputTransformPlan.OutputDesc.Width;
    Desc.Height = InOutResources.OutputTransformPlan.OutputDesc.Height;
    auto Created = Device->CreateFramebuffer(Desc);
    if (!Created.Succeeded())
    {
        Fail(OutReason, "borrowed preview output framebuffer creation failed");
        return Created.Result;
    }

    auto& MutableStage = InOutResources.Bindings.OutputTransformStages.back();
    const TSharedPtr<IRHIFramebuffer> Old = MutableStage.Stage.Framebuffer;
    if (Old)
    {
        const auto Found = std::find(
            InOutResources.OwnedFramebuffers.begin(),
            InOutResources.OwnedFramebuffers.end(), Old);
        if (Found != InOutResources.OwnedFramebuffers.end())
            InOutResources.OwnedFramebuffers.erase(Found);
        (void)Old->Invalidate();
    }
    MutableStage.Output = BorrowedFinalOutput;
    MutableStage.Stage.Framebuffer = std::move(Created.Object);
    InOutResources.OwnedFramebuffers.push_back(
        MutableStage.Stage.Framebuffer);
    InOutResources.Bindings.FormalOutput = BorrowedFinalOutput;
    return ERHIResult::Success;
}

} // namespace Stoner::Demo
