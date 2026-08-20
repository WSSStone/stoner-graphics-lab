#include "AssetCookerTargetProfileTests.h"

#include "AssetCookerDerivedDataTestSupport.h"

#include <filesystem>
#include <iostream>

namespace
{

using namespace Stoner;
using namespace Stoner::Tests::AssetCookerDDC;

void Record(FAssetCookerTargetProfileTestResult& Result, bool Passed, const char* Label)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Label << '\n';
}

bool ParseProfile(
    const std::filesystem::path& Path,
    Asset::FAssetTargetProfileEvidence& Out)
{
    return Asset::FAssetCookContractCodec::ParseTargetProfile(Read(Path), Out) ==
        Asset::EAssetResult::Success;
}

bool HasExactGenericSettings(const Asset::FAssetTargetProfile& Profile)
{
    for (const char* Name : {
             "cooker.cooked-image-texture",
             "cooker.cooked-material-shader",
             "cooker.cooked-static-model"})
    {
        Asset::FAssetParticipantId Participant;
        if (Asset::FAssetParticipantId::Create(
                Core::FString(Name), Participant) != Asset::EAssetResult::Success)
            return false;
        const auto* Record = Profile.BuildPolicy.FindProducer(Participant);
        if (!Record || Record->SchemaVersion != 1 || Record->Settings.size() != 1)
            return false;
        const auto* Setting = Record->Find(Core::FString("codecPolicy"));
        const auto* Value = Setting
            ? std::get_if<Core::FString>(&Setting->Value) : nullptr;
        if (!Value || *Value != Core::FString("exact-v1")) return false;
    }
    return true;
}

bool WriteProfile(
    const std::filesystem::path& Path,
    const Asset::FAssetTargetProfile& Profile,
    Asset::FAssetTargetProfileEvidence& Out)
{
    Core::FString Canonical;
    if (Asset::FAssetCookContractCodec::WriteTargetProfile(
            Profile, Canonical, &Out) != Asset::EAssetResult::Success)
        return false;
    Write(Path, Core::TArray<Core::uint8>(
        Canonical.View().begin(), Canonical.View().end()));
    return true;
}

} // namespace

FAssetCookerTargetProfileTestResult RunAssetCookerTargetProfileTests()
{
    using namespace Stoner;
    using namespace Stoner::Tests::AssetCookerDDC;
    FAssetCookerTargetProfileTestResult Result;
    const Core::TArray<std::filesystem::path> Profiles{
        "Config/AssetCooker/Profiles/Windows-Vulkan.json",
        "Config/AssetCooker/Profiles/Mac-Vulkan.json",
        "Config/AssetCooker/Profiles/Linux-Vulkan.json"};
    bool ProfilesValid = true;
    for (const auto& Path : Profiles)
    {
        Asset::FAssetTargetProfileEvidence Profile;
        ProfilesValid = ProfilesValid && ParseProfile(Path, Profile) &&
            Profile.Profile.GraphicsBackend ==
                Asset::EAssetGraphicsBackend::Vulkan &&
            !Profile.Profile.ShaderPayloadChoices.empty() &&
            !Profile.Profile.TextureCapabilities.empty() &&
            HasExactGenericSettings(Profile.Profile);
    }
    Record(Result, ProfilesValid,
        "Windows macOS and Linux Vulkan profiles are strict complete contracts");

    const Core::TArray<std::filesystem::path> MetalProfiles{
        "Config/AssetCooker/Profiles/Mac-Metal-Arm64.json",
        "Config/AssetCooker/Profiles/Mac-Metal-X86_64.json"};
    bool MetalProfilesValid = true;
    for (const auto& Path : MetalProfiles)
    {
        Asset::FAssetTargetProfileEvidence Profile;
        MetalProfilesValid = MetalProfilesValid && ParseProfile(Path, Profile) &&
            Profile.Profile.SchemaVersion == 2 &&
            Profile.Profile.Platform == Asset::EAssetTargetPlatform::MacOS &&
            Profile.Profile.GraphicsBackend ==
                Asset::EAssetGraphicsBackend::Metal &&
            Profile.Profile.MetalShaderTarget.has_value() &&
            Profile.Profile.MetalShaderTarget->DeploymentTarget ==
                Core::FString("12.0") &&
            Profile.Profile.MetalShaderTarget->MslVersion ==
                Core::FString("2.4") &&
            Profile.Profile.ShaderPayloadChoices.size() == 1 &&
            Profile.Profile.ShaderPayloadChoices.front().Format ==
                Asset::EAssetShaderPayloadFormat::MetalLibrary &&
            HasExactGenericSettings(Profile.Profile);
    }
    Record(Result, MetalProfilesValid,
        "arm64 and x86_64 Metal profiles require macOS 12 MSL 2.4 native libraries");

    Asset::FAssetTargetProfileEvidence Baseline;
    (void)ParseProfile(Profiles[1], Baseline);
    auto RenamedProfile = Baseline.Profile;
    RenamedProfile.DisplayName = Core::FString("Renamed Non-authoritative Profile");
    Core::FString RenamedText;
    Asset::FAssetTargetProfileEvidence Renamed;
    const bool RenamedWritten = Asset::FAssetCookContractCodec::WriteTargetProfile(
        RenamedProfile, RenamedText, &Renamed) == Asset::EAssetResult::Success;
    Record(Result,
        RenamedWritten && Baseline.EffectiveProfileDigest ==
            Renamed.EffectiveProfileDigest &&
        Baseline.CanonicalEffectiveConfiguration ==
            Renamed.CanonicalEffectiveConfiguration,
        "display-only rename preserves effective target identity");

    const auto Root = std::filesystem::temp_directory_path() /
        "stoner-cooker-target-profiles";
    std::filesystem::remove_all(Root);
    const auto Content = Root / "Content";
    SeedPng(Content);
    auto FirstRequest = Request(Root / "First", Content, 1);
    FirstRequest.TargetProfilePath = Core::FString(Profiles[1].generic_string());
    FirstRequest.TargetProfile = Baseline;
    const auto First = Run(FirstRequest);
    Write(Root / "renamed.json", Core::TArray<Core::uint8>(
        RenamedText.View().begin(), RenamedText.View().end()));
    auto SecondRequest = Request(Root / "Second", Content, 8);
    SecondRequest.TargetProfilePath = Core::FString(
        (Root / "renamed.json").generic_string());
    SecondRequest.TargetProfile = Renamed;
    const auto Second = Run(SecondRequest);
    bool EvidenceComplete = First.Result.Succeeded();
    for (const auto& AssetReport : First.Report.Assets)
        EvidenceComplete = EvidenceComplete &&
            AssetReport.EffectiveSettingsDigest.IsAvailable() &&
            AssetReport.RelevantProfileDigest.IsAvailable() &&
            !AssetReport.TargetDecision.IsEmpty();
    Record(Result,
        First.Result.Succeeded() && Second.Result.Succeeded() &&
            First.Result.Manifest.GenerationId == Second.Result.Manifest.GenerationId &&
            EqualArtifacts(First.Result.Artifacts, Second.Result.Artifacts) &&
            EvidenceComplete,
        "renamed-equivalent profiles preserve keys payloads generation and decision evidence");

    auto FallbackProfile = Baseline.Profile;
    FallbackProfile.TextureCapabilities = {Core::FString("rgba16-float")};
    FallbackProfile.TextureFallback = Asset::EAssetTextureFallback::Uncompressed;
    Asset::FAssetTargetProfileEvidence Fallback;
    const auto FallbackPath = Root / "fallback.json";
    const bool FallbackWritten = WriteProfile(
        FallbackPath, FallbackProfile, Fallback);
    auto FallbackRequest = Request(Root / "Fallback", Content, 4);
    FallbackRequest.TargetProfilePath = Core::FString(FallbackPath.generic_string());
    FallbackRequest.TargetProfile = Fallback;
    const auto FallbackRun = Run(FallbackRequest);
    bool SawFallback = FallbackWritten && FallbackRun.Result.Succeeded();
    for (const auto& AssetReport : FallbackRun.Report.Assets)
        SawFallback = SawFallback && AssetReport.bUsedFallback &&
            AssetReport.TargetDecision ==
                Core::FString("texture-fallback:uncompressed");
    Record(Result, SawFallback,
        "explicit uncompressed fallback is applied and reported for incompatible LDR target capabilities");

    auto FailProfile = FallbackProfile;
    FailProfile.TextureFallback = Asset::EAssetTextureFallback::Fail;
    Asset::FAssetTargetProfileEvidence FailEvidence;
    const auto FailPath = Root / "fail.json";
    const bool FailWritten = WriteProfile(FailPath, FailProfile, FailEvidence);
    auto FailRequest = Request(Root / "Fail", Content, 4);
    FailRequest.TargetProfilePath = Core::FString(FailPath.generic_string());
    FailRequest.TargetProfile = FailEvidence;
    const auto FailRun = Run(FailRequest);
    Record(Result,
        FailWritten && !FailRun.Result.Succeeded() &&
            FailRun.Result.Category ==
                AssetCooker::EAssetCookResultCategory::CookFailure &&
            !std::filesystem::exists(FailRequest.OutputRoot.ToStdString()),
        "unsupported texture requirement fails closed without publication");

    auto UnsupportedProfile = Baseline.Profile;
    UnsupportedProfile.GraphicsBackend = Asset::EAssetGraphicsBackend::Metal;
    UnsupportedProfile.ShaderPayloadChoices = {{
        Asset::EAssetGraphicsBackend::Metal,
        Core::FString("metal2.4"), Asset::EAssetShaderPayloadFormat::MSL}};
    Core::FString UnsupportedText;
    Asset::FAssetTargetProfileEvidence Unsupported;
    const bool UnsupportedWritten = Asset::FAssetCookContractCodec::WriteTargetProfile(
        UnsupportedProfile, UnsupportedText, &Unsupported) ==
        Asset::EAssetResult::Success;
    Record(Result, UnsupportedWritten,
        "unsupported target remains a valid explicit profile contract");

    const auto FullContent = Root / "UnsupportedContent";
    std::filesystem::create_directories(FullContent);
    for (const char* Name : {"Surface.shader.json", "Surface.vert",
             "Surface.frag", "Surface.vert.spv", "Surface.frag.spv"})
        std::filesystem::copy_file(
            std::filesystem::path("Tests/Fixtures/AssetCooker/Representative") /
                Name,
            FullContent / Name,
            std::filesystem::copy_options::overwrite_existing);
    const auto UnsupportedPath = Root / "unsupported.json";
    Write(UnsupportedPath, Core::TArray<Core::uint8>(
        UnsupportedText.View().begin(), UnsupportedText.View().end()));
    auto UnsupportedRequest = Request(Root / "Unsupported", FullContent, 4);
    UnsupportedRequest.TargetProfilePath = Core::FString(
        UnsupportedPath.generic_string());
    UnsupportedRequest.TargetProfile = Unsupported;
    const auto UnsupportedRun = Run(UnsupportedRequest);
    Record(Result,
        UnsupportedWritten && !UnsupportedRun.Result.Succeeded() &&
            UnsupportedRun.Result.Category ==
                AssetCooker::EAssetCookResultCategory::CookFailure,
        "unavailable backend-tagged shader payload fails closed");
    std::filesystem::remove_all(Root);
    return Result;
}
