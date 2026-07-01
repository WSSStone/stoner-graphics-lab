#include "Renderer/FMaterialDiagnostics.h"

#include <sstream>

namespace Stoner::Renderer
{

void FMaterialDiagnosticLog::Clear()
{
    Records.clear();
}

void FMaterialDiagnosticLog::Add(EMaterialDiagnosticSeverity Severity, EMaterialDiagnosticCategory Category, EMaterialResult Result,
    Stoner::Core::FString StableCode, Stoner::Core::FString SubjectName, Stoner::Core::FString Message)
{
    Records.push_back({Severity, Category, Result, std::move(StableCode), std::move(SubjectName), std::move(Message)});
}

bool FMaterialDiagnosticLog::HasErrors() const noexcept
{
    for (const FMaterialDiagnostic& Record : Records)
    {
        if (Record.Severity == EMaterialDiagnosticSeverity::Error)
        {
            return true;
        }
    }
    return false;
}

const Stoner::Core::TArray<FMaterialDiagnostic>& FMaterialDiagnosticLog::GetRecords() const noexcept
{
    return Records;
}

Stoner::Core::FString FMaterialDiagnosticLog::Format() const
{
    std::ostringstream Stream;
    for (const FMaterialDiagnostic& Record : Records)
    {
        Stream << ToString(Record.Severity) << ':' << ToString(Record.Category) << ':' << Record.StableCode.CStr()
            << ':' << ToString(Record.Result) << ':' << Record.SubjectName.CStr() << ':' << Record.Message.CStr() << '\n';
    }
    return Stoner::Core::FString(Stream.str());
}

const char* ToString(EMaterialResult Result) noexcept
{
    switch (Result)
    {
    case EMaterialResult::Success: return "Success";
    case EMaterialResult::InvalidState: return "InvalidState";
    case EMaterialResult::ValidationFailed: return "ValidationFailed";
    case EMaterialResult::DuplicateName: return "DuplicateName";
    case EMaterialResult::NotFound: return "NotFound";
    case EMaterialResult::TypeMismatch: return "TypeMismatch";
    case EMaterialResult::UnsupportedCombination: return "UnsupportedCombination";
    case EMaterialResult::CycleDetected: return "CycleDetected";
    case EMaterialResult::Invalidated: return "Invalidated";
    }
    return "Unknown";
}

const char* ToString(EMaterialValidationState State) noexcept
{
    switch (State)
    {
    case EMaterialValidationState::Draft: return "Draft";
    case EMaterialValidationState::Valid: return "Valid";
    case EMaterialValidationState::Invalid: return "Invalid";
    case EMaterialValidationState::CycleDetected: return "CycleDetected";
    case EMaterialValidationState::Invalidated: return "Invalidated";
    }
    return "Unknown";
}

const char* ToString(EMaterialDiagnosticSeverity Severity) noexcept
{
    switch (Severity)
    {
    case EMaterialDiagnosticSeverity::Info: return "Info";
    case EMaterialDiagnosticSeverity::Warning: return "Warning";
    case EMaterialDiagnosticSeverity::Error: return "Error";
    }
    return "Unknown";
}

const char* ToString(EMaterialDiagnosticCategory Category) noexcept
{
    switch (Category)
    {
    case EMaterialDiagnosticCategory::Material: return "Material";
    case EMaterialDiagnosticCategory::Instance: return "Instance";
    case EMaterialDiagnosticCategory::Parameter: return "Parameter";
    case EMaterialDiagnosticCategory::ShaderLibrary: return "ShaderLibrary";
    case EMaterialDiagnosticCategory::Permutation: return "Permutation";
    case EMaterialDiagnosticCategory::ResourceRequirement: return "ResourceRequirement";
    case EMaterialDiagnosticCategory::Invalidation: return "Invalidation";
    case EMaterialDiagnosticCategory::Dump: return "Dump";
    }
    return "Unknown";
}

} // namespace Stoner::Renderer
