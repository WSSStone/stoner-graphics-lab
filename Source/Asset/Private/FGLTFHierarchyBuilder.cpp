#include "FGLTFHierarchyBuilder.h"

#include "FGLTFStableKey.h"

#include "cgltf/cgltf.h"

#include <functional>
#include <set>
#include <string>

namespace Stoner::Asset::Private
{
namespace
{

bool TryDecomposeTRS(const Core::FMatrix4x4& Matrix, Core::FTransform& Out)
{
    Out = Core::FTransform::Identity();
    if (!Matrix.IsFinite() ||
        !Core::FMath::IsNearlyZero(Matrix.M[3][0]) ||
        !Core::FMath::IsNearlyZero(Matrix.M[3][1]) ||
        !Core::FMath::IsNearlyZero(Matrix.M[3][2]) ||
        !Core::FMath::IsNearlyEqual(Matrix.M[3][3], 1.0f))
    {
        return false;
    }
    Core::FVector3 X(Matrix.M[0][0], Matrix.M[1][0], Matrix.M[2][0]);
    Core::FVector3 Y(Matrix.M[0][1], Matrix.M[1][1], Matrix.M[2][1]);
    Core::FVector3 Z(Matrix.M[0][2], Matrix.M[1][2], Matrix.M[2][2]);
    Core::FVector3 Scale(X.Length(), Y.Length(), Z.Length());
    if (Scale.X <= Core::FMath::DefaultTolerance ||
        Scale.Y <= Core::FMath::DefaultTolerance ||
        Scale.Z <= Core::FMath::DefaultTolerance)
    {
        return false;
    }
    X = X / Scale.X;
    Y = Y / Scale.Y;
    Z = Z / Scale.Z;
    if (!Core::FMath::IsNearlyZero(X.Dot(Y)) ||
        !Core::FMath::IsNearlyZero(X.Dot(Z)) ||
        !Core::FMath::IsNearlyZero(Y.Dot(Z)))
    {
        return false;
    }
    const float Determinant = X.Dot(Y.Cross(Z));
    if (!Core::FMath::IsNearlyEqual(Core::FMath::Abs(Determinant), 1.0f))
        return false;
    if (Determinant < 0.0f)
    {
        X = -X;
        Scale.X = -Scale.X;
    }
    const Core::FMatrix4x4 Rotation(
        X.X, Y.X, Z.X, 0.0f,
        X.Y, Y.Y, Z.Y, 0.0f,
        X.Z, Y.Z, Z.Z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
    const float Trace = Rotation.M[0][0] + Rotation.M[1][1] + Rotation.M[2][2];
    Core::FQuat Q;
    if (Trace > 0.0f)
    {
        const float S = Core::FMath::Sqrt(Trace + 1.0f) * 2.0f;
        Q = Core::FQuat(
            (Rotation.M[2][1] - Rotation.M[1][2]) / S,
            (Rotation.M[0][2] - Rotation.M[2][0]) / S,
            (Rotation.M[1][0] - Rotation.M[0][1]) / S,
            0.25f * S);
    }
    else if (Rotation.M[0][0] > Rotation.M[1][1] &&
             Rotation.M[0][0] > Rotation.M[2][2])
    {
        const float S = Core::FMath::Sqrt(
            1.0f + Rotation.M[0][0] - Rotation.M[1][1] - Rotation.M[2][2]) * 2.0f;
        Q = Core::FQuat(0.25f * S,
            (Rotation.M[0][1] + Rotation.M[1][0]) / S,
            (Rotation.M[0][2] + Rotation.M[2][0]) / S,
            (Rotation.M[2][1] - Rotation.M[1][2]) / S);
    }
    else if (Rotation.M[1][1] > Rotation.M[2][2])
    {
        const float S = Core::FMath::Sqrt(
            1.0f + Rotation.M[1][1] - Rotation.M[0][0] - Rotation.M[2][2]) * 2.0f;
        Q = Core::FQuat(
            (Rotation.M[0][1] + Rotation.M[1][0]) / S, 0.25f * S,
            (Rotation.M[1][2] + Rotation.M[2][1]) / S,
            (Rotation.M[0][2] - Rotation.M[2][0]) / S);
    }
    else
    {
        const float S = Core::FMath::Sqrt(
            1.0f + Rotation.M[2][2] - Rotation.M[0][0] - Rotation.M[1][1]) * 2.0f;
        Q = Core::FQuat(
            (Rotation.M[0][2] + Rotation.M[2][0]) / S,
            (Rotation.M[1][2] + Rotation.M[2][1]) / S, 0.25f * S,
            (Rotation.M[1][0] - Rotation.M[0][1]) / S);
    }
    Out = Core::FTransform(
        Core::FVector3(Matrix.M[0][3], Matrix.M[1][3], Matrix.M[2][3]),
        Q.GetSafeNormal(), Scale);
    return Out.IsFinite() && Out.ToMatrix().NearlyEquals(Matrix);
}

EAssetResult ConvertTransform(const cgltf_node& Node, Core::FTransform& Out)
{
    Out = Core::FTransform::Identity();
    if (Node.has_matrix)
    {
        if (Node.has_translation || Node.has_rotation || Node.has_scale)
            return EAssetResult::MalformedSource;
        Core::FMatrix4x4 Source = Core::FMatrix4x4::Zero();
        for (int Row = 0; Row < 4; ++Row)
            for (int Column = 0; Column < 4; ++Column)
                Source.M[Row][Column] = Node.matrix[Column * 4 + Row];
        const Core::FMatrix4x4 C(
            0, 0, 1, 0,
            -1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 0, 1);
        const Core::FMatrix4x4 Converted = C * Source * C.Transposed();
        return TryDecomposeTRS(Converted, Out)
            ? EAssetResult::Success : EAssetResult::MalformedSource;
    }
    const Core::FVector3 SourceTranslation = Node.has_translation
        ? Core::FVector3(Node.translation[0], Node.translation[1], Node.translation[2])
        : Core::FVector3::Zero();
    const Core::FVector3 SourceScale = Node.has_scale
        ? Core::FVector3(Node.scale[0], Node.scale[1], Node.scale[2])
        : Core::FVector3(1.0f, 1.0f, 1.0f);
    const Core::FQuat SourceRotation = Node.has_rotation
        ? Core::FQuat(Node.rotation[0], Node.rotation[1], Node.rotation[2], Node.rotation[3])
        : Core::FQuat::Identity();
    if (!Core::FMath::IsFinite(SourceTranslation.X) ||
        !Core::FMath::IsFinite(SourceTranslation.Y) ||
        !Core::FMath::IsFinite(SourceTranslation.Z) ||
        !Core::FMath::IsFinite(SourceScale.X) ||
        !Core::FMath::IsFinite(SourceScale.Y) ||
        !Core::FMath::IsFinite(SourceScale.Z) || !SourceRotation.IsFinite() ||
        (Node.has_rotation && SourceRotation.Length() <= Core::FMath::DefaultTolerance))
    {
        return EAssetResult::MalformedSource;
    }
    Out.Translation = Core::FVector3(
        SourceTranslation.Z, -SourceTranslation.X, SourceTranslation.Y);
    Out.Scale = Core::FVector3(SourceScale.Z, SourceScale.X, SourceScale.Y);
    Out.Rotation = Core::FQuat(
        -SourceRotation.Z, SourceRotation.X, -SourceRotation.Y,
        SourceRotation.W).GetSafeNormal();
    return Out.IsFinite() ? EAssetResult::Success : EAssetResult::MalformedSource;
}

} // namespace

EAssetResult BuildGLTFSceneHierarchy(
    const cgltf_data& Data,
    Core::uint32 SceneIndex,
    const FGLTFPackageIdentityPlan& Identities,
    const FStaticModelImportProfile& Profile,
    Core::TArray<FStaticModelNode>& OutNodes,
    Core::TArray<Core::uint32>& OutRootNodeIndices)
{
    OutNodes.clear();
    OutRootNodeIndices.clear();
    if (SceneIndex >= Data.scenes_count ||
        Identities.MeshIds.size() != Data.meshes_count)
    {
        return EAssetResult::InvalidInput;
    }
    enum class EVisit : Core::uint8 { Unvisited, Visiting, Visited };
    Core::TArray<EVisit> Visits(Data.nodes_count, EVisit::Unvisited);
    std::set<Core::FString> StableKeys;
    EAssetResult Result = EAssetResult::Success;
    const auto SourceNodeIndex = [&Data](const cgltf_node* Node, Core::uint32& OutIndex)
    {
        if (Node == nullptr || Node < Data.nodes || Node >= Data.nodes + Data.nodes_count)
            return false;
        OutIndex = static_cast<Core::uint32>(Node - Data.nodes);
        return true;
    };
    std::function<bool(const cgltf_node*, Core::uint32, Core::uint32&)> AddNode;
    AddNode = [&](const cgltf_node* Source, Core::uint32 Depth, Core::uint32& OutIndex)
    {
        Core::uint32 GlobalIndex = 0;
        if (!SourceNodeIndex(Source, GlobalIndex))
        {
            Result = EAssetResult::MalformedSource;
            return false;
        }
        if (Depth > Profile.Limits.MaxHierarchyDepth)
        {
            Result = EAssetResult::CapacityExceeded;
            return false;
        }
        if (
            Visits[GlobalIndex] != EVisit::Unvisited || Source->skin != nullptr ||
            Source->weights_count != 0)
        {
            Result = Source->skin != nullptr || Source->weights_count != 0
                ? EAssetResult::Unsupported : EAssetResult::MalformedSource;
            return false;
        }
        Visits[GlobalIndex] = EVisit::Visiting;
        FStaticModelNode Node;
        bool Explicit = false;
        Result = MakeGLTFStableKey(
            Source->extras.data,
            Core::FString("idx.node." + std::to_string(GlobalIndex)),
            Node.StableKey,
            Explicit);
        (void)Explicit;
        if (Result != EAssetResult::Success)
        {
            return false;
        }
        if (!StableKeys.insert(Node.StableKey).second)
        {
            Result = EAssetResult::Conflict;
            return false;
        }
        Result = ConvertTransform(*Source, Node.LocalTransform);
        if (Result != EAssetResult::Success)
        {
            return false;
        }
        Node.DisplayName = Core::FString(Source->name != nullptr ? Source->name : "");
        Node.SourceNodeIndex = GlobalIndex;
        Node.bNegativeDeterminant =
            Node.LocalTransform.Scale.X * Node.LocalTransform.Scale.Y *
                Node.LocalTransform.Scale.Z < 0.0f;
        if (Source->mesh != nullptr)
        {
            if (Source->mesh < Data.meshes ||
                Source->mesh >= Data.meshes + Data.meshes_count)
            {
                Result = EAssetResult::MalformedSource;
                return false;
            }
            TSoftAssetRef<FStaticMeshAsset> Mesh;
            const Core::usize MeshIndex = static_cast<Core::usize>(Source->mesh - Data.meshes);
            if (TSoftAssetRef<FStaticMeshAsset>::Create(
                    Identities.MeshIds[MeshIndex], Mesh) != EAssetResult::Success)
            {
                Result = EAssetResult::InvalidIdentity;
                return false;
            }
            Node.Mesh = std::move(Mesh);
        }
        OutIndex = static_cast<Core::uint32>(OutNodes.size());
        OutNodes.push_back(std::move(Node));
        for (cgltf_size ChildIndex = 0; ChildIndex < Source->children_count; ++ChildIndex)
        {
            Core::uint32 Child = 0;
            if (!AddNode(Source->children[ChildIndex], Depth + 1, Child)) return false;
            OutNodes[OutIndex].Children.push_back(Child);
        }
        Visits[GlobalIndex] = EVisit::Visited;
        return true;
    };

    const cgltf_scene& Scene = Data.scenes[SceneIndex];
    for (cgltf_size RootIndex = 0; RootIndex < Scene.nodes_count; ++RootIndex)
    {
        Core::uint32 Root = 0;
        if (!AddNode(Scene.nodes[RootIndex], 1, Root))
        {
            OutNodes.clear();
            OutRootNodeIndices.clear();
            return Result;
        }
        OutRootNodeIndices.push_back(Root);
    }
    return OutNodes.empty() ? EAssetResult::MalformedSource : EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
