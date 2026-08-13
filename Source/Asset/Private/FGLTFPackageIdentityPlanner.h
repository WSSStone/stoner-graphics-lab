#pragma once

#include "Asset/FAssetId.h"
#include "Asset/FStaticModelImport.h"

struct cgltf_data;

namespace Stoner::Asset::Private
{

struct FGLTFPackageIdentityPlan
{
    Core::FString LogicalPath;
    Core::TArray<Core::FString> MeshKeys;
    Core::TArray<FAssetId> MeshIds;
    Core::TArray<Core::FString> SceneKeys;
    Core::TArray<FAssetId> ModelIds;
    Core::TArray<Core::FString> MaterialKeys;
    Core::TArray<FAssetId> MaterialIds;
    Core::TArray<Core::FString> ImageKeys;
    Core::TArray<FAssetId> ImageIds;
    Core::TArray<Core::FString> TextureKeys;
    Core::TArray<FAssetId> TextureIds;
    FAssetId DefaultMaterialId;
};

[[nodiscard]] EAssetResult PlanGLTFPackageIdentities(
    const cgltf_data& Data,
    const Core::FString& LogicalPath,
    const FStaticModelImportProfile& Profile,
    FGLTFPackageIdentityPlan& OutPlan);

} // namespace Stoner::Asset::Private
