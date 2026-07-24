#include "Renderer/FDeferredRenderer.h"
#include "Renderer/FDeferredLightVolume.h"

namespace Stoner::Renderer
{

namespace
{

FForwardViewData MakeForwardView(const FDeferredViewData& View)
{
    FForwardViewData Result;
    Result.ViewName = View.Name;
    Result.ViewMatrix = View.View;
    Result.ViewProjectionMatrix = View.ViewProjection;
    Result.CameraPosition = View.CameraPosition;
    Result.Viewport.Extent = {View.Extent.Width, View.Extent.Height};
    return Result;
}

FMeshDrawCandidate MakeForwardCandidate(const FDeferredDrawCandidate& Candidate)
{
    FMeshDrawCandidate Forward = Candidate.ForwardCandidate;
    if (Forward.ObjectId == 0)
    {
        Forward.ObjectId = Candidate.Identity.Slot;
        Forward.MeshId = Candidate.MeshId;
        Forward.DebugName = Candidate.Name;
        Forward.WorldPosition = Candidate.Model.TransformPoint(Stoner::Core::FVector3::Zero());
        Forward.bWantsOpaque = false;
        Forward.bWantsTransparent = true;
        Forward.MaterialBinding.MaterialId = Candidate.MaterialId;
        Forward.MaterialBinding.MaterialName = Candidate.Name;
        Forward.MaterialBinding.Domain = Candidate.Domain;
        Forward.MaterialBinding.BlendMode = Candidate.BlendMode;
        Forward.MaterialBinding.bHasMaterialBinding = true;
        Forward.MaterialBinding.bHasShaderBinding = Candidate.bHasShaderBinding;
        Forward.MaterialBinding.SurfaceInputs = {true, true, true, true, true, true, true, {}};
    }
    return Forward;
}

} // namespace

FDeferredRenderer::FDeferredRenderer(FDeferredRendererConfiguration InConfiguration)
    : Configuration(InConfiguration)
{
}

EDeferredResult FDeferredRenderer::PrepareFrame(const FDeferredFrameInputs& Inputs,
    FDeferredFramePlan& OutPlan)
{
    OutPlan.Reset();
    if (bInvalidated)
    {
        return EDeferredResult::InvalidConfiguration;
    }
    OutPlan.FrameId = Inputs.FrameId.IsEmpty() ? Stoner::Core::FString("DeferredFrame") : Inputs.FrameId;
    OutPlan.View = Inputs.View;
    OutPlan.Output = Inputs.Output;
    OutPlan.AmbientContribution = Inputs.AmbientContribution;
    OutPlan.SurfaceLayout = MakeDefaultDeferredSurfaceLayout(Inputs.View.Extent,
        Inputs.View.DepthPolicy.Convention, Inputs.View.DepthPolicy.NearPlane,
        Inputs.View.DepthPolicy.FarPlane);
    OutPlan.SurfaceLayout.SampleCount = Configuration.SampleCount;

    const bool bViewValid = Inputs.View.IsValid(&OutPlan.Diagnostics);
    const bool bOutputValid = bViewValid && Inputs.Output.IsValid(Inputs.View, &OutPlan.Diagnostics);
    const bool bLayoutValid = bOutputValid && OutPlan.SurfaceLayout.IsValid(&OutPlan.Diagnostics) &&
        OutPlan.SurfaceLayout.DepthPolicy.GetIdentity() == Inputs.View.DepthPolicy.GetIdentity();
    if (!bViewValid || !bOutputValid || !bLayoutValid)
    {
        OutPlan.Diagnostics.SortStable();
        OutPlan.DebugDump = BuildDeferredFrameDebugDump(OutPlan);
        LastFramePlan = OutPlan;
        return !bViewValid ? EDeferredResult::InvalidView
            : (!bOutputValid ? EDeferredResult::InvalidOutput : EDeferredResult::InvalidSurfaceLayout);
    }

    const FForwardViewData ForwardView = MakeForwardView(Inputs.View);
    for (const FDeferredDrawCandidate& Candidate : Inputs.DrawCandidates)
    {
        if (Candidate.BlendMode == EMaterialBlendMode::Translucent)
        {
            if (Configuration.bEnableForwardTransparencyHandoff)
            {
                FMeshDrawCandidate ForwardCandidate = MakeForwardCandidate(Candidate);
                FForwardDiagnosticLog ForwardDiagnostics;
                if (ValidateForwardMeshDrawCandidate(ForwardCandidate, EForwardDrawPass::Transparent,
                        &ForwardDiagnostics))
                {
                    FMeshDrawCommand Command(ForwardCandidate, EForwardDrawPass::Transparent, ForwardView);
                    Command.SetValidationState(EForwardValidationState::Accepted);
                    OutPlan.TransparentHandoff.push_back(Command);
                    continue;
                }
            }
            FDeferredDrawRecord Rejected;
            Rejected.Candidate = Candidate;
            Rejected.Reason = "transparent-handoff-unavailable";
            OutPlan.RejectedDraws.push_back(Rejected);
            continue;
        }
        if (Candidate.BlendMode == EMaterialBlendMode::Masked && !Configuration.bEnableMaskedGeometry)
        {
            FDeferredDrawRecord Rejected;
            Rejected.Candidate = Candidate;
            Rejected.Reason = "masked-disabled";
            OutPlan.RejectedDraws.push_back(Rejected);
            continue;
        }
        FDeferredDrawRecord Record;
        if (ValidateDeferredDrawCandidate(Candidate, Record, &OutPlan.Diagnostics))
        {
            OutPlan.AcceptedDraws.push_back(Record);
        }
        else
        {
            OutPlan.RejectedDraws.push_back(Record);
        }
    }
    SortDeferredDrawRecords(OutPlan.AcceptedDraws);
    SortDeferredDrawRecords(OutPlan.RejectedDraws);
    SortForwardTransparentDraws(OutPlan.TransparentHandoff);

    OutPlan.Lights = PrepareDeferredLightSet(Inputs.DirectionalLights, Inputs.PointLights,
        Inputs.SpotLights, &OutPlan.Diagnostics);
    ApplyDeferredLightVolumeCulling(OutPlan.Lights, Inputs.View,
        Configuration.bCullLocalLightsOutsideView);
    OutPlan.AddPass(EDeferredPassStage::SurfaceData, "DeferredSurfaceData",
        static_cast<Stoner::Core::uint32>(OutPlan.AcceptedDraws.size()), 0, {},
        {"BaseColorAO", "NormalRoughness", "EmissiveMetallic", "Depth", "LightingAccumulation"});
    const Stoner::Core::uint32 DirectionalCount =
        OutPlan.Lights.GetAcceptedCount(EDeferredLightType::Directional);
    const Stoner::Core::uint32 PointCount =
        OutPlan.Lights.GetAcceptedCount(EDeferredLightType::Point);
    const Stoner::Core::uint32 SpotCount =
        OutPlan.Lights.GetAcceptedCount(EDeferredLightType::Spot);
    const Stoner::Core::TArray<Stoner::Core::FString> SurfaceReads = {
        "BaseColorAO", "NormalRoughness", "EmissiveMetallic", "Depth"};
    if (DirectionalCount > 0)
    {
        OutPlan.AddPass(EDeferredPassStage::DirectionalLighting, "DeferredDirectionalLighting",
            DirectionalCount, DirectionalCount, SurfaceReads, {"LightingAccumulation"}, true);
    }
    if (PointCount > 0)
    {
        OutPlan.AddPass(EDeferredPassStage::PointLightVolumes, "DeferredPointLightVolumes",
            PointCount, PointCount, SurfaceReads, {"LightingAccumulation"}, true);
    }
    if (SpotCount > 0)
    {
        OutPlan.AddPass(EDeferredPassStage::SpotLightVolumes, "DeferredSpotLightVolumes",
            SpotCount, SpotCount, SurfaceReads, {"LightingAccumulation"}, true);
    }
    OutPlan.AddPass(EDeferredPassStage::Composition, "DeferredComposition", 1, 0,
        {"BaseColorAO", "EmissiveMetallic", "LightingAccumulation"}, {Inputs.Output.Name});
    if (!OutPlan.TransparentHandoff.empty())
    {
        OutPlan.AddPass(EDeferredPassStage::ForwardTransparency, "ForwardTransparency",
            static_cast<Stoner::Core::uint32>(OutPlan.TransparentHandoff.size()), 0,
            {Inputs.Output.Name}, {Inputs.Output.Name}, true);
    }
    if (Configuration.bEnableValidationReadback)
    {
        OutPlan.AddPass(EDeferredPassStage::ValidationReadback, "DeferredValidationReadback", 0, 0,
            {"BaseColorAO", "NormalRoughness", "EmissiveMetallic", "Depth", Inputs.Output.Name},
            {"ReadbackBuffers"}, true);
    }

    OutPlan.InputFingerprint = BuildDeferredInputFingerprint(OutPlan);
    OutPlan.Diagnostics.SortStable();
    OutPlan.bValid = true;
    OutPlan.DebugDump = BuildDeferredFrameDebugDump(OutPlan);
    LastFramePlan = OutPlan;
    return EDeferredResult::Success;
}

void FDeferredRenderer::Reset()
{
    LastFramePlan.Reset();
    bInvalidated = false;
}

void FDeferredRenderer::Invalidate()
{
    LastFramePlan.Reset();
    bInvalidated = true;
}

const FDeferredRendererConfiguration& FDeferredRenderer::GetConfiguration() const noexcept
{
    return Configuration;
}

void FDeferredRenderer::SetConfiguration(FDeferredRendererConfiguration InConfiguration) noexcept
{
    Configuration = InConfiguration;
}

const FDeferredFramePlan& FDeferredRenderer::GetLastFramePlan() const noexcept
{
    return LastFramePlan;
}

ERendererStrategy GetDefaultRendererStrategy() noexcept
{
    return ERendererStrategy::Forward;
}

const char* ToString(ERendererStrategy Strategy) noexcept
{
    return Strategy == ERendererStrategy::Forward ? "Forward" : "Deferred";
}

} // namespace Stoner::Renderer
