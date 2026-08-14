#include "AssetCookerReportTests.h"

#include "FAssetCookReportCodec.h"

#include <iostream>
#include <span>
#include <string>

namespace
{

using namespace Stoner;
using namespace Stoner::AssetCooker;
using namespace Stoner::AssetCooker::Private;

void Record(FAssetCookerReportTestResult& Result, bool Passed, const char* Label)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Label << '\n';
}

Asset::FAssetDigest Digest(const char* Text)
{
    const std::string Value(Text);
    return Asset::FAssetDigest::FromBytes(std::span<const Core::uint8>(
        reinterpret_cast<const Core::uint8*>(Value.data()), Value.size()));
}

FAssetCookReportDocument Document()
{
    FAssetCookReportDocument Value;
    Value.Command = EAssetCookReportCommand::Cook;
    Value.Result = EAssetCookResultCategory::Success;
    Value.StableReason = Core::FString("asset-cooker.cook.success");
    Value.bHasPipeline = true;
    Value.Pipeline.EffectiveProfileDigest = Digest("profile");
    Value.Pipeline.SnapshotDigest = Digest("snapshot");
    Value.Pipeline.Counts.ReachableAssets = 1;
    Value.Pipeline.Counts.ReuseEligible = 1;
    Value.Pipeline.Counts.Cooked = 1;
    Value.Pipeline.Counts.CacheMisses = 1;
    FAssetCookAssetReport Entry;
    Entry.Decision = EAssetCookDecision::Cooked;
    Entry.Action = EAssetCookAction::Cook;
    Entry.bReuseEligible = true;
    Entry.StableReason = Core::FString("ddc.entry.miss");
    (void)Asset::FAssetId::Create(Core::FString("Image"),
        Core::FString("Representative.png"), std::nullopt, Entry.AssetId);
    (void)Asset::FAssetDerivedKey::ParseLowerHex(Core::FString(
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
        Entry.DerivedKey);
    Entry.EffectiveSettingsDigest = Digest("settings");
    Entry.RelevantProfileDigest = Digest("relevant-profile");
    Entry.TargetDecision = Core::FString("texture:rgba8-srgb");
    Entry.SourceEvidence.emplace_back("file:Representative.png@source-digest");
    Entry.DependencyEvidence.emplace_back("build:Image:Representative.png@version");
    Value.Pipeline.Assets.push_back(std::move(Entry));
    Value.Pipeline.Telemetry.HostLabel = Core::FString("host-a");
    Value.Pipeline.Telemetry.WorkerCount = 4;
    return Value;
}

} // namespace

FAssetCookerReportTestResult RunAssetCookerReportTests()
{
    FAssetCookerReportTestResult Result;
    const EAssetCookAction Actions[]{EAssetCookAction::Hit,
        EAssetCookAction::Miss, EAssetCookAction::Invalidate,
        EAssetCookAction::Quarantine, EAssetCookAction::Cook,
        EAssetCookAction::Rebuild, EAssetCookAction::Fallback,
        EAssetCookAction::Ineligible, EAssetCookAction::Reuse,
        EAssetCookAction::Stage, EAssetCookAction::Validate,
        EAssetCookAction::Publish, EAssetCookAction::Fail};
    const char* Tokens[]{"hit", "miss", "invalidate", "quarantine", "cook",
        "rebuild", "fallback", "ineligible", "reuse", "stage", "validate",
        "publish", "fail"};
    bool Vocabulary = true;
    for (Core::usize Index = 0; Index < std::size(Actions); ++Index)
        Vocabulary = Vocabulary &&
            std::string(FAssetCookReportCodec::ActionToken(Actions[Index])) ==
                Tokens[Index];
    Record(Result, Vocabulary,
        "canonical report exposes the complete stable action vocabulary");

    auto Value = Document();
    Core::FString First;
    Core::FString Second;
    Asset::FAssetDigest FirstDigest;
    Asset::FAssetDigest SecondDigest;
    const bool FirstWritten = FAssetCookReportCodec::Write(
        Value, true, First, &FirstDigest) == Asset::EAssetResult::Success;
    const bool SecondWritten = FAssetCookReportCodec::Write(
        Value, true, Second, &SecondDigest) == Asset::EAssetResult::Success;
    Record(Result, FirstWritten && SecondWritten && First == Second &&
        FirstDigest == SecondDigest,
        "equivalent normalized reports are byte-identical with stable digest");
    Record(Result,
        First.View().find("\"effectiveSettingsDigest\"") != std::string_view::npos &&
        First.View().find("\"relevantProfileDigest\"") != std::string_view::npos &&
        First.View().find("\"targetDecision\"") != std::string_view::npos &&
        First.View().find("\"sourceEvidence\"") != std::string_view::npos &&
        First.View().find("\"dependencyEvidence\"") != std::string_view::npos &&
        First.View().find("\"reuseEligible\": true") != std::string_view::npos,
        "decision report carries source dependency profile target and reuse evidence");

    Value.Pipeline.Telemetry.HostLabel = Core::FString("different-host");
    Value.Pipeline.Telemetry.WallClockMilliseconds = 9999;
    Value.Pipeline.Telemetry.PeakResidentBytes = 123456;
    Value.Pipeline.Telemetry.WorkerCount = 8;
    Core::FString WithTelemetry;
    Asset::FAssetDigest TelemetryDigest;
    const bool TelemetryWritten = FAssetCookReportCodec::Write(
        Value, false, WithTelemetry, &TelemetryDigest) ==
        Asset::EAssetResult::Success;
    Record(Result, TelemetryWritten && TelemetryDigest == FirstDigest &&
        WithTelemetry != First &&
        WithTelemetry.View().find("\"telemetry\"") != std::string_view::npos,
        "host timing RSS and worker telemetry are excluded from deterministic digest");

    auto Partial = FAssetCookReportDocument{};
    Partial.Command = EAssetCookReportCommand::ValidateCache;
    Partial.Result = EAssetCookResultCategory::Success;
    Partial.StableReason = Core::FString("asset-cooker.cache.validate.success");
    Partial.bHasPipeline = true;
    Core::FString PartialText;
    Record(Result, FAssetCookReportCodec::Write(
        Partial, true, PartialText) == Asset::EAssetResult::Success &&
        PartialText.View().find("effectiveProfileDigest") == std::string_view::npos,
        "cache-only reports remain valid without inventing target profile evidence");

    Value.Pipeline.Assets.front().StableReason.Clear();
    Core::FString Invalid;
    Record(Result, FAssetCookReportCodec::Write(Value, true, Invalid) ==
        Asset::EAssetResult::InvalidInput,
        "invalid decision evidence fails closed before report publication");
    return Result;
}
