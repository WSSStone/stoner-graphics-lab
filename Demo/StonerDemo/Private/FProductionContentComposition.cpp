#include "FProductionContentComposition.h"
#include "RHI/FRHIBufferUploadDesc.h"
#include "RHI/IRHIBuffer.h"
#include "RHI/IRHIDevice.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <string>

namespace Stoner::Demo
{
namespace
{

using namespace Stoner::Core;
using namespace Stoner::Renderer;

void Fail(FString* OutReason, const char* Reason)
{
    if (OutReason) *OutReason = Reason;
}

bool AccumulateBounds(
    const FStaticModelRenderSnapshot& Snapshot, FBox& OutBounds)
{
    for (const auto& Draw : Snapshot.GetDraws())
    {
        if (Draw.NodeIndex >= Snapshot.GetNodes().size() ||
            !Draw.Bounds.Box.IsValid())
            return false;
        const auto& Matrix = Snapshot.GetNodes()[Draw.NodeIndex].WorldTransform;
        const auto& Min = Draw.Bounds.Box.Min;
        const auto& Max = Draw.Bounds.Box.Max;
        for (int X = 0; X < 2; ++X)
            for (int Y = 0; Y < 2; ++Y)
                for (int Z = 0; Z < 2; ++Z)
                    OutBounds.AddPoint(Matrix.TransformPoint(FVector3(
                        X == 0 ? Min.X : Max.X,
                        Y == 0 ? Min.Y : Max.Y,
                        Z == 0 ? Min.Z : Max.Z)));
    }
    return OutBounds.IsValid();
}

FMatrix4x4 MakePlacement(
    const FBox& Bounds, const FProductionContentCompositionConfig& Config)
{
    const FVector3 Extent = Bounds.GetExtent();
    const float MaximumExtent = std::max({Extent.X, Extent.Y, Extent.Z});
    const float Scale = Config.ModelHalfExtent / MaximumExtent;
    const FVector3 TargetCenter(Config.ModelForwardDistance, 0.0f, 0.0f);
    return FMatrix4x4::Translation(TargetCenter - Bounds.GetCenter() * Scale) *
        FMatrix4x4::Scale(FVector3(Scale, Scale, Scale));
}

const FMaterialParameter* FindParameter(
    const FMaterialAssetSnapshot& Material, const char* Name)
{
    return Material.Material.GetParameters().FindParameter(FString(Name));
}

float ScalarParameter(
    const FMaterialAssetSnapshot& Material, const char* Name, float Fallback)
{
    const auto* Parameter = FindParameter(Material, Name);
    return Parameter && Parameter->Value.Type == EMaterialParameterValueType::Scalar
        ? Parameter->Value.Scalar : Fallback;
}

FColor ColorParameter(
    const FMaterialAssetSnapshot& Material, const char* Name, FColor Fallback)
{
    const auto* Parameter = FindParameter(Material, Name);
    if (!Parameter) return Fallback;
    if (Parameter->Value.Type == EMaterialParameterValueType::Color)
        return Parameter->Value.Color;
    if (Parameter->Value.Type == EMaterialParameterValueType::Vector)
    {
        const auto& Value = Parameter->Value.Vector;
        return FColor(Value.X, Value.Y, Value.Z, Value.W);
    }
    return Fallback;
}

FForwardPBRSurfaceInputs CompleteSurfaceInputs()
{
    FForwardPBRSurfaceInputs Inputs;
    Inputs.bHasBaseColor = true;
    Inputs.bHasMetallic = true;
    Inputs.bHasRoughness = true;
    Inputs.bHasNormal = true;
    Inputs.bHasOcclusion = true;
    Inputs.bHasEmissive = true;
    Inputs.bHasAlpha = true;
    return Inputs;
}

} // namespace

bool FProductionContentCompositionConfig::IsValid() const noexcept
{
    return !WorkloadRevision.IsEmpty() && FrameToken != 0 && Width != 0 &&
        Height != 0 && FMath::IsFinite(ModelHalfExtent) &&
        ModelHalfExtent > 0.0f && FMath::IsFinite(ModelForwardDistance) &&
        ModelForwardDistance > ModelHalfExtent;
}

bool FProductionContentCompositionBuilder::Build(
    const TSharedPtr<const FStaticModelRenderSnapshot>& Snapshot,
    const FProductionContentCompositionConfig& Config,
    FProductionContentComposition& OutComposition,
    FString* OutReason)
{
    OutComposition = {};
    if (OutReason) OutReason->Clear();
    if (!Snapshot || !Config.IsValid())
    {
        Fail(OutReason, "invalid production composition input");
        return false;
    }
    if (!Snapshot->GetRootAssetId().IsValid() ||
        Snapshot->GetRootVersion().Validate() != Asset::EAssetResult::Success ||
        Snapshot->GetSnapshotGeneration() == 0 ||
        Snapshot->GetDraws().empty())
    {
        Fail(OutReason, "render snapshot is not published");
        return false;
    }

    FBox Bounds;
    if (!AccumulateBounds(*Snapshot, Bounds))
    {
        Fail(OutReason, "render snapshot bounds are invalid");
        return false;
    }
    const FVector3 Extent = Bounds.GetExtent();
    if (std::max({Extent.X, Extent.Y, Extent.Z}) <= FMath::DefaultTolerance)
    {
        Fail(OutReason, "render snapshot bounds are degenerate");
        return false;
    }

    FProductionContentComposition Candidate;
    Candidate.WorkloadRevision = Config.WorkloadRevision;
    Candidate.RootAssetId = Snapshot->GetRootAssetId();
    Candidate.RootVersion = Snapshot->GetRootVersion();
    Candidate.SnapshotGeneration = Snapshot->GetSnapshotGeneration();
    Candidate.FrameToken = Config.FrameToken;
    Candidate.ModelPlacement = MakePlacement(Bounds, Config);

    FProductionCameraPreset Camera;
    if (!ResolveProductionCameraPreset(
            Config.WorkloadRevision, Camera, OutReason))
        return false;
    Candidate.CameraPosition = Camera.CameraPosition;

    const FString FrameIdentity(Config.WorkloadRevision.ToStdString() +
        "/frame-" + std::to_string(Config.FrameToken));
    const float NearPlane = 0.1f;
    const float FarPlane = 100.0f;
    const FMatrix4x4& View = Camera.View;
    const FMatrix4x4& Projection = Camera.Projection;
    const FMatrix4x4& ViewProjection = Camera.ViewProjection;
    const FMatrix4x4& InverseViewProjection = Camera.InverseViewProjection;

    auto& Deferred = Candidate.DeferredInputs;
    Deferred.FrameId = FrameIdentity;
    Deferred.View.Name = "ProductionContentView";
    Deferred.View.View = View;
    Deferred.View.Projection = Projection;
    Deferred.View.ViewProjection = ViewProjection;
    Deferred.View.InverseViewProjection = InverseViewProjection;
    Deferred.View.CameraPosition = Candidate.CameraPosition;
    Deferred.View.Extent = {Config.Width, Config.Height};
    Deferred.View.DepthPolicy = MakeDeferredDepthPolicy(
        EDeferredDepthConvention::StandardZ, NearPlane, FarPlane);
    Deferred.Output = {
        "ProductionCompositionColor",
        RHI::ERHIFormat::R8G8B8A8_UNorm,
        Deferred.View.Extent};
    Deferred.AmbientContribution = FColor(0.03f, 0.03f, 0.03f, 1.0f);

    auto& Forward = Candidate.ForwardInputs;
    Forward.FrameName = FrameIdentity;
    Forward.View.ViewName = Deferred.View.Name;
    Forward.View.ViewMatrix = View;
    Forward.View.ViewProjectionMatrix = ViewProjection;
    Forward.View.CameraPosition = Candidate.CameraPosition;
    Forward.View.Viewport.Extent = {Config.Width, Config.Height};
    Forward.Output.ColorTargetName = Deferred.Output.Name;
    Forward.Output.DepthTargetName = "ProductionDepth";
    Forward.Output.FormatSummary = "RGBA8";
    Forward.Output.Extent = {Config.Width, Config.Height};
    Forward.Environment.Mode = EForwardBackgroundMode::Clear;
    Forward.Environment.BackgroundName = "ProductionClear";

    FDeferredDirectionalLight DeferredSun;
    DeferredSun.Identity = {1, 1};
    DeferredSun.Name = "ProductionKey";
    DeferredSun.Direction = FVector3(-1.0f, -0.35f, -0.6f).GetSafeNormal();
    DeferredSun.Color = FColor(1.0f, 0.96f, 0.90f, 1.0f);
    DeferredSun.Intensity = 3.0f;
    Deferred.DirectionalLights.push_back(DeferredSun);
    FDeferredPointLight DeferredFill;
    DeferredFill.Identity = {2, 1};
    DeferredFill.Name = "ProductionFill";
    DeferredFill.Position = FVector3(3.0f, -2.0f, 2.0f);
    DeferredFill.Color = FColor(0.32f, 0.48f, 1.0f, 1.0f);
    DeferredFill.Intensity = 8.0f;
    DeferredFill.Range = 6.0f;
    Deferred.PointLights.push_back(DeferredFill);

    FForwardDirectionalLight ForwardSun;
    ForwardSun.LightId = DeferredSun.Identity.Slot;
    ForwardSun.Name = DeferredSun.Name;
    ForwardSun.Direction = DeferredSun.Direction;
    ForwardSun.Color = DeferredSun.Color;
    ForwardSun.Intensity = DeferredSun.Intensity;
    Forward.DirectionalLights.push_back(ForwardSun);
    FForwardPointLight ForwardFill;
    ForwardFill.LightId = DeferredFill.Identity.Slot;
    ForwardFill.Name = DeferredFill.Name;
    ForwardFill.Position = DeferredFill.Position;
    ForwardFill.Color = DeferredFill.Color;
    ForwardFill.Intensity = DeferredFill.Intensity;
    ForwardFill.Range = DeferredFill.Range;
    Forward.PointLights.push_back(ForwardFill);

    for (Core::uint32 DrawIndex = 0;
         DrawIndex < Snapshot->GetDraws().size(); ++DrawIndex)
    {
        const auto& Draw = Snapshot->GetDraws()[DrawIndex];
        if (Draw.NodeIndex >= Snapshot->GetNodes().size() ||
            Draw.MaterialIndex >= Snapshot->GetMaterials().size() ||
            Draw.MeshIndex >= Snapshot->GetMeshes().size())
        {
            Fail(OutReason, "render snapshot draw references are invalid");
            return false;
        }
        const auto& Material = Snapshot->GetMaterials()[Draw.MaterialIndex];
        const auto& MaterialDesc = Material.Material.GetDesc();
        const FMatrix4x4 Model = Candidate.ModelPlacement *
            Snapshot->GetNodes()[Draw.NodeIndex].WorldTransform;

        FMeshDrawCandidate ForwardDraw;
        ForwardDraw.ObjectId = DrawIndex + 1;
        ForwardDraw.MeshId = Draw.MeshIndex + 1;
        ForwardDraw.DebugName = Draw.StableKey;
        ForwardDraw.WorldPosition = Model.TransformPoint(
            Draw.Bounds.Box.GetCenter());
        ForwardDraw.bWantsOpaque =
            MaterialDesc.BlendMode != EMaterialBlendMode::Translucent &&
            MaterialDesc.BlendMode != EMaterialBlendMode::Additive;
        ForwardDraw.bWantsTransparent = !ForwardDraw.bWantsOpaque;
        ForwardDraw.MaterialBinding.MaterialId = Draw.MaterialIndex + 1;
        ForwardDraw.MaterialBinding.MaterialName = Material.Material.GetName();
        ForwardDraw.MaterialBinding.Domain = MaterialDesc.Domain;
        ForwardDraw.MaterialBinding.BlendMode = MaterialDesc.BlendMode;
        ForwardDraw.MaterialBinding.bHasMaterialBinding = true;
        ForwardDraw.MaterialBinding.bHasShaderBinding =
            Draw.PipelineIndex < Snapshot->GetMaterialResources().size() &&
            Snapshot->GetMaterialResources()[Draw.PipelineIndex].Pipeline;
        ForwardDraw.MaterialBinding.SurfaceInputs = CompleteSurfaceInputs();
        ForwardDraw.MaterialBinding.ResourceRequirements =
            Material.ResourceRequirements;
        Forward.DrawCandidates.push_back(ForwardDraw);

        FDeferredDrawCandidate DeferredDraw;
        DeferredDraw.Identity = {DrawIndex + 1, 1};
        DeferredDraw.MeshId = ForwardDraw.MeshId;
        DeferredDraw.MaterialId = ForwardDraw.MaterialBinding.MaterialId;
        DeferredDraw.Name = Draw.StableKey;
        DeferredDraw.Model = Model;
        DeferredDraw.Domain = MaterialDesc.Domain;
        DeferredDraw.BlendMode = MaterialDesc.BlendMode;
        DeferredDraw.bHasShaderBinding =
            ForwardDraw.MaterialBinding.bHasShaderBinding;
        DeferredDraw.bHasRequiredSemantics = true;
        DeferredDraw.Surface.BaseColor = ColorParameter(
            Material, "BaseColorFactor", FColor::OpaqueWhite());
        DeferredDraw.Surface.Metallic = ScalarParameter(
            Material, "MetallicFactor", 1.0f);
        DeferredDraw.Surface.Roughness = ScalarParameter(
            Material, "RoughnessFactor", 1.0f);
        DeferredDraw.Surface.Emissive = ColorParameter(
            Material, "EmissiveFactor", FColor::Transparent());
        DeferredDraw.Surface.Alpha = DeferredDraw.Surface.BaseColor.A;
        DeferredDraw.Surface.AlphaCutoff = ScalarParameter(
            Material, "AlphaCutoff", 0.5f);
        DeferredDraw.ForwardCandidate = ForwardDraw;
        Deferred.DrawCandidates.push_back(std::move(DeferredDraw));
    }

    FDeferredRendererConfiguration DeferredConfig;
    DeferredConfig.bEnableValidationReadback = true;
    FDeferredFramePlan DeferredPlan;
    FForwardFramePlan ForwardPlan;
    const EDeferredResult DeferredResult =
        FDeferredRenderer(DeferredConfig).PrepareFrame(
            Deferred, DeferredPlan);
    const EForwardResult ForwardResult =
        FForwardRenderer().PrepareFrame(Forward, ForwardPlan);
    if (DeferredResult != EDeferredResult::Success ||
        !DeferredPlan.IsValid() || DeferredPlan.AcceptedDraws.empty() ||
        ForwardResult != EForwardResult::Success || !ForwardPlan.IsValid() ||
        !ForwardPlan.HasRenderableGeometry())
    {
        const std::string Detail =
            "production renderer inputs are invalid: deferred=" +
            std::string(ToString(DeferredResult)) + "/" +
            std::to_string(DeferredPlan.AcceptedDraws.size()) + "/" +
            std::to_string(DeferredPlan.RejectedDraws.size()) +
            ", forward=" + std::string(ToString(ForwardResult)) + "/" +
            std::to_string(ForwardPlan.AcceptedOpaqueDraws.size()) + "/" +
            std::to_string(ForwardPlan.AcceptedTransparentDraws.size()) + "/" +
            std::to_string(ForwardPlan.RejectedDraws.size()) +
            (DeferredPlan.RejectedDraws.empty()
                ? std::string{}
                : ", first-deferred-rejection=" +
                    DeferredPlan.RejectedDraws.front().Reason.ToStdString());
        if (OutReason) *OutReason = FString(Detail);
        return false;
    }

    OutComposition = std::move(Candidate);
    return true;
}

bool ApplyProductionCameraPreset(
    FProductionContentComposition& InOutComposition,
    const FProductionCameraPreset& Camera,
    FDeferredFramePlan& OutDeferredPlan,
    FForwardFramePlan& OutForwardPlan,
    FString* OutReason)
{
    if (OutReason) OutReason->Clear();
    if (!Camera.IsValid() || InOutComposition.WorkloadRevision.IsEmpty() ||
        Camera.WorkloadRevision != InOutComposition.WorkloadRevision)
    {
        Fail(OutReason, "camera preset does not match the production composition");
        return false;
    }

    FDeferredFrameInputs Deferred = InOutComposition.DeferredInputs;
    FForwardFrameInputs Forward = InOutComposition.ForwardInputs;
    Deferred.View.View = Camera.View;
    Deferred.View.Projection = Camera.Projection;
    Deferred.View.ViewProjection = Camera.ViewProjection;
    Deferred.View.InverseViewProjection = Camera.InverseViewProjection;
    Deferred.View.CameraPosition = Camera.CameraPosition;
    Forward.View.ViewMatrix = Camera.View;
    Forward.View.ViewProjectionMatrix = Camera.ViewProjection;
    Forward.View.CameraPosition = Camera.CameraPosition;

    FDeferredRendererConfiguration DeferredConfig;
    DeferredConfig.bEnableValidationReadback = true;
    FDeferredFramePlan DeferredPlan;
    FForwardFramePlan ForwardPlan;
    if (FDeferredRenderer(DeferredConfig).PrepareFrame(
            Deferred, DeferredPlan) != EDeferredResult::Success ||
        FForwardRenderer().PrepareFrame(Forward, ForwardPlan) !=
            EForwardResult::Success ||
        !DeferredPlan.IsValid() || DeferredPlan.AcceptedDraws.empty() ||
        !ForwardPlan.IsValid() || !ForwardPlan.HasRenderableGeometry() ||
        !DeferredPlan.View.View.NearlyEquals(
            ForwardPlan.ViewData.ViewMatrix) ||
        !DeferredPlan.View.ViewProjection.NearlyEquals(
            ForwardPlan.ViewData.ViewProjectionMatrix) ||
        DeferredPlan.View.CameraPosition !=
            ForwardPlan.ViewData.CameraPosition)
    {
        Fail(OutReason, "camera update did not produce matching renderer plans");
        return false;
    }

    InOutComposition.CameraPosition = Camera.CameraPosition;
    InOutComposition.DeferredInputs = std::move(Deferred);
    InOutComposition.ForwardInputs = std::move(Forward);
    OutDeferredPlan = std::move(DeferredPlan);
    OutForwardPlan = std::move(ForwardPlan);
    return true;
}

namespace
{

bool BuildDeferredBinding(
    const FStaticModelRenderSnapshot& Snapshot,
    Core::uint32 ObjectId,
    FDeferredSurfaceDrawBinding& Out,
    FString* OutReason)
{
    if (ObjectId == 0 || ObjectId > Snapshot.GetDraws().size())
    {
        Fail(OutReason, "production draw identity is outside the snapshot");
        return false;
    }
    const auto& Draw = Snapshot.GetDraws()[ObjectId - 1];
    if (Draw.MeshIndex >= Snapshot.GetMeshes().size() ||
        Draw.PipelineIndex >= Snapshot.GetMaterialResources().size())
    {
        Fail(OutReason, "production draw resource index is invalid");
        return false;
    }
    const auto& Mesh = Snapshot.GetMeshes()[Draw.MeshIndex];
    const auto& Material = Snapshot.GetMaterialResources()[Draw.PipelineIndex];
    if (ObjectId > Snapshot.GetDrawResources().size())
    {
        Fail(OutReason, "production draw binding is unavailable");
        return false;
    }
    const auto& DrawResources = Snapshot.GetDrawResources()[ObjectId - 1];
    if (Draw.SectionIndex >= Mesh.Sections.size() || !Mesh.VertexBuffer ||
        !Mesh.IndexBuffer || !Material.Pipeline)
    {
        Fail(OutReason, "production draw resource is incomplete");
        return false;
    }
    Out.VertexBuffer = Mesh.VertexBuffer;
    Out.IndexBuffer = Mesh.IndexBuffer;
    Out.IndexType = Mesh.IndexType;
    Out.Draw = MakeStaticMeshSectionDrawArguments(
        Mesh.Sections[Draw.SectionIndex]);
    Out.Pipeline = Material.Pipeline;
    Out.DescriptorSets = DrawResources.DescriptorSets;
    return true;
}

const FStaticModelBufferBindingResource* FindBufferBinding(
    const FStaticModelDrawResources& Resources,
    Core::uint32 SetIndex,
    Core::uint32 BindingSlot)
{
    const auto Found = std::find_if(
        Resources.BufferBindings.begin(), Resources.BufferBindings.end(),
        [SetIndex, BindingSlot](const auto& Binding)
        {
            return Binding.SetIndex == SetIndex &&
                Binding.BindingSlot == BindingSlot;
        });
    return Found == Resources.BufferBindings.end() ? nullptr : &*Found;
}

} // namespace

bool BindProductionDeferredDraws(
    const FStaticModelRenderSnapshot& Snapshot,
    const FDeferredFramePlan& Plan,
    FDeferredFrameExecutionBindings& InOutBindings,
    FString* OutReason)
{
    if (OutReason) OutReason->Clear();
    Core::TArray<FDeferredSurfaceDrawBinding> Candidate;
    Candidate.reserve(Plan.AcceptedDraws.size());
    for (const auto& Draw : Plan.AcceptedDraws)
    {
        FDeferredSurfaceDrawBinding Binding;
        if (!BuildDeferredBinding(
                Snapshot, Draw.Candidate.Identity.Slot,
                Binding, OutReason))
            return false;
        Candidate.push_back(std::move(Binding));
    }
    if (Candidate.empty())
    {
        Fail(OutReason, "production Deferred plan has no aggregate draws");
        return false;
    }
    InOutBindings.SurfaceDraws = std::move(Candidate);
    return true;
}

bool UploadProductionDeferredUniforms(
    RHI::IRHIDevice& Device,
    const FStaticModelRenderSnapshot& Snapshot,
    const FDeferredFramePlan& Plan,
    FString* OutReason)
{
    if (OutReason) OutReason->Clear();
    if (!Device.IsActive() || Plan.AcceptedDraws.empty() ||
        Snapshot.GetDrawResources().size() != Snapshot.GetDraws().size())
    {
        Fail(OutReason, "production uniform upload input is invalid");
        return false;
    }

    const FDeferredFrameViewUniform FrameUniform =
        BuildDeferredFrameViewUniform(Plan.View);
    std::set<const RHI::IRHIBuffer*> UploadedFrameBuffers;
    for (const auto& DrawResources : Snapshot.GetDrawResources())
    {
        const auto* Frame = FindBufferBinding(DrawResources, 0, 0);
        if (!Frame || !Frame->Buffer ||
            Frame->Buffer->GetSizeInBytes() < sizeof(FrameUniform))
        {
            Fail(OutReason, "production frame uniform binding is incomplete");
            return false;
        }
        if (UploadedFrameBuffers.insert(Frame->Buffer.get()).second &&
            Device.UploadBuffer(Frame->Buffer,
                {0, &FrameUniform, sizeof(FrameUniform)}) !=
                RHI::ERHIResult::Success)
        {
            Fail(OutReason, "production frame uniform upload failed");
            return false;
        }
    }

    for (const auto& Draw : Plan.AcceptedDraws)
    {
        const Core::uint32 ObjectId = Draw.Candidate.Identity.Slot;
        if (ObjectId == 0 || ObjectId > Snapshot.GetDrawResources().size())
        {
            Fail(OutReason, "production draw uniform identity is invalid");
            return false;
        }
        const auto* Binding = FindBufferBinding(
            Snapshot.GetDrawResources()[ObjectId - 1], 1, 0);
        const FDeferredDrawMaterialUniform Uniform =
            BuildDeferredDrawMaterialUniform(Draw);
        if (!Binding || !Binding->Buffer ||
            Binding->Buffer->GetSizeInBytes() < sizeof(Uniform) ||
            Device.UploadBuffer(Binding->Buffer,
                {0, &Uniform, sizeof(Uniform)}) != RHI::ERHIResult::Success)
        {
            Fail(OutReason, "production draw uniform upload failed");
            return false;
        }
    }
    return true;
}

bool BindProductionForwardDraws(
    const FStaticModelRenderSnapshot& Snapshot,
    const FForwardFramePlan& Plan,
    FForwardFrameExecutionBindings& InOutBindings,
    FString* OutReason)
{
    if (OutReason) OutReason->Clear();
    Core::TArray<FForwardFrameExecutionBindings::FDrawBinding> Candidate;
    Candidate.reserve(
        Plan.AcceptedOpaqueDraws.size() + Plan.AcceptedTransparentDraws.size());
    const auto Append = [&](const auto& Commands) -> bool
    {
        for (const auto& Command : Commands)
        {
            FDeferredSurfaceDrawBinding Common;
            if (!BuildDeferredBinding(
                    Snapshot, Command.GetObjectId(), Common, OutReason))
                return false;
            FForwardFrameExecutionBindings::FDrawBinding Binding;
            Binding.VertexBuffer = std::move(Common.VertexBuffer);
            Binding.IndexBuffer = std::move(Common.IndexBuffer);
            Binding.IndexType = Common.IndexType;
            Binding.Draw = Common.Draw;
            Binding.Pipeline = std::move(Common.Pipeline);
            Binding.DescriptorSets = std::move(Common.DescriptorSets);
            Candidate.push_back(std::move(Binding));
        }
        return true;
    };
    if (!Append(Plan.AcceptedOpaqueDraws) ||
        !Append(Plan.AcceptedTransparentDraws))
        return false;
    if (Candidate.empty())
    {
        Fail(OutReason, "production Forward plan has no aggregate draws");
        return false;
    }
    InOutBindings.Draws = std::move(Candidate);
    return true;
}

bool PrepareProductionForwardSmoke(
    RHI::IRHIDevice& Device,
    const FStaticModelRenderSnapshot& Snapshot,
    const FForwardFramePlan& ForwardPlan,
    const FDeferredFramePlan& DeferredPlan,
    FForwardFrameExecutionBindings& OutBindings,
    FString* OutReason)
{
    OutBindings = {};
    if (OutReason) OutReason->Clear();
    if (!Device.IsActive() || !ForwardPlan.IsValid() ||
        !ForwardPlan.HasRenderableGeometry() || !DeferredPlan.IsValid() ||
        ForwardPlan.OutputTarget.Extent.Width == 0 ||
        ForwardPlan.OutputTarget.Extent.Height == 0 ||
        ForwardPlan.OutputTarget.Extent.Width !=
            DeferredPlan.SurfaceLayout.Extent.Width ||
        ForwardPlan.OutputTarget.Extent.Height !=
            DeferredPlan.SurfaceLayout.Extent.Height)
    {
        Fail(OutReason, "production Forward smoke input is invalid");
        return false;
    }
    if (!UploadProductionDeferredUniforms(
            Device, Snapshot, DeferredPlan, OutReason))
        return false;

    const Core::uint32 Width = ForwardPlan.OutputTarget.Extent.Width;
    const Core::uint32 Height = ForwardPlan.OutputTarget.Extent.Height;
    RHI::FRHITextureDesc OutputDesc;
    OutputDesc.Width = Width;
    OutputDesc.Height = Height;
    OutputDesc.Format = RHI::ERHIFormat::R8G8B8A8_UNorm;
    OutputDesc.Usage = RHI::ERHITextureUsage::ColorAttachment |
        RHI::ERHITextureUsage::CopySource;
    auto Output = Device.CreateTexture(OutputDesc);
    RHI::FRHITextureDesc AuxiliaryDesc = OutputDesc;
    AuxiliaryDesc.Format = RHI::ERHIFormat::R16G16B16A16_Float;
    AuxiliaryDesc.Usage = RHI::ERHITextureUsage::ColorAttachment;
    auto NormalRoughness = Device.CreateTexture(AuxiliaryDesc);
    auto EmissiveMetallic = Device.CreateTexture(AuxiliaryDesc);
    RHI::FRHITextureDesc DepthDesc;
    DepthDesc.Width = Width;
    DepthDesc.Height = Height;
    DepthDesc.Format = RHI::ERHIFormat::D32_Float;
    DepthDesc.Usage = RHI::ERHITextureUsage::DepthStencilAttachment;
    auto Depth = Device.CreateTexture(DepthDesc);

    RHI::FRHIRenderPassDesc RenderPassDesc;
    RenderPassDesc.Attachments.push_back({
        RHI::ERHIAttachmentRole::Color,
        OutputDesc.Format,
        RHI::ERHISampleCount::One,
        RHI::ERHIAttachmentLoadOp::Clear,
        RHI::ERHIAttachmentStoreOp::Store});
    for (int Index = 0; Index < 2; ++Index)
        RenderPassDesc.Attachments.push_back({
            RHI::ERHIAttachmentRole::Color,
            AuxiliaryDesc.Format,
            RHI::ERHISampleCount::One,
            RHI::ERHIAttachmentLoadOp::Clear,
            RHI::ERHIAttachmentStoreOp::DontCare});
    RenderPassDesc.Attachments.push_back({
        RHI::ERHIAttachmentRole::DepthStencil,
        DepthDesc.Format,
        RHI::ERHISampleCount::One,
        RHI::ERHIAttachmentLoadOp::Clear,
        RHI::ERHIAttachmentStoreOp::DontCare});
    auto RenderPass = Device.CreateRenderPass(RenderPassDesc);
    RHI::FRHIFramebufferDesc FramebufferDesc;
    FramebufferDesc.RenderPass = RenderPass.Object;
    FramebufferDesc.Attachments.push_back({Output.Object, 0, 0});
    FramebufferDesc.Attachments.push_back({NormalRoughness.Object, 0, 0});
    FramebufferDesc.Attachments.push_back({EmissiveMetallic.Object, 0, 0});
    FramebufferDesc.Attachments.push_back({Depth.Object, 0, 0});
    FramebufferDesc.Width = Width;
    FramebufferDesc.Height = Height;
    auto Framebuffer = Device.CreateFramebuffer(FramebufferDesc);
    auto Commands = Device.CreateCommandBuffer(RHI::ERHIQueueType::Graphics);

    RHI::FRHITextureBufferCopyRegion Region;
    Region.Width = Width;
    Region.Height = Height;
    Core::uint64 ReadbackBytes = 0;
    if (!RHI::TryGetRHITextureBufferCopyByteSize(
            Region, OutputDesc.Format, ReadbackBytes))
    {
        Fail(OutReason, "production Forward readback footprint is invalid");
        return false;
    }
    RHI::FRHIBufferDesc ReadbackDesc;
    ReadbackDesc.SizeInBytes = ReadbackBytes;
    ReadbackDesc.Usage = RHI::ERHIBufferUsage::CopyDestination;
    ReadbackDesc.MemoryAccess = RHI::ERHIMemoryAccess::HostVisible;
    auto Readback = Device.CreateBuffer(ReadbackDesc);
    if (!Output.Succeeded() || !NormalRoughness.Succeeded() ||
        !EmissiveMetallic.Succeeded() || !Depth.Succeeded() ||
        !RenderPass.Succeeded() ||
        !Framebuffer.Succeeded() || !Commands.Succeeded() ||
        !Readback.Succeeded())
    {
        Fail(OutReason, "production Forward smoke resource creation failed");
        return false;
    }

    FForwardFrameExecutionBindings Candidate;
    Candidate.OutputTexture = std::move(Output.Object);
    Candidate.AuxiliaryColorTextures = {
        std::move(NormalRoughness.Object), std::move(EmissiveMetallic.Object)};
    Candidate.DepthTexture = std::move(Depth.Object);
    Candidate.RenderPass = std::move(RenderPass.Object);
    Candidate.Framebuffer = std::move(Framebuffer.Object);
    Candidate.CommandBuffer = std::move(Commands.Object);
    Candidate.ReadbackBuffer = std::move(Readback.Object);
    Candidate.ReadbackRegion = Region;
    Candidate.bTransitionToPresent = false;
    if (!BindProductionForwardDraws(
            Snapshot, ForwardPlan, Candidate, OutReason))
        return false;
    OutBindings = std::move(Candidate);
    return true;
}

void ReleaseProductionForwardSmoke(
    FForwardFrameExecutionBindings& Bindings) noexcept
{
    Bindings.Draws.clear();
    Bindings.CommandBuffer.reset();
    if (Bindings.Framebuffer) (void)Bindings.Framebuffer->Invalidate();
    if (Bindings.RenderPass) (void)Bindings.RenderPass->Invalidate();
    if (Bindings.ReadbackBuffer) (void)Bindings.ReadbackBuffer->Invalidate();
    if (Bindings.DepthTexture) (void)Bindings.DepthTexture->Invalidate();
    for (auto It = Bindings.AuxiliaryColorTextures.rbegin();
         It != Bindings.AuxiliaryColorTextures.rend(); ++It)
        if (*It) (void)(*It)->Invalidate();
    if (Bindings.OutputTexture) (void)Bindings.OutputTexture->Invalidate();
    Bindings = {};
}

} // namespace Stoner::Demo
