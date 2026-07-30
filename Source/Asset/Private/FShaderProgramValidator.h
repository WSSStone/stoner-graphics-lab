#pragma once

#include "Asset/FShaderAsset.h"

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult ValidateShaderProgram(
    FShaderAssetDesc& Desc,
    FAssetDiagnosticList* Diagnostics);

} // namespace Stoner::Asset::Private
