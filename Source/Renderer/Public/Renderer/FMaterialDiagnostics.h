#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::Renderer
{

enum class EMaterialResult
{
    Success,
    InvalidState,
    ValidationFailed,
    DuplicateName,
    NotFound,
    TypeMismatch,
    UnsupportedCombination,
    CycleDetected,
    Invalidated
};

enum class EMaterialValidationState
{
    Draft,
    Valid,
    Invalid,
    CycleDetected,
    Invalidated
};

enum class EMaterialDiagnosticSeverity
{
    Info,
    Warning,
    Error
};

enum class EMaterialDiagnosticCategory
{
    Material,
    Instance,
    Parameter,
    ShaderLibrary,
    Permutation,
    ResourceRequirement,
    Invalidation,
    Dump
};

struct FMaterialDiagnostic
{
    EMaterialDiagnosticSeverity Severity = EMaterialDiagnosticSeverity::Info;
    EMaterialDiagnosticCategory Category = EMaterialDiagnosticCategory::Material;
    EMaterialResult Result = EMaterialResult::Success;
    Stoner::Core::FString StableCode;
    Stoner::Core::FString SubjectName;
    Stoner::Core::FString Message;
};

class FMaterialDiagnosticLog
{
public:
    void Clear();
    void Add(EMaterialDiagnosticSeverity Severity, EMaterialDiagnosticCategory Category, EMaterialResult Result,
        Stoner::Core::FString StableCode, Stoner::Core::FString SubjectName, Stoner::Core::FString Message);

    [[nodiscard]] bool HasErrors() const noexcept;
    [[nodiscard]] const Stoner::Core::TArray<FMaterialDiagnostic>& GetRecords() const noexcept;
    [[nodiscard]] Stoner::Core::FString Format() const;

private:
    Stoner::Core::TArray<FMaterialDiagnostic> Records;
};

[[nodiscard]] const char* ToString(EMaterialResult Result) noexcept;
[[nodiscard]] const char* ToString(EMaterialValidationState State) noexcept;
[[nodiscard]] const char* ToString(EMaterialDiagnosticSeverity Severity) noexcept;
[[nodiscard]] const char* ToString(EMaterialDiagnosticCategory Category) noexcept;

} // namespace Stoner::Renderer
