#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::Application
{

enum class EApplicationResult
{
    Success,
    ValidationFailed,
    RuntimeUnavailable,
    InvalidLifecycle,
    UnsupportedMode,
    InvalidInput
};

enum class EApplicationDiagnosticSeverity
{
    Info,
    Warning,
    Error
};

enum class EApplicationDiagnosticCategory
{
    Window,
    Input,
    Driver,
    Loop,
    Validation,
    RuntimeAvailability,
    Dump
};

struct FApplicationDiagnosticRecord
{
    EApplicationDiagnosticSeverity Severity = EApplicationDiagnosticSeverity::Info;
    EApplicationDiagnosticCategory Category = EApplicationDiagnosticCategory::Dump;
    EApplicationResult Result = EApplicationResult::Success;
    Stoner::Core::FString StableCode;
    Stoner::Core::FString SubjectName;
    Stoner::Core::FString Message;
};

class FApplicationDiagnosticLog
{
public:
    void Add(EApplicationDiagnosticSeverity Severity,
        EApplicationDiagnosticCategory Category,
        EApplicationResult Result,
        Stoner::Core::FString StableCode,
        Stoner::Core::FString SubjectName,
        Stoner::Core::FString Message);
    void Merge(const FApplicationDiagnosticLog& Other);
    void SortStable();
    void Clear();

    [[nodiscard]] bool HasErrors() const noexcept;
    [[nodiscard]] bool IsEmpty() const noexcept;
    [[nodiscard]] int CountByCode(const Stoner::Core::FString& StableCode) const noexcept;
    [[nodiscard]] const Stoner::Core::TArray<FApplicationDiagnosticRecord>& GetRecords() const noexcept;
    [[nodiscard]] Stoner::Core::TArray<FApplicationDiagnosticRecord>& GetMutableRecords() noexcept;
    [[nodiscard]] Stoner::Core::FString Format() const;

private:
    Stoner::Core::TArray<FApplicationDiagnosticRecord> Records;
};

[[nodiscard]] const char* ToString(EApplicationResult Result) noexcept;
[[nodiscard]] const char* ToString(EApplicationDiagnosticSeverity Severity) noexcept;
[[nodiscard]] const char* ToString(EApplicationDiagnosticCategory Category) noexcept;

class FWindow;
class FInputManager;
struct FApplicationLoopState;

[[nodiscard]] Stoner::Core::FString BuildApplicationDebugDump(const FWindow& Window,
    const FInputManager& InputManager,
    const FApplicationLoopState* LoopState = nullptr);

} // namespace Stoner::Application
