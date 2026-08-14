#include "AssetCookerManifestTests.h"

#include "Asset/FAssetCookContractCodec.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

namespace
{

using namespace Stoner::Asset;
using namespace Stoner::Core;

void Record(FAssetCookerManifestTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FAssetDigest Digest(std::string_view Text)
{
    return FAssetDigest::FromBytes(std::span<const uint8>(
        reinterpret_cast<const uint8*>(Text.data()), Text.size()));
}

FAssetId Id(const char* Type, const char* Path)
{
    FAssetId Value;
    (void)FAssetId::Create(FString(Type), FString(Path), {}, Value);
    return Value;
}

FAssetCookManifestParticipant Participant(const char* Name)
{
    FAssetCookManifestParticipant Value;
    (void)FAssetParticipantId::Create(FString(Name), Value.Id);
    (void)FAssetProducerVersion::Create(FString("1.0.0"), Value.Version);
    return Value;
}

FAssetDerivedKey Key(std::string_view Seed)
{
    FAssetDerivedKey Value;
    (void)FAssetDerivedKey::ParseLowerHex(Digest(Seed).ToLowerHex(), Value);
    return Value;
}

FAssetTargetProfileEvidence Profile()
{
    std::ifstream Input(
        "Tests/Fixtures/AssetCooker/Contracts/Profiles/mac-vulkan.json",
        std::ios::binary);
    const std::string Text{
        std::istreambuf_iterator<char>(Input),
        std::istreambuf_iterator<char>()};
    FAssetTargetProfileEvidence Evidence;
    (void)FAssetCookContractCodec::ParseTargetProfile(
        std::span<const uint8>(
            reinterpret_cast<const uint8*>(Text.data()), Text.size()),
        Evidence);
    return Evidence;
}

FAssetCookManifestRecord MakeRecord(const FAssetId& AssetId, const char* Seed)
{
    FAssetCookManifestRecord Value;
    Value.AssetId = AssetId;
    Value.AssetType = AssetId.GetAssetType();
    Value.SourceVersion = Digest(std::string(Seed) + ".source");
    Value.SourceManifest = {{
        AssetId,
        Digest(std::string(Seed) + ".source-file"),
        FString("primary")}};
    Value.Importer = Participant("importer.test");
    Value.Cooker = Participant("cooker.test");
    Value.Codec = Participant("codec.test");
    Value.DerivedKey = Key(std::string(Seed) + ".key");
    Value.PayloadSchemaVersion = 1;
    Value.EnvelopeDigest = Digest(std::string(Seed) + ".envelope");
    Value.PayloadLocator = FString(
        std::string("Payloads/") +
        Value.EnvelopeDigest.ToLowerHex().ToStdString() + ".sgasset");
    Value.PayloadBytes = 128;
    return Value;
}

FAssetCookManifest MakeManifest()
{
    const FAssetId Image = Id("Image", "Cooker/A");
    const FAssetId Texture = Id("Texture", "Cooker/B");
    FAssetCookManifest Value;
    Value.TargetProfile = Profile();
    Value.Selection.Mode = EAssetCookSelectionMode::ExplicitRoots;
    Value.Selection.Roots = {Texture};
    Value.Selection.SourceScopes = {FString("Content")};
    Value.Selection.DiscoveryRulesVersion = Digest("discovery.v1");
    Value.SnapshotDigest = Digest("snapshot.v1");
    Value.LimitsDigest = Digest("limits.v1");
    Value.Records = {
        MakeRecord(Image, "image"),
        MakeRecord(Texture, "texture")};
    Value.Records[1].Dependencies = {{Image, FString("build"), {}}};
    Value.RequiredExtensions = {FString("stoner.core")};
    return Value;
}

TArray<uint8> Bytes(std::string_view Text)
{
    return TArray<uint8>(Text.begin(), Text.end());
}

void TestCanonicalRoundTrip(FAssetCookerManifestTestResult& Result)
{
    FAssetCookManifest Manifest = MakeManifest();
    FString First;
    const EAssetResult Write = FAssetCookContractCodec::WriteManifest(
        Manifest, {}, First);
    FAssetCookManifest Parsed;
    const EAssetResult Parse = FAssetCookContractCodec::ParseManifest(
        Bytes(First.View()), {}, Parsed);
    FString Second;
    const EAssetResult Rewrite = FAssetCookContractCodec::WriteManifest(
        Parsed, {}, Second);
    Record(
        Result,
        Write == EAssetResult::Success && Parse == EAssetResult::Success &&
            Rewrite == EAssetResult::Success && Parsed == Manifest &&
            First == Second,
        "canonical manifest round-trips byte-identically");

    std::string NonCanonical = First.ToStdString();
    NonCanonical.insert(0, " ");
    Record(
        Result,
        FAssetCookContractCodec::ParseManifest(
            Bytes(NonCanonical), {}, Parsed) == EAssetResult::InvalidDefinition,
        "non-canonical manifest JSON is rejected");
}

void TestGenerationIdentity(FAssetCookerManifestTestResult& Result)
{
    FAssetCookManifest Baseline = MakeManifest();
    FAssetDigest BaselineId;
    (void)FAssetCookContractCodec::ComputeManifestGenerationId(
        Baseline, BaselineId);

    FAssetCookManifest PhysicalRename = Baseline;
    PhysicalRename.TargetProfile.Profile.DisplayName = FString("Renamed display");
    PhysicalRename.Records[0].PayloadLocator = FString("Other/image.sgasset");
    FAssetDigest PhysicalId;
    (void)FAssetCookContractCodec::ComputeManifestGenerationId(
        PhysicalRename, PhysicalId);
    Record(
        Result,
        PhysicalId == BaselineId,
        "display identity and physical payload locator do not affect generation identity");

    FAssetCookManifest SemanticMutation = Baseline;
    SemanticMutation.Records[0].EnvelopeDigest = Digest("mutated.envelope");
    FAssetDigest MutatedId;
    (void)FAssetCookContractCodec::ComputeManifestGenerationId(
        SemanticMutation, MutatedId);
    Record(
        Result,
        MutatedId != BaselineId,
        "payload and semantic evidence affect generation identity");
}

void TestOrderingAndClosure(FAssetCookerManifestTestResult& Result)
{
    FAssetCookManifest Unsorted = MakeManifest();
    std::swap(Unsorted.Records[0], Unsorted.Records[1]);
    Unsorted.GenerationId = Digest("placeholder");
    Record(Result, Unsorted.Validate() != EAssetResult::Success,
        "manifest records require strict canonical identity order");

    FAssetCookManifest Duplicate = MakeManifest();
    Duplicate.Records[1] = Duplicate.Records[0];
    Duplicate.GenerationId = Digest("placeholder");
    Record(Result, Duplicate.Validate() != EAssetResult::Success,
        "duplicate asset identities are rejected");

    FAssetCookManifest Missing = MakeManifest();
    Missing.Records.pop_back();
    Missing.Records[0].Dependencies = {{
        Id("Texture", "Missing"), FString("build"), {}}};
    Missing.GenerationId = Digest("placeholder");
    Record(Result, Missing.Validate() == EAssetResult::UnresolvedDependency,
        "manifest dependency evidence must be closed over records");
}

void TestLocatorAndLimits(FAssetCookerManifestTestResult& Result)
{
    FAssetCookManifest Escape = MakeManifest();
    Escape.Records[0].PayloadLocator = FString("../outside.sgasset");
    Escape.GenerationId = Digest("placeholder");
    Record(Result, Escape.Validate() != EAssetResult::Success,
        "payload locators cannot escape the generation root");

    FAssetCookManifest Manifest = MakeManifest();
    FString Canonical;
    (void)FAssetCookContractCodec::WriteManifest(Manifest, {}, Canonical);
    FAssetCookManifestLimits Limits;
    Limits.MaxRecords = 1;
    FAssetCookManifest Parsed;
    Record(
        Result,
        FAssetCookContractCodec::ParseManifest(
            Bytes(Canonical.View()), Limits, Parsed) != EAssetResult::Success &&
            Parsed.Records.empty(),
        "manifest record limit rejects without partial caller-visible output");

    std::string DuplicateKey = Canonical.ToStdString();
    const std::size_t Brace = DuplicateKey.find('{');
    DuplicateKey.insert(
        Brace + 1, "\n  \"schema\": \"stoner.asset-cook-manifest\",");
    Record(
        Result,
        FAssetCookContractCodec::ParseManifest(
            Bytes(DuplicateKey), {}, Parsed) == EAssetResult::InvalidDefinition,
        "duplicate JSON keys fail closed");
}

void TestGoldenFixtures(FAssetCookerManifestTestResult& Result)
{
    const auto Read = [](const char* Path)
    {
        std::ifstream Input(Path, std::ios::binary);
        const std::string Text{
            std::istreambuf_iterator<char>(Input),
            std::istreambuf_iterator<char>()};
        return Bytes(Text);
    };
    FAssetCookManifest Parsed;
    const TArray<uint8> Valid = Read(
        "Tests/Fixtures/AssetCooker/Contracts/Manifests/valid-manifest.json");
    const EAssetResult ValidParse =
        FAssetCookContractCodec::ParseManifest(Valid, {}, Parsed);
    const bool ValidPassed = !Valid.empty() &&
        ValidParse == EAssetResult::Success && Parsed.Records.size() == 2;
    if (!ValidPassed)
    {
        std::cout << "[DETAIL] manifest golden parse="
                  << static_cast<int>(ValidParse)
                  << " bytes=" << Valid.size() << '\n';
    }
    bool InvalidRejected = true;
    for (const char* Name : {
             "invalid-noncanonical.json", "invalid-generation.json"})
    {
        const TArray<uint8> Invalid = Read(
            (std::string(
                "Tests/Fixtures/AssetCooker/Contracts/Manifests/") +
             Name).c_str());
        InvalidRejected = InvalidRejected && !Invalid.empty() &&
            FAssetCookContractCodec::ParseManifest(
                Invalid, {}, Parsed) != EAssetResult::Success;
    }
    Record(Result, ValidPassed, "checked-in manifest golden is accepted");
    Record(Result, InvalidRejected,
        "checked-in malformed manifest goldens are rejected");
}

} // namespace

FAssetCookerManifestTestResult RunAssetCookerManifestTests()
{
    FAssetCookerManifestTestResult Result;
    TestCanonicalRoundTrip(Result);
    TestGenerationIdentity(Result);
    TestOrderingAndClosure(Result);
    TestLocatorAndLimits(Result);
    TestGoldenFixtures(Result);
    return Result;
}
