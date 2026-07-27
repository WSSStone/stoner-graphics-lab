#pragma once

#include "Core/FMath.h"

namespace Stoner::Core
{

struct FVector3
{
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;

    constexpr FVector3() noexcept = default;
    constexpr FVector3(float InX, float InY, float InZ) noexcept
        : X(InX)
        , Y(InY)
        , Z(InZ)
    {
    }

    [[nodiscard]] static constexpr FVector3 Zero() noexcept { return FVector3(); }
    [[nodiscard]] static constexpr FVector3 UnitX() noexcept { return FVector3(1.0f, 0.0f, 0.0f); }
    [[nodiscard]] static constexpr FVector3 UnitY() noexcept { return FVector3(0.0f, 1.0f, 0.0f); }
    [[nodiscard]] static constexpr FVector3 UnitZ() noexcept { return FVector3(0.0f, 0.0f, 1.0f); }

    [[nodiscard]] constexpr FVector3 operator+(const FVector3& Other) const noexcept
    {
        return FVector3(X + Other.X, Y + Other.Y, Z + Other.Z);
    }

    [[nodiscard]] constexpr FVector3 operator-(const FVector3& Other) const noexcept
    {
        return FVector3(X - Other.X, Y - Other.Y, Z - Other.Z);
    }

    [[nodiscard]] constexpr FVector3 operator-() const noexcept
    {
        return FVector3(-X, -Y, -Z);
    }

    [[nodiscard]] constexpr FVector3 operator*(float Scalar) const noexcept
    {
        return FVector3(X * Scalar, Y * Scalar, Z * Scalar);
    }

    [[nodiscard]] constexpr FVector3 operator*(const FVector3& Other) const noexcept
    {
        return FVector3(X * Other.X, Y * Other.Y, Z * Other.Z);
    }

    [[nodiscard]] constexpr FVector3 operator/(float Scalar) const noexcept
    {
        return FVector3(X / Scalar, Y / Scalar, Z / Scalar);
    }

    [[nodiscard]] constexpr bool operator==(const FVector3& Other) const noexcept
    {
        return X == Other.X && Y == Other.Y && Z == Other.Z;
    }

    [[nodiscard]] constexpr bool operator!=(const FVector3& Other) const noexcept
    {
        return !(*this == Other);
    }

    [[nodiscard]] constexpr float Dot(const FVector3& Other) const noexcept
    {
        return X * Other.X + Y * Other.Y + Z * Other.Z;
    }

    [[nodiscard]] constexpr FVector3 Cross(const FVector3& Other) const noexcept
    {
        return FVector3(
            Y * Other.Z - Z * Other.Y,
            Z * Other.X - X * Other.Z,
            X * Other.Y - Y * Other.X);
    }

    [[nodiscard]] constexpr float LengthSquared() const noexcept
    {
        return Dot(*this);
    }

    [[nodiscard]] float Length() const noexcept
    {
        return std::hypot(X, Y, Z);
    }

    // Returns Zero for near-zero vectors, non-finite components, or invalid
    // tolerances. Finite large components are normalized without squaring
    // overflow.
    [[nodiscard]] FVector3 GetSafeNormal(float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        if (!FMath::IsFinite(X) ||
            !FMath::IsFinite(Y) ||
            !FMath::IsFinite(Z) ||
            !FMath::IsFinite(Tolerance) ||
            Tolerance < 0.0f)
        {
            return Zero();
        }

        const float Scale = FMath::Max(FMath::Max(FMath::Abs(X), FMath::Abs(Y)), FMath::Abs(Z));
        if (Scale == 0.0f)
        {
            return Zero();
        }

        const FVector3 Scaled = *this / Scale;
        const float ScaledLength = Scaled.Length();
        if (Scale * ScaledLength <= Tolerance)
        {
            return Zero();
        }
        return Scaled / ScaledLength;
    }

    [[nodiscard]] FVector3 Normalized(float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        return GetSafeNormal(Tolerance);
    }

    [[nodiscard]] bool NearlyEquals(
        const FVector3& Other,
        float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        return FMath::IsNearlyEqual(X, Other.X, Tolerance) &&
            FMath::IsNearlyEqual(Y, Other.Y, Tolerance) &&
            FMath::IsNearlyEqual(Z, Other.Z, Tolerance);
    }
};

[[nodiscard]] constexpr FVector3 operator*(float Scalar, const FVector3& Vector) noexcept
{
    return Vector * Scalar;
}

} // namespace Stoner::Core
