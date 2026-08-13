#include "FGLTFPackageValidator.h"

#include "Asset/FStaticMeshAsset.h"
#include "Asset/FStaticModelAsset.h"

#include <algorithm>
#include <set>

namespace Stoner::Asset::Private
{

EAssetResult ValidateGLTFPackageOutputs(
    const FGLTFPackageIdentityPlan& Identities,
    const Core::TArray<FAssetImportOutput>& Outputs,
    bool RequireMaterialPayloads)
{
    const Core::usize ExpectedMinimum =
        Identities.MeshIds.size() + Identities.ModelIds.size();
    if (Outputs.size() < ExpectedMinimum ||
        (RequireMaterialPayloads &&
         Outputs.size() < ExpectedMinimum + Identities.MaterialIds.size() + 1))
        return EAssetResult::DependencyMismatch;
    std::set<FAssetId> OutputIds;
    Core::usize MeshCount = 0;
    Core::usize ModelCount = 0;
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
    }
    if (MeshCount != Identities.MeshIds.size() ||
        ModelCount != Identities.ModelIds.size())
        return EAssetResult::DependencyMismatch;
    for (const FAssetId& Id : Identities.MeshIds)
        if (!OutputIds.contains(Id)) return EAssetResult::DependencyMismatch;
    for (const FAssetId& Id : Identities.ModelIds)
        if (!OutputIds.contains(Id)) return EAssetResult::DependencyMismatch;
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
