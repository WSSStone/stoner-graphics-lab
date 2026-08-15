#pragma once

#include "Asset/FAssetManagerConfig.h"
#include "IAssetLoadingStrategy.h"

namespace Stoner::Asset::Private
{

class FDevelopmentAssetLoadingStrategy final : public IAssetLoadingStrategy
{
public:
    explicit FDevelopmentAssetLoadingStrategy(FAssetManagerConfig Config);
    [[nodiscard]] FAssetLoadScratchResult Load(
        const FAssetLoadKey& Key,
        const FAssetRuntimeExecutionContext& Context) override;

private:
    FAssetManagerConfig Config_;
};

} // namespace Stoner::Asset::Private
