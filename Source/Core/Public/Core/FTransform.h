#pragma once

#include "Core/FMatrix4x4.h"
#include "Core/FQuat.h"
#include "Core/FVector3.h"

namespace Stoner::Core
{

// Transform applies scale, then rotation, then translation. Direction/vector
// transforms intentionally ignore translation.
struct FTransform
{
    FVector3 Translation = FVector3::Zero();
    FQuat Rotation = FQuat::Identity();
    FVector3 Scale = FVector3(1.0f, 1.0f, 1.0f);

    constexpr FTransform() noexcept = default;
    constexpr FTransform(
        const FVector3& InTranslation,
        const FQuat& InRotation,
        const FVector3& InScale = FVector3(1.0f, 1.0f, 1.0f)) noexcept
        : Translation(InTranslation)
        , Rotation(InRotation)
        , Scale(InScale)
    {
    }

    [[nodiscard]] static constexpr FTransform Identity() noexcept
    {
        return FTransform();
    }

    [[nodiscard]] FVector3 TransformPoint(const FVector3& Point) const noexcept
    {
        return Rotation.RotateVector(Point * Scale) + Translation;
    }

    [[nodiscard]] FVector3 TransformVector(const FVector3& Vector) const noexcept
    {
        return Rotation.RotateVector(Vector * Scale);
    }

    [[nodiscard]] FTransform operator*(const FTransform& Other) const noexcept
    {
        return FTransform(
            TransformPoint(Other.Translation),
            (Rotation * Other.Rotation).GetSafeNormal(),
            Scale * Other.Scale);
    }

    [[nodiscard]] FMatrix4x4 ToMatrix() const noexcept
    {
        FMatrix4x4 Result = Rotation.ToMatrix() * FMatrix4x4::Scale(Scale);
        Result.M[0][3] = Translation.X;
        Result.M[1][3] = Translation.Y;
        Result.M[2][3] = Translation.Z;
        return Result;
    }

    [[nodiscard]] bool TryInverse(
        FTransform& OutInverse,
        float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        if (FMath::IsNearlyZero(Scale.X, Tolerance) ||
            FMath::IsNearlyZero(Scale.Y, Tolerance) ||
            FMath::IsNearlyZero(Scale.Z, Tolerance))
        {
            OutInverse = Identity();
            return false;
        }

        const FVector3 InvScale(1.0f / Scale.X, 1.0f / Scale.Y, 1.0f / Scale.Z);
        const FQuat InvRotation = Rotation.Inversed();
        const FVector3 InvTranslation = InvRotation.RotateVector(-Translation) * InvScale;
        OutInverse = FTransform(InvTranslation, InvRotation, InvScale);
        return true;
    }
};

} // namespace Stoner::Core
