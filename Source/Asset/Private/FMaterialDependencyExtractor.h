#pragma once

#include "Asset/FMaterialAsset.h"
#include "Asset/FMaterialInstanceAsset.h"
#include "Asset/FShaderAsset.h"

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult ExtractMaterialDependencies(
    FMaterialAssetDesc& Desc);
[[nodiscard]] EAssetResult ExtractMaterialInstanceDependencies(
    FMaterialInstanceAssetDesc& Desc);
[[nodiscard]] EAssetResult ExtractShaderDependencies(
    FShaderAssetDesc& Desc);

} // namespace Stoner::Asset::Private
