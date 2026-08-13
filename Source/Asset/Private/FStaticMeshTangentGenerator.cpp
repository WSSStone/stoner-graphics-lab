#include "FStaticMeshTangentGenerator.h"

#include "mikktspace/mikktspace.h"

#include <limits>

namespace Stoner::Asset::Private
{
namespace
{

struct FMikkMesh
{
    const FStaticMeshVertexData* SourceVertices = nullptr;
    const FStaticMeshIndexData* SourceIndices = nullptr;
    Core::uint32 TexCoordSet = 0;
    Core::TArray<Core::FVector4> Tangents;
};

FMikkMesh& GetMesh(const SMikkTSpaceContext* Context)
{
    return *static_cast<FMikkMesh*>(Context->m_pUserData);
}

Core::uint32 GetSourceIndex(const FMikkMesh& Mesh, int Face, int Vertex)
{
    return Mesh.SourceIndices->GetIndex(
        static_cast<Core::uint32>(Face * 3 + Vertex));
}

int GetFaceCount(const SMikkTSpaceContext* Context)
{
    return static_cast<int>(GetMesh(Context).SourceIndices->GetIndexCount() / 3);
}

int GetFaceVertexCount(const SMikkTSpaceContext*, int)
{
    return 3;
}

void GetPosition(
    const SMikkTSpaceContext* Context,
    float Out[3],
    int Face,
    int Vertex)
{
    const FMikkMesh& Mesh = GetMesh(Context);
    const Core::FVector3& Value =
        Mesh.SourceVertices->Positions[GetSourceIndex(Mesh, Face, Vertex)];
    Out[0] = Value.X;
    Out[1] = Value.Y;
    Out[2] = Value.Z;
}

void GetNormal(
    const SMikkTSpaceContext* Context,
    float Out[3],
    int Face,
    int Vertex)
{
    const FMikkMesh& Mesh = GetMesh(Context);
    const Core::FVector3& Value =
        Mesh.SourceVertices->Normals[GetSourceIndex(Mesh, Face, Vertex)];
    Out[0] = Value.X;
    Out[1] = Value.Y;
    Out[2] = Value.Z;
}

void GetTexCoord(
    const SMikkTSpaceContext* Context,
    float Out[2],
    int Face,
    int Vertex)
{
    const FMikkMesh& Mesh = GetMesh(Context);
    const Core::FVector2& Value = Mesh.SourceVertices->TexCoords[Mesh.TexCoordSet]
        [GetSourceIndex(Mesh, Face, Vertex)];
    Out[0] = Value.X;
    Out[1] = Value.Y;
}

void SetTangent(
    const SMikkTSpaceContext* Context,
    const float Tangent[3],
    float Sign,
    int Face,
    int Vertex)
{
    FMikkMesh& Mesh = GetMesh(Context);
    Mesh.Tangents[static_cast<Core::usize>(Face * 3 + Vertex)] =
        Core::FVector4(Tangent[0], Tangent[1], Tangent[2], Sign);
}

} // namespace

EAssetResult GenerateStaticMeshTangents(
    FStaticMeshVertexData& Vertices,
    FStaticMeshIndexData& Indices,
    Core::uint32 TexCoordSet,
    Core::uint32 MaximumVertices)
{
    const Core::uint32 IndexCount = Indices.GetIndexCount();
    if (TexCoordSet >= Vertices.TexCoords.size() ||
        Vertices.Positions.empty() ||
        Vertices.Normals.size() != Vertices.Positions.size() ||
        Vertices.TexCoords[TexCoordSet].size() != Vertices.Positions.size() ||
        !Indices.IsValid(static_cast<Core::uint32>(Vertices.Positions.size())) ||
        IndexCount > MaximumVertices ||
        IndexCount > static_cast<Core::uint32>(std::numeric_limits<int>::max()))
    {
        return EAssetResult::InvalidInput;
    }

    FMikkMesh Mesh;
    Mesh.SourceVertices = &Vertices;
    Mesh.SourceIndices = &Indices;
    Mesh.TexCoordSet = TexCoordSet;
    Mesh.Tangents.resize(IndexCount);
    SMikkTSpaceInterface Interface{};
    Interface.m_getNumFaces = GetFaceCount;
    Interface.m_getNumVerticesOfFace = GetFaceVertexCount;
    Interface.m_getPosition = GetPosition;
    Interface.m_getNormal = GetNormal;
    Interface.m_getTexCoord = GetTexCoord;
    Interface.m_setTSpaceBasic = SetTangent;
    SMikkTSpaceContext Context{&Interface, &Mesh};
    if (!genTangSpaceDefault(&Context))
    {
        return EAssetResult::MalformedSource;
    }

    FStaticMeshVertexData Split;
    Split.Positions.reserve(IndexCount);
    Split.Normals.reserve(IndexCount);
    Split.Tangents.reserve(IndexCount);
    for (Core::usize Set = 0; Set < Split.TexCoords.size(); ++Set)
    {
        if (!Vertices.TexCoords[Set].empty())
        {
            Split.TexCoords[Set].reserve(IndexCount);
        }
    }
    Core::TArray<Core::uint32> SequentialIndices;
    SequentialIndices.reserve(IndexCount);
    for (Core::uint32 Corner = 0; Corner < IndexCount; ++Corner)
    {
        const Core::uint32 SourceIndex = Indices.GetIndex(Corner);
        SequentialIndices.push_back(Corner);
        Split.Positions.push_back(Vertices.Positions[SourceIndex]);
        Split.Normals.push_back(Vertices.Normals[SourceIndex]);
        Split.Tangents.push_back(Mesh.Tangents[Corner]);
        for (Core::usize Set = 0; Set < Split.TexCoords.size(); ++Set)
        {
            if (!Vertices.TexCoords[Set].empty())
            {
                Split.TexCoords[Set].push_back(Vertices.TexCoords[Set][SourceIndex]);
            }
        }
    }
    FStaticMeshIndexData SplitIndices;
    if (FStaticMeshIndexData::Create(
            std::move(SequentialIndices), SplitIndices) != EAssetResult::Success ||
        !Split.IsValid())
    {
        return EAssetResult::MalformedSource;
    }
    Vertices = std::move(Split);
    Indices = std::move(SplitIndices);
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
