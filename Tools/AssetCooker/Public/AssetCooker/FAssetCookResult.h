#pragma once

#include "Asset/FAssetCookManifest.h"
#include "Core/FPlatformTypes.h"
#include "Core/FString.h"
#include "Core/TArray.h"

namespace Stoner::AssetCooker
{

enum class EAssetCookResultCategory : Core::uint8
{
    Success,
    InvalidArguments,
    InvalidProfile,
    DiscoveryFailure,
    GraphFailure,
    CookFailure,
    CacheFailure,
    SourceChanged,
    LeaseTimeout,
    PublishedValidationFailure,
    PublicationFailure,
    IoFailure,
    InternalFailure
};

struct FAssetCookArtifact
{
    Asset::FAssetId AssetId;
    Core::FString RelativeLocator;
    Asset::FAssetDigest EnvelopeDigest;
    Core::TArray<Core::uint8> Bytes;
};

struct FAssetCookResult
{
    EAssetCookResultCategory Category =
        EAssetCookResultCategory::InternalFailure;
    Core::FString StableReason;
    Core::FString GenerationImageRoot;
    Asset::FAssetCookManifest Manifest;
    Core::FString CanonicalManifest;
    Core::TArray<FAssetCookArtifact> Artifacts;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return Category == EAssetCookResultCategory::Success;
    }
};

} // namespace Stoner::AssetCooker
