#pragma once

#include "Core/FVector3.h"

namespace Stoner::Core
{

// Bounding sphere with deterministic invalid state for non-finite centers,
// negative radii, or non-finite radii. Containment rejects non-finite points
// and invalid tolerances.
struct FSphere
{
    FVector3 Center = FVector3::Zero();
    float Radius = -1.0f;

    constexpr FSphere() noexcept = default;
    constexpr FSphere(const FVector3& InCenter, float InRadius) noexcept
        : Center(InCenter)
        , Radius(InRadius)
    {
    }

    [[nodiscard]] bool IsValid() const noexcept
    {
        return IsFiniteVector(Center) &&
            FMath::IsFinite(Radius) &&
            Radius >= 0.0f;
    }

    [[nodiscard]] bool Contains(
        const FVector3& Point,
        float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        if (!IsValid() ||
            !IsFiniteVector(Point) ||
            !FMath::IsFinite(Tolerance) ||
            Tolerance < 0.0f)
        {
            return false;
        }

        const double DeltaX = static_cast<double>(Point.X) - static_cast<double>(Center.X);
        const double DeltaY = static_cast<double>(Point.Y) - static_cast<double>(Center.Y);
        const double DeltaZ = static_cast<double>(Point.Z) - static_cast<double>(Center.Z);
        const double Distance = std::hypot(DeltaX, DeltaY, DeltaZ);
        const double AllowedRadius =
            static_cast<double>(Radius) + static_cast<double>(Tolerance);
        return Distance <= AllowedRadius;
    }

private:
    [[nodiscard]] static bool IsFiniteVector(const FVector3& Value) noexcept
    {
        return FMath::IsFinite(Value.X) &&
            FMath::IsFinite(Value.Y) &&
            FMath::IsFinite(Value.Z);
    }
};

} // namespace Stoner::Core
