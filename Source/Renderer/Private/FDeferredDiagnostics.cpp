#include "Renderer/FDeferredDiagnostics.h"

#include <algorithm>
#include <sstream>

namespace Stoner::Renderer
{

void FDeferredDiagnosticLog::Add(EDeferredDiagnosticSeverity Severity, EDeferredPassStage Stage,
    EDeferredResult Result, Stoner::Core::FString Code, Stoner::Core::FString Subject,
    Stoner::Core::FString Reason)
{
    Records.push_back({Severity, Stage, Result, std::move(Code), std::move(Subject),
        std::move(Reason), NextSequence++});
}

void FDeferredDiagnosticLog::Merge(const FDeferredDiagnosticLog& Other)
{
    for (const FDeferredDiagnostic& Record : Other.GetRecords())
    {
        Add(Record.Severity, Record.Stage, Record.Result, Record.Code, Record.Subject, Record.Reason);
    }
}

void FDeferredDiagnosticLog::Clear()
{
    Records.clear();
    NextSequence = 1;
}

void FDeferredDiagnosticLog::SortStable()
{
    std::stable_sort(Records.begin(), Records.end(), [](const FDeferredDiagnostic& Left,
        const FDeferredDiagnostic& Right) {
        if (Left.Severity != Right.Severity)
        {
            return Left.Severity > Right.Severity;
        }
        if (Left.Stage != Right.Stage)
        {
            return Left.Stage < Right.Stage;
        }
        if (Left.Subject != Right.Subject)
        {
            return Left.Subject < Right.Subject;
        }
        if (Left.Code != Right.Code)
        {
            return Left.Code < Right.Code;
        }
        return Left.Sequence < Right.Sequence;
    });
}

bool FDeferredDiagnosticLog::HasErrors() const noexcept
{
    return GetFirstError() != nullptr;
}

const FDeferredDiagnostic* FDeferredDiagnosticLog::GetFirstError() const noexcept
{
    const FDeferredDiagnostic* First = nullptr;
    for (const FDeferredDiagnostic& Record : Records)
    {
        if (Record.Severity == EDeferredDiagnosticSeverity::Error &&
            (!First || Record.Sequence < First->Sequence))
        {
            First = &Record;
        }
    }
    return First;
}

const Stoner::Core::TArray<FDeferredDiagnostic>& FDeferredDiagnosticLog::GetRecords() const noexcept
{
    return Records;
}

Stoner::Core::FString FDeferredDiagnosticLog::Dump() const
{
    std::ostringstream Stream;
    for (const FDeferredDiagnostic& Record : Records)
    {
        Stream << ToString(Record.Severity) << ' ' << ToString(Record.Stage) << ' '
            << ToString(Record.Result) << ' ' << Record.Code.CStr() << " subject="
            << Record.Subject.CStr() << " reason=" << Record.Reason.CStr() << '\n';
    }
    return Stoner::Core::FString(Stream.str());
}

const char* ToString(EDeferredResult Result) noexcept
{
    switch (Result)
    {
    case EDeferredResult::Success: return "Success";
    case EDeferredResult::InvalidConfiguration: return "InvalidConfiguration";
    case EDeferredResult::InvalidView: return "InvalidView";
    case EDeferredResult::InvalidOutput: return "InvalidOutput";
    case EDeferredResult::InvalidSurfaceLayout: return "InvalidSurfaceLayout";
    case EDeferredResult::InvalidDraw: return "InvalidDraw";
    case EDeferredResult::InvalidMaterial: return "InvalidMaterial";
    case EDeferredResult::InvalidLight: return "InvalidLight";
    case EDeferredResult::InvalidBinding: return "InvalidBinding";
    case EDeferredResult::GraphCompilationFailed: return "GraphCompilationFailed";
    case EDeferredResult::RecordFailed: return "RecordFailed";
    case EDeferredResult::SubmitFailed: return "SubmitFailed";
    case EDeferredResult::ReadbackFailed: return "ReadbackFailed";
    case EDeferredResult::ValidationFailed: return "ValidationFailed";
    case EDeferredResult::ComparisonInvalid: return "ComparisonInvalid";
    }
    return "Unknown";
}

const char* ToString(EDeferredPassStage Stage) noexcept
{
    switch (Stage)
    {
    case EDeferredPassStage::SurfaceData: return "SurfaceData";
    case EDeferredPassStage::DirectionalLighting: return "DirectionalLighting";
    case EDeferredPassStage::PointLightVolumes: return "PointLightVolumes";
    case EDeferredPassStage::SpotLightVolumes: return "SpotLightVolumes";
    case EDeferredPassStage::Composition: return "Composition";
    case EDeferredPassStage::ForwardTransparency: return "ForwardTransparency";
    case EDeferredPassStage::ValidationReadback: return "ValidationReadback";
    }
    return "Unknown";
}

const char* ToString(EDeferredDiagnosticSeverity Severity) noexcept
{
    switch (Severity)
    {
    case EDeferredDiagnosticSeverity::Info: return "Info";
    case EDeferredDiagnosticSeverity::Warning: return "Warning";
    case EDeferredDiagnosticSeverity::Error: return "Error";
    }
    return "Unknown";
}

} // namespace Stoner::Renderer
