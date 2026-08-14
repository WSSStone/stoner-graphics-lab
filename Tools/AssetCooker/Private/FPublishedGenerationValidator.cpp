#include "FPublishedGenerationValidator.h"

#include "Core/FPlatformFileSystem.h"
#include "Core/SGPlatform.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>
#include <span>
#include <string>

namespace Stoner::AssetCooker::Private
{
namespace
{

FPublishedGenerationValidationResult Fail(
    Asset::EAssetResult Result,
    EPublishedCorruptionCategory Category,
    const char* Reason)
{
    FPublishedGenerationValidationResult Out;
    Out.Result = Result;
    Out.Category = Category;
    Out.StableReason = Core::FString(Reason);
    return Out;
}

Core::FString Join(const Core::FString& Root, std::string_view Relative)
{
    return Core::FString((std::filesystem::path(Root.ToStdString()) /
        std::string(Relative)).lexically_normal().generic_string());
}

bool ReadBounded(
    const Core::FString& Path,
    Core::uint64 Maximum,
    Core::TArray<Core::uint8>& Out)
{
    Out.clear();
    Core::FPlatformFileInfo Info;
    if (!Core::FPlatformFileSystem::QueryRegularFile(
            Path, Maximum, Info).IsSuccess() ||
        !Core::FPlatformFileSystem::ReadFile(Path, Out) ||
        Out.size() != Info.ByteSize)
    {
        Out.clear();
        return false;
    }
    return true;
}

std::string Normalized(const std::filesystem::path& Path)
{
    std::error_code Error;
    const auto Canonical = std::filesystem::weakly_canonical(Path, Error);
    std::string Result;
    if (!Error) Result = Canonical.generic_string();
    else
    {
        Error.clear();
        const auto Absolute =
            std::filesystem::absolute(Path, Error).lexically_normal();
        Result = (Error ? Path.lexically_normal() : Absolute).generic_string();
    }
#if SG_PLATFORM_WINDOWS
    // Win32 paths are case-insensitive. Directory enumeration can preserve
    // different component casing than a path reconstructed from the manifest.
    std::transform(Result.begin(), Result.end(), Result.begin(),
        [](unsigned char Value)
        {
            return static_cast<char>(std::tolower(Value));
        });
#endif
    return Result;
}

} // namespace

FPublishedGenerationValidationResult FPublishedGenerationValidator::Validate(
    const FPublishedGenerationValidationRequest& Request)
{
    if (Request.SubjectRoot.IsEmpty() || Request.MaxFiles == 0 ||
        Request.MaxPathBytes == 0 ||
        (Request.Subject == EPublishedValidationSubject::GenerationDirectory &&
         !Request.ExpectedGenerationId.has_value()))
        return Fail(Asset::EAssetResult::InvalidInput,
            EPublishedCorruptionCategory::InvalidRequest,
            "published.validate.invalid-request");

    Asset::FCurrentGenerationPointer Pointer;
    Core::FString GenerationDirectory;
    Asset::FAssetDigest ExpectedGeneration;
    if (Request.Subject == EPublishedValidationSubject::CurrentPointer)
    {
        Core::TArray<Core::uint8> PointerBytes;
        if (!ReadBounded(Join(Request.SubjectRoot, "Current.json"),
                1024ULL * 1024ULL, PointerBytes))
            return Fail(Asset::EAssetResult::NotFound,
                EPublishedCorruptionCategory::PointerMissing,
                "published.pointer.missing");
        if (Asset::FAssetCookContractCodec::ParseCurrentPointer(
                PointerBytes, Pointer) != Asset::EAssetResult::Success)
            return Fail(Asset::EAssetResult::CorruptPayload,
                EPublishedCorruptionCategory::PointerInvalid,
                "published.pointer.invalid");
        ExpectedGeneration = Pointer.GenerationId;
        GenerationDirectory = Join(Request.SubjectRoot,
            "Generations/" + ExpectedGeneration.ToLowerHex().ToStdString());
    }
    else
    {
        ExpectedGeneration = *Request.ExpectedGenerationId;
        GenerationDirectory = Request.SubjectRoot;
    }
    if (!Core::FPlatformFileSystem::Exists(GenerationDirectory))
        return Fail(Asset::EAssetResult::NotFound,
            EPublishedCorruptionCategory::GenerationMissing,
            "published.generation.missing");

    Core::TArray<Core::uint8> ManifestBytes;
    if (!ReadBounded(Join(GenerationDirectory, "Manifest.json"),
            Request.ManifestLimits.MaxManifestBytes, ManifestBytes))
        return Fail(Asset::EAssetResult::NotFound,
            EPublishedCorruptionCategory::ManifestInvalid,
            "published.manifest.missing");
    Asset::FAssetCookManifest Manifest;
    if (Asset::FAssetCookContractCodec::ParseManifest(
            ManifestBytes, Request.ManifestLimits, Manifest) !=
            Asset::EAssetResult::Success)
        return Fail(Asset::EAssetResult::CorruptPayload,
            EPublishedCorruptionCategory::ManifestInvalid,
            "published.manifest.invalid");
    const auto ManifestDigest = Asset::FAssetDigest::FromBytes(ManifestBytes);
    if (Manifest.GenerationId != ExpectedGeneration)
        return Fail(Asset::EAssetResult::Conflict,
            EPublishedCorruptionCategory::GenerationMismatch,
            "published.generation.identity-mismatch");
    if (Request.Subject == EPublishedValidationSubject::CurrentPointer &&
        ManifestDigest != Pointer.ManifestDigest)
        return Fail(Asset::EAssetResult::Conflict,
            EPublishedCorruptionCategory::ManifestDigestMismatch,
            "published.manifest.digest-mismatch");

    std::set<std::string> ExpectedFiles{
        Normalized(std::filesystem::path(GenerationDirectory.ToStdString()) /
            "Manifest.json")};
    Core::uint32 ValidatedPayloads = 0;
    for (const auto& Record : Manifest.Records)
    {
        const std::string Digest = Record.EnvelopeDigest.ToLowerHex().ToStdString();
        const Core::FString ExpectedLocator(
            "Payloads/" + Digest.substr(0, 2) + "/" + Digest + ".sgasset");
        if (Record.PayloadLocator != ExpectedLocator)
            return Fail(Asset::EAssetResult::Conflict,
                EPublishedCorruptionCategory::PayloadMismatch,
                "published.payload.locator-mismatch");
        const Core::FString PayloadPath = Join(
            GenerationDirectory, Record.PayloadLocator.View());
        ExpectedFiles.insert(Normalized(PayloadPath.ToStdString()));
        Core::TArray<Core::uint8> PayloadBytes;
        if (!ReadBounded(PayloadPath,
                Request.PayloadLimits.MaxEnvelopeBytes, PayloadBytes))
            return Fail(Asset::EAssetResult::NotFound,
                EPublishedCorruptionCategory::PayloadMissing,
                "published.payload.missing");
        Core::TSharedPtr<const Asset::FAssetPayload> Payload;
        Asset::FAssetCookedPayloadEnvelope Envelope;
        if (PayloadBytes.size() != Record.PayloadBytes ||
            Asset::FAssetCookContractCodec::LoadTypedPayload(
                PayloadBytes, Request.PayloadLimits, Payload, &Envelope) !=
                Asset::EAssetResult::Success || !Payload ||
            Envelope.Header.AssetId != Record.AssetId ||
            Envelope.Header.AssetType != Record.AssetType ||
            Envelope.Header.CodecId != Record.Codec.Id.ToString() ||
            Envelope.Header.PayloadSchemaVersion != Record.PayloadSchemaVersion ||
            Envelope.EnvelopeDigest != Record.EnvelopeDigest)
            return Fail(Asset::EAssetResult::CorruptPayload,
                EPublishedCorruptionCategory::PayloadInvalid,
                "published.payload.invalid");
        ++ValidatedPayloads;
    }

    Core::FPlatformFileEnumerationOptions Options;
    Options.MaxFiles = Request.MaxFiles;
    Options.MaxDepth = 4;
    Options.MaxPathBytes = Request.MaxPathBytes;
    Core::TArray<Core::FPlatformFileInfo> Files;
    if (!Core::FPlatformFileSystem::EnumerateRegularFiles(
            GenerationDirectory, Options, Files).IsSuccess())
        return Fail(Asset::EAssetResult::AccessDenied,
            EPublishedCorruptionCategory::IoFailure,
            "published.generation.enumeration-failed");
    Core::uint32 UnexpectedFiles = 0;
    for (const auto& File : Files)
        if (!ExpectedFiles.contains(Normalized(File.Path.ToStdString())))
            ++UnexpectedFiles;
    if (Request.bRejectUnexpectedFiles && UnexpectedFiles != 0)
        return Fail(Asset::EAssetResult::Conflict,
            EPublishedCorruptionCategory::UnexpectedFile,
            "published.generation.unexpected-file");
    if (Files.size() != ExpectedFiles.size() + UnexpectedFiles)
        return Fail(Asset::EAssetResult::Conflict,
            EPublishedCorruptionCategory::PayloadMismatch,
            "published.generation.duplicate-or-ambiguous-files");

    FPublishedGenerationValidationResult Result;
    Result.Result = Asset::EAssetResult::Success;
    Result.Category = EPublishedCorruptionCategory::None;
    Result.StableReason = Core::FString("published.validate.success");
    Result.GenerationDirectory = GenerationDirectory;
    Result.Pointer = std::move(Pointer);
    Result.Manifest = std::move(Manifest);
    Result.ManifestDigest = ManifestDigest;
    Result.ValidatedPayloads = ValidatedPayloads;
    Result.UnexpectedFiles = UnexpectedFiles;
    return Result;
}

} // namespace Stoner::AssetCooker::Private
