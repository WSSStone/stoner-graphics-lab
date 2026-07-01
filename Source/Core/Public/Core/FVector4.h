#pragma once

#include "Core/FMath.h"

namespace Stoner::Core
{

struct FVector4
{
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float W = 0.0f;

    constexpr FVector4() noexcept = default;
    constexpr FVector4(float InX, float InY, float InZ, float InW) noexcept
        : X(InX)
        , Y(InY)
        , Z(InZ)
        , W(InW)
    {
    }

    [[nodiscard]] static constexpr FVector4 Zero() noexcept { return FVector4(); }

    [[nodiscard]] constexpr FVector4 operator+(const FVector4& Other) const noexcept
    {
        return FVector4(X + Other.X, Y + Other.Y, Z + Other.Z, W + Other.W);
    }

    [[nodiscard]] constexpr FVector4 operator-(const FVector4& Other) const noexcept
    {
        return FVector4(X - Other.X, Y - Other.Y, Z - Other.Z, W - Other.W);
    }

    [[nodiscard]] constexpr FVector4 operator-() const noexcept
    {
        return FVector4(-X, -Y, -Z, -W);
    }

    [[nodiscard]] constexpr FVector4 operator*(float Scalar) const noexcept
    {
        return FVector4(X * Scalar, Y * Scalar, Z * Scalar, W * Scalar);
    }

    [[nodiscard]] constexpr FVector4 operator/(float Scalar) const noexcept
    {
        return FVector4(X / Scalar, Y / Scalar, Z / Scalar, W / Scalar);
    }

    [[nodiscard]] constexpr bool operator==(const FVector4& Other) const noexcept
    {
        return X == Other.X && Y == Other.Y && Z == Other.Z && W == Other.W;
    }

    [[nodiscard]] constexpr bool operator!=(const FVector4& Other) const noexcept
    {
        return !(*this == Other);
    }

    [[nodiscard]] constexpr float Dot(const FVector4& Other) const noexcept
    {
        return X * Other.X + Y * Other.Y + Z * Other.Z + W * Other.W;
    }

    [[nodiscard]] constexpr float LengthSquared() const noexcept
    {
        return Dot(*this);
    }

    [[nodiscard]] float Length() const noexcept
    {
        return FMath::Sqrt(LengthSquared());
    }

    [[nodiscard]] FVector4 GetSafeNormal(float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        const float SquareLength = LengthSquared();
        if (SquareLength <= Tolerance * Tolerance)
        {
            return Zero();
        }
        return *this / FMath::Sqrt(SquareLength);
    }

    [[nodiscard]] FVector4 Normalized(float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        return GetSafeNormal(Tolerance);
    }

    [[nodiscard]] bool NearlyEquals(
        const FVector4& Other,
        float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        return FMath::IsNearlyEqual(X, Other.X, Tolerance) &&
            FMath::IsNearlyEqual(Y, Other.Y, Tolerance) &&
            FMath::IsNearlyEqual(Z, Other.Z, Tolerance) &&
            FMath::IsNearlyEqual(W, Other.W, Tolerance);
    }
};

[[nodiscard]] constexpr FVector4 operator*(float Scalar, const FVector4& Vector) noexcept
{
    return Vector * Scalar;
}

} // namespace Stoner::Core
