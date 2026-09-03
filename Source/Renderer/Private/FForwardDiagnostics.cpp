#include "Renderer/FForwardDiagnostics.h"

#include "Renderer/FForwardFramePlan.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace Stoner::Renderer
{

void FForwardDiagnosticLog::Add(EForwardDiagnosticSeverity Severity,
    EForwardDiagnosticCategory Category,
    EForwardResult Result,
    Stoner::Core::FString StableCode,
    Stoner::Core::FString SubjectName,
    Stoner::Core::FString Message)
{
    Records.push_back({Severity, Category, Result, std::move(StableCode), std::move(SubjectName), std::move(Message)});
}

void FForwardDiagnosticLog::Merge(const FForwardDiagnosticLog& Other)
{
    Records.insert(Records.end(), Other.Records.begin(), Other.Records.end());
    SortStable();
}

void FForwardDiagnosticLog::SortStable()
{
    std::stable_sort(Records.begin(), Records.end(), [](const FForwardDiagnosticRecord& Left, const FForwardDiagnosticRecord& Right) {
        if (Left.StableCode != Right.StableCode)
        {
            return Left.StableCode < Right.StableCode;
        }
        if (Left.SubjectName != Right.SubjectName)
        {
            return Left.SubjectName < Right.SubjectName;
        }
        return Left.Message < Right.Message;
    });
}

void FForwardDiagnosticLog::Clear()
{
    Records.clear();
}

bool FForwardDiagnosticLog::HasErrors() const noexcept
{
    return std::any_of(Records.begin(), Records.end(), [](const FForwardDiagnosticRecord& Record) {
        return Record.Severity == EForwardDiagnosticSeverity::Error;
    });
}

bool FForwardDiagnosticLog::IsEmpty() const noexcept
{
    return Records.empty();
}

int FForwardDiagnosticLog::CountByCode(const Stoner::Core::FString& StableCode) const noexcept
{
    return static_cast<int>(std::count_if(Records.begin(), Records.end(), [&StableCode](const FForwardDiagnosticRecord& Record) {
        return Record.StableCode == StableCode;
    }));
}

const Stoner::Core::TArray<FForwardDiagnosticRecord>& FForwardDiagnosticLog::GetRecords() const noexcept
{
    return Records;
}

Stoner::Core::TArray<FForwardDiagnosticRecord>& FForwardDiagnosticLog::GetMutableRecords() noexcept
{
    return Records;
}

Stoner::Core::FString FForwardDiagnosticLog::Format() const
{
    std::ostringstream Stream;
    for (const FForwardDiagnosticRecord& Record : Records)
    {
        Stream << Record.StableCode.CStr() << '[' << ToString(Record.Severity) << '/'
            << ToString(Record.Category) << '/' << ToString(Record.Result) << "] "
            << Record.SubjectName.CStr() << ": " << Record.Message.CStr() << '\n';
    }
    return Stoner::Core::FString(Stream.str());
}

const char* ToString(EForwardResult Result) noexcept
{
    switch (Result)
    {
    case EForwardResult::Success: return "Success";
    case EForwardResult::ValidationFailed: return "ValidationFailed";
    case EForwardResult::InvalidView: return "InvalidView";
    case EForwardResult::InvalidOutput: return "InvalidOutput";
    case EForwardResult::InvalidMaterial: return "InvalidMaterial";
    case EForwardResult::InvalidLight: return "InvalidLight";
    case EForwardResult::Invalidated: return "Invalidated";
    }
    return "Unknown";
}

const char* ToString(EForwardValidationState State) noexcept
{
    switch (State)
    {
    case EForwardValidationState::Draft: return "Draft";
    case EForwardValidationState::Accepted: return "Accepted";
    case EForwardValidationState::Rejected: return "Rejected";
    case EForwardValidationState::Invalidated: return "Invalidated";
    }
    return "Unknown";
}

const char* ToString(EForwardDiagnosticSeverity Severity) noexcept
{
    switch (Severity)
    {
    case EForwardDiagnosticSeverity::Info: return "Info";
    case EForwardDiagnosticSeverity::Warning: return "Warning";
    case EForwardDiagnosticSeverity::Error: return "Error";
    }
    return "Unknown";
}

const char* ToString(EForwardDiagnosticCategory Category) noexcept
{
    switch (Category)
    {
    case EForwardDiagnosticCategory::View: return "View";
    case EForwardDiagnosticCategory::Output: return "Output";
    case EForwardDiagnosticCategory::Light: return "Light";
    case EForwardDiagnosticCategory::Material: return "Material";
    case EForwardDiagnosticCategory::Draw: return "Draw";
    case EForwardDiagnosticCategory::Pass: return "Pass";
    case EForwardDiagnosticCategory::ResourceDeclaration: return "ResourceDeclaration";
    case EForwardDiagnosticCategory::Fallback: return "Fallback";
    case EForwardDiagnosticCategory::Dump: return "Dump";
    }
    return "Unknown";
}

Stoner::Core::FString BuildForwardFrameDebugDump(const FForwardFramePlan& Plan)
{
    std::ostringstream Stream;
    Stream << "ForwardFrame " << Plan.FrameName.CStr() << '\n';
    Stream << "  Valid=" << (Plan.bValid ? "true" : "false") << '\n';
    Stream << "View " << Plan.ViewData.ViewName.CStr()
        << " extent=" << Plan.ViewData.Viewport.Extent.Width << 'x' << Plan.ViewData.Viewport.Extent.Height
        << " camera=" << FormatForwardVector(Plan.ViewData.CameraPosition).CStr() << '\n';
    Stream << "Output color=" << Plan.OutputTarget.ColorTargetName.CStr()
        << " depth=" << Plan.OutputTarget.DepthTargetName.CStr()
        << " format=" << Plan.OutputTarget.FormatSummary.CStr() << '\n';
    Stream << "SceneColorHandoff producer="
        << ToString(Plan.SceneColorHandoff.GetProducer())
        << " state=" << ToString(Plan.SceneColorHandoff.GetState())
        << " sceneColorId=" << Plan.SceneColorHandoff.GetSceneColorId()
        << " viewId=" << Plan.SceneColorHandoff.GetViewId()
        << " frameToken=" << Plan.SceneColorHandoff.GetFrameToken() << '\n';

    Stream << "PassOrder\n";
    for (const FForwardPassRecord& Pass : Plan.PassOrder)
    {
        Stream << "  " << Pass.PassId.Value << ':' << ToString(Pass.Stage)
            << ':' << Pass.Name.CStr() << " draws=" << Pass.DrawCount << '\n';
    }

    Stream << "OpaqueDraws count=" << Plan.AcceptedOpaqueDraws.size() << '\n';
    for (const FMeshDrawCommand& Draw : Plan.AcceptedOpaqueDraws)
    {
        Stream << "  " << Draw.GetStableIdentity().CStr() << " material="
            << Draw.GetMaterialId() << " mesh=" << Draw.GetMeshId() << '\n';
    }

    Stream << "TransparentDraws count=" << Plan.AcceptedTransparentDraws.size() << '\n';
    for (const FMeshDrawCommand& Draw : Plan.AcceptedTransparentDraws)
    {
        Stream << "  " << Draw.GetStableIdentity().CStr() << " depth="
            << std::fixed << std::setprecision(3) << Draw.GetCameraSpaceDepth()
            << " material=" << Draw.GetMaterialId() << " object=" << Draw.GetObjectId() << '\n';
    }

    Stream << "Lights directional=" << (Plan.LightSet.bHasDirectionalLight ? 1 : 0)
        << " pointAccepted=" << Plan.LightSet.AcceptedPointLights.size()
        << " rejected=" << Plan.LightSet.RejectedLights.size() << '\n';
    if (Plan.LightSet.bHasDirectionalLight)
    {
        Stream << "  Directional " << Plan.LightSet.DirectionalLight.LightId << ':'
            << Plan.LightSet.DirectionalLight.Name.CStr() << '\n';
    }
    for (const FForwardPointLight& Light : Plan.LightSet.AcceptedPointLights)
    {
        Stream << "  Point " << Light.LightId << ':' << Light.Name.CStr()
            << " score=" << std::fixed << std::setprecision(6) << Light.InfluenceScore << '\n';
    }

    Stream << "Background mode=" << ToString(Plan.Environment.Mode)
        << " name=" << Plan.Environment.BackgroundName.CStr() << '\n';
    Stream << "AmbientFallback active=" << (Plan.AmbientFallback.bActive ? "true" : "false") << '\n';
    Stream << Plan.GraphDeclaration.Dump().CStr();
    Stream << "Diagnostics\n" << Plan.Diagnostics.Format().CStr();
    return Stoner::Core::FString(Stream.str());
}

} // namespace Stoner::Renderer
