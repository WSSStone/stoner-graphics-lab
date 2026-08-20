#include "FAssetCookReportCodec.h"

#include <span>
#include <string>
#include <string_view>

namespace Stoner::AssetCooker::Private
{
namespace
{

void Escape(std::string_view Text, std::string& Out)
{
    Out.push_back('"');
    constexpr char Hex[] = "0123456789abcdef";
    for (const unsigned char Character : Text)
    {
        switch (Character)
        {
        case '"': Out += "\\\""; break;
        case '\\': Out += "\\\\"; break;
        case '\b': Out += "\\b"; break;
        case '\f': Out += "\\f"; break;
        case '\n': Out += "\\n"; break;
        case '\r': Out += "\\r"; break;
        case '\t': Out += "\\t"; break;
        default:
            if (Character < 0x20)
            {
                Out += "\\u00";
                Out.push_back(Hex[Character >> 4U]);
                Out.push_back(Hex[Character & 0x0fU]);
            }
            else Out.push_back(static_cast<char>(Character));
        }
    }
    Out.push_back('"');
}

void StringField(
    std::string& Out,
    int Depth,
    const char* Name,
    std::string_view Value,
    bool Comma = true)
{
    Out.append(static_cast<std::size_t>(Depth) * 2U, ' ');
    Escape(Name, Out);
    Out += ": ";
    Escape(Value, Out);
    Out += Comma ? ",\n" : "\n";
}

void NumberField(
    std::string& Out,
    int Depth,
    const char* Name,
    Core::uint64 Value,
    bool Comma = true)
{
    Out.append(static_cast<std::size_t>(Depth) * 2U, ' ');
    Escape(Name, Out);
    Out += ": " + std::to_string(Value) + (Comma ? ",\n" : "\n");
}

void BoolField(
    std::string& Out,
    int Depth,
    const char* Name,
    bool Value,
    bool Comma = true)
{
    Out.append(static_cast<std::size_t>(Depth) * 2U, ' ');
    Escape(Name, Out);
    Out += Value ? ": true" : ": false";
    Out += Comma ? ",\n" : "\n";
}

void StringArrayField(
    std::string& Out,
    int Depth,
    const char* Name,
    const Core::TArray<Core::FString>& Values,
    bool Comma = true)
{
    Out.append(static_cast<std::size_t>(Depth) * 2U, ' ');
    Escape(Name, Out);
    Out += ": [";
    for (Core::usize Index = 0; Index < Values.size(); ++Index)
    {
        if (Index != 0) Out += ", ";
        Escape(Values[Index].View(), Out);
    }
    Out += Comma ? "],\n" : "]\n";
}

std::string DeterministicBody(const FAssetCookReportDocument& Document)
{
    std::string Out;
    Out += FAssetCookReportCodec::CommandToken(Document.Command);
    Out.push_back('\n');
    Out += FAssetCookReportCodec::ResultToken(Document.Result);
    Out.push_back('\n');
    Out += Document.StableReason.ToStdString();
    Out.push_back('\n');
    if (Document.bHasPipeline)
    {
        Out += Document.Pipeline.EffectiveProfileDigest.ToLowerHex().ToStdString();
        Out.push_back('\n');
        Out += Document.Pipeline.SnapshotDigest.ToLowerHex().ToStdString();
        Out.push_back('\n');
        for (const auto& Entry : Document.Pipeline.Assets)
        {
            Out += std::to_string(Entry.PlanIndex);
            Out.push_back('|');
            Out += Entry.AssetId.ToString().ToStdString();
            Out.push_back('|');
            Out += FAssetCookReportCodec::ActionToken(Entry.Action);
            Out.push_back('|');
            Out += Entry.StableReason.ToStdString();
            Out.push_back('|');
            Out += Entry.DerivedKey.ToString().ToStdString();
            Out.push_back('|');
            Out += Entry.TargetDecision.ToStdString();
            Out.push_back('|');
            Out += Entry.bUsedFallback ? "1\n" : "0\n";
            for (const auto& Source : Entry.SourceEvidence)
            {
                Out += "source|";
                Out += Source.ToStdString();
                Out.push_back('\n');
            }
            for (const auto& Dependency : Entry.DependencyEvidence)
            {
                Out += "dependency|";
                Out += Dependency.ToStdString();
                Out.push_back('\n');
            }
        }
    }
    if (Document.GenerationId.IsAvailable())
        Out += Document.GenerationId.ToLowerHex().ToStdString();
    Out.push_back('\n');
    for (const auto& Diagnostic : Document.Diagnostics)
    {
        Out += Diagnostic.Category.ToStdString();
        Out.push_back('|');
        Out += Diagnostic.Stage.ToStdString();
        Out.push_back('|');
        Out += Diagnostic.AssetId.ToStdString();
        Out.push_back('|');
        Out += Diagnostic.Field.ToStdString();
        Out.push_back('|');
        Out += Diagnostic.Reason.ToStdString();
        Out.push_back('\n');
    }
    return Out;
}

bool ValidatePartialPipeline(const FAssetCookReport& Pipeline)
{
    if (Pipeline.Counts.ReachableAssets != Pipeline.Assets.size() ||
        Pipeline.Counts.ReuseEligible + Pipeline.Counts.ReuseIneligible !=
            Pipeline.Counts.ReachableAssets)
        return false;
    for (Core::usize Index = 0; Index < Pipeline.Assets.size(); ++Index)
        if (Pipeline.Assets[Index].PlanIndex != Index ||
            !Pipeline.Assets[Index].AssetId.IsValid() ||
            Pipeline.Assets[Index].StableReason.IsEmpty())
            return false;
    return true;
}

void WriteSummary(std::string& Out, const FAssetCookReportDocument& Document)
{
    const auto& Counts = Document.Pipeline.Counts;
    Out += "  \"summary\": {\n";
    NumberField(Out, 2, "discovered", Counts.DiscoveredSources);
    NumberField(Out, 2, "selectedRoots", Counts.SelectedRoots);
    NumberField(Out, 2, "reachable", Counts.ReachableAssets);
    NumberField(Out, 2, "reuseEligible", Counts.ReuseEligible);
    NumberField(Out, 2, "reuseIneligible", Counts.ReuseIneligible);
    NumberField(Out, 2, "cacheHits", Counts.Reused);
    NumberField(Out, 2, "cacheMisses", Counts.CacheMisses);
    NumberField(Out, 2, "cacheCorrupt", Counts.Invalidated);
    NumberField(Out, 2, "quarantined", Counts.Quarantined);
    NumberField(Out, 2, "cooked", Counts.Cooked);
    NumberField(Out, 2, "regenerated", Counts.Rebuilt);
    NumberField(Out, 2, "reused", Counts.Reused);
    NumberField(Out, 2, "invalidated", Counts.Invalidated);
    NumberField(Out, 2, "failed", Counts.Failed);
    NumberField(Out, 2, "staged", Document.Staged);
    NumberField(Out, 2, "published", Document.Published);
    NumberField(Out, 2, "sourceBytes", Document.SourceBytes);
    NumberField(Out, 2, "payloadBytes", Document.PayloadBytes);
    NumberField(Out, 2, "generationBytes", Document.GenerationBytes, false);
    Out += "  },\n";
}

void WriteDecisions(std::string& Out, const FAssetCookReportDocument& Document)
{
    Out += "  \"decisions\": [";
    if (!Document.Pipeline.Assets.empty()) Out += "\n";
    for (Core::usize Index = 0; Index < Document.Pipeline.Assets.size(); ++Index)
    {
        const auto& Entry = Document.Pipeline.Assets[Index];
        Out += "    {\n";
        NumberField(Out, 3, "planIndex", Entry.PlanIndex);
        StringField(Out, 3, "assetId", Entry.AssetId.ToString().View());
        StringField(Out, 3, "action", FAssetCookReportCodec::ActionToken(Entry.Action));
        StringField(Out, 3, "reason", Entry.StableReason.View());
        if (Entry.DerivedKey.IsValid())
            StringField(Out, 3, "derivedKey", Entry.DerivedKey.ToString().View());
        if (Entry.EffectiveSettingsDigest.IsAvailable())
            StringField(Out, 3, "effectiveSettingsDigest",
                Entry.EffectiveSettingsDigest.ToLowerHex().View());
        if (Entry.RelevantProfileDigest.IsAvailable())
            StringField(Out, 3, "relevantProfileDigest",
                Entry.RelevantProfileDigest.ToLowerHex().View());
        if (!Entry.TargetDecision.IsEmpty())
            StringField(Out, 3, "targetDecision", Entry.TargetDecision.View());
        if (!Entry.SourceEvidence.empty())
            StringArrayField(Out, 3, "sourceEvidence", Entry.SourceEvidence);
        if (!Entry.DependencyEvidence.empty())
            StringArrayField(
                Out, 3, "dependencyEvidence", Entry.DependencyEvidence);
        BoolField(Out, 3, "usedFallback", Entry.bUsedFallback);
        BoolField(Out, 3, "reuseEligible", Entry.bReuseEligible, false);
        Out += Index + 1 == Document.Pipeline.Assets.size()
            ? "    }\n" : "    },\n";
    }
    Out += Document.Pipeline.Assets.empty() ? "],\n" : "  ],\n";
}

void WriteDiagnostics(std::string& Out, const FAssetCookReportDocument& Document)
{
    Out += "  \"diagnostics\": [";
    if (!Document.Diagnostics.empty()) Out += "\n";
    for (Core::usize Index = 0; Index < Document.Diagnostics.size(); ++Index)
    {
        const auto& Diagnostic = Document.Diagnostics[Index];
        Out += "    {\n";
        StringField(Out, 3, "category", Diagnostic.Category.View());
        StringField(Out, 3, "stage", Diagnostic.Stage.View());
        if (!Diagnostic.AssetId.IsEmpty())
            StringField(Out, 3, "assetId", Diagnostic.AssetId.View());
        if (!Diagnostic.DependencyChain.empty())
        {
            Out += "      \"dependencyChain\": [";
            for (Core::usize Dependency = 0;
                 Dependency < Diagnostic.DependencyChain.size(); ++Dependency)
            {
                if (Dependency != 0) Out += ", ";
                Escape(Diagnostic.DependencyChain[Dependency].View(), Out);
            }
            Out += "],\n";
        }
        if (!Diagnostic.SourceLocator.IsEmpty())
            StringField(Out, 3, "sourceLocator", Diagnostic.SourceLocator.View());
        if (Diagnostic.TargetProfileDigest.IsAvailable())
            StringField(Out, 3, "targetProfileDigest",
                Diagnostic.TargetProfileDigest.ToLowerHex().View());
        if (Diagnostic.DerivedKey.IsValid())
            StringField(Out, 3, "derivedKey", Diagnostic.DerivedKey.ToString().View());
        if (Diagnostic.GenerationId.IsAvailable())
            StringField(Out, 3, "generationId",
                Diagnostic.GenerationId.ToLowerHex().View());
        if (!Diagnostic.Field.IsEmpty())
            StringField(Out, 3, "field", Diagnostic.Field.View());
        StringField(Out, 3, "reason", Diagnostic.Reason.View(), false);
        Out += Index + 1 == Document.Diagnostics.size()
            ? "    }\n" : "    },\n";
    }
    Out += Document.Diagnostics.empty() ? "]" : "  ]";
}

} // namespace

const char* FAssetCookReportCodec::CommandToken(
    EAssetCookReportCommand Command) noexcept
{
    switch (Command)
    {
    case EAssetCookReportCommand::Cook: return "cook";
    case EAssetCookReportCommand::Plan: return "plan";
    case EAssetCookReportCommand::Validate: return "validate";
    case EAssetCookReportCommand::ValidateCache: return "validate-cache";
    case EAssetCookReportCommand::Inspect: return "inspect";
    case EAssetCookReportCommand::Doctor: return "doctor";
    }
    return "cook";
}

const char* FAssetCookReportCodec::ResultToken(
    EAssetCookResultCategory Result) noexcept
{
    switch (Result)
    {
    case EAssetCookResultCategory::Success: return "success";
    case EAssetCookResultCategory::InvalidArguments: return "invalid-arguments";
    case EAssetCookResultCategory::InvalidProfile: return "invalid-profile";
    case EAssetCookResultCategory::DiscoveryFailure: return "discovery-failure";
    case EAssetCookResultCategory::GraphFailure: return "graph-failure";
    case EAssetCookResultCategory::CookFailure: return "cook-failure";
    case EAssetCookResultCategory::CacheFailure: return "cache-failure";
    case EAssetCookResultCategory::SourceChanged: return "source-changed";
    case EAssetCookResultCategory::LeaseTimeout: return "lease-timeout";
    case EAssetCookResultCategory::PublishedValidationFailure:
        return "published-validation-failure";
    case EAssetCookResultCategory::PublicationFailure: return "publication-failure";
    case EAssetCookResultCategory::IoFailure: return "io-failure";
    case EAssetCookResultCategory::InternalFailure: return "internal-failure";
    }
    return "internal-failure";
}

const char* FAssetCookReportCodec::ActionToken(EAssetCookAction Action) noexcept
{
    switch (Action)
    {
    case EAssetCookAction::Hit: return "hit";
    case EAssetCookAction::Miss: return "miss";
    case EAssetCookAction::Invalidate: return "invalidate";
    case EAssetCookAction::Quarantine: return "quarantine";
    case EAssetCookAction::Cook: return "cook";
    case EAssetCookAction::Rebuild: return "rebuild";
    case EAssetCookAction::Fallback: return "fallback";
    case EAssetCookAction::Ineligible: return "ineligible";
    case EAssetCookAction::Reuse: return "reuse";
    case EAssetCookAction::Stage: return "stage";
    case EAssetCookAction::Validate: return "validate";
    case EAssetCookAction::Publish: return "publish";
    case EAssetCookAction::Fail: return "fail";
    }
    return "fail";
}

int FAssetCookReportCodec::ExitCode(EAssetCookResultCategory Result) noexcept
{
    switch (Result)
    {
    case EAssetCookResultCategory::Success: return 0;
    case EAssetCookResultCategory::InvalidArguments: return 2;
    case EAssetCookResultCategory::InvalidProfile: return 3;
    case EAssetCookResultCategory::DiscoveryFailure: return 4;
    case EAssetCookResultCategory::GraphFailure: return 5;
    case EAssetCookResultCategory::CookFailure: return 6;
    case EAssetCookResultCategory::CacheFailure: return 7;
    case EAssetCookResultCategory::SourceChanged: return 8;
    case EAssetCookResultCategory::LeaseTimeout: return 9;
    case EAssetCookResultCategory::PublishedValidationFailure: return 10;
    case EAssetCookResultCategory::PublicationFailure: return 11;
    case EAssetCookResultCategory::IoFailure: return 12;
    case EAssetCookResultCategory::InternalFailure: return 13;
    }
    return 13;
}

Asset::EAssetResult FAssetCookReportCodec::Write(
    const FAssetCookReportDocument& Document,
    bool bNormalized,
    Core::FString& OutCanonical,
    Asset::FAssetDigest* OutDeterministicDigest)
{
    OutCanonical.Clear();
    if (OutDeterministicDigest) *OutDeterministicDigest = {};
    if (Document.StableReason.IsEmpty() ||
        (Document.bHasPipeline &&
         (Document.Pipeline.EffectiveProfileDigest.IsAvailable()
              ? Document.Pipeline.Validate() != Asset::EAssetResult::Success
              : !ValidatePartialPipeline(Document.Pipeline))))
        return Asset::EAssetResult::InvalidInput;
    for (const auto& Entry : Document.Pipeline.Assets)
        if (Entry.StableReason.IsEmpty())
            return Asset::EAssetResult::InvalidInput;
    for (const auto& Diagnostic : Document.Diagnostics)
        if (Diagnostic.Category.IsEmpty() || Diagnostic.Stage.IsEmpty() ||
            Diagnostic.Reason.IsEmpty())
            return Asset::EAssetResult::InvalidInput;

    const std::string Body = DeterministicBody(Document);
    const auto Digest = Asset::FAssetDigest::FromBytes(
        std::span<const Core::uint8>(
            reinterpret_cast<const Core::uint8*>(Body.data()), Body.size()));
    std::string Out = "{\n";
    StringField(Out, 1, "schema", "stoner.asset-cook-report");
    NumberField(Out, 1, "schemaVersion", 1);
    StringField(Out, 1, "command", CommandToken(Document.Command));
    StringField(Out, 1, "result", ResultToken(Document.Result));
    if (Document.Pipeline.EffectiveProfileDigest.IsAvailable())
    {
        StringField(Out, 1, "effectiveProfileDigest",
            Document.Pipeline.EffectiveProfileDigest.ToLowerHex().View());
    }
    if (Document.Pipeline.SnapshotDigest.IsAvailable())
    {
        StringField(Out, 1, "snapshotDigest",
            Document.Pipeline.SnapshotDigest.ToLowerHex().View());
    }
    if (Document.GenerationId.IsAvailable())
        StringField(Out, 1, "generationId",
            Document.GenerationId.ToLowerHex().View());
    StringField(Out, 1, "deterministicDigest", Digest.ToLowerHex().View());
    WriteSummary(Out, Document);
    WriteDecisions(Out, Document);
    WriteDiagnostics(Out, Document);
    if (!bNormalized)
    {
        Out += ",\n  \"telemetry\": {\n";
        StringField(Out, 2, "host", Document.Pipeline.Telemetry.HostLabel.View());
        NumberField(Out, 2, "elapsedMilliseconds",
            Document.Pipeline.Telemetry.WallClockMilliseconds);
        NumberField(Out, 2, "peakRssBytes",
            Document.Pipeline.Telemetry.PeakResidentBytes,
            Document.Pipeline.Telemetry.WorkerCount != 0);
        if (Document.Pipeline.Telemetry.WorkerCount != 0)
            NumberField(Out, 2, "workerCount",
                Document.Pipeline.Telemetry.WorkerCount, false);
        Out += "  }";
    }
    Out += "\n}\n";
    OutCanonical = Core::FString(std::move(Out));
    if (OutDeterministicDigest) *OutDeterministicDigest = Digest;
    return Asset::EAssetResult::Success;
}

} // namespace Stoner::AssetCooker::Private
