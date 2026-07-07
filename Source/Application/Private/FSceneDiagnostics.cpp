#include "Application/FSceneDiagnostics.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace Stoner::Application
{

void FSceneDiagnosticLog::Add(ESceneDiagnosticSeverity Severity,
    ESceneDiagnosticCategory Category,
    ESceneResult Result,
    Stoner::Core::FString StableCode,
    Stoner::Core::FString SubjectName,
    Stoner::Core::FString Message)
{
    Records.push_back({Severity, Category, Result, std::move(StableCode), std::move(SubjectName), std::move(Message)});
}

void FSceneDiagnosticLog::Merge(const FSceneDiagnosticLog& Other)
{
    Records.insert(Records.end(), Other.Records.begin(), Other.Records.end());
    SortStable();
}

void FSceneDiagnosticLog::SortStable()
{
    std::stable_sort(Records.begin(), Records.end(), [](const FSceneDiagnosticRecord& Left, const FSceneDiagnosticRecord& Right) {
        if (Left.StableCode != Right.StableCode)
        {
            return Left.StableCode < Right.StableCode;
        }
        if (Left.SubjectName != Right.SubjectName)
        {
            return Left.SubjectName < Right.SubjectName;
        }
        return Left.Message < Right.Message;
    });
}

void FSceneDiagnosticLog::Clear()
{
    Records.clear();
}

bool FSceneDiagnosticLog::HasErrors() const noexcept
{
    return std::any_of(Records.begin(), Records.end(), [](const FSceneDiagnosticRecord& Record) {
        return Record.Severity == ESceneDiagnosticSeverity::Error;
    });
}

bool FSceneDiagnosticLog::IsEmpty() const noexcept
{
    return Records.empty();
}

int FSceneDiagnosticLog::CountByCode(const Stoner::Core::FString& StableCode) const noexcept
{
    return static_cast<int>(std::count_if(Records.begin(), Records.end(), [&StableCode](const FSceneDiagnosticRecord& Record) {
        return Record.StableCode == StableCode;
    }));
}

const Stoner::Core::TArray<FSceneDiagnosticRecord>& FSceneDiagnosticLog::GetRecords() const noexcept
{
    return Records;
}

Stoner::Core::FString FSceneDiagnosticLog::Format() const
{
    std::ostringstream Stream;
    for (const FSceneDiagnosticRecord& Record : Records)
    {
        Stream << Record.StableCode.CStr() << '[' << ToString(Record.Severity) << '/'
            << ToString(Record.Category) << '/' << ToString(Record.Result) << "] "
            << Record.SubjectName.CStr() << ": " << Record.Message.CStr() << '\n';
    }
    return Stoner::Core::FString(Stream.str());
}

const char* ToString(ESceneDiagnosticSeverity Severity) noexcept
{
    switch (Severity)
    {
    case ESceneDiagnosticSeverity::Info: return "Info";
    case ESceneDiagnosticSeverity::Warning: return "Warning";
    case ESceneDiagnosticSeverity::Error: return "Error";
    }
    return "Unknown";
}

const char* ToString(ESceneDiagnosticCategory Category) noexcept
{
    switch (Category)
    {
    case ESceneDiagnosticCategory::Entity: return "Entity";
    case ESceneDiagnosticCategory::Component: return "Component";
    case ESceneDiagnosticCategory::Hierarchy: return "Hierarchy";
    case ESceneDiagnosticCategory::RenderCollection: return "RenderCollection";
    case ESceneDiagnosticCategory::Validation: return "Validation";
    case ESceneDiagnosticCategory::Dump: return "Dump";
    }
    return "Unknown";
}

} // namespace Stoner::Application
