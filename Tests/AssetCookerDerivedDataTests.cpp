#include "AssetCookerDerivedDataTests.h"

#include "AssetCookerDerivedDataTestSupport.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace
{
using namespace Stoner;
using namespace Stoner::Tests::AssetCookerDDC;

void Record(FAssetCookerDerivedDataTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

void TestInstallHitAndInterruptedStage(FAssetCookerDerivedDataTestResult& Result)
{
    const auto Root = std::filesystem::temp_directory_path() / "stoner-ddc-hit";
    std::filesystem::remove_all(Root);
    const auto Content = Root / "Content";
    SeedPng(Content);
    const auto RequestValue = Request(Root, Content);
    const FRun First = Run(RequestValue);
    std::filesystem::create_directories(Root / "DDC/Staging/interrupted");
    Write(Root / "DDC/Staging/interrupted/Entry.json", {'b', 'a', 'd'});
    const FRun Second = Run(RequestValue);
    Record(Result,
        First.Result.Succeeded() && First.Report.Counts.Cooked == 2 &&
            First.Report.Counts.CacheMisses == 2 &&
            Second.Result.Succeeded() && Second.Report.Counts.Reused == 2 &&
            Second.Report.Counts.Cooked == 0 &&
            Second.Report.Counts.ReuseEligible == 2 &&
            EqualArtifacts(First.Result.Artifacts, Second.Result.Artifacts) &&
            First.Result.CanonicalManifest == Second.Result.CanonicalManifest,
        "immutable entries install, validate, hit, and ignore staging remnants");
    std::filesystem::remove_all(Root);
}

void TestStrictAndOrdinaryCorruption(FAssetCookerDerivedDataTestResult& Result)
{
    const auto Root = std::filesystem::temp_directory_path() / "stoner-ddc-corrupt";
    std::filesystem::remove_all(Root);
    const auto Content = Root / "Content";
    SeedPng(Content);
    auto RequestValue = Request(Root, Content);
    const FRun Baseline = Run(RequestValue);
    const auto Entry = FirstEntry(Root / "DDC");
    auto Payload = Read(Entry / "Payload.sgasset");
    if (!Payload.empty()) Payload.pop_back();
    Write(Entry / "Payload.sgasset", Payload);
    const auto CorruptBytes = Read(Entry / "Payload.sgasset");
    RequestValue.CachePolicy = AssetCooker::EAssetCookCachePolicy::StrictValidate;
    const FRun Strict = Run(RequestValue);
    const bool StrictPreserved = !Strict.Result.Succeeded() &&
        Strict.Result.Category == AssetCooker::EAssetCookResultCategory::CacheFailure &&
        Read(Entry / "Payload.sgasset") == CorruptBytes &&
        !std::filesystem::exists(Root / "DDC/Quarantine" / Entry.filename());
    RequestValue.CachePolicy = AssetCooker::EAssetCookCachePolicy::Incremental;
    const FRun Rebuilt = Run(RequestValue);
    const bool Quarantined = std::filesystem::exists(
        Root / "DDC/Quarantine" / Entry.filename());
    Record(Result, Baseline.Result.Succeeded() && StrictPreserved,
        "strict cache validation fails without mutation or rebuild");
    Record(Result,
        Rebuilt.Result.Succeeded() && Quarantined &&
            Rebuilt.Report.Counts.Invalidated == 1 &&
            Rebuilt.Report.Counts.Quarantined == 1 &&
            Rebuilt.Report.Counts.Rebuilt == 1 &&
            Rebuilt.Report.Counts.Reused == 1 &&
            EqualArtifacts(Baseline.Result.Artifacts, Rebuilt.Result.Artifacts),
        "ordinary corruption is quarantined and converges by rebuilding only the bad entry");
    std::filesystem::remove_all(Root);
}

bool ReplaceFirst(
    Core::TArray<Core::uint8>& BytesValue,
    std::string_view Needle,
    std::string_view Replacement)
{
    std::string Text(BytesValue.begin(), BytesValue.end());
    const auto Position = Text.find(Needle);
    if (Position == std::string::npos) return false;
    Text.replace(Position, Needle.size(), Replacement);
    BytesValue.assign(Text.begin(), Text.end());
    return true;
}

bool MutateCacheCase(
    int Case,
    const std::filesystem::path& Entry,
    const std::filesystem::path& Ddc)
{
    const auto MetadataPath = Entry / "Entry.json";
    const auto PayloadPath = Entry / "Payload.sgasset";
    auto Metadata = Read(MetadataPath);
    auto Payload = Read(PayloadPath);
    switch (Case)
    {
    case 1: return std::filesystem::remove(MetadataPath);
    case 2: if (Metadata.empty()) return false; Metadata.resize(Metadata.size() / 2); Write(MetadataPath, Metadata); return true;
    case 3: Metadata.insert(Metadata.begin(), Core::uint8{' '}); Write(MetadataPath, Metadata); return true;
    case 4: if (!ReplaceFirst(Metadata, "stoner.asset-derived-entry", "stoner.asset-derived-broken")) return false; Write(MetadataPath, Metadata); return true;
    case 5: if (!ReplaceFirst(Metadata, "\"schemaVersion\":1", "\"schemaVersion\":2")) return false; Write(MetadataPath, Metadata); return true;
    case 6:
    {
        const std::string Key = Entry.filename().string();
        std::string Replacement = Key;
        Replacement[0] = Replacement[0] == '0' ? '1' : '0';
        if (!ReplaceFirst(Metadata, Key, Replacement)) return false;
        Write(MetadataPath, Metadata); return true;
    }
    case 7:
    {
        const std::string Marker = "\"sourceVersion\":\"";
        std::string Text(Metadata.begin(), Metadata.end());
        const auto Position = Text.find(Marker);
        if (Position == std::string::npos) return false;
        const auto Digit = Position + Marker.size();
        Text[Digit] = Text[Digit] == '0' ? '1' : '0';
        Metadata.assign(Text.begin(), Text.end()); Write(MetadataPath, Metadata); return true;
    }
    case 8: return std::filesystem::remove(PayloadPath);
    case 9: if (Payload.empty()) return false; Payload.pop_back(); Write(PayloadPath, Payload); return true;
    case 10: if (Payload.empty()) return false; Payload.back() ^= 0x01U; Write(PayloadPath, Payload); return true;
    case 11:
    {
        const std::string Needle = "\"payloadBytes\":" + std::to_string(Payload.size());
        if (!ReplaceFirst(Metadata, Needle, "\"payloadBytes\":1")) return false;
        Write(MetadataPath, Metadata); return true;
    }
    case 12:
    {
        const std::string Marker = "\"envelopeDigest\":\"";
        std::string Text(Metadata.begin(), Metadata.end());
        const auto Position = Text.find(Marker);
        if (Position == std::string::npos) return false;
        const auto Digit = Position + Marker.size();
        Text[Digit] = Text[Digit] == '0' ? '1' : '0';
        Metadata.assign(Text.begin(), Text.end()); Write(MetadataPath, Metadata); return true;
    }
    case 13: if (!ReplaceFirst(Metadata, "stoner.image", "stoner.broken")) return false; Write(MetadataPath, Metadata); return true;
    case 14:
    {
        const std::string Marker = "\"relevantProfileDigest\":\"";
        std::string Text(Metadata.begin(), Metadata.end());
        const auto Position = Text.find(Marker);
        if (Position == std::string::npos) return false;
        const auto Digit = Position + Marker.size();
        Text[Digit] = Text[Digit] == '0' ? '1' : '0';
        Metadata.assign(Text.begin(), Text.end()); Write(MetadataPath, Metadata); return true;
    }
    case 15: Write(Ddc / "Staging/interrupted/Entry.json", {'b', 'a', 'd'}); return true;
    default: return false;
    }
}

void TestCorruptionCorpus(FAssetCookerDerivedDataTestResult& Result)
{
    bool Passed = true;
    int Detected = 0;
    for (int Case = 1; Case <= 15; ++Case)
    {
        const auto Root = std::filesystem::temp_directory_path() /
            ("stoner-ddc-case-" + std::to_string(Case));
        std::filesystem::remove_all(Root);
        const auto Content = Root / "Content";
        SeedPng(Content);
        auto RequestValue = Request(Root, Content);
        const FRun Baseline = Run(RequestValue);
        const auto Entry = FirstEntry(Root / "DDC");
        Passed = Passed && Baseline.Result.Succeeded() && !Entry.empty() &&
            MutateCacheCase(Case, Entry, Root / "DDC");
        RequestValue.CachePolicy = AssetCooker::EAssetCookCachePolicy::StrictValidate;
        const FRun Strict = Run(RequestValue);
        RequestValue.CachePolicy = AssetCooker::EAssetCookCachePolicy::Incremental;
        const FRun Ordinary = Run(RequestValue);
        if (Case == 15)
            Passed = Passed && Strict.Result.Succeeded() && Ordinary.Result.Succeeded() &&
                Strict.Report.Counts.Reused == 2 && Ordinary.Report.Counts.Reused == 2;
        else
        {
            const bool CasePassed = !Strict.Result.Succeeded() &&
                Strict.Result.Category == AssetCooker::EAssetCookResultCategory::CacheFailure &&
                Ordinary.Result.Succeeded() && Ordinary.Report.Counts.Invalidated == 1 &&
                Ordinary.Report.Counts.Rebuilt == 1;
            Passed = Passed && CasePassed;
            if (CasePassed) ++Detected;
        }
        std::filesystem::remove_all(Root);
    }
    std::cout << "[EVIDENCE] corrupt-cache-rejected=" << Detected
              << " staging-remnants-ignored=1\n";
    Record(Result, Passed && Detected == 14,
        "fifteen-case cache corruption corpus has deterministic strict and ordinary outcomes");
}

} // namespace

FAssetCookerDerivedDataTestResult RunAssetCookerDerivedDataTests()
{
    FAssetCookerDerivedDataTestResult Result;
    TestInstallHitAndInterruptedStage(Result);
    TestStrictAndOrdinaryCorruption(Result);
    TestCorruptionCorpus(Result);
    return Result;
}
