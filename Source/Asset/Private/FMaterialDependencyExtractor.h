#pragma once

#include "Asset/FMaterialAsset.h"
#include "Asset/FMaterialInstanceAsset.h"

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult ExtractMaterialDependencies(
    FMaterialAssetDesc& Desc);
[[nodiscard]] EAssetResult ExtractMaterialInstanceDependencies(
    FMaterialInstanceAssetDesc& Desc);

} // namespace Stoner::Asset::Private
