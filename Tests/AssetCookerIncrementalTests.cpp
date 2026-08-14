#include "AssetCookerIncrementalTests.h"

#include "AssetCookerDerivedDataTestSupport.h"

#include <filesystem>
#include <iostream>
#include <array>

namespace
{

Stoner::Asset::FAssetParticipantId Participant(const char* Text)
{
    Stoner::Asset::FAssetParticipantId Value;
    (void)Stoner::Asset::FAssetParticipantId::Create(
        Stoner::Core::FString(Text), Value);
    return Value;
}

Stoner::Asset::FAssetProducerVersion ProducerVersion(const char* Text)
{
    Stoner::Asset::FAssetProducerVersion Value;
    (void)Stoner::Asset::FAssetProducerVersion::Create(
        Stoner::Core::FString(Text), Value);
    return Value;
}

Stoner::Asset::FAssetDigest DigestByte(Stoner::Core::uint8 Byte)
{
    const std::array<Stoner::Core::uint8, 1> Bytes{Byte};
    return Stoner::Asset::FAssetDigest::FromBytes(Bytes);
}

bool CompleteEvidenceMutationsChangeKeys()
{
    using namespace Stoner;
    using namespace Stoner::Tests::AssetCookerDDC;
    Asset::FAssetDerivedKeyEvidence Base;
    Base.AssetId = [] { Asset::FAssetId Value; (void)Asset::FAssetId::Create(
        Core::FString("Texture"), Core::FString("Incremental/A"), {}, Value); return Value; }();
    Base.SourceVersion = DigestByte(1);
    Asset::FAssetSourceLocator Locator;
    (void)Asset::FAssetSourceLocator::Create(
        Core::FString("content"), Core::FString("A.png"), Locator);
    Base.SourceManifest = {{Locator, DigestByte(2)}};
    Asset::FAssetDerivedDependencyEvidence Dependency;
    (void)Asset::FAssetId::Create(Core::FString("Image"), Core::FString("Incremental/A"), {}, Dependency.Id);
    Dependency.Version.SourceDigest = DigestByte(3);
    Base.Dependencies = {Dependency};
    Base.ImporterId = Participant("importer.image");
    Base.ImporterVersion = ProducerVersion("1");
    Base.CookerId = Participant("cooker.texture");
    Base.CookerVersion = ProducerVersion("1");
    Base.CodecId = Participant("stoner.texture");
    Base.CodecVersion = ProducerVersion("1");
    Base.PayloadSchemaVersion = 1;
    Base.EffectiveSettingsDigest = DigestByte(4);
    Base.RelevantProfileDigest = DigestByte(5);
    Asset::FAssetDerivedKey Baseline;
    if (Asset::FAssetCookContractCodec::BuildDerivedKey(Base, Baseline) != Asset::EAssetResult::Success)
        return false;
    Core::TArray<Asset::FAssetDerivedKeyEvidence> Variants;
    auto Add = [&](auto Mutator) { auto Value = Base; Mutator(Value); Variants.push_back(std::move(Value)); };
    Add([](auto& V) { V.SourceVersion = DigestByte(11); });
    Add([](auto& V) { V.Dependencies.front().Version.SourceDigest = DigestByte(12); });
    Add([](auto& V) { V.ImporterVersion = ProducerVersion("2"); });
    Add([](auto& V) { V.CookerVersion = ProducerVersion("2"); });
    Add([](auto& V) { V.CodecVersion = ProducerVersion("2"); });
    Add([](auto& V) { V.PayloadSchemaVersion = 2; });
    Add([](auto& V) { V.EffectiveSettingsDigest = DigestByte(13); });
    Add([](auto& V) { V.RelevantProfileDigest = DigestByte(14); });
    for (const auto& Variant : Variants)
    {
        Asset::FAssetDerivedKey Key;
        if (Asset::FAssetCookContractCodec::BuildDerivedKey(Variant, Key) != Asset::EAssetResult::Success ||
            Key == Baseline) return false;
    }
    return true;
}

} // namespace

FAssetCookerIncrementalTestResult RunAssetCookerIncrementalTests()
{
    using namespace Stoner;
    using namespace Stoner::Tests::AssetCookerDDC;
    FAssetCookerIncrementalTestResult Result;
    const auto Record = [&Result](bool Passed, const char* Name)
    {
        (Passed ? ++Result.Passed : ++Result.Failed);
        std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
    };
    const auto Root = std::filesystem::temp_directory_path() / "stoner-ddc-incremental";
    std::filesystem::remove_all(Root);
    const auto Content = Root / "Content";
    SeedPng(Content);
    auto RequestValue = Request(Root, Content);
    const FRun First = Run(RequestValue);
    const FRun Unchanged = Run(RequestValue);
    Record(First.Result.Succeeded() && Unchanged.Result.Succeeded() &&
            Unchanged.Report.Counts.ReuseEligible == 2 &&
            Unchanged.Report.Counts.Reused == 2 &&
            Unchanged.Report.Counts.Cooked == 0 &&
            EqualArtifacts(First.Result.Artifacts, Unchanged.Result.Artifacts),
        "unchanged incremental cook reuses 100 percent of eligible entries");
    Record(CompleteEvidenceMutationsChangeKeys(),
        "source, dependency, importer, cooker, codec, schema, settings, and capability evidence invalidate keys");

    auto Source = Read(Content / "Representative.png");
    Source.push_back(0);
    Write(Content / "Representative.png", Source);
    const FRun InvalidSource = Run(RequestValue);
    Record(!InvalidSource.Result.Succeeded(),
        "malformed source edits never fall back to stale derived data");

    SeedPng(Content);
    const FRun Restored = Run(RequestValue);
    Record(Restored.Result.Succeeded() && Restored.Report.Counts.Reused == 2 &&
            EqualArtifacts(First.Result.Artifacts, Restored.Result.Artifacts),
        "restoring source state converges to the original immutable entries");

    Write(Content / "Added.png", Read("Tests/Fixtures/Images/Valid/png-gray-3x5.png"));
    const FRun Added = Run(RequestValue);
    Write(Content / "Representative.png", Read("Tests/Fixtures/Images/Valid/png-rgba-5x3.png"));
    const FRun Edited = Run(RequestValue);
    std::filesystem::remove(Content / "Added.png");
    const FRun Removed = Run(RequestValue);
    std::filesystem::rename(Content / "Representative.png", Content / "Renamed.png");
    const FRun Renamed = Run(RequestValue);
    auto CleanRequest = Request(Root / "CleanFinal", Content);
    CleanRequest.CachePolicy = AssetCooker::EAssetCookCachePolicy::IgnoreExisting;
    const FRun CleanFinal = Run(CleanRequest);
    Record(Added.Result.Succeeded() && Added.Report.Counts.Reused == 2 &&
            Added.Report.Counts.Cooked == 2 && Edited.Result.Succeeded() &&
            Edited.Report.Counts.Reused == 2 && Edited.Report.Counts.Cooked == 2 &&
            Removed.Result.Succeeded() && Removed.Result.Manifest.Records.size() == 2 &&
            Renamed.Result.Succeeded() && Renamed.Result.Manifest.Records.size() == 2 &&
            CleanFinal.Result.Succeeded() &&
            Renamed.Result.CanonicalManifest == CleanFinal.Result.CanonicalManifest &&
            EqualArtifacts(Renamed.Result.Artifacts, CleanFinal.Result.Artifacts),
        "addition, edit, removal, and rename converge artifact-for-artifact with clean final state");
    std::filesystem::remove_all(Root);
    return Result;
}
