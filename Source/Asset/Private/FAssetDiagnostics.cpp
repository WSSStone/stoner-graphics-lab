#include "Asset/FAssetDiagnostics.h"

#include <algorithm>
#include <string>
#include <tuple>

namespace Stoner::Asset
{
namespace
{

const char* ToText(EAssetStage Stage)
{
    switch (Stage)
    {
    case EAssetStage::Identity: return "identity";
    case EAssetStage::Registry: return "registry";
    case EAssetStage::Resolve: return "resolve";
    case EAssetStage::Probe: return "probe";
    case EAssetStage::Import: return "import";
    case EAssetStage::Load: return "load";
    case EAssetStage::Cook: return "cook";
    case EAssetStage::Inspect: return "inspect";
    case EAssetStage::Decode: return "decode";
    case EAssetStage::Validate: return "validate";
    case EAssetStage::Mip: return "mip";
    }
    return "unknown";
}

const char* ToText(EAssetDiagnosticSeverity Severity)
{
    switch (Severity)
    {
    case EAssetDiagnosticSeverity::Info: return "info";
    case EAssetDiagnosticSeverity::Warning: return "warning";
    case EAssetDiagnosticSeverity::Error: return "error";
    }
    return "unknown";
}

} // namespace

Core::FString FAssetDiagnostics::Format(const FAssetDiagnostic& Diagnostic)
{
    std::string Text = std::string(ToText(Diagnostic.Severity)) + "|" +
        ToText(Diagnostic.Stage) + "|result=" +
        std::to_string(static_cast<int>(Diagnostic.Result)) + "|" +
        Diagnostic.Code.ToStdString();
    if (!Diagnostic.Subject.IsEmpty())
    {
        Text += "|subject=" + Diagnostic.Subject.ToStdString();
    }
    if (!Diagnostic.Participant.IsEmpty())
    {
        Text += "|participant=" + Diagnostic.Participant.ToStdString();
    }
    if (!Diagnostic.Field.IsEmpty())
    {
        Text += "|field=" + Diagnostic.Field.ToStdString();
    }
    if (!Diagnostic.Limit.IsEmpty())
    {
        Text += "|limit=" + Diagnostic.Limit.ToStdString();
    }
    if (!Diagnostic.Reason.IsEmpty())
    {
        Text += "|reason=" + Diagnostic.Reason.ToStdString();
    }
    return Core::FString(std::move(Text));
}

Core::FString FAssetDiagnostics::FormatNormalized(FAssetDiagnosticList Diagnostics)
{
    std::sort(
        Diagnostics.begin(),
        Diagnostics.end(),
        [](const FAssetDiagnostic& Left, const FAssetDiagnostic& Right)
        {
            return std::tuple(
                Left.Severity,
                Left.Stage,
                Left.Code,
                Left.Subject,
                Left.Participant,
                Left.Field,
                Left.Limit,
                Left.Reason) <
                std::tuple(
                    Right.Severity,
                    Right.Stage,
                    Right.Code,
                    Right.Subject,
                    Right.Participant,
                    Right.Field,
                    Right.Limit,
                    Right.Reason);
        });
    std::string Text;
    for (const FAssetDiagnostic& Diagnostic : Diagnostics)
    {
        if (!Text.empty())
        {
            Text.push_back('\n');
        }
        Text += Format(Diagnostic).ToStdString();
    }
    return Core::FString(std::move(Text));
}

FAssetDiagnostic FAssetDiagnostics::FirstActionable(FAssetDiagnosticList Diagnostics)
{
    std::sort(
        Diagnostics.begin(),
        Diagnostics.end(),
        [](const FAssetDiagnostic& Left, const FAssetDiagnostic& Right)
        {
            return std::tuple(Left.Severity, Left.Stage, Left.Code, Left.Subject) >
                std::tuple(Right.Severity, Right.Stage, Right.Code, Right.Subject);
        });
    return Diagnostics.empty() ? FAssetDiagnostic{} : Diagnostics.front();
}

} // namespace Stoner::Asset
