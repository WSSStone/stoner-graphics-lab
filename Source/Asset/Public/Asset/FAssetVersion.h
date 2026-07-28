#pragma once

#include "Asset/FAssetDigest.h"
#include "Asset/FAssetParticipant.h"

#include <optional>

namespace Stoner::Asset
{

struct FAssetVersion
{
    FAssetDigest SourceDigest;
    FAssetDigest ContentDigest;
    FAssetDigest CookDigest;
    FAssetParticipantId Producer;
    FAssetProducerVersion ProducerVersion;
    std::optional<Core::FString> TargetProfile;

    [[nodiscard]] EAssetResult Validate() const noexcept
    {
        if (CookDigest.IsAvailable() &&
            (!Producer.IsValid() || !ProducerVersion.IsValid() ||
             !TargetProfile.has_value() || TargetProfile->IsEmpty()))
        {
            return EAssetResult::InvalidInput;
        }
        return EAssetResult::Success;
    }

    [[nodiscard]] bool operator==(const FAssetVersion&) const = default;
};

} // namespace Stoner::Asset
