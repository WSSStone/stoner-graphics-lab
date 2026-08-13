#include "Asset/FStaticMeshTypes.h"

#include <algorithm>
#include <limits>

namespace Stoner::Asset
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
        Core::FMath::IsFinite(Value.Y) &&
        Core::FMath::IsFinite(Value.Z);
}

bool IsFinite(const Core::FVector4& Value) noexcept
{
    return Core::FMath::IsFinite(Value.X) &&
        Core::FMath::IsFinite(Value.Y) &&
        Core::FMath::IsFinite(Value.Z) &&
        Core::FMath::IsFinite(Value.W);
}

bool IsNormalized(const Core::FVector3& Value, float Tolerance) noexcept
{
    return IsFinite(Value) &&
        Core::FMath::IsNearlyEqual(Value.Length(), 1.0f, Tolerance);
}

} // namespace

bool FStaticMeshBounds::IsValid() const noexcept
{
    return Box.IsValid() && Sphere.IsValid() &&
        Sphere.Contains(Box.Min) && Sphere.Contains(Box.Max);
}

bool FStaticMeshBounds::Contains(const Core::FVector3& Position) const noexcept
{
    return IsValid() && Box.Contains(Position) && Sphere.Contains(Position);
}

bool FStaticMeshVertexData::IsValid(float Tolerance) const noexcept
{
    if (Positions.empty() ||
        !Core::FMath::IsFinite(Tolerance) || Tolerance < 0.0f ||
        Normals.size() != Positions.size())
    {
        return false;
    }

    for (const Core::FVector3& Position : Positions)
    {
        if (!IsFinite(Position))
        {
            return false;
        }
    }
    for (const Core::FVector3& Normal : Normals)
    {
        if (!IsNormalized(Normal, Tolerance))
        {
            return false;
        }
    }
    if (!Tangents.empty())
    {
        if (Tangents.size() != Positions.size())
        {
            return false;
        }
        for (const Core::FVector4& Tangent : Tangents)
        {
            const Core::FVector3 Direction(Tangent.X, Tangent.Y, Tangent.Z);
            if (!IsFinite(Tangent) || !IsNormalized(Direction, Tolerance) ||
                (Tangent.W != -1.0f && Tangent.W != 1.0f))
            {
                return false;
            }
        }
    }
    for (const Core::TArray<Core::FVector2>& TexCoordSet : TexCoords)
    {
        if (TexCoordSet.empty())
        {
            continue;
        }
        if (TexCoordSet.size() != Positions.size() ||
            !std::all_of(
                TexCoordSet.begin(), TexCoordSet.end(),
                [](const Core::FVector2& TexCoord)
                {
                    return IsFinite(TexCoord);
                }))
        {
            return false;
        }
    }
    return true;
}

EAssetResult FStaticMeshIndexData::Create(
    Core::TArray<Core::uint32> SourceIndices,
    FStaticMeshIndexData& OutIndices)
{
    OutIndices = {};
    if (SourceIndices.empty() || SourceIndices.size() % 3 != 0)
    {
        return EAssetResult::InvalidInput;
    }

    const Core::uint32 Maximum = *std::max_element(
        SourceIndices.begin(), SourceIndices.end());
    if (Maximum <= std::numeric_limits<Core::uint16>::max())
    {
        Core::TArray<Core::uint16> Narrow;
        Narrow.reserve(SourceIndices.size());
        for (const Core::uint32 Value : SourceIndices)
        {
            Narrow.push_back(static_cast<Core::uint16>(Value));
        }
        OutIndices.Data_ = std::move(Narrow);
    }
    else
    {
        OutIndices.Data_ = std::move(SourceIndices);
    }
    return EAssetResult::Success;
}

bool FStaticMeshIndexData::IsValid(Core::uint32 VertexCount) const noexcept
{
    const Core::uint32 Count = GetIndexCount();
    if (VertexCount == 0 || Count == 0 || Count % 3 != 0)
    {
        return false;
    }
    for (Core::uint32 Index = 0; Index < Count; ++Index)
    {
        if (GetIndex(Index) >= VertexCount)
        {
            return false;
        }
    }
    return true;
}

bool FStaticMeshIndexData::Uses16BitIndices() const noexcept
{
    return std::holds_alternative<Core::TArray<Core::uint16>>(Data_);
}

Core::uint32 FStaticMeshIndexData::GetIndexCount() const noexcept
{
    return static_cast<Core::uint32>(std::visit(
        [](const auto& Indices) { return Indices.size(); }, Data_));
}

Core::uint32 FStaticMeshIndexData::GetIndex(Core::uint32 Index) const noexcept
{
    return std::visit(
        [Index](const auto& Indices) -> Core::uint32
        {
            return Index < Indices.size() ?
                static_cast<Core::uint32>(Indices[Index]) : 0;
        },
        Data_);
}

bool FStaticMeshPrimitive::IsValid(Core::uint32 MaterialSlotCount) const noexcept
{
    if (StableKey.IsEmpty() || MaterialSlotIndex >= MaterialSlotCount ||
        !Vertices.IsValid() ||
        !Indices.IsValid(static_cast<Core::uint32>(Vertices.Positions.size())) ||
        !LocalBounds.IsValid())
    {
        return false;
    }
    return std::all_of(
        Vertices.Positions.begin(), Vertices.Positions.end(),
        [this](const Core::FVector3& Position)
        {
            return LocalBounds.Contains(Position);
        });
}

} // namespace Stoner::Asset
