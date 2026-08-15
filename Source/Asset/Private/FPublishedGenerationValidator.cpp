#include "Asset/FPublishedGenerationValidator.h"

#include "Asset/FAssetCookContractCodec.h"
#include "Asset/FAssetPayload.h"
#include "Core/FPlatformFileSystem.h"
#include "Core/SGPlatform.h"
#include "Core/TArray.h"
#include "Core/TSharedPtr.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>
#include <string>

namespace Stoner::Asset
{
namespace
{

FPublishedGenerationValidationResult Fail(
    EAssetResult Result,
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
    Core::TArray<Core::uint8>& Out,
    Core::FPlatformFileInfo* OutInfo = nullptr)
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
    if (OutInfo) *OutInfo = Info;
    return true;
}

std::string Normalized(const std::filesystem::path& Path)
{
    std::error_code Error;
#if SG_PLATFORM_WINDOWS
    const auto Absolute =
        std::filesystem::absolute(Path, Error).lexically_normal();
    std::string Result =
        (Error ? Path.lexically_normal() : Absolute).generic_string();
    std::transform(Result.begin(), Result.end(), Result.begin(),
        [](unsigned char Value)
        {
            return static_cast<char>(std::tolower(Value));
        });
#else
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
#endif
    return Result;
}

bool CodecVersionMatches(
    const FAssetCookManifestRecord& Record,
    const FAssetCookedPayloadEnvelope& Envelope)
{
    return Record.Codec.Version.ToString() == Core::FString(
        std::to_string(Envelope.Header.CodecVersion));
}

} // namespace

FPublishedGenerationValidationResult FPublishedGenerationValidator::Validate(
    const FPublishedGenerationValidationRequest& Request)
{
    if (Request.SubjectRoot.IsEmpty() || Request.MaxFiles == 0 ||
        Request.MaxPathBytes == 0 ||
        (Request.Policy != EPublishedGenerationValidationPolicy::FullPayloads &&
         Request.Policy !=
             EPublishedGenerationValidationPolicy::IndexAndLayout) ||
        (Request.Subject == EPublishedValidationSubject::GenerationDirectory &&
         !Request.ExpectedGenerationId.has_value()))
        return Fail(EAssetResult::InvalidInput,
            EPublishedCorruptionCategory::InvalidRequest,
            "published.validate.invalid-request");

    FCurrentGenerationPointer Pointer;
    Core::FString GenerationDirectory;
    FAssetDigest ExpectedGeneration;
    if (Request.Subject == EPublishedValidationSubject::CurrentPointer)
    {
        Core::TArray<Core::uint8> PointerBytes;
        if (!ReadBounded(Join(Request.SubjectRoot, "Current.json"),
                1024ULL * 1024ULL, PointerBytes))
            return Fail(EAssetResult::NotFound,
                EPublishedCorruptionCategory::PointerMissing,
                "published.pointer.missing");
        if (FAssetCookContractCodec::ParseCurrentPointer(
                PointerBytes, Pointer) != EAssetResult::Success)
            return Fail(EAssetResult::CorruptPayload,
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
        return Fail(EAssetResult::NotFound,
            EPublishedCorruptionCategory::GenerationMissing,
            "published.generation.missing");

    Core::TArray<Core::uint8> ManifestBytes;
    Core::FPlatformFileInfo ManifestInfo;
    if (!ReadBounded(Join(GenerationDirectory, "Manifest.json"),
            Request.ManifestLimits.MaxManifestBytes, ManifestBytes,
            &ManifestInfo))
        return Fail(EAssetResult::NotFound,
            EPublishedCorruptionCategory::ManifestInvalid,
            "published.manifest.missing");
    FAssetCookManifest Manifest;
    if (FAssetCookContractCodec::ParseManifest(
            ManifestBytes, Request.ManifestLimits, Manifest) !=
            EAssetResult::Success)
        return Fail(EAssetResult::CorruptPayload,
            EPublishedCorruptionCategory::ManifestInvalid,
            "published.manifest.invalid");
    const auto ManifestDigest = FAssetDigest::FromBytes(ManifestBytes);
    if (Manifest.GenerationId != ExpectedGeneration)
        return Fail(EAssetResult::Conflict,
            EPublishedCorruptionCategory::GenerationMismatch,
            "published.generation.identity-mismatch");
    if (Request.Subject == EPublishedValidationSubject::CurrentPointer &&
        ManifestDigest != Pointer.ManifestDigest)
        return Fail(EAssetResult::Conflict,
            EPublishedCorruptionCategory::ManifestDigestMismatch,
            "published.manifest.digest-mismatch");

    const Core::FString CanonicalGenerationDirectory(
        std::filesystem::path(ManifestInfo.Path.ToStdString())
            .parent_path().generic_string());
    std::set<std::string> ExpectedFiles{
        Normalized(ManifestInfo.Path.ToStdString())};
    Core::uint32 ValidatedPayloads = 0;
    Core::uint32 IndexedPayloads = 0;
    for (const auto& Record : Manifest.Records)
    {
        const std::string Digest =
            Record.EnvelopeDigest.ToLowerHex().ToStdString();
        const Core::FString ExpectedLocator(
            "Payloads/" + Digest.substr(0, 2) + "/" + Digest + ".sgasset");
        if (Record.PayloadLocator != ExpectedLocator)
            return Fail(EAssetResult::Conflict,
                EPublishedCorruptionCategory::PayloadMismatch,
                "published.payload.locator-mismatch");
        const Core::FString PayloadPath = Join(
            GenerationDirectory, Record.PayloadLocator.View());
        ExpectedFiles.insert(Normalized(Join(
            CanonicalGenerationDirectory, Record.PayloadLocator.View())
                .ToStdString()));
        Core::TArray<Core::uint8> PayloadBytes;
        Core::FPlatformFileInfo PayloadInfo;
        const auto PayloadStatus = Core::FPlatformFileSystem::QueryRegularFile(
            PayloadPath, Request.PayloadLimits.MaxEnvelopeBytes, PayloadInfo);
        if (!PayloadStatus.IsSuccess())
            return Fail(EAssetResult::NotFound,
                EPublishedCorruptionCategory::PayloadMissing,
                "published.payload.missing");
        if (PayloadInfo.ByteSize != Record.PayloadBytes)
            return Fail(EAssetResult::CorruptPayload,
                EPublishedCorruptionCategory::PayloadInvalid,
                "published.payload.size-mismatch");
        ++IndexedPayloads;
        if (Request.Policy ==
            EPublishedGenerationValidationPolicy::IndexAndLayout)
            continue;
        if (!ReadBounded(PayloadPath,
                Request.PayloadLimits.MaxEnvelopeBytes, PayloadBytes))
            return Fail(EAssetResult::NotFound,
                EPublishedCorruptionCategory::PayloadMissing,
                "published.payload.missing");
        Core::TSharedPtr<const FAssetPayload> Payload;
        FAssetCookedPayloadEnvelope Envelope;
        if (PayloadBytes.size() != Record.PayloadBytes ||
            FAssetCookContractCodec::LoadTypedPayload(
                PayloadBytes, Request.PayloadLimits, Payload, &Envelope) !=
                EAssetResult::Success || !Payload ||
            Envelope.Header.AssetId != Record.AssetId ||
            Envelope.Header.AssetType != Record.AssetType ||
            Envelope.Header.CodecId != Record.Codec.Id.ToString() ||
            !CodecVersionMatches(Record, Envelope) ||
            Envelope.Header.PayloadSchemaVersion !=
                Record.PayloadSchemaVersion ||
            Envelope.EnvelopeDigest != Record.EnvelopeDigest)
            return Fail(EAssetResult::CorruptPayload,
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
        return Fail(EAssetResult::AccessDenied,
            EPublishedCorruptionCategory::IoFailure,
            "published.generation.enumeration-failed");
    Core::uint32 UnexpectedFiles = 0;
    for (const auto& File : Files)
        if (!ExpectedFiles.contains(Normalized(File.Path.ToStdString())))
            ++UnexpectedFiles;
    if (Request.bRejectUnexpectedFiles && UnexpectedFiles != 0)
        return Fail(EAssetResult::Conflict,
            EPublishedCorruptionCategory::UnexpectedFile,
            "published.generation.unexpected-file");
    if (Files.size() != ExpectedFiles.size() + UnexpectedFiles)
        return Fail(EAssetResult::Conflict,
            EPublishedCorruptionCategory::PayloadMismatch,
            "published.generation.duplicate-or-ambiguous-files");

    FPublishedGenerationValidationResult Result;
    Result.Result = EAssetResult::Success;
    Result.Category = EPublishedCorruptionCategory::None;
    Result.StableReason = Core::FString("published.validate.success");
    Result.GenerationDirectory = GenerationDirectory;
    Result.Pointer = std::move(Pointer);
    Result.Manifest = std::move(Manifest);
    Result.ManifestDigest = ManifestDigest;
    Result.ValidatedPayloads = ValidatedPayloads;
    Result.IndexedPayloads = IndexedPayloads;
    Result.UnexpectedFiles = UnexpectedFiles;
    return Result;
}

} // namespace Stoner::Asset
