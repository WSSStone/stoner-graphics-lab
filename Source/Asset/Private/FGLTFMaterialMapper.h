#pragma once

#include "Asset/FGLTFMaterialMappingProfile.h"
#include "Asset/FMaterialAsset.h"
#include "FGLTFImageTextureBridge.h"
#include "FGLTFPackageIdentityPlanner.h"

struct cgltf_data;
struct cgltf_material;

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult MapGLTFMaterial(
    const cgltf_data& Data,
    const cgltf_material* Material,
    const FAssetId& MaterialId,
    const Core::TArray<FGLTFTextureVariant>& TextureVariants,
    const FGLTFMaterialMappingProfile& Mapping,
    Core::TSharedPtr<const FMaterialAsset>& OutMaterial,
    FAssetDiagnosticList* Diagnostics);

} // namespace Stoner::Asset::Private
