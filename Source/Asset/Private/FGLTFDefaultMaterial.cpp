#include "FGLTFMaterialMapper.h"

namespace Stoner::Asset::Private
{

EAssetResult BuildGLTFDefaultMaterial(
    const cgltf_data& Data,
    const FAssetId& MaterialId,
    const Core::TArray<FGLTFTextureVariant>& TextureVariants,
    const FGLTFMaterialMappingProfile& Mapping,
    Core::TSharedPtr<const FMaterialAsset>& OutMaterial,
    FAssetDiagnosticList* Diagnostics)
{
    return MapGLTFMaterial(
        Data, nullptr, MaterialId, TextureVariants,
        Mapping, OutMaterial, Diagnostics);
}

} // namespace Stoner::Asset::Private
