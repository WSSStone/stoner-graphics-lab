#pragma once

#include "Core/FVector3.h"

#include <limits>

namespace Stoner::Core
{

// Axis-aligned bounding box. The default box is explicitly invalid/empty and
// becomes valid after adding a point or constructing with ordered min/max values.
struct FBox
{
    FVector3 Min = FVector3(
        std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::infinity());
    FVector3 Max = FVector3(
        -std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity());
    bool bValid = false;

    FBox() noexcept = default;

    FBox(const FVector3& InMin, const FVector3& InMax) noexcept
        : Min(InMin)
        , Max(InMax)
        , bValid(
            InMin.X <= InMax.X &&
            InMin.Y <= InMax.Y &&
            InMin.Z <= InMax.Z)
    {
    }

    [[nodiscard]] bool IsValid() const noexcept
    {
        return bValid;
    }

    void AddPoint(const FVector3& Point) noexcept
    {
        if (!bValid)
        {
            Min = Point;
            Max = Point;
            bValid = true;
            return;
        }

        Min = FVector3(
            FMath::Min(Min.X, Point.X),
            FMath::Min(Min.Y, Point.Y),
            FMath::Min(Min.Z, Point.Z));
        Max = FVector3(
            FMath::Max(Max.X, Point.X),
            FMath::Max(Max.Y, Point.Y),
            FMath::Max(Max.Z, Point.Z));
    }

    void Combine(const FBox& Other) noexcept
    {
        if (!Other.bValid)
        {
            return;
        }

        AddPoint(Other.Min);
        AddPoint(Other.Max);
    }

    [[nodiscard]] bool Contains(const FVector3& Point) const noexcept
    {
        return bValid &&
            Point.X >= Min.X && Point.X <= Max.X &&
            Point.Y >= Min.Y && Point.Y <= Max.Y &&
            Point.Z >= Min.Z && Point.Z <= Max.Z;
    }

    [[nodiscard]] FVector3 GetCenter() const noexcept
    {
        if (!bValid)
        {
            return FVector3::Zero();
        }
        return (Min + Max) * 0.5f;
    }

    [[nodiscard]] FVector3 GetExtent() const noexcept
    {
        if (!bValid)
        {
            return FVector3::Zero();
        }
        return (Max - Min) * 0.5f;
    }
};

} // namespace Stoner::Core
