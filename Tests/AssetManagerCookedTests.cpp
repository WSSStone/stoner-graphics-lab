#include "AssetManagerCookedTests.h"

#include "AssetCookerPublicationTestSupport.h"
#include "Asset/FAssetManager.h"
#include "Asset/FAssetManagerConfig.h"
#include "Asset/FAssetCookedEnvelopeAuthentication.h"
#include "Asset/FKTX2TextureArtifact.h"
#include "Core/SGPlatform.h"
#include "FBoundCookedGeneration.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

namespace
{
using namespace Stoner;
using namespace Stoner::Tests::AssetCookerPublication;

Core::usize CountEntries(const std::filesystem::path& Root)
{
    Core::usize Count = 0;
    std::error_code Error;
    for (std::filesystem::recursive_directory_iterator It(Root, Error), End;
         !Error && It != End; It.increment(Error))
        ++Count;
    return Count;
}

bool WaitTerminal(
    const Asset::FAssetManager& Manager,
    Asset::FAssetRequestHandle Request,
    Asset::FAssetRequestSnapshot& Out)
{
    using namespace std::chrono_literals;
    for (int Attempt = 0; Attempt < 400; ++Attempt)
    {
        if (Manager.Query(Request, Out) != Asset::EAssetResult::Success)
            return false;
        if (Out.State == Asset::EAssetRequestState::Ready ||
            Out.State == Asset::EAssetRequestState::Failed ||
            Out.State == Asset::EAssetRequestState::Cancelled)
            return true;
        std::this_thread::sleep_for(5ms);
    }
    return false;
}
} // namespace

FAssetManagerCookedTestResult RunAssetManagerCookedTests()
{
    using namespace Stoner;
    namespace CookerPrivate = AssetCooker::Private;
    FAssetManagerCookedTestResult Result;
    const auto Token = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    const auto Root = std::filesystem::temp_directory_path() /
        ("sg-runtime-cooked-" + std::to_string(Token));
    std::filesystem::path Content;
    const auto SeedRun = Seed(Root, Content);
    const auto Publication = Root / "Published";
    const auto Published = CookerPrivate::FCookedGenerationPublisher::Publish(
        Request(SeedRun, Publication));
    const auto Coordination = Root / "Coordination";
    std::filesystem::create_directories(Coordination);

    Asset::FAssetManagerConfig Config;
    Config.Mode = Asset::EAssetManagerMode::StrictCooked;
    Config.ExtensionRegistry = Core::MakeShared<Asset::FAssetExtensionRegistry>();
    Config.PublicationRoot = Core::FString(Publication.generic_string());
    Config.LeaseCoordinationRoot = Core::FString(Coordination.generic_string());
    Config.TargetEvidence = Core::MakeShared<const Asset::FAssetTargetProfileEvidence>(
        SeedRun.Result.Manifest.TargetProfile);

#if !SG_PLATFORM_WINDOWS
    std::filesystem::permissions(
        Publication,
        std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::replace);
#endif
    const Core::usize Before = CountEntries(Publication);
    Asset::Private::FBoundCookedGeneration Bound;
    Asset::FAssetDiagnosticList Diagnostics;
    const auto BoundResult =
        Asset::Private::FBoundCookedGeneration::Bind(Config, Bound, Diagnostics);
    Record(Result.Passed, Result.Failed,
        Published.Succeeded() && BoundResult == Asset::EAssetResult::Success &&
            Bound.IsBound() &&
            Bound.GetManifest().GenerationId ==
                SeedRun.Result.Manifest.GenerationId &&
            CountEntries(Publication) == Before,
        "cooked binding uses explicit coordination without modifying publication");

    const auto TextureRecord = std::find_if(
        SeedRun.Result.Manifest.Records.begin(),
        SeedRun.Result.Manifest.Records.end(),
        [](const auto& Value)
        {
            return Value.AssetType == Core::FString("Texture");
        });
    Core::TSharedPtr<Asset::FAssetManager> Manager;
    Diagnostics.clear();
    const auto ManagerResult = Asset::FAssetManager::Create(
        Config, Manager, Diagnostics);
    Asset::FAssetRequestHandle TextureRequest;
    const auto RequestResult =
        ManagerResult == Asset::EAssetResult::Success &&
            TextureRecord != SeedRun.Result.Manifest.Records.end()
        ? Manager->Request<Asset::FKTX2TextureArtifact>(
              TextureRecord->AssetId, TextureRequest)
        : Asset::EAssetResult::ProcessingFailure;
    Asset::FAssetRequestSnapshot TextureSnapshot;
    const bool TextureTerminal = RequestResult == Asset::EAssetResult::Success &&
        WaitTerminal(*Manager, TextureRequest, TextureSnapshot);
    Asset::TAssetHandle<Asset::FKTX2TextureArtifact> Texture;
    Record(Result.Passed, Result.Failed,
        TextureTerminal &&
            TextureSnapshot.State == Asset::EAssetRequestState::Ready &&
            Manager->GetResult(TextureRequest, Texture) ==
                Asset::EAssetResult::Success &&
            Texture.IsValid() && !Texture->GetBytes().empty(),
        "strict cooked manager loads KTX2 texture and required image closure");
    Texture.Reset();
    (void)Manager->ReleaseRequest(TextureRequest);

    const auto PayloadPath = Publication / "Generations" /
        SeedRun.Result.Manifest.GenerationId.ToLowerHex().ToStdString() /
        TextureRecord->PayloadLocator.ToStdString();
    const auto OriginalPayload = Read(PayloadPath);
    auto CorruptPayload = OriginalPayload;
    if (!CorruptPayload.empty()) CorruptPayload.back() ^= 0x01U;
    Write(PayloadPath, CorruptPayload);
    Asset::FAssetRequestHandle CorruptRequest;
    const auto CorruptAdmission =
        Manager->Request<Asset::FKTX2TextureArtifact>(
            TextureRecord->AssetId, CorruptRequest);
    Asset::FAssetRequestSnapshot CorruptSnapshot;
    const bool CorruptTerminal =
        CorruptAdmission == Asset::EAssetResult::Success &&
        WaitTerminal(*Manager, CorruptRequest, CorruptSnapshot);
    Record(Result.Passed, Result.Failed,
        CorruptTerminal &&
            CorruptSnapshot.State == Asset::EAssetRequestState::Failed &&
            CorruptSnapshot.Result == Asset::EAssetResult::CorruptPayload,
        "strict cooked request rejects same-size payload corruption");
    Write(PayloadPath, OriginalPayload);
    (void)Manager->Shutdown();

    Core::TSharedPtr<Asset::FAssetCookedEnvelopeAuthentication>
        WrongGenerationAuthentication;
    const auto WrongContextResult =
        Asset::FAssetCookedEnvelopeAuthentication::Create(
            Config.PublicationRoot,
            Config.LeaseCoordinationRoot,
            SeedRun.Result.Manifest.TargetProfile.EffectiveProfileDigest,
            Config.LeaseTimeoutMilliseconds,
            static_cast<Core::uint32>(
                SeedRun.Result.Manifest.Records.size()),
            WrongGenerationAuthentication);
    auto WrongGenerationConfig = Config;
    WrongGenerationConfig.CookedEnvelopeAuthentication =
        WrongGenerationAuthentication;
    Core::TSharedPtr<Asset::FAssetManager> WrongGenerationManager;
    Diagnostics.clear();
    Record(Result.Passed, Result.Failed,
        WrongContextResult == Asset::EAssetResult::Success &&
            WrongGenerationAuthentication &&
            Asset::FAssetManager::Create(
                WrongGenerationConfig,
                WrongGenerationManager,
                Diagnostics) == Asset::EAssetResult::InvalidInput &&
            !WrongGenerationManager,
        "strict authentication context rejects a different bound generation");

    Core::TSharedPtr<Asset::FAssetCookedEnvelopeAuthentication>
        Authentication;
    const bool AuthenticationCreated =
        Asset::FAssetCookedEnvelopeAuthentication::Create(
            Config.PublicationRoot,
            Config.LeaseCoordinationRoot,
            SeedRun.Result.Manifest.GenerationId,
            Config.LeaseTimeoutMilliseconds,
            static_cast<Core::uint32>(
                SeedRun.Result.Manifest.Records.size()),
            Authentication) == Asset::EAssetResult::Success;
    auto AuthenticatedConfig = Config;
    AuthenticatedConfig.CookedEnvelopeAuthentication = Authentication;
    Write(PayloadPath, CorruptPayload);
    Core::TSharedPtr<Asset::FAssetManager> FirstCorruptManager;
    Diagnostics.clear();
    const auto FirstCorruptCreate = Asset::FAssetManager::Create(
        AuthenticatedConfig, FirstCorruptManager, Diagnostics);
    Asset::FAssetRequestHandle FirstCorruptRequest;
    const auto FirstCorruptAdmission =
        FirstCorruptCreate == Asset::EAssetResult::Success
        ? FirstCorruptManager->Request<Asset::FKTX2TextureArtifact>(
              TextureRecord->AssetId, FirstCorruptRequest)
        : Asset::EAssetResult::ProcessingFailure;
    Asset::FAssetRequestSnapshot FirstCorruptSnapshot;
    const bool FirstCorruptTerminal =
        FirstCorruptAdmission == Asset::EAssetResult::Success &&
        WaitTerminal(
            *FirstCorruptManager,
            FirstCorruptRequest,
            FirstCorruptSnapshot);
    Record(Result.Passed, Result.Failed,
        AuthenticationCreated && FirstCorruptTerminal &&
            FirstCorruptSnapshot.State == Asset::EAssetRequestState::Failed &&
            FirstCorruptSnapshot.Result == Asset::EAssetResult::CorruptPayload &&
            !Authentication->CanReuse(TextureRecord->EnvelopeDigest),
        "first corrupted envelope fails authentication and is never trusted");
    (void)FirstCorruptManager->Shutdown();
    Write(PayloadPath, OriginalPayload);

    Core::TSharedPtr<Asset::FAssetManager> FirstAuthenticatedManager;
    Diagnostics.clear();
    const auto FirstAuthenticatedCreate = Asset::FAssetManager::Create(
        AuthenticatedConfig, FirstAuthenticatedManager, Diagnostics);
    Asset::FAssetRequestHandle FirstAuthenticatedRequest;
    const auto FirstAuthenticatedAdmission =
        FirstAuthenticatedCreate == Asset::EAssetResult::Success
        ? FirstAuthenticatedManager->Request<Asset::FKTX2TextureArtifact>(
              TextureRecord->AssetId, FirstAuthenticatedRequest)
        : Asset::EAssetResult::ProcessingFailure;
    Asset::FAssetRequestSnapshot FirstAuthenticatedSnapshot;
    const bool FirstAuthenticatedTerminal =
        FirstAuthenticatedAdmission == Asset::EAssetResult::Success &&
        WaitTerminal(
            *FirstAuthenticatedManager,
            FirstAuthenticatedRequest,
            FirstAuthenticatedSnapshot);
    (void)FirstAuthenticatedManager->Shutdown();
    const auto FirstAuthenticationInspection = Authentication->Inspect();
    Core::TSharedPtr<Asset::FAssetManager> ReusedAuthenticationManager;
    Diagnostics.clear();
    const auto ReusedAuthenticationCreate = Asset::FAssetManager::Create(
        AuthenticatedConfig, ReusedAuthenticationManager, Diagnostics);
    Asset::FAssetRequestHandle ReusedAuthenticationRequest;
    const auto ReusedAuthenticationAdmission =
        ReusedAuthenticationCreate == Asset::EAssetResult::Success
        ? ReusedAuthenticationManager->Request<Asset::FKTX2TextureArtifact>(
              TextureRecord->AssetId, ReusedAuthenticationRequest)
        : Asset::EAssetResult::ProcessingFailure;
    Asset::FAssetRequestSnapshot ReusedAuthenticationSnapshot;
    const bool ReusedAuthenticationTerminal =
        ReusedAuthenticationAdmission == Asset::EAssetResult::Success &&
        WaitTerminal(
            *ReusedAuthenticationManager,
            ReusedAuthenticationRequest,
            ReusedAuthenticationSnapshot);
    const auto ReusedAuthenticationInspection = Authentication->Inspect();
    Record(Result.Passed, Result.Failed,
        FirstAuthenticatedTerminal &&
            FirstAuthenticatedSnapshot.State ==
                Asset::EAssetRequestState::Ready &&
            ReusedAuthenticationTerminal &&
            ReusedAuthenticationSnapshot.State ==
                Asset::EAssetRequestState::Ready &&
            FirstAuthenticationInspection.AuthenticatedEnvelopeCount > 0 &&
            ReusedAuthenticationInspection.ReuseHits >
                FirstAuthenticationInspection.ReuseHits,
        "exact generation authentication is reused without retaining payloads");
    (void)ReusedAuthenticationManager->Shutdown();

#if !SG_PLATFORM_WINDOWS
    std::filesystem::permissions(
        Publication,
        std::filesystem::perms::owner_all,
        std::filesystem::perm_options::replace);
#endif
    const auto OriginalGeneration = Bound.GetPointer().GenerationId;
    Write(Publication / "Current.json", {'{', '}'});
    Record(Result.Passed, Result.Failed,
        Bound.IsBound() && Bound.GetPointer().GenerationId == OriginalGeneration,
        "bound generation remains stable after Current pointer replacement");

    Asset::FAssetManagerConfig MissingCoordination = Config;
    MissingCoordination.LeaseCoordinationRoot =
        Core::FString((Root / "MissingCoordination").generic_string());
    Asset::Private::FBoundCookedGeneration Rejected;
    Diagnostics.clear();
    Record(Result.Passed, Result.Failed,
        Asset::Private::FBoundCookedGeneration::Bind(
            MissingCoordination, Rejected, Diagnostics) !=
            Asset::EAssetResult::Success &&
            !Rejected.IsBound(),
        "failed startup rolls back reader ownership and bound state");

    auto RequiredProfile = SeedRun.Result.Manifest.TargetProfile.Profile;
    RequiredProfile.RequiredExtensions = {
        Core::FString("vendor.runtime-required")};
    Core::FString RequiredProfileText;
    Asset::FAssetTargetProfileEvidence RequiredEvidence;
    const bool RequiredProfileValid =
        Asset::FAssetCookContractCodec::WriteTargetProfile(
            RequiredProfile, RequiredProfileText, &RequiredEvidence) ==
        Asset::EAssetResult::Success;
    const auto RequiredProfilePath = Root / "required-profile.json";
    Write(RequiredProfilePath, Core::TArray<Core::uint8>(
        RequiredProfileText.View().begin(), RequiredProfileText.View().end()));
    auto RequiredCookRequest = Tests::AssetCookerDDC::Request(
        Root / "RequiredSeed", Content, 2);
    RequiredCookRequest.TargetProfilePath =
        Core::FString(RequiredProfilePath.generic_string());
    RequiredCookRequest.TargetProfile = RequiredEvidence;
    RequiredCookRequest.CachePolicy =
        AssetCooker::EAssetCookCachePolicy::IgnoreExisting;
    const auto RequiredRun = Tests::AssetCookerDDC::Run(RequiredCookRequest);
    const auto RequiredPublication = Root / "RequiredPublished";
    const auto RequiredPublished =
        CookerPrivate::FCookedGenerationPublisher::Publish(
            Request(RequiredRun, RequiredPublication));
    const auto RequiredCoordination = Root / "RequiredCoordination";
    std::filesystem::create_directories(RequiredCoordination);
    Asset::FAssetManagerConfig RequiredConfig;
    RequiredConfig.Mode = Asset::EAssetManagerMode::StrictCooked;
    RequiredConfig.ExtensionRegistry =
        Core::MakeShared<Asset::FAssetExtensionRegistry>();
    RequiredConfig.PublicationRoot =
        Core::FString(RequiredPublication.generic_string());
    RequiredConfig.LeaseCoordinationRoot =
        Core::FString(RequiredCoordination.generic_string());
    RequiredConfig.TargetEvidence =
        Core::MakeShared<const Asset::FAssetTargetProfileEvidence>(
            RequiredEvidence);
    Asset::Private::FBoundCookedGeneration MissingExtension;
    Diagnostics.clear();
    Record(Result.Passed, Result.Failed,
        RequiredProfileValid && RequiredRun.Result.Succeeded() &&
            RequiredPublished.Succeeded() &&
            Asset::Private::FBoundCookedGeneration::Bind(
                RequiredConfig, MissingExtension, Diagnostics) ==
                Asset::EAssetResult::UnknownRequiredExtension &&
            !MissingExtension.IsBound(),
        "strict cooked binding rejects an unavailable required runtime extension");

    Bound.Reset();
    std::error_code Error;
    std::filesystem::remove_all(Root, Error);
    return Result;
}
