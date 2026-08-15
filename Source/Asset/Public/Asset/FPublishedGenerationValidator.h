#pragma once

#include "Asset/EAssetResult.h"
#include "Asset/FAssetCookManifest.h"
#include "Asset/FAssetCookedPayload.h"
#include "Asset/FCurrentGenerationPointer.h"
#include "Core/FString.h"

#include <optional>

namespace Stoner::Asset
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
    std::optional<FAssetDigest> ExpectedGenerationId;
    bool bRejectUnexpectedFiles = true;
    FAssetCookManifestLimits ManifestLimits;
    FAssetCookedPayloadLimits PayloadLimits;
    Core::uint32 MaxFiles = 100002;
    Core::uint32 MaxPathBytes = 4096;
};

struct FPublishedGenerationValidationResult
{
    EAssetResult Result = EAssetResult::ProcessingFailure;
    EPublishedCorruptionCategory Category =
        EPublishedCorruptionCategory::InvalidRequest;
    Core::FString StableReason;
    Core::FString GenerationDirectory;
    FCurrentGenerationPointer Pointer;
    FAssetCookManifest Manifest;
    FAssetDigest ManifestDigest;
    Core::uint32 ValidatedPayloads = 0;
    Core::uint32 UnexpectedFiles = 0;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return Result == EAssetResult::Success;
    }
};

class FPublishedGenerationValidator
{
public:
    [[nodiscard]] static FPublishedGenerationValidationResult Validate(
        const FPublishedGenerationValidationRequest& Request);
};

} // namespace Stoner::Asset
