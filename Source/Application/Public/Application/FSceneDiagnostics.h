#pragma once

#include "Application/ESceneComponentType.h"
#include "Application/ESceneResult.h"
#include "Core/CoreMinimal.h"

namespace Stoner::Application
{

enum class ESceneDiagnosticSeverity
{
    Info,
    Warning,
    Error
};

enum class ESceneDiagnosticCategory
{
    Entity,
    Component,
    Hierarchy,
    RenderCollection,
    Validation,
    Dump
};

struct FSceneDiagnosticRecord
{
    ESceneDiagnosticSeverity Severity = ESceneDiagnosticSeverity::Info;
    ESceneDiagnosticCategory Category = ESceneDiagnosticCategory::Dump;
    ESceneResult Result = ESceneResult::Success;
    Stoner::Core::FString StableCode;
    Stoner::Core::FString SubjectName;
    Stoner::Core::FString Message;
};

class FSceneDiagnosticLog
{
public:
    void Add(ESceneDiagnosticSeverity Severity,
        ESceneDiagnosticCategory Category,
        ESceneResult Result,
        Stoner::Core::FString StableCode,
        Stoner::Core::FString SubjectName,
        Stoner::Core::FString Message);
    void Merge(const FSceneDiagnosticLog& Other);
    void SortStable();
    void Clear();

    [[nodiscard]] bool HasErrors() const noexcept;
    [[nodiscard]] bool IsEmpty() const noexcept;
    [[nodiscard]] int CountByCode(const Stoner::Core::FString& StableCode) const noexcept;
    [[nodiscard]] const Stoner::Core::TArray<FSceneDiagnosticRecord>& GetRecords() const noexcept;
    [[nodiscard]] Stoner::Core::FString Format() const;

private:
    Stoner::Core::TArray<FSceneDiagnosticRecord> Records;
};

[[nodiscard]] const char* ToString(ESceneDiagnosticSeverity Severity) noexcept;
[[nodiscard]] const char* ToString(ESceneDiagnosticCategory Category) noexcept;

} // namespace Stoner::Application
