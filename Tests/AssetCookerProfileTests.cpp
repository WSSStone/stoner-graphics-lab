#include "AssetCookerProfileTests.h"

#include "Asset/FAssetTargetProfile.h"
#include "FAssetTargetProfileCodec.h"
#include "FTextureCookPolicy.h"

#include <fstream>
#include <iostream>
#include <span>
#include <string>

namespace
{

using namespace Stoner::Asset;
using namespace Stoner::Asset::Private;
using namespace Stoner::Core;

void Record(FAssetCookerProfileTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

TArray<uint8> Bytes(std::string_view Text)
{
    return TArray<uint8>(Text.begin(), Text.end());
}

FString ReadText(const char* Path)
{
    std::ifstream Input(Path, std::ios::binary);
    return FString(std::string(
        std::istreambuf_iterator<char>(Input),
        std::istreambuf_iterator<char>()));
}

FAssetParticipantId Participant(const char* Text)
{
    FAssetParticipantId Value;
    (void)FAssetParticipantId::Create(FString(Text), Value);
    return Value;
}

FAssetTargetProfile MakeProfile(const char* DisplayName = "Mac Vulkan Dev")
{
    FAssetTargetProfile Profile;
    Profile.DisplayName = FString(DisplayName);
    Profile.Platform = EAssetTargetPlatform::MacOS;
    Profile.CpuArchitecture = EAssetTargetCpuArchitecture::Arm64;
    Profile.GraphicsBackend = EAssetGraphicsBackend::Vulkan;
    Profile.ShaderPayloadChoices = {{
        EAssetGraphicsBackend::Vulkan,
        FString("vulkan-1.3"),
        EAssetShaderPayloadFormat::SpirV}};
    Profile.TextureCapabilities = {FString("rgba8-unorm")};
    Profile.TextureFallback = EAssetTextureFallback::PortableKTX2;
    const auto MakeCodecSettings = [](const char* Name)
    {
        FAssetProducerSettingsRecord Record;
        Record.Producer = Participant(Name);
        Record.SchemaVersion = 1;
        Record.Settings = {{FString("codecPolicy"), FString("exact-v1")}};
        return Record;
    };
    FAssetProducerSettingsRecord KTX;
    KTX.Producer = Participant("cooker.ktx2");
    KTX.SchemaVersion = 1;
    KTX.Settings = {
        {FString("allowLossyData"), false},
        {FString("compressionPolicy"), FString("default-by-semantic")},
        {FString("portableProfile"), FString("stoner.ktx2.portable.v1")},
        {FString("quality"), FString("balanced")}};
    Profile.BuildPolicy.ProducerSettings = {
        MakeCodecSettings("cooker.cooked-image-texture"),
        MakeCodecSettings("cooker.cooked-material-shader"),
        MakeCodecSettings("cooker.cooked-static-model"),
        std::move(KTX)};
    return Profile;
}

void TestCanonicalRoundTrip(FAssetCookerProfileTestResult& Result)
{
    const FString Fixture = ReadText(
        "Tests/Fixtures/AssetCooker/Contracts/Profiles/mac-vulkan.json");
    FAssetTargetProfileEvidence Parsed;
    const auto FixtureBytes = Bytes(Fixture.View());
    const EAssetResult ParseResult = ParseAssetTargetProfile(FixtureBytes, Parsed);
    FString Rewritten;
    FAssetTargetProfileEvidence Written;
    const EAssetResult WriteResult = WriteAssetTargetProfile(
        Parsed.Profile, Rewritten, &Written);
    Record(
        Result,
        ParseResult == EAssetResult::Success &&
            WriteResult == EAssetResult::Success && Rewritten == Fixture &&
            Written == Parsed,
        "strict target profile fixture round-trips canonically");
}

void TestEffectiveIdentity(FAssetCookerProfileTestResult& Result)
{
    FString FirstText;
    FString SecondText;
    FAssetTargetProfileEvidence First;
    FAssetTargetProfileEvidence Second;
    (void)WriteAssetTargetProfile(MakeProfile("Mac A"), FirstText, &First);
    (void)WriteAssetTargetProfile(MakeProfile("Mac B"), SecondText, &Second);
    Record(
        Result,
        FirstText != SecondText &&
            First.EffectiveProfileDigest == Second.EffectiveProfileDigest &&
            First.CanonicalEffectiveConfiguration ==
                Second.CanonicalEffectiveConfiguration,
        "display rename preserves normalized effective identity");

    const TArray<FString> NoTargetFields;
    FAssetProfileProjectionEvidence FirstProjection;
    FAssetProfileProjectionEvidence IrrelevantProjection;
    (void)BuildAssetProfileProjection(
        First, Participant("cooker.ktx2"), 1,
        NoTargetFields, FirstProjection);
    FAssetTargetProfile IrrelevantProfile = First.Profile;
    IrrelevantProfile.GraphicsBackend = EAssetGraphicsBackend::Metal;
    IrrelevantProfile.ShaderPayloadChoices = {{
        EAssetGraphicsBackend::Metal,
        FString("metal2.4"),
        EAssetShaderPayloadFormat::MSL}};
    FString IrrelevantText;
    FAssetTargetProfileEvidence Irrelevant;
    (void)WriteAssetTargetProfile(
        IrrelevantProfile, IrrelevantText, &Irrelevant);
    (void)BuildAssetProfileProjection(
        Irrelevant, Participant("cooker.ktx2"), 1,
        NoTargetFields, IrrelevantProjection);
    Record(
        Result,
        First.EffectiveProfileDigest != Irrelevant.EffectiveProfileDigest &&
            FirstProjection.RelevantProfileDigest ==
                IrrelevantProjection.RelevantProfileDigest &&
            FirstProjection.EffectiveSettingsDigest ==
                IrrelevantProjection.EffectiveSettingsDigest,
        "cooker projection excludes irrelevant target fields");

    const TArray<FString> BackendField = {FString("graphicsBackend")};
    FAssetProfileProjectionEvidence FirstBackend;
    FAssetProfileProjectionEvidence OtherBackend;
    (void)BuildAssetProfileProjection(
        First, Participant("cooker.ktx2"), 1,
        BackendField, FirstBackend);
    (void)BuildAssetProfileProjection(
        Irrelevant, Participant("cooker.ktx2"), 1,
        BackendField, OtherBackend);
    Record(
        Result,
        FirstBackend.RelevantProfileDigest != OtherBackend.RelevantProfileDigest,
        "declared relevant target field invalidates projection");
}

void TestStrictFailures(FAssetCookerProfileTestResult& Result)
{
    FString Canonical;
    (void)WriteAssetTargetProfile(MakeProfile(), Canonical);
    const std::string Unknown = Canonical.ToStdString().substr(
        0, Canonical.Len() - 2) + ",\n  \"unknown\": true\n}\n";
    FAssetTargetProfileEvidence Parsed;
    Record(
        Result,
        ParseAssetTargetProfile(Bytes(Unknown), Parsed) != EAssetResult::Success,
        "unknown profile fields fail closed");

    std::string WrongSchema = Canonical.ToStdString();
    const std::size_t Version = WrongSchema.find("\"schemaVersion\": 1");
    WrongSchema[Version + 17] = '2';
    Record(
        Result,
        ParseAssetTargetProfile(Bytes(WrongSchema), Parsed) ==
            EAssetResult::UnsupportedSchema,
        "unknown profile schema revision is rejected");

    FAssetTargetProfile Unsorted = MakeProfile();
    FAssetProducerSettingsRecord Earlier =
        Unsorted.BuildPolicy.ProducerSettings.front();
    Earlier.Producer = Participant("cooker.aaa");
    Unsorted.BuildPolicy.ProducerSettings.push_back(std::move(Earlier));
    Record(
        Result,
        Unsorted.Validate() != EAssetResult::Success,
        "producer settings must be unique and sorted");

    FAssetTargetProfile Missing = MakeProfile();
    Missing.BuildPolicy.ProducerSettings.clear();
    Record(
        Result,
        Missing.Validate() != EAssetResult::Success,
        "profile requires explicit producer settings");
}

void TestKTX2Projection(FAssetCookerProfileTestResult& Result)
{
    FString Canonical;
    FAssetTargetProfileEvidence Evidence;
    (void)WriteAssetTargetProfile(MakeProfile(), Canonical, &Evidence);
    FTextureCookSettings Settings;
    Settings.Quality = ETextureCookQuality::High;
    FTextureCookSettings Resolved;
    FAssetProfileProjectionEvidence Projection;
    const EAssetResult Resolution = ResolveTextureProfileSettings(
        MakeShared<FAssetTargetProfileEvidence>(Evidence),
        Settings,
        Resolved,
        Projection);
    FKTX2TextureCooker Cooker;
    FAssetProfileProjectionEvidence Declared;
    const EAssetResult DeclaredResult =
        Cooker.GetRelevantProfileEvidence(Evidence, Declared);
    Record(
        Result,
        Resolution == EAssetResult::Success &&
            DeclaredResult == EAssetResult::Success &&
            Resolved.Quality == ETextureCookQuality::Balanced &&
            Resolved.CompressionPolicy ==
                ETextureCompressionPolicy::DefaultBySemantic &&
            !Resolved.bAllowLossyData &&
            Projection == Declared,
        "KTX2 cooker consumes validated profile settings instead of legacy parameters");

    FAssetTargetProfile MissingProfile = MakeProfile();
    MissingProfile.BuildPolicy.ProducerSettings.front().Producer =
        Participant("cooker.other");
    FString MissingCanonical;
    FAssetTargetProfileEvidence MissingEvidence;
    (void)WriteAssetTargetProfile(
        MissingProfile, MissingCanonical, &MissingEvidence);
    Record(
        Result,
        ResolveTextureProfileSettings(
            MakeShared<FAssetTargetProfileEvidence>(MissingEvidence),
            Settings,
            Resolved,
            Projection) != EAssetResult::Success,
        "KTX2 cooker fails closed when its producer settings are absent");
}

} // namespace

FAssetCookerProfileTestResult RunAssetCookerProfileTests()
{
    FAssetCookerProfileTestResult Result;
    TestCanonicalRoundTrip(Result);
    TestEffectiveIdentity(Result);
    TestStrictFailures(Result);
    TestKTX2Projection(Result);
    return Result;
}
