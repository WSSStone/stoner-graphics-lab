#pragma once

#include "Core/FMatrix4x4.h"
#include "Core/FVector3.h"

namespace Stoner::Core
{

// Unit quaternions represent right-handed rotations. Composition follows the
// Hamilton product; `A * B` applies B first, then A, when rotating vectors.
struct FQuat
{
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float W = 1.0f;

    constexpr FQuat() noexcept = default;
    constexpr FQuat(float InX, float InY, float InZ, float InW) noexcept
        : X(InX)
        , Y(InY)
        , Z(InZ)
        , W(InW)
    {
    }

    [[nodiscard]] static constexpr FQuat Identity() noexcept
    {
        return FQuat();
    }

    [[nodiscard]] static FQuat FromAxisAngle(const FVector3& Axis, float Radians) noexcept
    {
        const FVector3 NormalizedAxis = Axis.GetSafeNormal();
        if (NormalizedAxis == FVector3::Zero())
        {
            return Identity();
        }

        const float HalfAngle = Radians * 0.5f;
        const float Sine = FMath::Sin(HalfAngle);
        return FQuat(
            NormalizedAxis.X * Sine,
            NormalizedAxis.Y * Sine,
            NormalizedAxis.Z * Sine,
            FMath::Cos(HalfAngle));
    }

    [[nodiscard]] constexpr bool operator==(const FQuat& Other) const noexcept
    {
        return X == Other.X && Y == Other.Y && Z == Other.Z && W == Other.W;
    }

    [[nodiscard]] FQuat operator*(const FQuat& Other) const noexcept
    {
        return FQuat(
            W * Other.X + X * Other.W + Y * Other.Z - Z * Other.Y,
            W * Other.Y - X * Other.Z + Y * Other.W + Z * Other.X,
            W * Other.Z + X * Other.Y - Y * Other.X + Z * Other.W,
            W * Other.W - X * Other.X - Y * Other.Y - Z * Other.Z);
    }

    [[nodiscard]] float LengthSquared() const noexcept
    {
        return X * X + Y * Y + Z * Z + W * W;
    }

    [[nodiscard]] float Length() const noexcept
    {
        return FMath::Sqrt(LengthSquared());
    }

    [[nodiscard]] FQuat GetSafeNormal(float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        const float SquareLength = LengthSquared();
        if (SquareLength <= Tolerance * Tolerance)
        {
            return Identity();
        }

        const float InvLength = 1.0f / FMath::Sqrt(SquareLength);
        return FQuat(X * InvLength, Y * InvLength, Z * InvLength, W * InvLength);
    }

    [[nodiscard]] FQuat Normalized(float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        return GetSafeNormal(Tolerance);
    }

    [[nodiscard]] constexpr FQuat Conjugated() const noexcept
    {
        return FQuat(-X, -Y, -Z, W);
    }

    [[nodiscard]] FQuat Inversed(float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        const float SquareLength = LengthSquared();
        if (SquareLength <= Tolerance * Tolerance)
        {
            return Identity();
        }

        const FQuat Conjugate = Conjugated();
        return FQuat(
            Conjugate.X / SquareLength,
            Conjugate.Y / SquareLength,
            Conjugate.Z / SquareLength,
            Conjugate.W / SquareLength);
    }

    [[nodiscard]] FVector3 RotateVector(const FVector3& Vector) const noexcept
    {
        const FQuat NormalizedSelf = GetSafeNormal();
        const FQuat VectorQuat(Vector.X, Vector.Y, Vector.Z, 0.0f);
        const FQuat Rotated = NormalizedSelf * VectorQuat * NormalizedSelf.Inversed();
        return FVector3(Rotated.X, Rotated.Y, Rotated.Z);
    }

    [[nodiscard]] FMatrix4x4 ToMatrix() const noexcept
    {
        const FQuat Q = GetSafeNormal();
        const float XX = Q.X * Q.X;
        const float YY = Q.Y * Q.Y;
        const float ZZ = Q.Z * Q.Z;
        const float XY = Q.X * Q.Y;
        const float XZ = Q.X * Q.Z;
        const float YZ = Q.Y * Q.Z;
        const float WX = Q.W * Q.X;
        const float WY = Q.W * Q.Y;
        const float WZ = Q.W * Q.Z;

        return FMatrix4x4(
            1.0f - 2.0f * (YY + ZZ), 2.0f * (XY - WZ), 2.0f * (XZ + WY), 0.0f,
            2.0f * (XY + WZ), 1.0f - 2.0f * (XX + ZZ), 2.0f * (YZ - WX), 0.0f,
            2.0f * (XZ - WY), 2.0f * (YZ + WX), 1.0f - 2.0f * (XX + YY), 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);
    }

    [[nodiscard]] bool NearlyEquals(
        const FQuat& Other,
        float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        return FMath::IsNearlyEqual(X, Other.X, Tolerance) &&
            FMath::IsNearlyEqual(Y, Other.Y, Tolerance) &&
            FMath::IsNearlyEqual(Z, Other.Z, Tolerance) &&
            FMath::IsNearlyEqual(W, Other.W, Tolerance);
    }
};

} // namespace Stoner::Core
