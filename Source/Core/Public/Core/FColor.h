#pragma once

#include "Core/FMath.h"
#include "Core/FPlatformTypes.h"

#include <cmath>

namespace Stoner::Core
{

struct FColorBytes
{
    uint8 R = 0;
    uint8 G = 0;
    uint8 B = 0;
    uint8 A = 255;
};

// RGBA color with normalized float channels. Byte conversion clamps to [0, 1]
// and rounds to the nearest byte value.
struct FColor
{
    float R = 0.0f;
    float G = 0.0f;
    float B = 0.0f;
    float A = 1.0f;

    constexpr FColor() noexcept = default;
    constexpr FColor(float InR, float InG, float InB, float InA = 1.0f) noexcept
        : R(InR)
        , G(InG)
        , B(InB)
        , A(InA)
    {
    }

    [[nodiscard]] static constexpr FColor Transparent() noexcept
    {
        return FColor(0.0f, 0.0f, 0.0f, 0.0f);
    }

    [[nodiscard]] static constexpr FColor OpaqueWhite() noexcept
    {
        return FColor(1.0f, 1.0f, 1.0f, 1.0f);
    }

    [[nodiscard]] static constexpr FColor OpaqueBlack() noexcept
    {
        return FColor(0.0f, 0.0f, 0.0f, 1.0f);
    }

    [[nodiscard]] static FColor FromBytes(uint8 R, uint8 G, uint8 B, uint8 A = 255) noexcept
    {
        constexpr float InvByte = 1.0f / 255.0f;
        return FColor(R * InvByte, G * InvByte, B * InvByte, A * InvByte);
    }

    [[nodiscard]] FColorBytes ToBytes() const noexcept
    {
        return FColorBytes{
            ToByte(R),
            ToByte(G),
            ToByte(B),
            ToByte(A),
        };
    }

    [[nodiscard]] constexpr bool operator==(const FColor& Other) const noexcept
    {
        return R == Other.R && G == Other.G && B == Other.B && A == Other.A;
    }

    [[nodiscard]] bool NearlyEquals(
        const FColor& Other,
        float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        return FMath::IsNearlyEqual(R, Other.R, Tolerance) &&
            FMath::IsNearlyEqual(G, Other.G, Tolerance) &&
            FMath::IsNearlyEqual(B, Other.B, Tolerance) &&
            FMath::IsNearlyEqual(A, Other.A, Tolerance);
    }

private:
    [[nodiscard]] static uint8 ToByte(float Value) noexcept
    {
        const float Clamped = FMath::Clamp(Value, 0.0f, 1.0f);
        return static_cast<uint8>(std::lround(Clamped * 255.0f));
    }
};

} // namespace Stoner::Core
