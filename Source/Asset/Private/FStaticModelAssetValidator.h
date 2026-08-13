#pragma once

#include "Asset/FStaticModelAsset.h"

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult ValidateStaticModelAsset(
    FStaticModelAssetDesc& Desc,
    Core::uint32 MaximumHierarchyDepth,
    FAssetDiagnosticList* Diagnostics);

} // namespace Stoner::Asset::Private
