#pragma once

#include "Asset/FMaterialInstanceAsset.h"

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult ResolveMaterialInternal(
    const FAssetId& MaterialOrInstance,
    const IMaterialAssetLookup& Lookup,
    const FMaterialShaderAssetLimits& Limits,
    FResolvedMaterialAsset& OutMaterial,
    FAssetDiagnosticList* Diagnostics);

} // namespace Stoner::Asset::Private
