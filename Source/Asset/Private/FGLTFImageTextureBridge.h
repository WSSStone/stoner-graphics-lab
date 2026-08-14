#pragma once

#include "Asset/FStaticModelImport.h"
#include "Asset/FImageTypes.h"
#include "FGLTFPackageIdentityPlanner.h"

#include <functional>

struct cgltf_buffer_view;
struct cgltf_data;
struct cgltf_texture;

namespace Stoner::Asset::Private
{

struct FGLTFTextureVariant
{
    Core::uint32 SourceTextureIndex = 0;
    ETextureSemantic Semantic = ETextureSemantic::Data;
    FAssetId TextureId;
};

using FGLTFBufferViewReader = std::function<EAssetResult(
    const cgltf_buffer_view*, Core::TArray<Core::uint8>&)>;

[[nodiscard]] EAssetResult PlanGLTFTextureVariants(
    const cgltf_data& Data,
    const FGLTFPackageIdentityPlan& Identities,
    Core::TArray<FGLTFTextureVariant>& OutVariants);

[[nodiscard]] const FAssetId* FindGLTFTextureVariant(
    const Core::TArray<FGLTFTextureVariant>& Variants,
    const cgltf_data& Data,
    const cgltf_texture* Texture,
    ETextureSemantic Semantic);

[[nodiscard]] EAssetResult BuildGLTFImageTextureOutputs(
    const cgltf_data& Data,
    const FGLTFPackageIdentityPlan& Identities,
    const Core::TArray<FGLTFTextureVariant>& Variants,
    const FAssetImportRequest& MainRequest,
    const Core::TSharedPtr<IAssetResolver>& Resolver,
    const FStaticModelImportProfile& Profile,
    const FGLTFBufferViewReader& ReadBufferView,
    Core::TArray<FAssetImportOutput>& OutOutputs,
    FAssetDiagnosticList* Diagnostics);

} // namespace Stoner::Asset::Private
