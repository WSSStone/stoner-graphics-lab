#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::Renderer
{

enum class EForwardResult
{
    Success,
    ValidationFailed,
    InvalidView,
    InvalidOutput,
    InvalidMaterial,
    InvalidLight,
    Invalidated
};

enum class EForwardValidationState
{
    Draft,
    Accepted,
    Rejected,
    Invalidated
};

enum class EForwardDiagnosticSeverity
{
    Info,
    Warning,
    Error
};

enum class EForwardDiagnosticCategory
{
    View,
    Output,
    Light,
    Material,
    Draw,
    Pass,
    ResourceDeclaration,
    Fallback,
    Dump
};

struct FForwardDiagnosticRecord
{
    EForwardDiagnosticSeverity Severity = EForwardDiagnosticSeverity::Info;
    EForwardDiagnosticCategory Category = EForwardDiagnosticCategory::Dump;
    EForwardResult Result = EForwardResult::Success;
    Stoner::Core::FString StableCode;
    Stoner::Core::FString SubjectName;
    Stoner::Core::FString Message;
};

class FForwardDiagnosticLog
{
public:
    void Add(EForwardDiagnosticSeverity Severity,
        EForwardDiagnosticCategory Category,
        EForwardResult Result,
        Stoner::Core::FString StableCode,
        Stoner::Core::FString SubjectName,
        Stoner::Core::FString Message);
    void Merge(const FForwardDiagnosticLog& Other);
    void SortStable();
    void Clear();

    [[nodiscard]] bool HasErrors() const noexcept;
    [[nodiscard]] bool IsEmpty() const noexcept;
    [[nodiscard]] int CountByCode(const Stoner::Core::FString& StableCode) const noexcept;
    [[nodiscard]] const Stoner::Core::TArray<FForwardDiagnosticRecord>& GetRecords() const noexcept;
    [[nodiscard]] Stoner::Core::TArray<FForwardDiagnosticRecord>& GetMutableRecords() noexcept;
    [[nodiscard]] Stoner::Core::FString Format() const;

private:
    Stoner::Core::TArray<FForwardDiagnosticRecord> Records;
};

[[nodiscard]] const char* ToString(EForwardResult Result) noexcept;
[[nodiscard]] const char* ToString(EForwardValidationState State) noexcept;
[[nodiscard]] const char* ToString(EForwardDiagnosticSeverity Severity) noexcept;
[[nodiscard]] const char* ToString(EForwardDiagnosticCategory Category) noexcept;
struct FForwardFramePlan;

[[nodiscard]] Stoner::Core::FString BuildForwardFrameDebugDump(const FForwardFramePlan& Plan);

} // namespace Stoner::Renderer
