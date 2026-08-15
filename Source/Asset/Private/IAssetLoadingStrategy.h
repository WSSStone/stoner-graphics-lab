#pragma once

#include "FAssetRuntimeTypes.h"

namespace Stoner::Asset::Private
{

class IAssetLoadingStrategy
{
public:
    virtual ~IAssetLoadingStrategy() = default;
    [[nodiscard]] virtual FAssetLoadScratchResult Load(
        const FAssetLoadKey& Key,
        const FAssetRuntimeExecutionContext& Context) = 0;
};

} // namespace Stoner::Asset::Private
