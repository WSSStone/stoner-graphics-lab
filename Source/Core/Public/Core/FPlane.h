#pragma once

#include "Core/FVector3.h"

namespace Stoner::Core
{

enum class EPlaneClassification
{
    Back = -1,
    On = 0,
    Front = 1,
};

// Plane equation is Dot(Normal, Point) - Distance = 0. Front/back
// classification uses the configured tolerance around the plane.
struct FPlane
{
    FVector3 Normal = FVector3::UnitZ();
    float Distance = 0.0f;
    bool bValid = false;

    FPlane() noexcept = default;

    FPlane(const FVector3& InNormal, float InDistance) noexcept
    {
        const FVector3 SafeNormal = InNormal.GetSafeNormal();
        if (SafeNormal != FVector3::Zero())
        {
            Normal = SafeNormal;
            Distance = InDistance;
            bValid = true;
        }
    }

    [[nodiscard]] static FPlane FromPointNormal(const FVector3& Point, const FVector3& InNormal) noexcept
    {
        const FVector3 SafeNormal = InNormal.GetSafeNormal();
        if (SafeNormal == FVector3::Zero())
        {
            return FPlane();
        }
        return FPlane(SafeNormal, SafeNormal.Dot(Point));
    }

    [[nodiscard]] static FPlane FromPoints(
        const FVector3& A,
        const FVector3& B,
        const FVector3& C) noexcept
    {
        const FVector3 NormalFromPoints = (B - A).Cross(C - A).GetSafeNormal();
        if (NormalFromPoints == FVector3::Zero())
        {
            return FPlane();
        }
        return FromPointNormal(A, NormalFromPoints);
    }

    [[nodiscard]] bool IsValid() const noexcept
    {
        return bValid;
    }

    [[nodiscard]] float SignedDistanceTo(const FVector3& Point) const noexcept
    {
        return bValid ? Normal.Dot(Point) - Distance : 0.0f;
    }

    [[nodiscard]] EPlaneClassification ClassifyPoint(
        const FVector3& Point,
        float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        const float SignedDistance = SignedDistanceTo(Point);
        if (SignedDistance > Tolerance)
        {
            return EPlaneClassification::Front;
        }
        if (SignedDistance < -Tolerance)
        {
            return EPlaneClassification::Back;
        }
        return EPlaneClassification::On;
    }
};

} // namespace Stoner::Core
