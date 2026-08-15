#pragma once

#include "Asset/FAssetManagerConfig.h"
#include "FBoundCookedGeneration.h"
#include "IAssetLoadingStrategy.h"

namespace Stoner::Asset::Private
{

class FCookedAssetLoadingStrategy final : public IAssetLoadingStrategy
{
public:
    FCookedAssetLoadingStrategy(
        FAssetManagerConfig Config,
        const FBoundCookedGeneration& Generation);
    [[nodiscard]] FAssetLoadScratchResult Load(
        const FAssetLoadKey& Key,
        const FAssetRuntimeExecutionContext& Context) override;

private:
    FAssetManagerConfig Config_;
    const FBoundCookedGeneration& Generation_;
};

} // namespace Stoner::Asset::Private
