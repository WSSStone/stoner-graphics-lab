#pragma once

#include "Core/FMath.h"

namespace Stoner::Core
{

struct FVector2
{
    float X = 0.0f;
    float Y = 0.0f;

    constexpr FVector2() noexcept = default;
    constexpr FVector2(float InX, float InY) noexcept
        : X(InX)
        , Y(InY)
    {
    }

    [[nodiscard]] static constexpr FVector2 Zero() noexcept { return FVector2(); }
    [[nodiscard]] static constexpr FVector2 UnitX() noexcept { return FVector2(1.0f, 0.0f); }
    [[nodiscard]] static constexpr FVector2 UnitY() noexcept { return FVector2(0.0f, 1.0f); }

    [[nodiscard]] constexpr FVector2 operator+(const FVector2& Other) const noexcept
    {
        return FVector2(X + Other.X, Y + Other.Y);
    }

    [[nodiscard]] constexpr FVector2 operator-(const FVector2& Other) const noexcept
    {
        return FVector2(X - Other.X, Y - Other.Y);
    }

    [[nodiscard]] constexpr FVector2 operator-() const noexcept
    {
        return FVector2(-X, -Y);
    }

    [[nodiscard]] constexpr FVector2 operator*(float Scalar) const noexcept
    {
        return FVector2(X * Scalar, Y * Scalar);
    }

    [[nodiscard]] constexpr FVector2 operator/(float Scalar) const noexcept
    {
        return FVector2(X / Scalar, Y / Scalar);
    }

    [[nodiscard]] constexpr bool operator==(const FVector2& Other) const noexcept
    {
        return X == Other.X && Y == Other.Y;
    }

    [[nodiscard]] constexpr bool operator!=(const FVector2& Other) const noexcept
    {
        return !(*this == Other);
    }

    [[nodiscard]] constexpr float Dot(const FVector2& Other) const noexcept
    {
        return X * Other.X + Y * Other.Y;
    }

    // Component-wise multiply (Hadamard product)
    [[nodiscard]] constexpr FVector2 operator*(const FVector2& Other) const noexcept
    {
        return FVector2(X * Other.X, Y * Other.Y);
    }

    // Scalar cross product (perp-dot): a.x*b.y - a.y*b.x
    [[nodiscard]] constexpr float Cross(const FVector2& Other) const noexcept
    {
        return X * Other.Y - Y * Other.X;
    }

    [[nodiscard]] constexpr float LengthSquared() const noexcept
    {
        return Dot(*this);
    }

    [[nodiscard]] float Length() const noexcept
    {
        return FMath::Sqrt(LengthSquared());
    }

    [[nodiscard]] FVector2 GetSafeNormal(float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        const float SquareLength = LengthSquared();
        if (SquareLength <= Tolerance * Tolerance)
        {
            return Zero();
        }
        return *this / FMath::Sqrt(SquareLength);
    }

    [[nodiscard]] FVector2 Normalized(float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        return GetSafeNormal(Tolerance);
    }

    [[nodiscard]] bool NearlyEquals(
        const FVector2& Other,
        float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        return FMath::IsNearlyEqual(X, Other.X, Tolerance) &&
            FMath::IsNearlyEqual(Y, Other.Y, Tolerance);
    }
};

[[nodiscard]] constexpr FVector2 operator*(float Scalar, const FVector2& Vector) noexcept
{
    return Vector * Scalar;
}

} // namespace Stoner::Core
