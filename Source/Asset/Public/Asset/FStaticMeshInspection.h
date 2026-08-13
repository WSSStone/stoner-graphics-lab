#pragma once

#include "Asset/FAssetDigest.h"
#include "Asset/FStaticMeshAsset.h"

namespace Stoner::Asset
{

class FStaticMeshInspection
{
public:
    [[nodiscard]] static FAssetDigest ComputeGeometryDigest(
        const FStaticMeshAsset& Asset);
    [[nodiscard]] static Core::FString Format(const FStaticMeshAsset& Asset);
};

} // namespace Stoner::Asset
