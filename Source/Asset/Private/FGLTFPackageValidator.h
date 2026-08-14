#pragma once

#include "Asset/IAssetImporter.h"
#include "FGLTFPackageIdentityPlanner.h"

#include <span>

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult ValidateGLTFPackageOutputs(
    const FGLTFPackageIdentityPlan& Identities,
    const Core::TArray<FAssetImportOutput>& Outputs,
    bool RequireMaterialPayloads = false,
    std::span<const FAssetId> ExpectedTextureIds = {});

} // namespace Stoner::Asset::Private
