#pragma once

#include "Asset/FAssetExtensionRegistry.h"
#include "Asset/FAssetDiagnostics.h"
#include "Asset/FAssetMetadata.h"
#include "Asset/FAssetPayload.h"
#include "Asset/FAssetTargetProfile.h"

#include <utility>

namespace Stoner::Asset
{

class FAssetCookParameters
{
public:
    virtual ~FAssetCookParameters() = default;
};

struct FAssetCookRequest
{
    FAssetMetadata Metadata;
    Core::TSharedPtr<const FAssetPayload> Payload;
    Core::FString TargetProfile;
    Core::TSharedPtr<const FAssetCookParameters> Parameters;
    Core::TSharedPtr<const FAssetTargetProfileEvidence> TargetProfileEvidence;
};

struct FAssetCookResult
{
    FAssetCookResult() = default;
    FAssetCookResult(
        EAssetResult InResult,
        Core::FString InTargetProfile,
        Core::TArray<Core::uint8> InArtifact,
        FAssetDigest InCookDigest,
        Core::TSharedPtr<const FAssetPayload> InPayload,
        FAssetDiagnosticList InDiagnostics)
        : Result(InResult),
          TargetProfile(std::move(InTargetProfile)),
          Artifact(std::move(InArtifact)),
          CookDigest(std::move(InCookDigest)),
          Payload(std::move(InPayload)),
          Diagnostics(std::move(InDiagnostics))
    {
    }

    EAssetResult Result = EAssetResult::Unsupported;
    Core::FString TargetProfile;
    Core::TArray<Core::uint8> Artifact;
    FAssetDigest CookDigest;
    Core::TSharedPtr<const FAssetPayload> Payload;
    FAssetDiagnosticList Diagnostics;
    Core::TSharedPtr<const FAssetTargetProfileEvidence> TargetProfileEvidence;
    FAssetProfileProjectionEvidence ProfileProjection;
};

class IAssetCooker : public IAssetExtension
{
public:
    [[nodiscard]] virtual EAssetResult GetRelevantProfileEvidence(
        const FAssetTargetProfileEvidence& Profile,
        FAssetProfileProjectionEvidence& OutEvidence) const
    {
        (void)Profile;
        OutEvidence = {};
        return EAssetResult::Unsupported;
    }

    [[nodiscard]] virtual FAssetCookResult Cook(const FAssetCookRequest& Request) = 0;
};

} // namespace Stoner::Asset
