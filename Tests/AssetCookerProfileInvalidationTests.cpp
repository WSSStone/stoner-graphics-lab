#include "AssetCookerProfileInvalidationTests.h"

#include "Asset/AssetMinimal.h"
#include "Asset/IAssetCooker.h"
#include "AssetCookerDerivedDataTestSupport.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace
{

using namespace Stoner;

struct FProjectionSet
{
    Asset::FAssetProfileProjectionEvidence Image;
    Asset::FAssetProfileProjectionEvidence Material;
    Asset::FAssetProfileProjectionEvidence StaticModel;
};

bool ProjectOne(
    Asset::EAssetCookedFamily Family,
    const Asset::FAssetTargetProfileEvidence& Profile,
    Asset::FAssetProfileProjectionEvidence& Out)
{
    Asset::FAssetExtensionRegistry Registry;
    Asset::FAssetCookedExtensionRegistrations Registrations;
    if (Asset::RegisterCookedAssetExtensions(Registry, Registrations) !=
        Asset::EAssetResult::Success) return false;
    Asset::FAssetParticipantId Participant;
    if (Asset::GetAssetCookedParticipant(
            Family, Asset::EAssetExtensionKind::Cooker, Participant) !=
        Asset::EAssetResult::Success) return false;
    auto Lease = Registry.Acquire(Asset::EAssetExtensionKind::Cooker, Participant);
    auto Cooker = Lease.Get<Asset::IAssetCooker>();
    return Cooker && Cooker->GetRelevantProfileEvidence(Profile, Out) ==
        Asset::EAssetResult::Success;
}

bool Project(
    const Asset::FAssetTargetProfileEvidence& Profile,
    FProjectionSet& Out)
{
    Asset::FAssetExtensionRegistry Registry;
    Asset::FAssetCookedExtensionRegistrations Registrations;
    if (Asset::RegisterCookedAssetExtensions(Registry, Registrations) !=
        Asset::EAssetResult::Success) return false;
    const std::array Families{
        Asset::EAssetCookedFamily::ImageTexture,
        Asset::EAssetCookedFamily::MaterialShader,
        Asset::EAssetCookedFamily::StaticModel};
    std::array<Asset::FAssetProfileProjectionEvidence*, 3> Outputs{
        &Out.Image, &Out.Material, &Out.StaticModel};
    for (Core::usize Index = 0; Index < Families.size(); ++Index)
    {
        Asset::FAssetParticipantId Participant;
        if (Asset::GetAssetCookedParticipant(
                Families[Index], Asset::EAssetExtensionKind::Cooker,
                Participant) != Asset::EAssetResult::Success) return false;
        auto Lease = Registry.Acquire(Asset::EAssetExtensionKind::Cooker, Participant);
        auto Cooker = Lease.Get<Asset::IAssetCooker>();
        if (!Cooker || Cooker->GetRelevantProfileEvidence(
                Profile, *Outputs[Index]) != Asset::EAssetResult::Success)
            return false;
    }
    return true;
}

Asset::FAssetTargetProfileEvidence Evidence(Asset::FAssetTargetProfile Profile)
{
    Core::FString Canonical;
    Asset::FAssetTargetProfileEvidence Result;
    (void)Asset::FAssetCookContractCodec::WriteTargetProfile(
        Profile, Canonical, &Result);
    return Result;
}

void Record(
    FAssetCookerProfileInvalidationTestResult& Result,
    bool Passed,
    const char* Label)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Label << '\n';
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
    Tests::AssetCookerDDC::Write(Path, Core::TArray<Core::uint8>(
        Canonical.View().begin(), Canonical.View().end()));
    return true;
}

bool HasExactDecisions(
    const AssetCooker::FAssetCookReport& Report,
    std::optional<Asset::EAssetCookedFamily> CookedFamily)
{
    if (Report.Assets.empty()) return false;
    for (const auto& Entry : Report.Assets)
    {
        Asset::EAssetCookedFamily Family;
        if (Asset::GetAssetCookedFamily(
                Entry.AssetId.GetAssetType(), Family) !=
            Asset::EAssetResult::Success) return false;
        const auto Expected = CookedFamily && Family == *CookedFamily
            ? AssetCooker::EAssetCookDecision::Cooked
            : AssetCooker::EAssetCookDecision::Reused;
        if (Entry.Decision != Expected) return false;
    }
    return true;
}

} // namespace

FAssetCookerProfileInvalidationTestResult
RunAssetCookerProfileInvalidationTests()
{
    using namespace Stoner;
    FAssetCookerProfileInvalidationTestResult Result;
    Asset::FAssetTargetProfileEvidence Base;
    (void)Asset::FAssetCookContractCodec::ParseTargetProfile(
        Tests::AssetCookerDDC::Read(
            "Tests/Fixtures/AssetCooker/Contracts/Profiles/mac-vulkan.json"),
        Base);
    FProjectionSet Original;
    const bool BaseProjected = Project(Base, Original);

    auto TextureProfile = Base.Profile;
    TextureProfile.TextureCapabilities.push_back(Core::FString("z-test-format"));
    FProjectionSet Texture;
    const bool TextureProjected = Project(Evidence(TextureProfile), Texture);
    Record(Result,
        BaseProjected && TextureProjected &&
            Texture.Image.RelevantProfileDigest != Original.Image.RelevantProfileDigest &&
            Texture.Material == Original.Material &&
            Texture.StaticModel == Original.StaticModel,
        "texture capability invalidates only image-texture projection");

    auto ShaderProfile = Base.Profile;
    ShaderProfile.ShaderPayloadChoices.push_back({
        Asset::EAssetGraphicsBackend::Vulkan, Core::FString("vulkan-1.4"),
        Asset::EAssetShaderPayloadFormat::SpirV});
    FProjectionSet Shader;
    const bool ShaderProjected = Project(Evidence(ShaderProfile), Shader);
    Record(Result,
        ShaderProjected && Shader.Material.RelevantProfileDigest !=
            Original.Material.RelevantProfileDigest &&
            Shader.Image == Original.Image &&
            Shader.StaticModel == Original.StaticModel,
        "shader choice invalidates only material-shader projection");

    auto HostProfile = Base.Profile;
    HostProfile.Platform = Asset::EAssetTargetPlatform::Linux;
    HostProfile.CpuArchitecture = Asset::EAssetTargetCpuArchitecture::X86_64;
    FProjectionSet Host;
    const bool HostProjected = Project(Evidence(HostProfile), Host);
    Record(Result,
        HostProjected && Host.Image == Original.Image &&
            Host.Material == Original.Material &&
            Host.StaticModel == Original.StaticModel,
        "host platform fields do not invalidate target-independent family projections");

    auto FallbackProfile = Base.Profile;
    FallbackProfile.TextureFallback = Asset::EAssetTextureFallback::Uncompressed;
    FProjectionSet Fallback;
    const bool FallbackProjected = Project(Evidence(FallbackProfile), Fallback);
    Record(Result,
        FallbackProjected && Fallback.Image.RelevantProfileDigest !=
            Original.Image.RelevantProfileDigest &&
            Fallback.Material == Original.Material &&
            Fallback.StaticModel == Original.StaticModel,
        "texture fallback invalidates only image-texture projection");

    auto BackendProfile = Base.Profile;
    BackendProfile.GraphicsBackend = Asset::EAssetGraphicsBackend::Metal;
    FProjectionSet Backend;
    const bool BackendProjected = Project(Evidence(BackendProfile), Backend);
    Record(Result,
        BackendProjected && Backend.Material.RelevantProfileDigest !=
            Original.Material.RelevantProfileDigest &&
            Backend.Image == Original.Image &&
            Backend.StaticModel == Original.StaticModel,
        "graphics backend invalidates only material-shader projection");

    auto DisplayProfile = Base.Profile;
    DisplayProfile.DisplayName = Core::FString("Projection Display Rename");
    FProjectionSet Display;
    const bool DisplayProjected = Project(Evidence(DisplayProfile), Display);
    Record(Result,
        DisplayProjected && Display.Image == Original.Image &&
            Display.Material == Original.Material &&
            Display.StaticModel == Original.StaticModel,
        "display-only profile fields invalidate no payload family");

    auto StaticSettings = Base.Profile;
    Asset::FAssetParticipantId StaticProducer;
    (void)Asset::FAssetParticipantId::Create(
        Core::FString("cooker.cooked-static-model"), StaticProducer);
    auto StaticRecord = std::find_if(
        StaticSettings.BuildPolicy.ProducerSettings.begin(),
        StaticSettings.BuildPolicy.ProducerSettings.end(),
        [&StaticProducer](const auto& Record)
        { return Record.Producer == StaticProducer; });
    if (StaticRecord != StaticSettings.BuildPolicy.ProducerSettings.end())
        StaticRecord->Settings.front().Value = Core::FString("exact-v2");
    const auto InvalidStaticEvidence = Evidence(StaticSettings);
    Asset::FAssetProfileProjectionEvidence StaticImage;
    Asset::FAssetProfileProjectionEvidence StaticMaterial;
    Asset::FAssetProfileProjectionEvidence StaticModel;
    const bool StaticImageProjected = ProjectOne(
        Asset::EAssetCookedFamily::ImageTexture,
        InvalidStaticEvidence, StaticImage);
    const bool StaticMaterialProjected = ProjectOne(
        Asset::EAssetCookedFamily::MaterialShader,
        InvalidStaticEvidence, StaticMaterial);
    const bool StaticModelProjected = ProjectOne(
        Asset::EAssetCookedFamily::StaticModel,
        InvalidStaticEvidence, StaticModel);
    Record(Result,
        StaticImageProjected && StaticImage == Original.Image &&
            StaticMaterialProjected && StaticMaterial == Original.Material &&
            !StaticModelProjected,
        "unsupported producer setting is rejected only by its owning family");

    const auto Root = std::filesystem::temp_directory_path() /
        "stoner-cooker-profile-invalidation";
    std::filesystem::remove_all(Root);
    const auto Content = std::filesystem::path(
        "Tests/Fixtures/AssetCooker/Representative");
    const auto SharedDdc = Root / "DDC";
    auto CookProfile = [&](const char* Name, Asset::FAssetTargetProfile Profile)
    {
        Asset::FAssetTargetProfileEvidence ProfileEvidence;
        const auto Path = Root / (std::string(Name) + ".json");
        (void)WriteProfile(Path, Profile, ProfileEvidence);
        auto Request = Tests::AssetCookerDDC::Request(
            Root / Name, Content, 4);
        Request.TargetProfilePath = Core::FString(Path.generic_string());
        Request.TargetProfile = ProfileEvidence;
        Request.DerivedDataRoot = Core::FString(SharedDdc.generic_string());
        return Tests::AssetCookerDDC::Run(Request);
    };

    const auto Initial = CookProfile("initial", Base.Profile);
    auto Renamed = Base.Profile;
    Renamed.DisplayName = Core::FString("Renamed DDC Profile");
    const auto RenamedRun = CookProfile("renamed", Renamed);
    Record(Result,
        Initial.Result.Succeeded() && RenamedRun.Result.Succeeded() &&
            HasExactDecisions(RenamedRun.Report, std::nullopt),
        "display-only mutation reuses every DDC entry");

    auto DdcTexture = Base.Profile;
    DdcTexture.TextureCapabilities = {
        Core::FString("bc7-rgba"), Core::FString("rgba8-unorm")};
    const auto TextureRun = CookProfile("texture", DdcTexture);
    Record(Result,
        TextureRun.Result.Succeeded() && HasExactDecisions(
            TextureRun.Report, Asset::EAssetCookedFamily::ImageTexture),
        "texture capability mutation recooks only image-texture DDC entries");

    auto DdcShader = Base.Profile;
    DdcShader.ShaderPayloadChoices.push_back({
        Asset::EAssetGraphicsBackend::Vulkan, Core::FString("vulkan-1.4"),
        Asset::EAssetShaderPayloadFormat::SpirV});
    const auto ShaderRun = CookProfile("shader", DdcShader);
    Record(Result,
        ShaderRun.Result.Succeeded() && HasExactDecisions(
            ShaderRun.Report, Asset::EAssetCookedFamily::MaterialShader),
        "shader choice mutation recooks only material-shader DDC entries");

    auto DdcHost = Base.Profile;
    DdcHost.Platform = Asset::EAssetTargetPlatform::Linux;
    DdcHost.CpuArchitecture = Asset::EAssetTargetCpuArchitecture::X86_64;
    const auto HostRun = CookProfile("host", DdcHost);
    Record(Result,
        HostRun.Result.Succeeded() &&
            HasExactDecisions(HostRun.Report, std::nullopt),
        "irrelevant host mutation reuses every DDC entry");
    std::filesystem::remove_all(Root);
    return Result;
}
