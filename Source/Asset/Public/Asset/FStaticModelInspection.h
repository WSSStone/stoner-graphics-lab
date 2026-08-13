#pragma once

#include "Asset/FStaticModelAsset.h"
#include "Asset/IAssetImporter.h"

namespace Stoner::Asset
{

class FStaticModelInspection
{
public:
    [[nodiscard]] static FAssetDigest ComputeHierarchyDigest(
        const FStaticModelAsset& Asset);
    [[nodiscard]] static Core::FString Format(const FStaticModelAsset& Asset);
    [[nodiscard]] static Core::FString FormatPackage(
        const Core::TArray<FAssetImportOutput>& Outputs);
};

} // namespace Stoner::Asset
