#include "FCookedGenerationPublisher.h"

#include "Core/FPlatformFileLease.h"
#include "Core/FPlatformFileSystem.h"
#include "FPublishedGenerationValidator.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <span>
#include <string>

namespace Stoner::AssetCooker::Private
{
namespace
{

Core::FString Join(const Core::FString& Root, std::string_view Relative)
{
    return Core::FString((std::filesystem::path(Root.ToStdString()) /
        std::string(Relative)).lexically_normal().generic_string());
}

Core::TArray<Core::uint8> Bytes(std::string_view Text)
{
    return {Text.begin(), Text.end()};
}

bool Injected(
    const FCookedGenerationPublicationRequest& Request,
    EPublicationBoundary Boundary)
{
    return Request.TestHooks && Request.TestHooks->ShouldFail &&
        Request.TestHooks->ShouldFail(Boundary);
}

FCookedGenerationPublicationResult Fail(
    EAssetCookResultCategory Category,
    const char* Reason)
{
    FCookedGenerationPublicationResult Result;
    Result.Category = Category;
    Result.StableReason = Core::FString(Reason);
    return Result;
}

Core::FString UniqueStage(const Core::FString& StagingRoot)
{
    static std::atomic<Core::uint64> Sequence{0};
    const Core::uint64 Tick = static_cast<Core::uint64>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const Core::uint64 Token =
        Sequence.fetch_add(1, std::memory_order_relaxed);
    return Join(StagingRoot,
        "publish-" + std::to_string(Tick) + "-" + std::to_string(Token));
}

Core::FString UniqueRequestImage(const Core::FString& ScratchRoot)
{
    return UniqueStage(Join(ScratchRoot, "Requests"));
}

FPublishedGenerationValidationRequest ValidationRequest(
    const FCookedGenerationPublicationRequest& Request,
    const Core::FString& GenerationDirectory)
{
    FPublishedGenerationValidationRequest Validation;
    Validation.SubjectRoot = GenerationDirectory;
    Validation.Subject = EPublishedValidationSubject::GenerationDirectory;
    Validation.ExpectedGenerationId = Request.Manifest.GenerationId;
    Validation.ManifestLimits = Request.ManifestLimits;
    Validation.PayloadLimits = Request.PayloadLimits;
    return Validation;
}

bool CopyGenerationImage(
    const FCookedGenerationPublicationRequest& Request,
    const FPublishedGenerationValidationResult& Image,
    const Core::FString& Stage)
{
    if (!Core::FPlatformFileSystem::CreateDirectory(Stage)) return false;
    for (const auto& Record : Image.Manifest.Records)
    {
        if (Injected(Request, EPublicationBoundary::PayloadCopy)) return false;
        Core::TArray<Core::uint8> Payload;
        const Core::FString Source = Join(
            Request.RequestImageRoot, Record.PayloadLocator.View());
        const Core::FString Destination = Join(
            Stage, Record.PayloadLocator.View());
        if (!Core::FPlatformFileSystem::ReadFile(Source, Payload) ||
            !Core::FPlatformFileSystem::CreateDirectory(Core::FString(
                std::filesystem::path(Destination.ToStdString())
                    .parent_path().generic_string())) ||
            !Core::FPlatformFileSystem::WriteFileDurable(
                Destination, Payload).IsSuccess())
            return false;
    }
    if (Injected(Request, EPublicationBoundary::ManifestCopy)) return false;
    return Core::FPlatformFileSystem::WriteFileDurable(
        Join(Stage, "Manifest.json"),
        Bytes(Request.CanonicalManifest.View())).IsSuccess();
}

bool EqualInstalledGeneration(
    const FCookedGenerationPublicationRequest& Request,
    const Core::FString& Directory,
    const Asset::FAssetDigest& ManifestDigest)
{
    const auto Existing = FPublishedGenerationValidator::Validate(
        ValidationRequest(Request, Directory));
    return Existing.Result == Asset::EAssetResult::Success &&
        Existing.ManifestDigest == ManifestDigest;
}

} // namespace

FCookedGenerationImageResult FCookedGenerationPublisher::BuildRequestImage(
    const FCookedGenerationImageRequest& Request)
{
    FCookedGenerationImageResult Result;
    if (Request.ScratchRoot.IsEmpty() ||
        Request.Manifest.Validate(Request.ManifestLimits) !=
            Asset::EAssetResult::Success ||
        Request.CanonicalManifest.IsEmpty() || Request.Artifacts.empty())
    {
        Result.Result = Asset::EAssetResult::InvalidInput;
        Result.StableReason = Core::FString("generation-image.request.invalid");
        return Result;
    }
    const Core::FString ImageRoot = UniqueRequestImage(Request.ScratchRoot);
    const Core::FString PayloadRoot = Join(ImageRoot, "Payloads");
    if (!Core::FPlatformFileSystem::CreateDirectory(PayloadRoot))
    {
        Result.Result = Asset::EAssetResult::AccessDenied;
        Result.StableReason = Core::FString("generation-image.root.create-failed");
        return Result;
    }
    for (const auto& Artifact : Request.Artifacts)
    {
        const Core::FString Destination = Join(
            ImageRoot, Artifact.RelativeLocator.View());
        if (!Core::FPlatformFileSystem::CreateDirectory(Core::FString(
                std::filesystem::path(Destination.ToStdString())
                    .parent_path().generic_string())) ||
            !Core::FPlatformFileSystem::WriteFileDurable(
                Destination, Artifact.Bytes).IsSuccess())
        {
            (void)Core::FPlatformFileSystem::RemoveTreeContained(
                Request.ScratchRoot, ImageRoot);
            Result.Result = Asset::EAssetResult::AccessDenied;
            Result.StableReason = Core::FString("generation-image.payload.write-failed");
            return Result;
        }
    }
    if (!Core::FPlatformFileSystem::WriteFileDurable(
            Join(ImageRoot, "Manifest.json"),
            Bytes(Request.CanonicalManifest.View())).IsSuccess())
    {
        (void)Core::FPlatformFileSystem::RemoveTreeContained(
            Request.ScratchRoot, ImageRoot);
        Result.Result = Asset::EAssetResult::AccessDenied;
        Result.StableReason = Core::FString("generation-image.manifest.write-failed");
        return Result;
    }
    FCookedGenerationPublicationRequest PublicationView;
    PublicationView.RequestImageRoot = ImageRoot;
    PublicationView.Manifest = Request.Manifest;
    PublicationView.CanonicalManifest = Request.CanonicalManifest;
    PublicationView.ManifestLimits = Request.ManifestLimits;
    PublicationView.PayloadLimits = Request.PayloadLimits;
    auto Validation = ValidationRequest(PublicationView, ImageRoot);
    const auto Validated = FPublishedGenerationValidator::Validate(Validation);
    if (Validated.Result != Asset::EAssetResult::Success ||
        Validated.Manifest != Request.Manifest)
    {
        (void)Core::FPlatformFileSystem::RemoveTreeContained(
            Request.ScratchRoot, ImageRoot);
        Result.Result = Asset::EAssetResult::CorruptPayload;
        Result.StableReason = Core::FString("generation-image.validation.failed");
        return Result;
    }
    Result.Result = Asset::EAssetResult::Success;
    Result.StableReason = Core::FString("generation-image.success");
    Result.ImageRoot = ImageRoot;
    return Result;
}

FCookedGenerationPublicationResult FCookedGenerationPublisher::Publish(
    const FCookedGenerationPublicationRequest& Request)
{
    if (Request.RequestImageRoot.IsEmpty() || Request.OutputRoot.IsEmpty() ||
        Request.Manifest.Validate() != Asset::EAssetResult::Success ||
        Request.CanonicalManifest.IsEmpty() || !Request.RevalidateInputs ||
        Request.LeaseTimeout.count() < 0 ||
        Request.LeaseTimeout > std::chrono::minutes(10))
        return Fail(EAssetCookResultCategory::InvalidArguments,
            "publication.request.invalid");

    if (Injected(Request, EPublicationBoundary::RequestImageValidation))
        return Fail(EAssetCookResultCategory::PublishedValidationFailure,
            "publication.request-image.injected-failure");
    const auto Image = FPublishedGenerationValidator::Validate(
        ValidationRequest(Request, Request.RequestImageRoot));
    if (Image.Result != Asset::EAssetResult::Success)
    {
        auto Result = Fail(EAssetCookResultCategory::PublishedValidationFailure,
            "publication.request-image.standalone-invalid");
        Result.StableReason = Image.StableReason;
        return Result;
    }
    if (Image.Manifest != Request.Manifest)
        return Fail(EAssetCookResultCategory::PublishedValidationFailure,
            "publication.request-image.manifest-mismatch");
    if (Image.ManifestDigest != Asset::FAssetDigest::FromBytes(
            std::span<const Core::uint8>(
                reinterpret_cast<const Core::uint8*>(
                    Request.CanonicalManifest.View().data()),
                Request.CanonicalManifest.Len())))
        return Fail(EAssetCookResultCategory::PublishedValidationFailure,
            "publication.request-image.digest-mismatch");

    // The stable lease file requires an existing parent. Creating only the root
    // does not expose staging or current content.
    if (!Core::FPlatformFileSystem::CreateDirectory(Request.OutputRoot))
        return Fail(EAssetCookResultCategory::IoFailure,
            "publication.output-root.create-failed");
    Core::FPlatformFileLease Lease;
    const auto LeaseStatus = Core::FPlatformFileLease::Acquire(
        Join(Request.OutputRoot, ".publish.lock"),
        static_cast<Core::uint64>(Request.LeaseTimeout.count()),
        Core::FString("stoner.asset-cooker.publish"), Lease);
    if (!LeaseStatus.IsSuccess())
        return Fail(
            LeaseStatus.Result == Core::EPlatformFileResult::TimedOut
                ? EAssetCookResultCategory::LeaseTimeout
                : EAssetCookResultCategory::PublicationFailure,
            LeaseStatus.Result == Core::EPlatformFileResult::TimedOut
                ? "publication.lease.timeout"
                : "publication.lease.failed");
    if (Injected(Request, EPublicationBoundary::LeaseAcquired))
        return Fail(EAssetCookResultCategory::PublicationFailure,
            "publication.lease.injected-failure");

    const Core::FString GenerationsRoot = Join(Request.OutputRoot, "Generations");
    const Core::FString StagingRoot = Join(Request.OutputRoot, "Staging");
    if (!Core::FPlatformFileSystem::CreateDirectory(GenerationsRoot) ||
        !Core::FPlatformFileSystem::CreateDirectory(StagingRoot) ||
        Injected(Request, EPublicationBoundary::OutputStageCreate))
        return Fail(EAssetCookResultCategory::PublicationFailure,
            "publication.stage-root.failed");

    const Core::FString Stage = UniqueStage(StagingRoot);
    const auto Cleanup = [&Request, &StagingRoot, &Stage]()
    {
        if (!Core::FPlatformFileSystem::Exists(Stage)) return;
        if (Injected(Request, EPublicationBoundary::Cleanup)) return;
        (void)Core::FPlatformFileSystem::RemoveTreeContained(StagingRoot, Stage);
    };
    if (!CopyGenerationImage(Request, Image, Stage))
    {
        Cleanup();
        return Fail(EAssetCookResultCategory::PublicationFailure,
            "publication.stage-copy.failed");
    }
    if (Injected(Request, EPublicationBoundary::StagedValidation) ||
        FPublishedGenerationValidator::Validate(
            ValidationRequest(Request, Stage)).Result !=
            Asset::EAssetResult::Success)
    {
        Cleanup();
        return Fail(EAssetCookResultCategory::PublishedValidationFailure,
            "publication.stage.invalid");
    }
    if (Injected(Request, EPublicationBoundary::InputRevalidation) ||
        Request.RevalidateInputs() != Asset::EAssetResult::Success)
    {
        Cleanup();
        return Fail(EAssetCookResultCategory::SourceChanged,
            "publication.inputs.changed");
    }

    const Core::FString GenerationDirectory = Join(
        GenerationsRoot,
        Request.Manifest.GenerationId.ToLowerHex().ToStdString());
    const auto ManifestDigest = Image.ManifestDigest;
    if (Injected(Request, EPublicationBoundary::GenerationInstall))
    {
        Cleanup();
        return Fail(EAssetCookResultCategory::PublicationFailure,
            "publication.generation-install.injected-failure");
    }
    const auto Move = Core::FPlatformFileSystem::MoveDirectoryNoReplace(
        Stage, GenerationDirectory);
    if (!Move.IsSuccess())
    {
        Cleanup();
        if (Move.Result != Core::EPlatformFileResult::AlreadyExists ||
            !EqualInstalledGeneration(
                Request, GenerationDirectory, ManifestDigest))
            return Fail(EAssetCookResultCategory::PublicationFailure,
                "publication.generation-install.failed");
    }

    Asset::FCurrentGenerationPointer Pointer;
    Pointer.GenerationId = Request.Manifest.GenerationId;
    Pointer.ManifestLocator = Core::FString(
        "Generations/" + Pointer.GenerationId.ToLowerHex().ToStdString() +
        "/Manifest.json");
    Pointer.ManifestDigest = ManifestDigest;
    Core::FString CanonicalPointer;
    if (Asset::FAssetCookContractCodec::WriteCurrentPointer(
            Pointer, CanonicalPointer) != Asset::EAssetResult::Success ||
        Injected(Request, EPublicationBoundary::PointerWrite))
        return Fail(EAssetCookResultCategory::PublicationFailure,
            "publication.pointer.write-precondition-failed");
    const Core::FString NextPath = Join(Request.OutputRoot, "Current.next");
    const auto PointerBytes = Bytes(CanonicalPointer.View());
    if (!Core::FPlatformFileSystem::WriteFileDurable(
            NextPath, PointerBytes).IsSuccess())
        return Fail(EAssetCookResultCategory::PublicationFailure,
            "publication.pointer.write-failed");
    Core::TArray<Core::uint8> ReReadPointer;
    Asset::FCurrentGenerationPointer ParsedPointer;
    if (!Core::FPlatformFileSystem::ReadFile(NextPath, ReReadPointer) ||
        ReReadPointer != PointerBytes ||
        Asset::FAssetCookContractCodec::ParseCurrentPointer(
            ReReadPointer, ParsedPointer) != Asset::EAssetResult::Success ||
        ParsedPointer != Pointer)
        return Fail(EAssetCookResultCategory::PublicationFailure,
            "publication.pointer.reread-failed");
    if (Injected(Request, EPublicationBoundary::PointerReplace))
        return Fail(EAssetCookResultCategory::PublicationFailure,
            "publication.pointer.replace-injected-failure");
    if (!Core::FPlatformFileSystem::ReplaceFileAtomic(
            NextPath, Join(Request.OutputRoot, "Current.json")).IsSuccess())
        return Fail(EAssetCookResultCategory::PublicationFailure,
            "publication.pointer.replace-failed");

    FCookedGenerationPublicationResult Result;
    Result.Category = EAssetCookResultCategory::Success;
    Result.StableReason = Core::FString("publication.committed");
    Result.GenerationDirectory = GenerationDirectory;
    Result.bCommitted = true;
    const bool bInjectedAudit =
        Injected(Request, EPublicationBoundary::PostCommitAudit);
    const auto Audit = bInjectedAudit
        ? FPublishedGenerationValidationResult{}
        : FPublishedGenerationValidator::Validate([&Request]()
        {
            FPublishedGenerationValidationRequest Validation;
            Validation.SubjectRoot = Request.OutputRoot;
            Validation.Subject = EPublishedValidationSubject::CurrentPointer;
            Validation.ManifestLimits = Request.ManifestLimits;
            Validation.PayloadLimits = Request.PayloadLimits;
            return Validation;
        }());
    if (bInjectedAudit ||
        Audit.Result != Asset::EAssetResult::Success)
    {
        Result.bPostCommitAuditWarning = true;
        Result.StableReason = Core::FString(
            "publication.committed.audit-warning");
    }
    Cleanup();
    return Result;
}

} // namespace Stoner::AssetCooker::Private
