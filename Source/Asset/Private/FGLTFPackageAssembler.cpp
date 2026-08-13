#include "FGLTFPackageAssembler.h"

#include "FGLTFHierarchyBuilder.h"
#include "FStaticMeshBounds.h"

#include "cgltf/cgltf.h"

#include <algorithm>
#include <functional>

namespace Stoner::Asset::Private
{
namespace
{

FAssetDigest MakeModelDigest(
    const FAssetDigest& Source,
    const FAssetDigest& Profile,
    Core::uint32 SceneIndex)
{
    Core::TArray<Core::uint8> Bytes;
    Bytes.insert(Bytes.end(), Source.GetBytes().begin(), Source.GetBytes().end());
    Bytes.insert(Bytes.end(), Profile.GetBytes().begin(), Profile.GetBytes().end());
    Bytes.insert(Bytes.end(), {'m', 'o', 'd', 'e', 'l'});
    for (Core::uint32 Shift = 0; Shift < 32; Shift += 8)
        Bytes.push_back(static_cast<Core::uint8>(SceneIndex >> Shift));
    return FAssetDigest::FromBytes(Bytes);
}

const FStaticMeshAsset* FindMesh(
    const FAssetId& Id,
    const Core::TArray<Core::TSharedPtr<const FStaticMeshAsset>>& Meshes)
{
    const auto Found = std::find_if(Meshes.begin(), Meshes.end(),
        [&Id](const auto& Mesh) { return Mesh && Mesh->GetDesc().Id == Id; });
    return Found == Meshes.end() ? nullptr : Found->get();
}

EAssetResult BuildModelBounds(
    const FStaticModelAssetDesc& Desc,
    const Core::TArray<Core::TSharedPtr<const FStaticMeshAsset>>& Meshes,
    FStaticMeshBounds& OutBounds)
{
    Core::TArray<Core::FVector3> Points;
    std::function<EAssetResult(Core::uint32, const Core::FMatrix4x4&)> Visit;
    Visit = [&](Core::uint32 NodeIndex, const Core::FMatrix4x4& Parent)
    {
        const FStaticModelNode& Node = Desc.Nodes[NodeIndex];
        const Core::FMatrix4x4 World = Parent * Node.LocalTransform.ToMatrix();
        if (!World.IsFinite()) return EAssetResult::MalformedSource;
        if (Node.Mesh)
        {
            const FAssetId* Id = Node.Mesh->GetId() ? &*Node.Mesh->GetId() : nullptr;
            const FStaticMeshAsset* Mesh = Id ? FindMesh(*Id, Meshes) : nullptr;
            if (Mesh == nullptr) return EAssetResult::DependencyMismatch;
            const Core::FBox& Box = Mesh->GetDesc().Bounds.Box;
            for (int X = 0; X < 2; ++X)
                for (int Y = 0; Y < 2; ++Y)
                    for (int Z = 0; Z < 2; ++Z)
                    {
                        const Core::FVector3 Corner(
                            X ? Box.Max.X : Box.Min.X,
                            Y ? Box.Max.Y : Box.Min.Y,
                            Z ? Box.Max.Z : Box.Min.Z);
                        const Core::FVector3 Transformed = World.TransformPoint(Corner);
                        if (!Core::FMath::IsFinite(Transformed.X) ||
                            !Core::FMath::IsFinite(Transformed.Y) ||
                            !Core::FMath::IsFinite(Transformed.Z))
                            return EAssetResult::MalformedSource;
                        Points.push_back(Transformed);
                    }
        }
        for (const Core::uint32 Child : Node.Children)
        {
            const EAssetResult Result = Visit(Child, World);
            if (Result != EAssetResult::Success) return Result;
        }
        return EAssetResult::Success;
    };
    for (const Core::uint32 Root : Desc.RootNodeIndices)
    {
        const EAssetResult Result = Visit(Root, Core::FMatrix4x4::Identity());
        if (Result != EAssetResult::Success) return Result;
    }
    if (Points.empty()) Points.push_back(Core::FVector3::Zero());
    return BuildStaticMeshBounds(Points, OutBounds);
}

} // namespace

EAssetResult AssembleGLTFModels(
    const cgltf_data& Data,
    const FGLTFPackageIdentityPlan& Identities,
    const FStaticModelImportProfile& Profile,
    const FAssetSourceVersionRecord& SourceRecord,
    const FAssetParticipantId& Producer,
    const FAssetProducerVersion& ProducerVersion,
    const Core::TArray<Core::TSharedPtr<const FStaticMeshAsset>>& Meshes,
    Core::TArray<Core::TSharedPtr<const FStaticModelAsset>>& OutModels,
    FAssetDiagnosticList* Diagnostics)
{
    OutModels.clear();
    if (Identities.ModelIds.size() != Data.scenes_count ||
        Identities.SceneKeys.size() != Data.scenes_count ||
        Meshes.size() != Data.meshes_count || !SourceRecord.Id.IsValid())
        return EAssetResult::InvalidInput;
    const FAssetDigest ProfileDigest = Profile.GetDigest();
    for (Core::uint32 SceneIndex = 0;
         SceneIndex < static_cast<Core::uint32>(Data.scenes_count); ++SceneIndex)
    {
        FStaticModelAssetDesc Desc;
        Desc.Id = Identities.ModelIds[SceneIndex];
        Desc.SceneStableKey = Identities.SceneKeys[SceneIndex];
        Desc.bSourceDefaultScene = Data.scene == &Data.scenes[SceneIndex];
        Desc.ImportProfileDigest = ProfileDigest;
        Desc.SourceManifest.push_back(SourceRecord);
        Desc.Version.SourceDigest = SourceRecord.Version.SourceDigest;
        Desc.Version.ContentDigest = MakeModelDigest(
            SourceRecord.Version.SourceDigest, ProfileDigest, SceneIndex);
        Desc.Version.Producer = Producer;
        Desc.Version.ProducerVersion = ProducerVersion;
        EAssetResult Result = BuildGLTFSceneHierarchy(
            Data, SceneIndex, Identities, Profile,
            Desc.Nodes, Desc.RootNodeIndices);
        if (Result != EAssetResult::Success)
        {
            OutModels.clear();
            return Result;
        }
        for (const FStaticModelNode& Node : Desc.Nodes)
        {
            if (!Node.Mesh) continue;
            const FAssetDependency Dependency{
                *Node.Mesh->GetId(),
                EAssetDependencyRole::Runtime,
                EAssetDependencyStrength::Required,
                EAssetDependencyResolution::Unresolved};
            if (std::none_of(Desc.Dependencies.begin(), Desc.Dependencies.end(),
                [&Dependency](const FAssetDependency& Existing)
                {
                    return Existing.SameDeclaration(Dependency);
                }))
                Desc.Dependencies.push_back(Dependency);
        }
        Result = BuildModelBounds(Desc, Meshes, Desc.Bounds);
        if (Result != EAssetResult::Success)
        {
            OutModels.clear();
            return Result;
        }
        FStaticModelAsset Model;
        Result = FStaticModelAsset::CreateValidated(
            std::move(Desc), Profile.Limits.MaxHierarchyDepth, Model, Diagnostics);
        if (Result != EAssetResult::Success)
        {
            OutModels.clear();
            return Result;
        }
        OutModels.push_back(Core::MakeShared<const FStaticModelAsset>(std::move(Model)));
    }
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
