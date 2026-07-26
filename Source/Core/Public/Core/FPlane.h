#pragma once

#include "Core/FVector3.h"

#include <limits>

namespace Stoner::Core
{

enum class EPlaneClassification
{
    Back = -1,
    On = 0,
    Front = 1,
};

// Plane equation is Dot(Normal, Point) - Distance = 0. The normal/distance
// constructor treats both arguments as equation coefficients and normalizes
// both together. Invalid planes, points, or tolerances classify as On.
struct FPlane
{
    FVector3 Normal = FVector3::UnitZ();
    float Distance = 0.0f;
    bool bValid = false;

    FPlane() noexcept = default;

    FPlane(const FVector3& InNormal, float InDistance) noexcept
    {
        if (!IsFiniteVector(InNormal) || !FMath::IsFinite(InDistance))
        {
            return;
        }

        const double NormalLength = std::hypot(
            static_cast<double>(InNormal.X),
            static_cast<double>(InNormal.Y),
            static_cast<double>(InNormal.Z));
        const FVector3 SafeNormal = InNormal.GetSafeNormal();
        const double NormalizedDistance =
            static_cast<double>(InDistance) / NormalLength;
        const double MaxFloat = static_cast<double>(std::numeric_limits<float>::max());
        if (SafeNormal != FVector3::Zero() &&
            std::isfinite(NormalizedDistance) &&
            NormalizedDistance <= MaxFloat &&
            NormalizedDistance >= -MaxFloat)
        {
            Normal = SafeNormal;
            Distance = static_cast<float>(NormalizedDistance);
            bValid = true;
        }
    }

    [[nodiscard]] static FPlane FromPointNormal(const FVector3& Point, const FVector3& InNormal) noexcept
    {
        if (!IsFiniteVector(Point))
        {
            return FPlane();
        }

        const FVector3 SafeNormal = InNormal.GetSafeNormal();
        if (SafeNormal == FVector3::Zero())
        {
            return FPlane();
        }

        float PlaneDistance = 0.0f;
        if (!TryDot(SafeNormal, Point, PlaneDistance))
        {
            return FPlane();
        }
        return FPlane(SafeNormal, PlaneDistance);
    }

    [[nodiscard]] static FPlane FromPoints(
        const FVector3& A,
        const FVector3& B,
        const FVector3& C) noexcept
    {
        FVector3 NormalFromPoints;
        if (!TryNormalFromPoints(A, B, C, NormalFromPoints))
        {
            return FPlane();
        }
        return FromPointNormal(A, NormalFromPoints);
    }

    [[nodiscard]] bool IsValid() const noexcept
    {
        return bValid &&
            IsFiniteVector(Normal) &&
            FMath::IsFinite(Distance) &&
            FMath::IsNearlyEqual(Normal.Length(), 1.0f);
    }

    [[nodiscard]] float SignedDistanceTo(const FVector3& Point) const noexcept
    {
        if (!IsValid() || !IsFiniteVector(Point))
        {
            return 0.0f;
        }

        const double SignedDistance =
            static_cast<double>(Normal.X) * static_cast<double>(Point.X) +
            static_cast<double>(Normal.Y) * static_cast<double>(Point.Y) +
            static_cast<double>(Normal.Z) * static_cast<double>(Point.Z) -
            static_cast<double>(Distance);
        const double MaxFloat = static_cast<double>(std::numeric_limits<float>::max());
        if (SignedDistance > MaxFloat)
        {
            return std::numeric_limits<float>::max();
        }
        if (SignedDistance < -MaxFloat)
        {
            return -std::numeric_limits<float>::max();
        }
        return static_cast<float>(SignedDistance);
    }

    [[nodiscard]] EPlaneClassification ClassifyPoint(
        const FVector3& Point,
        float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        if (!IsValid() ||
            !IsFiniteVector(Point) ||
            !FMath::IsFinite(Tolerance) ||
            Tolerance < 0.0f)
        {
            return EPlaneClassification::On;
        }

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

private:
    [[nodiscard]] static bool IsFiniteVector(const FVector3& Value) noexcept
    {
        return FMath::IsFinite(Value.X) &&
            FMath::IsFinite(Value.Y) &&
            FMath::IsFinite(Value.Z);
    }

    [[nodiscard]] static bool TryDot(
        const FVector3& Left,
        const FVector3& Right,
        float& OutDot) noexcept
    {
        const double Dot =
            static_cast<double>(Left.X) * static_cast<double>(Right.X) +
            static_cast<double>(Left.Y) * static_cast<double>(Right.Y) +
            static_cast<double>(Left.Z) * static_cast<double>(Right.Z);
        const double MaxFloat = static_cast<double>(std::numeric_limits<float>::max());
        if (!std::isfinite(Dot) || Dot > MaxFloat || Dot < -MaxFloat)
        {
            OutDot = 0.0f;
            return false;
        }
        OutDot = static_cast<float>(Dot);
        return true;
    }

    [[nodiscard]] static bool TryNormalFromPoints(
        const FVector3& A,
        const FVector3& B,
        const FVector3& C,
        FVector3& OutNormal) noexcept
    {
        OutNormal = FVector3::Zero();
        if (!IsFiniteVector(A) || !IsFiniteVector(B) || !IsFiniteVector(C))
        {
            return false;
        }

        const double ABX = static_cast<double>(B.X) - static_cast<double>(A.X);
        const double ABY = static_cast<double>(B.Y) - static_cast<double>(A.Y);
        const double ABZ = static_cast<double>(B.Z) - static_cast<double>(A.Z);
        const double ACX = static_cast<double>(C.X) - static_cast<double>(A.X);
        const double ACY = static_cast<double>(C.Y) - static_cast<double>(A.Y);
        const double ACZ = static_cast<double>(C.Z) - static_cast<double>(A.Z);
        const double CrossX = ABY * ACZ - ABZ * ACY;
        const double CrossY = ABZ * ACX - ABX * ACZ;
        const double CrossZ = ABX * ACY - ABY * ACX;
        const double CrossLength = std::hypot(CrossX, CrossY, CrossZ);
        if (!std::isfinite(CrossLength) ||
            CrossLength <= static_cast<double>(FMath::DefaultTolerance))
        {
            return false;
        }

        OutNormal = FVector3(
            static_cast<float>(CrossX / CrossLength),
            static_cast<float>(CrossY / CrossLength),
            static_cast<float>(CrossZ / CrossLength));
        return IsFiniteVector(OutNormal);
    }
};

} // namespace Stoner::Core
