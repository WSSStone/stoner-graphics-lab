#pragma once

#include "Asset/AssetMinimal.h"
#include "AssetCooker/FAssetCookResult.h"
#include "Core/FString.h"

#include <chrono>
#include <functional>

namespace Stoner::AssetCooker::Private
{

enum class EPublicationBoundary : Core::uint8
{
    RequestImageValidation,
    LeaseAcquired,
    OutputStageCreate,
    PayloadCopy,
    ManifestCopy,
    StagedValidation,
    InputRevalidation,
    GenerationInstall,
    PointerWrite,
    PointerReplace,
    PostCommitAudit,
    Cleanup
};

struct FPublicationTestHooks
{
    std::function<bool(EPublicationBoundary)> ShouldFail;
};

struct FCookedGenerationPublicationRequest
{
    Core::FString RequestImageRoot;
    Core::FString OutputRoot;
    Asset::FAssetCookManifest Manifest;
    Core::FString CanonicalManifest;
    Asset::FAssetCookManifestLimits ManifestLimits;
    Asset::FAssetCookedPayloadLimits PayloadLimits;
    std::chrono::milliseconds LeaseTimeout{30000};
    std::function<Asset::EAssetResult()> RevalidateInputs;
    const FPublicationTestHooks* TestHooks = nullptr;
};

struct FCookedGenerationImageRequest
{
    Core::FString ScratchRoot;
    Asset::FAssetCookManifest Manifest;
    Core::FString CanonicalManifest;
    Core::TArray<FAssetCookArtifact> Artifacts;
    Asset::FAssetCookManifestLimits ManifestLimits;
    Asset::FAssetCookedPayloadLimits PayloadLimits;
};

struct FCookedGenerationImageResult
{
    Asset::EAssetResult Result = Asset::EAssetResult::ProcessingFailure;
    Core::FString StableReason;
    Core::FString ImageRoot;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return Result == Asset::EAssetResult::Success;
    }
};

struct FCookedGenerationPublicationResult
{
    EAssetCookResultCategory Category =
        EAssetCookResultCategory::PublicationFailure;
    Core::FString StableReason;
    Core::FString GenerationDirectory;
    bool bCommitted = false;
    bool bPostCommitAuditWarning = false;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return Category == EAssetCookResultCategory::Success;
    }
};

class FCookedGenerationPublisher
{
public:
    [[nodiscard]] static FCookedGenerationImageResult BuildRequestImage(
        const FCookedGenerationImageRequest& Request);
    [[nodiscard]] static FCookedGenerationPublicationResult Publish(
        const FCookedGenerationPublicationRequest& Request);
};

} // namespace Stoner::AssetCooker::Private
