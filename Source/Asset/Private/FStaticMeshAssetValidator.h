#pragma once

#include "Asset/FStaticMeshAsset.h"

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult ValidateStaticMeshAsset(
    FStaticMeshAssetDesc& Desc,
    FAssetDiagnosticList* Diagnostics);

} // namespace Stoner::Asset::Private
