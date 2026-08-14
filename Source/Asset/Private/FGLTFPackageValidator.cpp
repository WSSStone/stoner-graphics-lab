#include "FGLTFPackageValidator.h"

#include "Asset/FImageAsset.h"
#include "Asset/FMaterialAsset.h"
#include "Asset/FStaticMeshAsset.h"
#include "Asset/FStaticModelAsset.h"
#include "Asset/FTextureAsset.h"

#include "cgltf/cgltf.h"

#include <algorithm>
#include <set>
#include <string_view>

namespace Stoner::Asset::Private
{

EAssetResult ValidateGLTFStaticPackageSupport(const cgltf_data& Data)
{
    if (Data.asset.version == nullptr ||
        std::string_view(Data.asset.version) != "2.0" ||
        (Data.asset.min_version != nullptr &&
         std::string_view(Data.asset.min_version) != "2.0") ||
        Data.extensions_required_count != 0 || Data.skins_count != 0 ||
        Data.variants_count != 0)
        return EAssetResult::Unsupported;
    for (const cgltf_buffer_view& View :
         std::span<const cgltf_buffer_view>(Data.buffer_views, Data.buffer_views_count))
        if (View.has_meshopt_compression) return EAssetResult::Unsupported;
    for (const cgltf_node& Node :
         std::span<const cgltf_node>(Data.nodes, Data.nodes_count))
        if (Node.skin != nullptr || Node.weights_count != 0 ||
            Node.has_mesh_gpu_instancing)
            return EAssetResult::Unsupported;
    for (const cgltf_mesh& Mesh :
         std::span<const cgltf_mesh>(Data.meshes, Data.meshes_count))
    {
        if (Mesh.weights_count != 0 || Mesh.target_names_count != 0)
            return EAssetResult::Unsupported;
        for (const cgltf_primitive& Primitive :
             std::span<const cgltf_primitive>(Mesh.primitives, Mesh.primitives_count))
        {
            if (Primitive.type != cgltf_primitive_type_triangles ||
                Primitive.targets_count != 0 ||
                Primitive.has_draco_mesh_compression)
                return EAssetResult::Unsupported;
            for (const cgltf_attribute& Attribute :
                 std::span<const cgltf_attribute>(
                     Primitive.attributes, Primitive.attributes_count))
            {
                const bool Supported =
                    Attribute.type == cgltf_attribute_type_position ||
                    Attribute.type == cgltf_attribute_type_normal ||
                    Attribute.type == cgltf_attribute_type_tangent ||
                    (Attribute.type == cgltf_attribute_type_texcoord &&
                     Attribute.index >= 0 && Attribute.index <= 1);
                if (!Supported) return EAssetResult::Unsupported;
            }
        }
    }
    return EAssetResult::Success;
}

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
