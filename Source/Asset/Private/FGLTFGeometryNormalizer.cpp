#include "FGLTFGeometryNormalizer.h"

namespace Stoner::Asset::Private
{
namespace
{

bool IsFinite(const Core::FVector2& Value) noexcept
{
    return Core::FMath::IsFinite(Value.X) && Core::FMath::IsFinite(Value.Y);
}

bool IsFinite(const Core::FVector3& Value) noexcept
{
    return Core::FMath::IsFinite(Value.X) &&
        Core::FMath::IsFinite(Value.Y) && Core::FMath::IsFinite(Value.Z);
}

} // namespace

EAssetResult FGLTFGeometryNormalizer::ConvertPosition(
    const Core::FVector3& Source,
    Core::FVector3& OutPosition) noexcept
{
    OutPosition = {};
    if (!IsFinite(Source))
    {
        return EAssetResult::MalformedSource;
    }
    OutPosition = Core::FVector3(Source.Z, -Source.X, Source.Y);
    return EAssetResult::Success;
}

EAssetResult FGLTFGeometryNormalizer::ConvertDirection(
    const Core::FVector3& Source,
    Core::FVector3& OutDirection) noexcept
{
    OutDirection = {};
    Core::FVector3 Converted;
    if (ConvertPosition(Source, Converted) != EAssetResult::Success)
    {
        return EAssetResult::MalformedSource;
    }
    OutDirection = Converted.GetSafeNormal();
    return OutDirection == Core::FVector3::Zero()
        ? EAssetResult::MalformedSource : EAssetResult::Success;
}

EAssetResult FGLTFGeometryNormalizer::ConvertTangent(
    const Core::FVector4& Source,
    Core::FVector4& OutTangent) noexcept
{
    OutTangent = {};
    if (!Core::FMath::IsFinite(Source.W) ||
        (Source.W != -1.0f && Source.W != 1.0f))
    {
        return EAssetResult::MalformedSource;
    }
    Core::FVector3 Direction;
    if (ConvertDirection(
            Core::FVector3(Source.X, Source.Y, Source.Z), Direction) !=
        EAssetResult::Success)
    {
        return EAssetResult::MalformedSource;
    }
    OutTangent = Core::FVector4(
        Direction.X, Direction.Y, Direction.Z, -Source.W);
    return EAssetResult::Success;
}

EAssetResult FGLTFGeometryNormalizer::NormalizeSourceStreams(
    FStaticMeshVertexData& Vertices) noexcept
{
    const Core::usize VertexCount = Vertices.Positions.size();
    if (VertexCount == 0 ||
        (!Vertices.Normals.empty() && Vertices.Normals.size() != VertexCount) ||
        (!Vertices.Tangents.empty() && Vertices.Tangents.size() != VertexCount))
    {
        return EAssetResult::MalformedSource;
    }
    for (const auto& TexCoords : Vertices.TexCoords)
    {
        if (!TexCoords.empty() && TexCoords.size() != VertexCount)
        {
            return EAssetResult::MalformedSource;
        }
        for (const Core::FVector2& TexCoord : TexCoords)
        {
            if (!IsFinite(TexCoord))
            {
                return EAssetResult::MalformedSource;
            }
        }
    }
    for (Core::FVector3& Position : Vertices.Positions)
    {
        Core::FVector3 Converted;
        if (ConvertPosition(Position, Converted) != EAssetResult::Success)
        {
            return EAssetResult::MalformedSource;
        }
        Position = Converted;
    }
    for (Core::FVector3& Normal : Vertices.Normals)
    {
        Core::FVector3 Converted;
        if (ConvertDirection(Normal, Converted) != EAssetResult::Success)
        {
            return EAssetResult::MalformedSource;
        }
        Normal = Converted;
    }
    for (Core::FVector4& Tangent : Vertices.Tangents)
    {
        Core::FVector4 Converted;
        if (ConvertTangent(Tangent, Converted) != EAssetResult::Success)
        {
            return EAssetResult::MalformedSource;
        }
        Tangent = Converted;
    }
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
