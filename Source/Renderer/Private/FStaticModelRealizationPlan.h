#pragma once

#include "Renderer/FStaticModelRealization.h"

namespace Stoner::Renderer::Private
{

struct FStaticModelPlannedMaterial
{
    Asset::FAssetId AssetId;
    Asset::FResolvedMaterialAsset Resolved;
    Asset::FSelectedShaderProgram SelectedShader;
    Core::TArray<Asset::FAssetId> TextureIds;
};

struct FStaticModelPlannedDraw
{
    Core::uint32 NodePlanIndex = 0;
    Core::uint32 MeshPlanIndex = 0;
    Core::uint32 SectionIndex = 0;
    Core::uint32 MaterialPlanIndex = 0;
    Core::FString StableKey;
    Asset::FStaticMeshBounds Bounds;
};

struct FStaticModelRealizationPlan
{
    Core::TArray<FStaticModelRenderNode> Nodes;
    Core::TArray<Core::TSharedPtr<const Asset::FStaticMeshAsset>> Meshes;
    Core::TArray<FStaticModelPlannedMaterial> Materials;
    Core::TArray<Core::TSharedPtr<const Asset::FKTX2TextureArtifact>> Textures;
    Core::TArray<FStaticModelPlannedDraw> Draws;
    Core::uint64 EstimatedResourceBytes = 0;
};

[[nodiscard]] RHI::ERHIResult BuildStaticModelRealizationPlan(
    const FStaticModelRealizationRequest& Request,
    FStaticModelRealizationPlan& OutPlan,
    FStaticModelRealizationDiagnostic& OutFailure);

} // namespace Stoner::Renderer::Private
