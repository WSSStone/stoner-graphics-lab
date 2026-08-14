#pragma once

#include "Asset/IAssetImporter.h"
#include "FGLTFPackageIdentityPlanner.h"

#include <span>

struct cgltf_data;

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult ValidateGLTFStaticPackageSupport(
    const cgltf_data& Data);

[[nodiscard]] EAssetResult ValidateGLTFPackageOutputs(
    const FGLTFPackageIdentityPlan& Identities,
    const Core::TArray<FAssetImportOutput>& Outputs,
    bool RequireMaterialPayloads = false,
    std::span<const FAssetId> ExpectedTextureIds = {});

} // namespace Stoner::Asset::Private
