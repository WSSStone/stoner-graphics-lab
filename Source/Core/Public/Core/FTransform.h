#pragma once

#include "Core/FMatrix4x4.h"
#include "Core/FQuat.h"
#include "Core/FVector3.h"

namespace Stoner::Core
{

// Transform applies scale, then rotation, then translation. Direction/vector
// transforms intentionally ignore translation. Composition, inverse, and
// relative conversion are explicit Try operations because a rotated
// non-uniform scale can introduce shear that this editable TRS value cannot
// represent. Failed operations write Identity and never return an approximation.
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

    [[nodiscard]] bool TryCompose(
        const FTransform& Other,
        FTransform& OutComposed,
        float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        OutComposed = Identity();
        if (!IsFinite() ||
            !Other.IsFinite() ||
            !FMath::IsFinite(Tolerance) ||
            Tolerance < 0.0f)
        {
            return false;
        }

        const FMatrix4x4 ExactMatrix = ToMatrix() * Other.ToMatrix();
        const FTransform Candidate(
            TransformPoint(Other.Translation),
            (Rotation * Other.Rotation).GetSafeNormal(),
            Scale * Other.Scale);
        if (!ExactMatrix.IsFinite() ||
            !Candidate.IsFinite())
        {
            return false;
        }

        if (Candidate.ToMatrix().NearlyEquals(ExactMatrix, Tolerance))
        {
            OutComposed = Candidate;
            return true;
        }
        return TryDecomposeTRS(ExactMatrix, OutComposed, Tolerance);
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
        OutInverse = Identity();
        if (!IsFinite() || !FMath::IsFinite(Tolerance) || Tolerance < 0.0f)
        {
            return false;
        }

        FMatrix4x4 ExactInverse;
        if (!ToMatrix().TryInverse(ExactInverse, Tolerance))
        {
            return false;
        }

        return TryDecomposeTRS(ExactInverse, OutInverse, Tolerance);
    }

    [[nodiscard]] bool TryRelativeTo(
        const FTransform& Parent,
        FTransform& OutRelative,
        float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        OutRelative = Identity();
        if (!IsFinite() ||
            !Parent.IsFinite() ||
            !FMath::IsFinite(Tolerance) ||
            Tolerance < 0.0f)
        {
            return false;
        }

        FMatrix4x4 ParentInverse;
        if (!Parent.ToMatrix().TryInverse(ParentInverse, Tolerance))
        {
            return false;
        }

        const FMatrix4x4 ExactRelative = ParentInverse * ToMatrix();
        return TryDecomposeTRS(ExactRelative, OutRelative, Tolerance);
    }

    [[nodiscard]] bool IsFinite() const noexcept
    {
        return FMath::IsFinite(Translation.X) &&
            FMath::IsFinite(Translation.Y) &&
            FMath::IsFinite(Translation.Z) &&
            Rotation.IsFinite() &&
            FMath::IsFinite(Scale.X) &&
            FMath::IsFinite(Scale.Y) &&
            FMath::IsFinite(Scale.Z);
    }

private:
    [[nodiscard]] static bool TryDecomposeTRS(
        const FMatrix4x4& Matrix,
        FTransform& OutTransform,
        float Tolerance) noexcept
    {
        OutTransform = Identity();
        if (!Matrix.IsFinite() ||
            !FMath::IsFinite(Tolerance) ||
            Tolerance < 0.0f ||
            !FMath::IsNearlyZero(Matrix.M[3][0], Tolerance) ||
            !FMath::IsNearlyZero(Matrix.M[3][1], Tolerance) ||
            !FMath::IsNearlyZero(Matrix.M[3][2], Tolerance) ||
            !FMath::IsNearlyEqual(Matrix.M[3][3], 1.0f, Tolerance))
        {
            return false;
        }

        FVector3 AxisX(Matrix.M[0][0], Matrix.M[1][0], Matrix.M[2][0]);
        FVector3 AxisY(Matrix.M[0][1], Matrix.M[1][1], Matrix.M[2][1]);
        FVector3 AxisZ(Matrix.M[0][2], Matrix.M[1][2], Matrix.M[2][2]);
        FVector3 DecomposedScale(AxisX.Length(), AxisY.Length(), AxisZ.Length());
        if (!FMath::IsFinite(DecomposedScale.X) ||
            !FMath::IsFinite(DecomposedScale.Y) ||
            !FMath::IsFinite(DecomposedScale.Z) ||
            DecomposedScale.X <= Tolerance ||
            DecomposedScale.Y <= Tolerance ||
            DecomposedScale.Z <= Tolerance)
        {
            return false;
        }

        AxisX = AxisX / DecomposedScale.X;
        AxisY = AxisY / DecomposedScale.Y;
        AxisZ = AxisZ / DecomposedScale.Z;
        if (!FMath::IsNearlyZero(AxisX.Dot(AxisY), Tolerance) ||
            !FMath::IsNearlyZero(AxisX.Dot(AxisZ), Tolerance) ||
            !FMath::IsNearlyZero(AxisY.Dot(AxisZ), Tolerance))
        {
            return false;
        }

        float Determinant = AxisX.Dot(AxisY.Cross(AxisZ));
        if (!FMath::IsFinite(Determinant) ||
            !FMath::IsNearlyEqual(FMath::Abs(Determinant), 1.0f, Tolerance))
        {
            return false;
        }
        if (Determinant < 0.0f)
        {
            AxisX = -AxisX;
            DecomposedScale.X = -DecomposedScale.X;
        }

        const FMatrix4x4 RotationMatrix(
            AxisX.X, AxisY.X, AxisZ.X, 0.0f,
            AxisX.Y, AxisY.Y, AxisZ.Y, 0.0f,
            AxisX.Z, AxisY.Z, AxisZ.Z, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);
        const FQuat DecomposedRotation = QuaternionFromRotationMatrix(RotationMatrix);
        const FTransform Candidate(
            FVector3(Matrix.M[0][3], Matrix.M[1][3], Matrix.M[2][3]),
            DecomposedRotation,
            DecomposedScale);
        if (!Candidate.IsFinite() ||
            !Candidate.ToMatrix().NearlyEquals(Matrix, Tolerance))
        {
            return false;
        }

        OutTransform = Candidate;
        return true;
    }

    [[nodiscard]] static FQuat QuaternionFromRotationMatrix(
        const FMatrix4x4& Matrix) noexcept
    {
        const float Trace = Matrix.M[0][0] + Matrix.M[1][1] + Matrix.M[2][2];
        FQuat Result;
        if (Trace > 0.0f)
        {
            const float ScaleFactor = FMath::Sqrt(Trace + 1.0f) * 2.0f;
            Result = FQuat(
                (Matrix.M[2][1] - Matrix.M[1][2]) / ScaleFactor,
                (Matrix.M[0][2] - Matrix.M[2][0]) / ScaleFactor,
                (Matrix.M[1][0] - Matrix.M[0][1]) / ScaleFactor,
                0.25f * ScaleFactor);
        }
        else if (Matrix.M[0][0] > Matrix.M[1][1] &&
            Matrix.M[0][0] > Matrix.M[2][2])
        {
            const float ScaleFactor = FMath::Sqrt(
                1.0f + Matrix.M[0][0] - Matrix.M[1][1] - Matrix.M[2][2]) * 2.0f;
            Result = FQuat(
                0.25f * ScaleFactor,
                (Matrix.M[0][1] + Matrix.M[1][0]) / ScaleFactor,
                (Matrix.M[0][2] + Matrix.M[2][0]) / ScaleFactor,
                (Matrix.M[2][1] - Matrix.M[1][2]) / ScaleFactor);
        }
        else if (Matrix.M[1][1] > Matrix.M[2][2])
        {
            const float ScaleFactor = FMath::Sqrt(
                1.0f + Matrix.M[1][1] - Matrix.M[0][0] - Matrix.M[2][2]) * 2.0f;
            Result = FQuat(
                (Matrix.M[0][1] + Matrix.M[1][0]) / ScaleFactor,
                0.25f * ScaleFactor,
                (Matrix.M[1][2] + Matrix.M[2][1]) / ScaleFactor,
                (Matrix.M[0][2] - Matrix.M[2][0]) / ScaleFactor);
        }
        else
        {
            const float ScaleFactor = FMath::Sqrt(
                1.0f + Matrix.M[2][2] - Matrix.M[0][0] - Matrix.M[1][1]) * 2.0f;
            Result = FQuat(
                (Matrix.M[0][2] + Matrix.M[2][0]) / ScaleFactor,
                (Matrix.M[1][2] + Matrix.M[2][1]) / ScaleFactor,
                0.25f * ScaleFactor,
                (Matrix.M[1][0] - Matrix.M[0][1]) / ScaleFactor);
        }
        return Result.GetSafeNormal();
    }
};

} // namespace Stoner::Core
