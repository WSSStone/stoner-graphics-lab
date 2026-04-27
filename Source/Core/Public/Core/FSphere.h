#pragma once

#include "Core/FVector3.h"

namespace Stoner::Core
{

// Bounding sphere with deterministic invalid state for negative radii.
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
        return Radius >= 0.0f;
    }

    [[nodiscard]] bool Contains(
        const FVector3& Point,
        float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        if (!IsValid())
        {
            return false;
        }

        const float AllowedRadius = Radius + Tolerance;
        return (Point - Center).LengthSquared() <= AllowedRadius * AllowedRadius;
    }
};

} // namespace Stoner::Core
