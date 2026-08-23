#pragma once

#include "Core/CoreMinimal.h"
#include "FDemoConfiguration.h"

namespace Stoner::Demo
{

enum class EDemoStage
{
    Configuration,
    Window,
    Runtime,
    Shader,
    Upload,
    Pipeline,
    Acquire,
    Record,
    Submit,
    Readback,
    Present,
    Memory,
    Report,
    Shutdown
};

struct FDemoDiagnostic
{
    Stoner::Core::uint64 Sequence = 0;
    EDemoStage Stage = EDemoStage::Configuration;
    EDemoExitCode Result = EDemoExitCode::Success;
    Stoner::Core::FString Subject;
    Stoner::Core::FString Reason;
};

class FDemoDiagnostics
{
public:
    void Add(EDemoStage Stage, EDemoExitCode Result, const char* Subject, const char* Reason);
    [[nodiscard]] bool HasPrimaryFailure() const noexcept { return bHasPrimaryFailure; }
    [[nodiscard]] EDemoExitCode GetPrimaryExitCode() const noexcept { return PrimaryExitCode; }
    [[nodiscard]] const Stoner::Core::TArray<FDemoDiagnostic>& GetRecords() const noexcept { return Records; }
    [[nodiscard]] Stoner::Core::FString BuildStableText() const;

private:
    Stoner::Core::TArray<FDemoDiagnostic> Records;
    Stoner::Core::uint64 NextSequence = 1;
    bool bHasPrimaryFailure = false;
    EDemoExitCode PrimaryExitCode = EDemoExitCode::Success;
};

[[nodiscard]] const char* ToString(EDemoStage Stage) noexcept;

} // namespace Stoner::Demo
