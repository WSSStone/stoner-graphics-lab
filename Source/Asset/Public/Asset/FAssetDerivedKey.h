#pragma once

#include "Asset/FAssetDependency.h"
#include "Asset/FAssetDigest.h"
#include "Asset/FAssetId.h"
#include "Asset/FAssetParticipant.h"
#include "Asset/FAssetSource.h"
#include "Asset/FAssetVersion.h"
#include "Core/FPlatformTypes.h"
#include "Core/FString.h"
#include "Core/TArray.h"

namespace Stoner::Asset
{

struct FAssetDerivedSourceEvidence
{
    FAssetSourceLocator Locator;
    FAssetDigest Version;

    [[nodiscard]] bool operator==(const FAssetDerivedSourceEvidence&) const = default;
};

struct FAssetDerivedDependencyEvidence
{
    FAssetId Id;
    FAssetVersion Version;
    EAssetDependencyRole Role = EAssetDependencyRole::Build;

    [[nodiscard]] bool operator==(const FAssetDerivedDependencyEvidence&) const = default;
};

struct FAssetDerivedKeyEvidence
{
    static constexpr Core::uint32 CurrentKeyFormatVersion = 1;

    Core::uint32 KeyFormatVersion = CurrentKeyFormatVersion;
    FAssetId AssetId;
    FAssetDigest SourceVersion;
    Core::TArray<FAssetDerivedSourceEvidence> SourceManifest;
    Core::TArray<FAssetDerivedDependencyEvidence> Dependencies;
    FAssetParticipantId ImporterId;
    FAssetProducerVersion ImporterVersion;
    FAssetParticipantId CookerId;
    FAssetProducerVersion CookerVersion;
    FAssetParticipantId CodecId;
    FAssetProducerVersion CodecVersion;
    Core::uint32 PayloadSchemaVersion = 0;
    FAssetDigest EffectiveSettingsDigest;
    FAssetDigest RelevantProfileDigest;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(const FAssetDerivedKeyEvidence&) const = default;
};

class FAssetDerivedKey
{
public:
    FAssetDerivedKey() = default;

    [[nodiscard]] static EAssetResult ParseLowerHex(
        const Core::FString& Text,
        FAssetDerivedKey& OutKey) noexcept;

    [[nodiscard]] bool IsValid() const noexcept { return Digest_.IsAvailable(); }
    [[nodiscard]] const FAssetDigest& GetDigest() const noexcept { return Digest_; }
    [[nodiscard]] Core::FString ToString() const { return Digest_.ToLowerHex(); }
    [[nodiscard]] bool operator==(const FAssetDerivedKey&) const = default;

private:
    explicit FAssetDerivedKey(FAssetDigest Digest)
        : Digest_(std::move(Digest))
    {
    }

    FAssetDigest Digest_;

    friend class FAssetDerivedKeyBuilder;
};

} // namespace Stoner::Asset
