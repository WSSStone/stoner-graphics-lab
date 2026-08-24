#include "FDemoDiagnostics.h"

#include <sstream>

namespace Stoner::Demo
{

void FDemoDiagnostics::Add(EDemoStage Stage, EDemoExitCode Result, const char* Subject, const char* Reason)
{
    const Stoner::Core::uint64 Sequence = NextSequence++;
    if (Result != EDemoExitCode::Success && !bHasPrimaryFailure)
    {
        bHasPrimaryFailure = true;
        PrimaryExitCode = Result;
    }
    if (Result == EDemoExitCode::Success &&
        SuccessfulRecordCount >= MaximumSuccessfulRecords)
    {
        ++DroppedSuccessfulRecordCount;
        return;
    }
    if (Result == EDemoExitCode::Success)
        ++SuccessfulRecordCount;
    Records.push_back({Sequence, Stage, Result, Subject ? Subject : "", Reason ? Reason : ""});
}

Stoner::Core::FString FDemoDiagnostics::BuildStableText() const
{
    std::ostringstream Stream;
    for (const FDemoDiagnostic& Record : Records)
    {
        Stream << "diagnostic sequence=" << Record.Sequence
               << " stage=" << ToString(Record.Stage)
               << " result=" << static_cast<int>(Record.Result)
               << " subject=" << Record.Subject.CStr()
               << " reason=" << Record.Reason.CStr() << '\n';
    }
    if (DroppedSuccessfulRecordCount != 0)
        Stream << "diagnostic dropped-success="
               << DroppedSuccessfulRecordCount << '\n';
    return Stream.str().c_str();
}

const char* ToString(EDemoStage Stage) noexcept
{
    switch (Stage)
    {
    case EDemoStage::Configuration: return "Configuration";
    case EDemoStage::Window: return "Window";
    case EDemoStage::Runtime: return "Runtime";
    case EDemoStage::Shader: return "Shader";
    case EDemoStage::Upload: return "Upload";
    case EDemoStage::Pipeline: return "Pipeline";
    case EDemoStage::Acquire: return "Acquire";
    case EDemoStage::Record: return "Record";
    case EDemoStage::Submit: return "Submit";
    case EDemoStage::Readback: return "Readback";
    case EDemoStage::Present: return "Present";
    case EDemoStage::Memory: return "Memory";
    case EDemoStage::Report: return "Report";
    case EDemoStage::Shutdown: return "Shutdown";
    }
    return "Unknown";
}

} // namespace Stoner::Demo
