#pragma once

#include "Asset/FAssetDependency.h"
#include "Asset/FAssetSource.h"
#include "Asset/FAssetVersion.h"
#include "Core/TArray.h"

#include <utility>

namespace Stoner::Asset
{

using FAssetAttribute = std::pair<Core::FString, Core::FString>;

struct FAssetMetadata
{
    FAssetId Id;
    FAssetVersion Version;
    FAssetSourceLocator Source;
    FAssetParticipantId Producer;
    FAssetProducerVersion ProducerVersion;
    Core::TArray<FAssetAttribute> Attributes;
    Core::TArray<FAssetDependency> Dependencies;

    [[nodiscard]] EAssetResult Validate() const;
    [[nodiscard]] bool IsCanonicallyEquivalent(const FAssetMetadata& Other) const;
};

} // namespace Stoner::Asset
