#pragma once

#include "FGLTFMaterialMapper.h"

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult BuildGLTFDefaultMaterial(
    const cgltf_data& Data,
    const FAssetId& MaterialId,
    const Core::TArray<FGLTFTextureVariant>& TextureVariants,
    const FGLTFMaterialMappingProfile& Mapping,
    Core::TSharedPtr<const FMaterialAsset>& OutMaterial,
    FAssetDiagnosticList* Diagnostics);

} // namespace Stoner::Asset::Private
