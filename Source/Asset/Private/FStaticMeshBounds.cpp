#include "FStaticMeshBounds.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Stoner::Asset::Private
{

EAssetResult BuildStaticMeshBounds(
    std::span<const Core::FVector3> Positions,
    FStaticMeshBounds& OutBounds) noexcept
{
    OutBounds = {};
    if (Positions.empty())
    {
        return EAssetResult::InvalidInput;
    }
    Core::FBox Box;
    for (const Core::FVector3& Position : Positions)
    {
        if (!Core::FMath::IsFinite(Position.X) ||
            !Core::FMath::IsFinite(Position.Y) ||
            !Core::FMath::IsFinite(Position.Z))
        {
            return EAssetResult::MalformedSource;
        }
        Box.AddPoint(Position);
    }
    if (!Box.IsValid())
    {
        return EAssetResult::MalformedSource;
    }
    const Core::FVector3 Center = Box.GetCenter();
    const double DeltaX = std::max(
        std::abs(static_cast<double>(Box.Min.X) - Center.X),
        std::abs(static_cast<double>(Box.Max.X) - Center.X));
    const double DeltaY = std::max(
        std::abs(static_cast<double>(Box.Min.Y) - Center.Y),
        std::abs(static_cast<double>(Box.Max.Y) - Center.Y));
    const double DeltaZ = std::max(
        std::abs(static_cast<double>(Box.Min.Z) - Center.Z),
        std::abs(static_cast<double>(Box.Max.Z) - Center.Z));
    const double RequiredRadius = std::hypot(DeltaX, DeltaY, DeltaZ);
    if (!std::isfinite(RequiredRadius) ||
        RequiredRadius > std::numeric_limits<float>::max())
    {
        return EAssetResult::CapacityExceeded;
    }
    float Radius = static_cast<float>(RequiredRadius);
    if (static_cast<double>(Radius) < RequiredRadius)
        Radius = std::nextafter(Radius, std::numeric_limits<float>::infinity());
    OutBounds.Box = Box;
    OutBounds.Sphere = Core::FSphere(Center, Radius);
    return OutBounds.IsValid()
        ? EAssetResult::Success : EAssetResult::ProcessingFailure;
}

EAssetResult BuildAggregateStaticMeshBounds(
    std::span<const FStaticMeshPrimitive> Primitives,
    FStaticMeshBounds& OutBounds) noexcept
{
    Core::TArray<Core::FVector3> Positions;
    for (const FStaticMeshPrimitive& Primitive : Primitives)
    {
        Positions.insert(
            Positions.end(),
            Primitive.Vertices.Positions.begin(),
            Primitive.Vertices.Positions.end());
    }
    return BuildStaticMeshBounds(Positions, OutBounds);
}

} // namespace Stoner::Asset::Private
