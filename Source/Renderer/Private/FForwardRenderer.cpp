#include "Renderer/FForwardRenderer.h"

namespace Stoner::Renderer
{

FForwardRenderer::FForwardRenderer(FForwardRendererConfiguration InConfiguration)
    : Configuration(InConfiguration)
{
}

EForwardResult FForwardRenderer::PrepareFrame(const FForwardFrameInputs& Inputs, FForwardFramePlan& OutPlan)
{
    Diagnostics.Clear();
    OutPlan.Reset();
    OutPlan.FrameId.Value = NextFrameId++;
    OutPlan.FrameName = Inputs.FrameName.IsEmpty() ? Stoner::Core::FString("ForwardFrame") : Inputs.FrameName;
    OutPlan.ViewData = Inputs.View;
    OutPlan.OutputTarget = Inputs.Output;

    FForwardDiagnosticLog LocalDiagnostics;
    const bool bViewValid = Inputs.View.IsValid(&LocalDiagnostics);
    const bool bOutputValid = bViewValid && Inputs.Output.IsValid(Inputs.View, &LocalDiagnostics);
    if (!bViewValid || !bOutputValid)
    {
        LocalDiagnostics.SortStable();
        OutPlan.Diagnostics = LocalDiagnostics;
        OutPlan.DebugDump = BuildForwardFrameDebugDump(OutPlan);
        Diagnostics = LocalDiagnostics;
        ValidationState = EForwardValidationState::Rejected;
        LastFramePlan = OutPlan;
        return !bViewValid ? EForwardResult::InvalidView : EForwardResult::InvalidOutput;
    }

    OutPlan.Environment = ValidateForwardEnvironmentBackground(Inputs.Environment, Configuration.bEnableSkyBackground, &LocalDiagnostics);

    for (const FMeshDrawCandidate& Candidate : Inputs.DrawCandidates)
    {
        bool bAcceptedAnyPass = false;
        if (Candidate.bWantsOpaque)
        {
            FForwardDiagnosticLog DrawDiagnostics;
            if (ValidateForwardMeshDrawCandidate(Candidate, EForwardDrawPass::Opaque, &DrawDiagnostics))
            {
                FMeshDrawCommand Draw(Candidate, EForwardDrawPass::Opaque, Inputs.View);
                Draw.SetValidationState(EForwardValidationState::Accepted);
                OutPlan.AcceptedOpaqueDraws.push_back(Draw);
                bAcceptedAnyPass = true;
            }
            LocalDiagnostics.Merge(DrawDiagnostics);
        }
        if (Candidate.bWantsTransparent)
        {
            FForwardDiagnosticLog DrawDiagnostics;
            if (ValidateForwardMeshDrawCandidate(Candidate, EForwardDrawPass::Transparent, &DrawDiagnostics))
            {
                FMeshDrawCommand Draw(Candidate, EForwardDrawPass::Transparent, Inputs.View);
                Draw.SetValidationState(EForwardValidationState::Accepted);
                OutPlan.AcceptedTransparentDraws.push_back(Draw);
                bAcceptedAnyPass = true;
            }
            LocalDiagnostics.Merge(DrawDiagnostics);
        }
        if (!bAcceptedAnyPass)
        {
            OutPlan.RejectedDraws.push_back(Candidate);
        }
    }

    SortForwardOpaqueDraws(OutPlan.AcceptedOpaqueDraws);
    SortForwardTransparentDraws(OutPlan.AcceptedTransparentDraws);

    OutPlan.LightSet = PrepareForwardLightSet(Inputs.DirectionalLights, Inputs.PointLights, Inputs.View,
        Configuration.PointLightLimit, &LocalDiagnostics);
    if (OutPlan.HasRenderableGeometry() && !OutPlan.LightSet.HasAcceptedLights() && Configuration.bEnableAmbientFallback)
    {
        OutPlan.AmbientFallback.bActive = true;
        LocalDiagnostics.Add(EForwardDiagnosticSeverity::Warning, EForwardDiagnosticCategory::Fallback,
            EForwardResult::Success, "FWD-AMBIENT-FALLBACK", OutPlan.FrameName,
            "valid geometry prepared with constant ambient-only fallback because no lights were accepted");
    }

    if (!OutPlan.AcceptedOpaqueDraws.empty())
    {
        OutPlan.AddPass(EForwardPassStage::Depth, "ForwardDepthPrepass", static_cast<Stoner::Core::uint32>(OutPlan.AcceptedOpaqueDraws.size()));
        OutPlan.AddPass(EForwardPassStage::Opaque, "ForwardOpaqueLighting", static_cast<Stoner::Core::uint32>(OutPlan.AcceptedOpaqueDraws.size()));
    }
    OutPlan.AddPass(EForwardPassStage::SkyBackground, "ForwardSkyBackground", 0);
    if (!OutPlan.AcceptedTransparentDraws.empty())
    {
        OutPlan.AddPass(EForwardPassStage::Transparent, "ForwardTransparent", static_cast<Stoner::Core::uint32>(OutPlan.AcceptedTransparentDraws.size()));
    }

    OutPlan.GraphDeclaration = BuildForwardRenderGraphDeclaration(OutPlan, &LocalDiagnostics);
    LocalDiagnostics.SortStable();
    OutPlan.Diagnostics = LocalDiagnostics;
    OutPlan.bValid = true;
    OutPlan.DebugDump = BuildForwardFrameDebugDump(OutPlan);

    Diagnostics = LocalDiagnostics;
    LastFramePlan = OutPlan;
    ValidationState = EForwardValidationState::Accepted;
    return EForwardResult::Success;
}

void FForwardRenderer::Reset()
{
    LastFramePlan.Reset();
    Diagnostics.Clear();
    ValidationState = EForwardValidationState::Draft;
}

void FForwardRenderer::Invalidate()
{
    LastFramePlan.Reset();
    Diagnostics.Clear();
    ValidationState = EForwardValidationState::Invalidated;
}

const FForwardRendererConfiguration& FForwardRenderer::GetConfiguration() const noexcept
{
    return Configuration;
}

void FForwardRenderer::SetConfiguration(FForwardRendererConfiguration InConfiguration) noexcept
{
    Configuration = InConfiguration;
}

EForwardValidationState FForwardRenderer::GetValidationState() const noexcept
{
    return ValidationState;
}

const FForwardFramePlan& FForwardRenderer::GetLastFramePlan() const noexcept
{
    return LastFramePlan;
}

const FForwardDiagnosticLog& FForwardRenderer::GetDiagnostics() const noexcept
{
    return Diagnostics;
}

const char* ToString(EForwardTransparentSortPolicy Policy) noexcept
{
    switch (Policy)
    {
    case EForwardTransparentSortPolicy::CameraDepthDescendingMaterialObject:
        return "CameraDepthDescendingMaterialObject";
    }
    return "Unknown";
}

} // namespace Stoner::Renderer
