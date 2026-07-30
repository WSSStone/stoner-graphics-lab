#pragma once

#include "FMaterialShaderJsonCodec.h"

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult ValidateMaterialShaderDefinition(
    FMaterialShaderDefinition& Definition,
    FAssetDiagnosticList* Diagnostics);

} // namespace Stoner::Asset::Private
