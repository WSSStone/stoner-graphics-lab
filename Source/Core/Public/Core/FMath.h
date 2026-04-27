#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace Stoner::Core
{

// Core math uses a right-handed coordinate convention. Matrix values are stored
// in row-major memory order, and computed floating-point results should be
// compared through the tolerance helpers below instead of exact equality.
class FMath
{
public:
    static constexpr float Pi = 3.14159265358979323846f;
    static constexpr float TwoPi = Pi * 2.0f;
    static constexpr float HalfPi = Pi * 0.5f;
    static constexpr float DefaultTolerance = 1.0e-5f;

    template <typename T>
    [[nodiscard]] static constexpr T Min(T A, T B) noexcept
    {
        return A < B ? A : B;
    }

    template <typename T>
    [[nodiscard]] static constexpr T Max(T A, T B) noexcept
    {
        return A > B ? A : B;
    }

    template <typename T>
    [[nodiscard]] static constexpr T Clamp(T Value, T MinValue, T MaxValue) noexcept
    {
        return Max(MinValue, Min(Value, MaxValue));
    }

    [[nodiscard]] static constexpr float Abs(float Value) noexcept
    {
        return Value < 0.0f ? -Value : Value;
    }

    [[nodiscard]] static constexpr float Lerp(float A, float B, float Alpha) noexcept
    {
        return A + (B - A) * Alpha;
    }

    [[nodiscard]] static constexpr float DegreesToRadians(float Degrees) noexcept
    {
        return Degrees * (Pi / 180.0f);
    }

    [[nodiscard]] static constexpr float RadiansToDegrees(float Radians) noexcept
    {
        return Radians * (180.0f / Pi);
    }

    [[nodiscard]] static float Sin(float Radians) noexcept
    {
        return std::sin(Radians);
    }

    [[nodiscard]] static float Cos(float Radians) noexcept
    {
        return std::cos(Radians);
    }

    [[nodiscard]] static float Tan(float Radians) noexcept
    {
        return std::tan(Radians);
    }

    [[nodiscard]] static float Sqrt(float Value) noexcept
    {
        return std::sqrt(Value);
    }

    [[nodiscard]] static bool IsNearlyEqual(
        float A,
        float B,
        float Tolerance = DefaultTolerance) noexcept
    {
        return Abs(A - B) <= Tolerance;
    }

    [[nodiscard]] static bool IsNearlyZero(
        float Value,
        float Tolerance = DefaultTolerance) noexcept
    {
        return Abs(Value) <= Tolerance;
    }

    [[nodiscard]] static bool IsFinite(float Value) noexcept
    {
        return std::isfinite(Value);
    }
};

} // namespace Stoner::Core
