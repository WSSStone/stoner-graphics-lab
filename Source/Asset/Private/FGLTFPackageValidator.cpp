#include "FGLTFPackageValidator.h"

#include "Asset/FImageAsset.h"
#include "Asset/FMaterialAsset.h"
#include "Asset/FStaticMeshAsset.h"
#include "Asset/FStaticModelAsset.h"
#include "Asset/FTextureAsset.h"

#include <algorithm>
#include <set>

namespace Stoner::Asset::Private
{

EAssetResult ValidateGLTFPackageOutputs(
    const FGLTFPackageIdentityPlan& Identities,
    const Core::TArray<FAssetImportOutput>& Outputs,
    bool RequireMaterialPayloads,
    std::span<const FAssetId> ExpectedTextureIds)
{
    Core::usize ExpectedCount =
        Identities.MeshIds.size() + Identities.ModelIds.size();
    if (RequireMaterialPayloads)
        ExpectedCount += Identities.MaterialIds.size() + 1 +
            Identities.ImageIds.size() + ExpectedTextureIds.size();
    if (Outputs.size() != ExpectedCount)
        return EAssetResult::DependencyMismatch;
    std::set<FAssetId> OutputIds;
    Core::usize MeshCount = 0;
    Core::usize ModelCount = 0;
    Core::usize MaterialCount = 0;
    Core::usize ImageCount = 0;
    Core::usize TextureCount = 0;
    FAssetId Previous;
    bool HasPrevious = false;
    for (const FAssetImportOutput& Output : Outputs)
    {
        if (!Output.Payload ||
            Output.Payload->GetAssetType() != Output.Metadata.Id.GetAssetType() ||
            !OutputIds.insert(Output.Metadata.Id).second ||
            (HasPrevious && !(Previous < Output.Metadata.Id)))
            return EAssetResult::InvalidInput;
        Previous = Output.Metadata.Id;
        HasPrevious = true;
        if (const auto Mesh =
                std::dynamic_pointer_cast<const FStaticMeshAsset>(Output.Payload))
        {
            if (Mesh->GetDesc().Id != Output.Metadata.Id)
                return EAssetResult::InvalidInput;
            ++MeshCount;
        }
        else if (const auto Model =
                     std::dynamic_pointer_cast<const FStaticModelAsset>(Output.Payload))
        {
            if (Model->GetDesc().Id != Output.Metadata.Id)
                return EAssetResult::InvalidInput;
            ++ModelCount;
            for (const FStaticModelNode& Node : Model->GetDesc().Nodes)
            {
                if (Node.Mesh &&
                    (!Node.Mesh->GetId() ||
                     std::find(Identities.MeshIds.begin(), Identities.MeshIds.end(),
                         *Node.Mesh->GetId()) == Identities.MeshIds.end()))
                    return EAssetResult::DependencyMismatch;
            }
        }
        else if (const auto Material =
                     std::dynamic_pointer_cast<const FMaterialAsset>(Output.Payload))
        {
            if (Material->GetDesc().Id != Output.Metadata.Id)
                return EAssetResult::InvalidInput;
            ++MaterialCount;
        }
        else if (const auto Image =
                     std::dynamic_pointer_cast<const FImageAsset>(Output.Payload))
        {
            if (Image->GetId() != Output.Metadata.Id)
                return EAssetResult::InvalidInput;
            ++ImageCount;
        }
        else if (const auto Texture =
                     std::dynamic_pointer_cast<const FTextureAsset>(Output.Payload))
        {
            if (Texture->GetId() != Output.Metadata.Id)
                return EAssetResult::InvalidInput;
            ++TextureCount;
        }
        else return EAssetResult::InvalidInput;
    }
    if (MeshCount != Identities.MeshIds.size() ||
        ModelCount != Identities.ModelIds.size() ||
        (RequireMaterialPayloads &&
         (MaterialCount != Identities.MaterialIds.size() + 1 ||
          ImageCount != Identities.ImageIds.size() ||
          TextureCount != ExpectedTextureIds.size())))
        return EAssetResult::DependencyMismatch;
    for (const FAssetId& Id : Identities.MeshIds)
        if (!OutputIds.contains(Id)) return EAssetResult::DependencyMismatch;
    for (const FAssetId& Id : Identities.ModelIds)
        if (!OutputIds.contains(Id)) return EAssetResult::DependencyMismatch;
    if (RequireMaterialPayloads)
    {
        for (const FAssetId& Id : Identities.MaterialIds)
            if (!OutputIds.contains(Id)) return EAssetResult::DependencyMismatch;
        if (!OutputIds.contains(Identities.DefaultMaterialId))
            return EAssetResult::DependencyMismatch;
        for (const FAssetId& Id : Identities.ImageIds)
            if (!OutputIds.contains(Id)) return EAssetResult::DependencyMismatch;
        for (const FAssetId& Id : ExpectedTextureIds)
            if (!OutputIds.contains(Id)) return EAssetResult::DependencyMismatch;
    }
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
