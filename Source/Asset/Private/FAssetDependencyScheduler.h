#pragma once

#include "FAssetRuntimeTypes.h"

namespace Stoner::Asset::Private
{

class IAssetLoadingStrategy;
class FAssetNodeLoadCoordinator;
class FAssetRuntimeCache;

class FAssetDependencyScheduler
{
public:
    [[nodiscard]] static FAssetLoadScratchResult LoadClosure(
        const FAssetLoadKey& Root,
        IAssetLoadingStrategy& Strategy,
        const FAssetRuntimeExecutionContext& Context,
        const FAssetManagerLimits& Limits,
        FAssetNodeLoadCoordinator* Coordinator = nullptr,
        const FAssetRuntimeCache* Cache = nullptr);
};

} // namespace Stoner::Asset::Private
