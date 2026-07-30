#pragma once

#include "Asset/FMaterialInstanceAsset.h"
#include "Renderer/FMaterial.h"
#include "Renderer/FMaterialResourceRequirement.h"
#include "Renderer/FShaderAssetConversion.h"

namespace Stoner::Renderer
{

struct FMaterialAssetConversionRequest
{
    const Asset::FResolvedMaterialAsset* ResolvedMaterial = nullptr;
    const FShaderAssetSnapshot* Shader = nullptr;
};

struct FMaterialAssetSnapshot
{
    Core::TArray<Asset::FAssetSourceVersionRecord> SourceManifest;
    FMaterial Material;
    Core::TArray<FMaterialResourceRequirement> ResourceRequirements;
};

[[nodiscard]] EMaterialResult ConvertMaterialAsset(
    const FMaterialAssetConversionRequest& Request,
    FMaterialAssetSnapshot& OutSnapshot,
    FMaterialDiagnosticLog* Diagnostics = nullptr);

} // namespace Stoner::Renderer
