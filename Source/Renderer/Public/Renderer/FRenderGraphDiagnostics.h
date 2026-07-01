#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::Renderer
{

enum class ERenderGraphResult
{
    Success,
    InvalidState,
    InvalidHandle,
    ValidationFailed,
    CompileFailed,
    ExecutionFailed,
    ResourceUnavailable
};

enum class ERenderGraphState
{
    Draft,
    Compiled,
    Failed,
    Executed,
    Invalidated
};

enum class ERenderGraphDiagnosticCategory
{
    Validation,
    Scheduling,
    Culling,
    Lifetime,
    Aliasing,
    Transition,
    Execution,
    Invalidation
};

struct FRenderGraphDiagnostic
{
    ERenderGraphDiagnosticCategory Category = ERenderGraphDiagnosticCategory::Validation;
    ERenderGraphResult Result = ERenderGraphResult::Success;
    Stoner::Core::uint32 PassIndex = InvalidIndex;
    Stoner::Core::uint32 ResourceIndex = InvalidIndex;
    Stoner::Core::FString Message;

    static constexpr Stoner::Core::uint32 InvalidIndex = 0xFFFFFFFFu;
};

class FRenderGraphDiagnosticLog
{
public:
    void Clear();
    void Add(ERenderGraphDiagnosticCategory Category, ERenderGraphResult Result, Stoner::Core::FString Message);
    void AddForPass(ERenderGraphDiagnosticCategory Category, ERenderGraphResult Result, Stoner::Core::uint32 PassIndex, Stoner::Core::FString Message);
    void AddForResource(ERenderGraphDiagnosticCategory Category, ERenderGraphResult Result, Stoner::Core::uint32 ResourceIndex, Stoner::Core::FString Message);

    [[nodiscard]] bool HasErrors() const noexcept;
    [[nodiscard]] const Stoner::Core::TArray<FRenderGraphDiagnostic>& GetRecords() const noexcept;
    [[nodiscard]] Stoner::Core::FString Format() const;

private:
    Stoner::Core::TArray<FRenderGraphDiagnostic> Records;
};

[[nodiscard]] const char* ToString(ERenderGraphResult Result) noexcept;
[[nodiscard]] const char* ToString(ERenderGraphState State) noexcept;
[[nodiscard]] const char* ToString(ERenderGraphDiagnosticCategory Category) noexcept;

} // namespace Stoner::Renderer
