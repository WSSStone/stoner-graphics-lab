#pragma once

#include "Asset/FAssetDerivedKey.h"

namespace Stoner::Asset
{

class FAssetDerivedKeyBuilder
{
public:
    [[nodiscard]] static EAssetResult Build(
        const FAssetDerivedKeyEvidence& Evidence,
        FAssetDerivedKey& OutKey);

    [[nodiscard]] static EAssetResult BuildCanonicalStreamForTesting(
        const FAssetDerivedKeyEvidence& Evidence,
        Core::TArray<Core::uint8>& OutBytes);
};

} // namespace Stoner::Asset
