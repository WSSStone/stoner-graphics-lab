#pragma once

#include "Asset/FAssetExtensionRegistry.h"
#include "Asset/FAssetPayload.h"
#include "Asset/FAssetTargetProfile.h"
#include "Core/FString.h"
#include "Core/TArray.h"

namespace Stoner::Asset
{

enum class EAssetCookedFamily : Core::uint8
{
    ImageTexture,
    MaterialShader,
    StaticModel
};

struct FAssetCookedTargetDecision
{
    Core::FString Selection;
    bool bUsedFallback = false;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(
        const FAssetCookedTargetDecision&) const = default;
};

class FAssetCookedExtensionRegistrations
{
public:
    void Reset() noexcept;
    [[nodiscard]] bool IsComplete() const noexcept;

private:
    friend EAssetResult RegisterCookedAssetExtensions(
        FAssetExtensionRegistry&,
        FAssetCookedExtensionRegistrations&);
    Core::TArray<FAssetRegistrationToken> Tokens_;
};

[[nodiscard]] EAssetResult GetAssetCookedFamily(
    const Core::FString& AssetType,
    EAssetCookedFamily& OutFamily) noexcept;

[[nodiscard]] EAssetResult GetAssetCookedParticipant(
    EAssetCookedFamily Family,
    EAssetExtensionKind Kind,
    FAssetParticipantId& OutParticipant) noexcept;

[[nodiscard]] EAssetResult ResolveAssetCookedTargetDecision(
    EAssetCookedFamily Family,
    const FAssetPayload& Payload,
    const FAssetTargetProfile& Profile,
    FAssetCookedTargetDecision& OutDecision);

[[nodiscard]] EAssetResult RegisterCookedAssetExtensions(
    FAssetExtensionRegistry& Registry,
    FAssetCookedExtensionRegistrations& OutRegistrations);

} // namespace Stoner::Asset
