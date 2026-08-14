#pragma once

#include "AssetCooker/FAssetCookReport.h"
#include "AssetCooker/FAssetCookRequest.h"
#include "AssetCooker/FAssetCookResult.h"

namespace Stoner::AssetCooker
{

class FAssetCookRunner
{
public:
    [[nodiscard]] static FAssetCookResult Run(
        const FAssetCookRequest& Request,
        FAssetCookReport& OutReport);
};

} // namespace Stoner::AssetCooker
