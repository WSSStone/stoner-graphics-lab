#include "Renderer/FForwardFramePlan.h"

namespace Stoner::Renderer
{

bool FForwardOutputTarget::IsValid(const FForwardViewData& View, FForwardDiagnosticLog* Diagnostics) const
{
    return ValidateForwardOutputTarget(*this, View, Diagnostics);
}

void FForwardFramePlan::Reset()
{
    *this = FForwardFramePlan{};
}

void FForwardFramePlan::AddPass(EForwardPassStage Stage, Stoner::Core::FString Name, Stoner::Core::uint32 DrawCount)
{
    FForwardPassRecord Pass;
    Pass.PassId.Value = static_cast<Stoner::Core::uint32>(PassOrder.size() + 1);
    Pass.Stage = Stage;
    Pass.Name = std::move(Name);
    Pass.DrawCount = DrawCount;
    PassOrder.push_back(Pass);
}

bool FForwardFramePlan::IsValid() const noexcept
{
    return bValid;
}

bool FForwardFramePlan::HasRenderableGeometry() const noexcept
{
    return !AcceptedOpaqueDraws.empty() || !AcceptedTransparentDraws.empty();
}

const FForwardPassRecord* FForwardFramePlan::FindPass(EForwardPassStage Stage) const noexcept
{
    for (const FForwardPassRecord& Pass : PassOrder)
    {
        if (Pass.Stage == Stage)
        {
            return &Pass;
        }
    }
    return nullptr;
}

bool ValidateForwardOutputTarget(const FForwardOutputTarget& Output,
    const FForwardViewData& View,
    FForwardDiagnosticLog* Diagnostics)
{
    bool bValid = true;
    if (Output.ColorTargetName.IsEmpty())
    {
        bValid = false;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EForwardDiagnosticSeverity::Error, EForwardDiagnosticCategory::Output,
                EForwardResult::InvalidOutput, "FWD-OUTPUT-COLOR", "Output", "color target name is required");
        }
    }
    if (Output.FormatSummary != "RGBA16F")
    {
        bValid = false;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EForwardDiagnosticSeverity::Error,
                EForwardDiagnosticCategory::Output,
                EForwardResult::InvalidOutput, "FWD-OUTPUT-FORMAT",
                Output.ColorTargetName,
                "SceneColor handoff format must be RGBA16F");
        }
    }
    if (!IsPositiveForwardExtent(Output.Extent))
    {
        bValid = false;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EForwardDiagnosticSeverity::Error, EForwardDiagnosticCategory::Output,
                EForwardResult::InvalidOutput, "FWD-OUTPUT-EXTENT", Output.ColorTargetName,
                "output extent must be positive");
        }
    }
    if (IsPositiveForwardExtent(Output.Extent) &&
        (Output.Extent.Width != View.Viewport.Extent.Width || Output.Extent.Height != View.Viewport.Extent.Height))
    {
        bValid = false;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EForwardDiagnosticSeverity::Error, EForwardDiagnosticCategory::Output,
                EForwardResult::InvalidOutput, "FWD-OUTPUT-VIEW-EXTENT", Output.ColorTargetName,
                "output extent must match the active view viewport extent");
        }
    }
    return bValid;
}

FForwardEnvironmentBackground ValidateForwardEnvironmentBackground(const FForwardEnvironmentBackground& Environment,
    bool bEnableSkyBackground,
    FForwardDiagnosticLog* Diagnostics)
{
    FForwardEnvironmentBackground Result = Environment;
    if (!bEnableSkyBackground)
    {
        Result.Mode = EForwardBackgroundMode::Clear;
        Result.BackgroundName = "Clear";
        Result.ResourceReference.Clear();
        return Result;
    }

    if (Result.BackgroundName.IsEmpty())
    {
        Result.BackgroundName = Result.Mode == EForwardBackgroundMode::Clear ? "Clear" : "Environment";
    }
    if (Result.Mode == EForwardBackgroundMode::EnvironmentReference && Result.ResourceReference.IsEmpty())
    {
        Result.Mode = EForwardBackgroundMode::Clear;
        Result.BackgroundName = "Clear";
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EForwardDiagnosticSeverity::Warning, EForwardDiagnosticCategory::ResourceDeclaration,
                EForwardResult::ValidationFailed, "FWD-ENV-RESOURCE", "Environment",
                "environment reference was missing; clear background will be used");
        }
    }
    return Result;
}

const char* ToString(EForwardPassStage Stage) noexcept
{
    switch (Stage)
    {
    case EForwardPassStage::Depth: return "Depth";
    case EForwardPassStage::Opaque: return "Opaque";
    case EForwardPassStage::SkyBackground: return "SkyBackground";
    case EForwardPassStage::Transparent: return "Transparent";
    }
    return "Unknown";
}

const char* ToString(EForwardBackgroundMode Mode) noexcept
{
    switch (Mode)
    {
    case EForwardBackgroundMode::Clear: return "Clear";
    case EForwardBackgroundMode::Sky: return "Sky";
    case EForwardBackgroundMode::EnvironmentReference: return "EnvironmentReference";
    }
    return "Unknown";
}

} // namespace Stoner::Renderer
