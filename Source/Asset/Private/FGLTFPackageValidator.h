#pragma once

#include "Asset/IAssetImporter.h"
#include "FGLTFPackageIdentityPlanner.h"

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult ValidateGLTFPackageOutputs(
    const FGLTFPackageIdentityPlan& Identities,
    const Core::TArray<FAssetImportOutput>& Outputs,
    bool RequireMaterialPayloads = false);

} // namespace Stoner::Asset::Private
