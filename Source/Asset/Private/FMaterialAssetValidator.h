#pragma once

#include "Asset/FMaterialAsset.h"
#include "Asset/FMaterialInstanceAsset.h"

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult ValidateMaterialAsset(
    FMaterialAssetDesc& Desc,
    FAssetDiagnosticList* Diagnostics);
[[nodiscard]] EAssetResult ValidateMaterialInstanceAsset(
    FMaterialInstanceAssetDesc& Desc,
    FAssetDiagnosticList* Diagnostics);

} // namespace Stoner::Asset::Private
