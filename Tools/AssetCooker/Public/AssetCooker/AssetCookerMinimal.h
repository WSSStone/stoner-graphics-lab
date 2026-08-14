#pragma once

#include "Core/FString.h"
#include "AssetCooker/FAssetCookReport.h"
#include "AssetCooker/FAssetCookRequest.h"
#include "AssetCooker/FAssetCookResult.h"
#include "AssetCooker/FAssetCookRunner.h"

namespace Stoner::AssetCooker
{

[[nodiscard]] Core::FString GetAssetCookerName();

} // namespace Stoner::AssetCooker
