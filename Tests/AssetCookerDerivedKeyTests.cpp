#include "AssetCookerDerivedKeyTests.h"

#include "Asset/FAssetDerivedKey.h"
#include "Asset/FAssetCookContractCodec.h"
#include "Asset/FAssetDerivedDataEntry.h"
#include "FAssetDerivedKeyBuilder.h"

#include <fstream>
#include <iostream>
#include <string_view>

namespace
{

using namespace Stoner::Asset;
using namespace Stoner::Core;

void Record(FAssetCookerDerivedKeyTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FAssetDigest Digest(std::string_view Text)
{
    return FAssetDigest::FromBytes(std::span<const uint8>(
        reinterpret_cast<const uint8*>(Text.data()), Text.size()));
}

FAssetParticipantId Participant(const char* Text)
{
    FAssetParticipantId Value;
    (void)FAssetParticipantId::Create(FString(Text), Value);
    return Value;
}

FAssetProducerVersion Version(const char* Text)
{
    FAssetProducerVersion Value;
    (void)FAssetProducerVersion::Create(FString(Text), Value);
    return Value;
}

FAssetId AssetId(const char* Type, const char* Path)
{
    FAssetId Value;
    (void)FAssetId::Create(FString(Type), FString(Path), {}, Value);
    return Value;
}

FAssetDerivedKeyEvidence MakeEvidence()
{
    FAssetDerivedKeyEvidence Evidence;
    Evidence.AssetId = AssetId("Texture", "Cooker/Test");
    Evidence.SourceVersion = Digest("source-v1");
    FAssetSourceLocator Locator;
    (void)FAssetSourceLocator::Create(
        FString("file"), FString("Content/test.png"), Locator);
    Evidence.SourceManifest = {{Locator, Digest("source-file-v1")}};
    FAssetDerivedDependencyEvidence Dependency;
    Dependency.Id = AssetId("Image", "Cooker/Test");
    Dependency.Version.ContentDigest = Digest("image-content-v1");
    Dependency.Role = EAssetDependencyRole::Build;
    Evidence.Dependencies = {std::move(Dependency)};
    Evidence.ImporterId = Participant("importer.image");
    Evidence.ImporterVersion = Version("021-v1");
    Evidence.CookerId = Participant("cooker.ktx2");
    Evidence.CookerVersion = Version("022-v1");
    Evidence.CodecId = Participant("codec.ktx2");
    Evidence.CodecVersion = Version("1");
    Evidence.PayloadSchemaVersion = 1;
    Evidence.EffectiveSettingsDigest = Digest("ktx2-settings-v1");
    Evidence.RelevantProfileDigest = Digest("ktx2-profile-v1");
    return Evidence;
}

FString ReadExpectedKey()
{
    std::ifstream Input(
        "Tests/Fixtures/AssetCooker/Contracts/DerivedKeys/baseline.expected.txt",
        std::ios::binary);
    std::string Text{
        std::istreambuf_iterator<char>(Input),
        std::istreambuf_iterator<char>()};
    while (!Text.empty() && (Text.back() == '\n' || Text.back() == '\r'))
    {
        Text.pop_back();
    }
    return FString(std::move(Text));
}

void TestStableAndComplete(FAssetCookerDerivedKeyTestResult& Result)
{
    const FAssetDerivedKeyEvidence Evidence = MakeEvidence();
    FAssetDerivedKey First;
    FAssetDerivedKey Second;
    const bool Built = FAssetDerivedKeyBuilder::Build(Evidence, First) ==
            EAssetResult::Success &&
        FAssetDerivedKeyBuilder::Build(Evidence, Second) == EAssetResult::Success;
    Record(
        Result,
        Built && First == Second && First.IsValid() &&
            First.ToString().Len() == 64,
        "complete evidence produces a stable SHA-256 derived key");

    FAssetDerivedKeyEvidence Missing = Evidence;
    Missing.RelevantProfileDigest = {};
    FAssetDerivedKey Invalid;
    Record(
        Result,
        FAssetDerivedKeyBuilder::Build(Missing, Invalid) !=
                EAssetResult::Success &&
            !Invalid.IsValid(),
        "incomplete byte-affecting evidence cannot produce a key");
}

void TestDomainAndBoundaries(FAssetCookerDerivedKeyTestResult& Result)
{
    FAssetDerivedKeyEvidence FirstEvidence = MakeEvidence();
    FAssetDerivedKeyEvidence SecondEvidence = MakeEvidence();
    FirstEvidence.ImporterId = Participant("importer.ab");
    FirstEvidence.ImporterVersion = Version("c");
    SecondEvidence.ImporterId = Participant("importer.a");
    SecondEvidence.ImporterVersion = Version("bc");
    FAssetDerivedKey First;
    FAssetDerivedKey Second;
    (void)FAssetDerivedKeyBuilder::Build(FirstEvidence, First);
    (void)FAssetDerivedKeyBuilder::Build(SecondEvidence, Second);
    Record(
        Result,
        First != Second,
        "type tags and lengths prevent ambiguous participant boundaries");

    TArray<uint8> Stream;
    (void)FAssetDerivedKeyBuilder::BuildCanonicalStreamForTesting(
        MakeEvidence(), Stream);
    const std::string_view Domain = "stoner.asset-derived-key.v1";
    const bool HasDomain = Stream.size() >= 8 + Domain.size() &&
        std::equal(Domain.begin(), Domain.end(), Stream.begin() + 8);
    Record(Result, HasDomain, "derived key stream is explicitly domain separated");
}

void TestRelevantInvalidation(FAssetCookerDerivedKeyTestResult& Result)
{
    FAssetDerivedKeyEvidence Baseline = MakeEvidence();
    FAssetDerivedKeyEvidence SettingsChanged = Baseline;
    FAssetDerivedKeyEvidence ProfileChanged = Baseline;
    SettingsChanged.EffectiveSettingsDigest = Digest("ktx2-settings-v2");
    ProfileChanged.RelevantProfileDigest = Digest("ktx2-profile-v2");
    FAssetDerivedKey A;
    FAssetDerivedKey B;
    FAssetDerivedKey C;
    (void)FAssetDerivedKeyBuilder::Build(Baseline, A);
    (void)FAssetDerivedKeyBuilder::Build(SettingsChanged, B);
    (void)FAssetDerivedKeyBuilder::Build(ProfileChanged, C);
    Record(
        Result,
        A != B && A != C && B != C,
        "settings and relevant-profile mutations independently invalidate keys");

    FAssetDerivedKeyEvidence Renamed = Baseline;
    Renamed.Dependencies.front().Version.TargetProfile = FString("Display B");
    Baseline.Dependencies.front().Version.TargetProfile = FString("Display A");
    FAssetDerivedKey BeforeRename;
    FAssetDerivedKey AfterRename;
    (void)FAssetDerivedKeyBuilder::Build(Baseline, BeforeRename);
    (void)FAssetDerivedKeyBuilder::Build(Renamed, AfterRename);
    Record(
        Result,
        BeforeRename == AfterRename,
        "legacy display profile names are excluded from derived identity");

    const FString Golden = ReadExpectedKey();
    if (A.ToString() != Golden)
    {
        std::cout << "  derived-key actual=" << A.ToString().ToStdString() << '\n';
    }
    Record(
        Result,
        A.ToString() == Golden,
        "baseline derived key matches checked-in golden value");
}

void TestVersionTwoEvidenceInvalidation(FAssetCookerDerivedKeyTestResult& Result)
{
    FAssetDerivedKeyEvidence Baseline = MakeEvidence();
    Baseline.KeyFormatVersion = FAssetDerivedKeyEvidence::CurrentKeyFormatVersion;
    Baseline.AdditionalEvidence = {
        {FString("binding-policy"), Digest("binding-v1")},
        {FString("profile"), Digest("metal-arm64-v1")},
        {FString("transformation"), Digest("spirv-cross-v1")}};
    FAssetDerivedKey First;
    const bool Built = FAssetDerivedKeyBuilder::Build(Baseline, First) ==
        EAssetResult::Success;
    bool Invalidated = Built;
    for (usize Index = 0; Index < Baseline.AdditionalEvidence.size(); ++Index)
    {
        FAssetDerivedKeyEvidence Changed = Baseline;
        Changed.AdditionalEvidence[Index].Digest = Digest(
            std::string("changed-") + std::to_string(Index));
        FAssetDerivedKey Next;
        Invalidated = Invalidated &&
            FAssetDerivedKeyBuilder::Build(Changed, Next) ==
                EAssetResult::Success &&
            Next != First;
    }
    FAssetDerivedKeyEvidence Unsorted = Baseline;
    std::swap(Unsorted.AdditionalEvidence[0], Unsorted.AdditionalEvidence[1]);
    FAssetDerivedKey Rejected;
    Record(
        Result,
        Invalidated &&
            FAssetDerivedKeyBuilder::Build(Unsorted, Rejected) ==
                EAssetResult::InvalidInput,
        "v2 named evidence invalidates independently and requires canonical order");
}

void TestVersionTwoEntryRoundTrip(FAssetCookerDerivedKeyTestResult& Result)
{
    FAssetDerivedDataEntry Entry;
    Entry.Evidence = MakeEvidence();
    Entry.Evidence.KeyFormatVersion =
        FAssetDerivedKeyEvidence::CurrentKeyFormatVersion;
    Entry.Evidence.AdditionalEvidence = {
        {FString("apple-toolchain"), Digest("toolchain-v1")},
        {FString("authoritative-glsl"), Digest("glsl-v1")},
        {FString("binding-policy"), Digest("binding-v1")},
        {FString("native-library-contract"), Digest("library-v2")},
        {FString("target-profile"), Digest("profile-v1")},
        {FString("target-tuple"), Digest("mac-arm64-v1")},
        {FString("transformation"), Digest("spirv-cross-v1")}};
    Entry.AssetId = Entry.Evidence.AssetId;
    Entry.CodecId = Entry.Evidence.CodecId;
    Entry.CodecVersion = Entry.Evidence.CodecVersion;
    Entry.PayloadSchemaVersion = Entry.Evidence.PayloadSchemaVersion;
    Entry.RelevantProfileDigest = Entry.Evidence.RelevantProfileDigest;
    Entry.PayloadBytes = 128;
    Entry.EnvelopeDigest = Digest("metal-envelope-v2");
    const bool Built = FAssetCookContractCodec::BuildDerivedKey(
        Entry.Evidence, Entry.DerivedKey) == EAssetResult::Success;
    FString Canonical;
    const bool Written = Built &&
        FAssetCookContractCodec::WriteDerivedDataEntry(Entry, Canonical) ==
            EAssetResult::Success;
    FAssetDerivedDataEntry Parsed;
    const TArray<uint8> Bytes(Canonical.View().begin(), Canonical.View().end());
    const bool Read = Written &&
        FAssetCookContractCodec::ParseDerivedDataEntry(Bytes, {}, Parsed) ==
            EAssetResult::Success;
    Record(Result,
        Read && Parsed == Entry &&
            Parsed.Evidence.AdditionalEvidence.size() == 7,
        "v2 named evidence survives canonical DDC metadata round-trip");
}

} // namespace

FAssetCookerDerivedKeyTestResult RunAssetCookerDerivedKeyTests()
{
    FAssetCookerDerivedKeyTestResult Result;
    TestStableAndComplete(Result);
    TestDomainAndBoundaries(Result);
    TestRelevantInvalidation(Result);
    TestVersionTwoEvidenceInvalidation(Result);
    TestVersionTwoEntryRoundTrip(Result);
    return Result;
}
