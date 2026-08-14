#pragma once

#include "Asset/AssetMinimal.h"
#include "Core/FString.h"

#include <optional>

namespace Stoner::AssetCooker::Private
{

enum class EPublishedValidationSubject : Core::uint8
{
    CurrentPointer,
    GenerationDirectory
};

enum class EPublishedCorruptionCategory : Core::uint8
{
    None,
    InvalidRequest,
    PointerMissing,
    PointerInvalid,
    GenerationMissing,
    ManifestInvalid,
    ManifestDigestMismatch,
    GenerationMismatch,
    PayloadMissing,
    PayloadInvalid,
    PayloadMismatch,
    UnexpectedFile,
    IoFailure
};

struct FPublishedGenerationValidationRequest
{
    Core::FString SubjectRoot;
    EPublishedValidationSubject Subject =
        EPublishedValidationSubject::CurrentPointer;
    std::optional<Asset::FAssetDigest> ExpectedGenerationId;
    bool bRejectUnexpectedFiles = true;
    Asset::FAssetCookManifestLimits ManifestLimits;
    Asset::FAssetCookedPayloadLimits PayloadLimits;
    Core::uint32 MaxFiles = 100002;
    Core::uint32 MaxPathBytes = 4096;
};

struct FPublishedGenerationValidationResult
{
    Asset::EAssetResult Result = Asset::EAssetResult::ProcessingFailure;
    EPublishedCorruptionCategory Category =
        EPublishedCorruptionCategory::InvalidRequest;
    Core::FString StableReason;
    Core::FString GenerationDirectory;
    Asset::FCurrentGenerationPointer Pointer;
    Asset::FAssetCookManifest Manifest;
    Asset::FAssetDigest ManifestDigest;
    Core::uint32 ValidatedPayloads = 0;
    Core::uint32 UnexpectedFiles = 0;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return Result == Asset::EAssetResult::Success;
    }
};

class FPublishedGenerationValidator
{
public:
    [[nodiscard]] static FPublishedGenerationValidationResult Validate(
        const FPublishedGenerationValidationRequest& Request);
};

} // namespace Stoner::AssetCooker::Private
