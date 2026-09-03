#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::Renderer
{

enum class EOutputTransformResult
{
    Success,
    InvalidHandoff,
    InvalidSettings,
    Unsupported,
    InvalidGraph,
    DuplicateFormalWriter,
    InvalidBinding,
    ExecutionFailed,
    TerminalFailed
};

enum class EOutputTransformDiagnosticSeverity
{
    Info,
    Warning,
    Error
};

struct FOutputTransformDiagnostic
{
    EOutputTransformDiagnosticSeverity Severity =
        EOutputTransformDiagnosticSeverity::Info;
    EOutputTransformResult Result = EOutputTransformResult::Success;
    Stoner::Core::FString Code;
    Stoner::Core::FString Stage;
    Stoner::Core::FString Subject;
    Stoner::Core::FString Message;
};

struct FOutputTransformInsertionDiagnosticRecord
{
    Stoner::Core::FString OperationId;
    Stoner::Core::FString StrategyVersion;
    Stoner::Core::FString InsertionPoint;
    Stoner::Core::int32 OrderKey = 0;
    Stoner::Core::uint32 ResolvedIndex = 0;
    Stoner::Core::FString ColorDomain;
    Stoner::Core::uint32 DeclaredReadCount = 0;
    Stoner::Core::uint32 DeclaredWriteCount = 0;
    bool bOwnsTemporalState = false;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return !OperationId.IsEmpty() && !StrategyVersion.IsEmpty() &&
            !InsertionPoint.IsEmpty() && ResolvedIndex != 0 &&
            !ColorDomain.IsEmpty() && DeclaredReadCount != 0 &&
            DeclaredWriteCount != 0 && !bOwnsTemporalState;
    }
};

struct FOutputTransformDiagnosticBypassRecord
{
    Stoner::Core::FString StageName;
    Stoner::Core::FString SourceDomain;
    Stoner::Core::FString Mode;
    float ReferenceWhiteNits = 0.0f;
    float TargetPeakNits = 0.0f;
    bool bNonAuthoritative = true;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return !StageName.IsEmpty() && !SourceDomain.IsEmpty() &&
            !Mode.IsEmpty() && bNonAuthoritative;
    }
};

class FOutputTransformDiagnosticLog
{
public:
    static constexpr Stoner::Core::uint32 MaximumRecords = 64;

    void Add(EOutputTransformDiagnosticSeverity Severity,
        EOutputTransformResult Result,
        Stoner::Core::FString Code,
        Stoner::Core::FString Stage,
        Stoner::Core::FString Subject,
        Stoner::Core::FString Message);
    void Merge(const FOutputTransformDiagnosticLog& Other);
    [[nodiscard]] bool HasError() const noexcept;
    [[nodiscard]] const FOutputTransformDiagnostic* GetFirstError() const noexcept;
    [[nodiscard]] const Stoner::Core::TArray<FOutputTransformDiagnostic>&
        GetRecords() const noexcept { return Records; }
    [[nodiscard]] Stoner::Core::FString Dump() const;

private:
    Stoner::Core::TArray<FOutputTransformDiagnostic> Records;
    Stoner::Core::uint32 FirstErrorIndex = MaximumRecords;
};

[[nodiscard]] const char* ToString(EOutputTransformResult Result) noexcept;
[[nodiscard]] const char* ToString(
    EOutputTransformDiagnosticSeverity Severity) noexcept;

} // namespace Stoner::Renderer
