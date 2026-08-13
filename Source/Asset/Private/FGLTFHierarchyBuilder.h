#pragma once

#include "Asset/FStaticModelAsset.h"
#include "Asset/FStaticModelImport.h"
#include "FGLTFPackageIdentityPlanner.h"

struct cgltf_data;

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult BuildGLTFSceneHierarchy(
    const cgltf_data& Data,
    Core::uint32 SceneIndex,
    const FGLTFPackageIdentityPlan& Identities,
    const FStaticModelImportProfile& Profile,
    Core::TArray<FStaticModelNode>& OutNodes,
    Core::TArray<Core::uint32>& OutRootNodeIndices);

} // namespace Stoner::Asset::Private
