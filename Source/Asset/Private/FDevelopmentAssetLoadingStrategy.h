#pragma once

#include "Asset/FAssetManagerConfig.h"
#include "IAssetLoadingStrategy.h"

namespace Stoner::Asset::Private
{

class FDevelopmentAssetLoadingStrategy final : public IAssetLoadingStrategy
{
public:
    FDevelopmentAssetLoadingStrategy(
        FAssetManagerConfig Config,
        Core::TSharedPtr<FAssetManagerExecutionCounterState> Counters);
    [[nodiscard]] FAssetLoadScratchResult Load(
        const FAssetLoadKey& Key,
        const FAssetRuntimeExecutionContext& Context) override;

private:
    FAssetManagerConfig Config_;
    Core::TSharedPtr<FAssetManagerExecutionCounterState> Counters_;
};

} // namespace Stoner::Asset::Private
