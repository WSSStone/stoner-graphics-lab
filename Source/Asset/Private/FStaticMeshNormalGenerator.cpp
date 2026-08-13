#include "FStaticMeshNormalGenerator.h"

namespace Stoner::Asset::Private
{

EAssetResult GenerateFlatStaticMeshNormals(
    FStaticMeshVertexData& Vertices,
    FStaticMeshIndexData& Indices,
    Core::uint32 MaximumVertices)
{
    const Core::uint32 IndexCount = Indices.GetIndexCount();
    if (Vertices.Positions.empty() || IndexCount == 0 || IndexCount % 3 != 0 ||
        IndexCount > MaximumVertices ||
        !Indices.IsValid(static_cast<Core::uint32>(Vertices.Positions.size())))
    {
        return EAssetResult::InvalidInput;
    }
    for (const auto& TexCoords : Vertices.TexCoords)
    {
        if (!TexCoords.empty() && TexCoords.size() != Vertices.Positions.size())
        {
            return EAssetResult::MalformedSource;
        }
    }

    FStaticMeshVertexData Split;
    Split.Positions.reserve(IndexCount);
    Split.Normals.reserve(IndexCount);
    for (Core::usize Set = 0; Set < Split.TexCoords.size(); ++Set)
    {
        if (!Vertices.TexCoords[Set].empty())
        {
            Split.TexCoords[Set].reserve(IndexCount);
        }
    }
    Core::TArray<Core::uint32> SequentialIndices;
    SequentialIndices.reserve(IndexCount);

    for (Core::uint32 Triangle = 0; Triangle < IndexCount; Triangle += 3)
    {
        const Core::uint32 I0 = Indices.GetIndex(Triangle);
        const Core::uint32 I1 = Indices.GetIndex(Triangle + 1);
        const Core::uint32 I2 = Indices.GetIndex(Triangle + 2);
        const Core::FVector3& P0 = Vertices.Positions[I0];
        const Core::FVector3& P1 = Vertices.Positions[I1];
        const Core::FVector3& P2 = Vertices.Positions[I2];
        const Core::FVector3 Normal = (P2 - P0).Cross(P1 - P0).GetSafeNormal();
        if (Normal == Core::FVector3::Zero())
        {
            return EAssetResult::MalformedSource;
        }
        const Core::uint32 SourceIndices[3] = {I0, I1, I2};
        for (const Core::uint32 SourceIndex : SourceIndices)
        {
            SequentialIndices.push_back(
                static_cast<Core::uint32>(Split.Positions.size()));
            Split.Positions.push_back(Vertices.Positions[SourceIndex]);
            Split.Normals.push_back(Normal);
            for (Core::usize Set = 0; Set < Split.TexCoords.size(); ++Set)
            {
                if (!Vertices.TexCoords[Set].empty())
                {
                    Split.TexCoords[Set].push_back(
                        Vertices.TexCoords[Set][SourceIndex]);
                }
            }
        }
    }

    FStaticMeshIndexData SplitIndices;
    if (FStaticMeshIndexData::Create(
            std::move(SequentialIndices), SplitIndices) != EAssetResult::Success)
    {
        return EAssetResult::ProcessingFailure;
    }
    Vertices = std::move(Split);
    Indices = std::move(SplitIndices);
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
