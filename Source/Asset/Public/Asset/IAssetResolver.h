#pragma once

#include "Asset/FAssetExtensionRegistry.h"
#include "Asset/FAssetSource.h"

namespace Stoner::Asset
{

struct FAssetResolveRequest
{
    FAssetSourceLocator Location;
};

struct FAssetResolveResult
{
    EAssetResult Result = EAssetResult::NotFound;
    FAssetSourceDescriptor Descriptor;
    FAssetSourceLease Source;
};

class IAssetResolver : public IAssetExtension
{
public:
    [[nodiscard]] virtual FAssetResolveResult Resolve(
        const FAssetResolveRequest& Request) = 0;
};

} // namespace Stoner::Asset
