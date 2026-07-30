#pragma once

#include "Asset/FMaterialShaderSourceLoader.h"
#include "Asset/FShaderAsset.h"

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult LoadShaderDependencies(
    const FMaterialShaderLoadRequest& Request,
    FShaderAssetDesc& Desc,
    Core::TArray<Core::TSharedPtr<const FAssetPayload>>& OutPayloads,
    Core::TArray<FAssetMetadata>& OutMetadata,
    FAssetDiagnosticList* Diagnostics);

} // namespace Stoner::Asset::Private
