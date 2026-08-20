#pragma once

#include "Asset/EAssetResult.h"
#include "Asset/FAssetDigest.h"
#include "Asset/FAssetParticipant.h"
#include "Core/FString.h"
#include "Core/TArray.h"

namespace Stoner::Asset
{

struct FShaderNativeLibraryEvidence
{
    FAssetDigest DerivationEvidenceDigest;
    Core::FString TargetProfile;
    Core::FString Architecture;
    Core::FString Compiler;
    Core::FString XcodeBuild;
    Core::FString Sdk;
    Core::FString DeploymentTarget;
    Core::FString LanguageVersion;
    FAssetDigest ArgumentDigest;
    FAssetDigest LibraryDigest;
    Core::uint64 SizeBytes = 0;
    FAssetParticipantId Finalizer;
    FAssetProducerVersion FinalizerVersion;
    FAssetDigest CanonicalDigest;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(
        const FShaderNativeLibraryEvidence&) const = default;
};

[[nodiscard]] EAssetResult FinalizeShaderNativeLibraryEvidence(
    FShaderNativeLibraryEvidence& Evidence) noexcept;

} // namespace Stoner::Asset
