#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::Renderer
{

enum class EDeferredResult
{
    Success,
    InvalidConfiguration,
    InvalidView,
    InvalidOutput,
    InvalidSurfaceLayout,
    InvalidDraw,
    InvalidMaterial,
    InvalidLight,
    InvalidBinding,
    GraphCompilationFailed,
    RecordFailed,
    SubmitFailed,
    ReadbackFailed,
    ValidationFailed,
    ComparisonInvalid
};

enum class EDeferredPassStage
{
    SurfaceData,
    DirectionalLighting,
    PointLightVolumes,
    SpotLightVolumes,
    Composition,
    ForwardTransparency,
    ValidationReadback
};

enum class EDeferredDiagnosticSeverity
{
    Info,
    Warning,
    Error
};

struct FDeferredDiagnostic
{
    EDeferredDiagnosticSeverity Severity = EDeferredDiagnosticSeverity::Info;
    EDeferredPassStage Stage = EDeferredPassStage::SurfaceData;
    EDeferredResult Result = EDeferredResult::Success;
    Stoner::Core::FString Code;
    Stoner::Core::FString Subject;
    Stoner::Core::FString Reason;
    Stoner::Core::uint32 Sequence = 0;
};

class FDeferredDiagnosticLog
{
public:
    void Add(EDeferredDiagnosticSeverity Severity, EDeferredPassStage Stage, EDeferredResult Result,
        Stoner::Core::FString Code, Stoner::Core::FString Subject, Stoner::Core::FString Reason);
    void Merge(const FDeferredDiagnosticLog& Other);
    void Clear();
    void SortStable();

    [[nodiscard]] bool HasErrors() const noexcept;
    [[nodiscard]] const FDeferredDiagnostic* GetFirstError() const noexcept;
    [[nodiscard]] const Stoner::Core::TArray<FDeferredDiagnostic>& GetRecords() const noexcept;
    [[nodiscard]] Stoner::Core::FString Dump() const;

private:
    Stoner::Core::TArray<FDeferredDiagnostic> Records;
    Stoner::Core::uint32 NextSequence = 1;
};

[[nodiscard]] const char* ToString(EDeferredResult Result) noexcept;
[[nodiscard]] const char* ToString(EDeferredPassStage Stage) noexcept;
[[nodiscard]] const char* ToString(EDeferredDiagnosticSeverity Severity) noexcept;

} // namespace Stoner::Renderer
