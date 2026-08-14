#pragma once

#include "AssetCooker/FAssetCookRequest.h"
#include "FCookInputSnapshot.h"

#include <filesystem>

namespace Stoner::AssetCooker::Private
{

struct FDiscoveredAssetSource
{
    Asset::FAssetSourceDescriptor Descriptor;
    Core::FString NormalizedRelativePath;
    Asset::FAssetVersion SourceVersion;
    Core::TArray<Asset::FAssetImportOutput> Outputs;
};

struct FAssetSourceCatalogResult
{
    Core::TArray<FDiscoveredAssetSource> Sources;
    Core::TArray<Asset::FAssetImportOutput> Outputs;
    Core::TArray<FCookInputSource> SnapshotSources;
    FCookInputResolve Revalidate;
};

class FAssetSourceCatalog
{
public:
    [[nodiscard]] static Asset::EAssetResult Discover(
        const FAssetCookRequest& Request,
        FAssetSourceCatalogResult& OutCatalog);
};

} // namespace Stoner::AssetCooker::Private
