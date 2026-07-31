#pragma once

#include "Core/FMatrix4x4.h"
#include "Core/FVector3.h"

namespace Stoner::Core
{

// Unit quaternions use Hamilton-product component algebra; `A * B` applies B
// first, then A, when rotating vectors in the engine's named world convention.
// Safe normalization and inversion return Identity for non-finite components,
// invalid tolerances, or near-zero magnitudes. A quaternion and its negation
// compare as the same rotation through NearlyEquals.
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
        if (!FMath::IsFinite(Radians))
        {
            return Identity();
        }

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
        return std::hypot(std::hypot(X, Y), std::hypot(Z, W));
    }

    [[nodiscard]] FQuat GetSafeNormal(float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        if (!IsFinite() || !FMath::IsFinite(Tolerance) || Tolerance < 0.0f)
        {
            return Identity();
        }

        const float Scale = FMath::Max(
            FMath::Max(FMath::Abs(X), FMath::Abs(Y)),
            FMath::Max(FMath::Abs(Z), FMath::Abs(W)));
        if (Scale == 0.0f)
        {
            return Identity();
        }

        const FQuat Scaled(X / Scale, Y / Scale, Z / Scale, W / Scale);
        const float ScaledLength = Scaled.Length();
        if (Scale <= Tolerance / ScaledLength)
        {
            return Identity();
        }
        return FQuat(
            Scaled.X / ScaledLength,
            Scaled.Y / ScaledLength,
            Scaled.Z / ScaledLength,
            Scaled.W / ScaledLength);
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
        if (!IsFinite() || !FMath::IsFinite(Tolerance) || Tolerance < 0.0f)
        {
            return Identity();
        }

        const float Scale = FMath::Max(
            FMath::Max(FMath::Abs(X), FMath::Abs(Y)),
            FMath::Max(FMath::Abs(Z), FMath::Abs(W)));
        if (Scale == 0.0f)
        {
            return Identity();
        }

        const FQuat Scaled(X / Scale, Y / Scale, Z / Scale, W / Scale);
        const float ScaledSquareLength = Scaled.LengthSquared();
        const float ScaledLength = FMath::Sqrt(ScaledSquareLength);
        if (Scale <= Tolerance / ScaledLength)
        {
            return Identity();
        }

        const float InverseFactor = (1.0f / Scale) / ScaledSquareLength;
        const FQuat Conjugate = Scaled.Conjugated();
        return FQuat(
            Conjugate.X * InverseFactor,
            Conjugate.Y * InverseFactor,
            Conjugate.Z * InverseFactor,
            Conjugate.W * InverseFactor);
    }

    [[nodiscard]] FVector3 RotateVector(const FVector3& Vector) const noexcept
    {
        const FQuat NormalizedSelf = GetSafeNormal();
        const FQuat VectorQuat(Vector.X, Vector.Y, Vector.Z, 0.0f);
        const FQuat Rotated = NormalizedSelf * VectorQuat * NormalizedSelf.Conjugated();
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
        const bool bDirect = FMath::IsNearlyEqual(X, Other.X, Tolerance) &&
            FMath::IsNearlyEqual(Y, Other.Y, Tolerance) &&
            FMath::IsNearlyEqual(Z, Other.Z, Tolerance) &&
            FMath::IsNearlyEqual(W, Other.W, Tolerance);
        const bool bNegated = FMath::IsNearlyEqual(X, -Other.X, Tolerance) &&
            FMath::IsNearlyEqual(Y, -Other.Y, Tolerance) &&
            FMath::IsNearlyEqual(Z, -Other.Z, Tolerance) &&
            FMath::IsNearlyEqual(W, -Other.W, Tolerance);
        return bDirect || bNegated;
    }

    [[nodiscard]] bool IsFinite() const noexcept
    {
        return FMath::IsFinite(X) &&
            FMath::IsFinite(Y) &&
            FMath::IsFinite(Z) &&
            FMath::IsFinite(W);
    }
};

} // namespace Stoner::Core
