#include "Asset/FAssetDiagnostics.h"

#include <algorithm>
#include <cctype>
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
    case EAssetStage::Container: return "container";
    case EAssetStage::Transcode: return "transcode";
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

bool ContainsNativeDetail(std::string_view Text)
{
    if (Text.find("0x") != std::string_view::npos ||
        Text.find("/Users/") != std::string_view::npos ||
        Text.find("/home/") != std::string_view::npos ||
        Text.find("\\Users\\") != std::string_view::npos ||
        Text.find("VK_") != std::string_view::npos ||
        Text.find("HRESULT") != std::string_view::npos)
    {
        return true;
    }
    return Text.size() >= 3 &&
        std::isalpha(static_cast<unsigned char>(Text[0])) &&
        Text[1] == ':' && (Text[2] == '\\' || Text[2] == '/');
}

std::string StableText(const Core::FString& Text)
{
    return ContainsNativeDetail(Text.View())
        ? "[redacted]"
        : Text.ToStdString();
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
        Text += "|subject=" + StableText(Diagnostic.Subject);
    }
    if (!Diagnostic.Participant.IsEmpty())
    {
        Text += "|participant=" + StableText(Diagnostic.Participant);
    }
    if (!Diagnostic.Field.IsEmpty())
    {
        Text += "|field=" + StableText(Diagnostic.Field);
    }
    if (!Diagnostic.Actual.IsEmpty())
    {
        Text += "|actual=" + StableText(Diagnostic.Actual);
    }
    if (!Diagnostic.Limit.IsEmpty())
    {
        Text += "|limit=" + StableText(Diagnostic.Limit);
    }
    if (Diagnostic.Level.has_value())
    {
        Text += "|level=" + std::to_string(*Diagnostic.Level);
    }
    if (!Diagnostic.Reason.IsEmpty())
    {
        Text += "|reason=" + StableText(Diagnostic.Reason);
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
                Left.Actual,
                Left.Limit,
                Left.Level,
                Left.Reason) <
                std::tuple(
                    Right.Severity,
                    Right.Stage,
                    Right.Code,
                    Right.Subject,
                    Right.Participant,
                    Right.Field,
                    Right.Actual,
                    Right.Limit,
                    Right.Level,
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
