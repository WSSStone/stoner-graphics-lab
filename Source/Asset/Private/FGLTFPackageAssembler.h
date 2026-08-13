#pragma once

#include "Asset/FAssetParticipant.h"
#include "Asset/FStaticModelAsset.h"
#include "FGLTFPackageIdentityPlanner.h"

struct cgltf_data;

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult AssembleGLTFModels(
    const cgltf_data& Data,
    const FGLTFPackageIdentityPlan& Identities,
    const FStaticModelImportProfile& Profile,
    const FAssetSourceVersionRecord& SourceRecord,
    const FAssetParticipantId& Producer,
    const FAssetProducerVersion& ProducerVersion,
    const Core::TArray<Core::TSharedPtr<const FStaticMeshAsset>>& Meshes,
    Core::TArray<Core::TSharedPtr<const FStaticModelAsset>>& OutModels,
    FAssetDiagnosticList* Diagnostics = nullptr);

} // namespace Stoner::Asset::Private
