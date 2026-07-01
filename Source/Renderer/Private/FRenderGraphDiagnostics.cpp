#include "Renderer/FRenderGraphDiagnostics.h"

#include <sstream>

namespace Stoner::Renderer
{

void FRenderGraphDiagnosticLog::Clear()
{
    Records.clear();
}

void FRenderGraphDiagnosticLog::Add(ERenderGraphDiagnosticCategory Category, ERenderGraphResult Result, std::string Message)
{
    Records.push_back({Category, Result, FRenderGraphDiagnostic::InvalidIndex, FRenderGraphDiagnostic::InvalidIndex, std::move(Message)});
}

void FRenderGraphDiagnosticLog::AddForPass(ERenderGraphDiagnosticCategory Category, ERenderGraphResult Result, Stoner::Core::uint32 PassIndex, std::string Message)
{
    Records.push_back({Category, Result, PassIndex, FRenderGraphDiagnostic::InvalidIndex, std::move(Message)});
}

void FRenderGraphDiagnosticLog::AddForResource(ERenderGraphDiagnosticCategory Category, ERenderGraphResult Result, Stoner::Core::uint32 ResourceIndex, std::string Message)
{
    Records.push_back({Category, Result, FRenderGraphDiagnostic::InvalidIndex, ResourceIndex, std::move(Message)});
}

bool FRenderGraphDiagnosticLog::HasErrors() const noexcept
{
    for (const FRenderGraphDiagnostic& Record : Records)
    {
        if (Record.Result != ERenderGraphResult::Success)
        {
            return true;
        }
    }
    return false;
}

const Stoner::Core::TArray<FRenderGraphDiagnostic>& FRenderGraphDiagnosticLog::GetRecords() const noexcept
{
    return Records;
}

std::string FRenderGraphDiagnosticLog::Format() const
{
    std::ostringstream Stream;
    for (const FRenderGraphDiagnostic& Record : Records)
    {
        Stream << ToString(Record.Category) << ':' << ToString(Record.Result);
        if (Record.PassIndex != FRenderGraphDiagnostic::InvalidIndex)
        {
            Stream << ":pass=" << Record.PassIndex;
        }
        if (Record.ResourceIndex != FRenderGraphDiagnostic::InvalidIndex)
        {
            Stream << ":resource=" << Record.ResourceIndex;
        }
        Stream << ':' << Record.Message << '\n';
    }
    return Stream.str();
}

const char* ToString(ERenderGraphResult Result) noexcept
{
    switch (Result)
    {
    case ERenderGraphResult::Success: return "Success";
    case ERenderGraphResult::InvalidState: return "InvalidState";
    case ERenderGraphResult::InvalidHandle: return "InvalidHandle";
    case ERenderGraphResult::ValidationFailed: return "ValidationFailed";
    case ERenderGraphResult::CompileFailed: return "CompileFailed";
    case ERenderGraphResult::ExecutionFailed: return "ExecutionFailed";
    case ERenderGraphResult::ResourceUnavailable: return "ResourceUnavailable";
    }
    return "Unknown";
}

const char* ToString(ERenderGraphState State) noexcept
{
    switch (State)
    {
    case ERenderGraphState::Draft: return "Draft";
    case ERenderGraphState::Compiled: return "Compiled";
    case ERenderGraphState::Failed: return "Failed";
    case ERenderGraphState::Executed: return "Executed";
    case ERenderGraphState::Invalidated: return "Invalidated";
    }
    return "Unknown";
}

const char* ToString(ERenderGraphDiagnosticCategory Category) noexcept
{
    switch (Category)
    {
    case ERenderGraphDiagnosticCategory::Validation: return "Validation";
    case ERenderGraphDiagnosticCategory::Scheduling: return "Scheduling";
    case ERenderGraphDiagnosticCategory::Culling: return "Culling";
    case ERenderGraphDiagnosticCategory::Lifetime: return "Lifetime";
    case ERenderGraphDiagnosticCategory::Aliasing: return "Aliasing";
    case ERenderGraphDiagnosticCategory::Transition: return "Transition";
    case ERenderGraphDiagnosticCategory::Execution: return "Execution";
    case ERenderGraphDiagnosticCategory::Invalidation: return "Invalidation";
    }
    return "Unknown";
}

} // namespace Stoner::Renderer
