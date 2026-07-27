#pragma once

#include "Core/FMath.h"
#include "Core/FVector3.h"
#include "Core/FVector4.h"

#include <algorithm>

namespace Stoner::Core
{

// Row-major 4x4 matrix. Point transforms use an implicit column vector with
// translation in the last column; direction transforms use W=0 and ignore translation.
struct FMatrix4x4
{
    float M[4][4] = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };

    constexpr FMatrix4x4() noexcept = default;

    constexpr FMatrix4x4(
        float M00, float M01, float M02, float M03,
        float M10, float M11, float M12, float M13,
        float M20, float M21, float M22, float M23,
        float M30, float M31, float M32, float M33) noexcept
        : M{
            {M00, M01, M02, M03},
            {M10, M11, M12, M13},
            {M20, M21, M22, M23},
            {M30, M31, M32, M33},
        }
    {
    }

    [[nodiscard]] static constexpr FMatrix4x4 Identity() noexcept
    {
        return FMatrix4x4();
    }

    [[nodiscard]] static constexpr FMatrix4x4 Zero() noexcept
    {
        return FMatrix4x4(
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f);
    }

    [[nodiscard]] static constexpr FMatrix4x4 Translation(const FVector3& Value) noexcept
    {
        return FMatrix4x4(
            1.0f, 0.0f, 0.0f, Value.X,
            0.0f, 1.0f, 0.0f, Value.Y,
            0.0f, 0.0f, 1.0f, Value.Z,
            0.0f, 0.0f, 0.0f, 1.0f);
    }

    [[nodiscard]] static constexpr FMatrix4x4 Scale(const FVector3& Value) noexcept
    {
        return FMatrix4x4(
            Value.X, 0.0f, 0.0f, 0.0f,
            0.0f, Value.Y, 0.0f, 0.0f,
            0.0f, 0.0f, Value.Z, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);
    }

    [[nodiscard]] constexpr float At(int Row, int Column) const noexcept
    {
        return M[Row][Column];
    }

    [[nodiscard]] constexpr bool operator==(const FMatrix4x4& Other) const noexcept
    {
        for (int Row = 0; Row < 4; ++Row)
        {
            for (int Column = 0; Column < 4; ++Column)
            {
                if (M[Row][Column] != Other.M[Row][Column])
                {
                    return false;
                }
            }
        }
        return true;
    }

    [[nodiscard]] bool NearlyEquals(
        const FMatrix4x4& Other,
        float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        for (int Row = 0; Row < 4; ++Row)
        {
            for (int Column = 0; Column < 4; ++Column)
            {
                if (!FMath::IsNearlyEqual(M[Row][Column], Other.M[Row][Column], Tolerance))
                {
                    return false;
                }
            }
        }
        return true;
    }

    [[nodiscard]] bool IsFinite() const noexcept
    {
        for (int Row = 0; Row < 4; ++Row)
        {
            for (int Column = 0; Column < 4; ++Column)
            {
                if (!FMath::IsFinite(M[Row][Column]))
                {
                    return false;
                }
            }
        }
        return true;
    }

    [[nodiscard]] FMatrix4x4 operator*(const FMatrix4x4& Other) const noexcept
    {
        FMatrix4x4 Result = Zero();
        for (int Row = 0; Row < 4; ++Row)
        {
            for (int Column = 0; Column < 4; ++Column)
            {
                for (int Index = 0; Index < 4; ++Index)
                {
                    Result.M[Row][Column] += M[Row][Index] * Other.M[Index][Column];
                }
            }
        }
        return Result;
    }

    [[nodiscard]] FMatrix4x4 Transposed() const noexcept
    {
        FMatrix4x4 Result = Zero();
        for (int Row = 0; Row < 4; ++Row)
        {
            for (int Column = 0; Column < 4; ++Column)
            {
                Result.M[Row][Column] = M[Column][Row];
            }
        }
        return Result;
    }

    [[nodiscard]] FVector4 TransformVector4(const FVector4& Value) const noexcept
    {
        return FVector4(
            M[0][0] * Value.X + M[0][1] * Value.Y + M[0][2] * Value.Z + M[0][3] * Value.W,
            M[1][0] * Value.X + M[1][1] * Value.Y + M[1][2] * Value.Z + M[1][3] * Value.W,
            M[2][0] * Value.X + M[2][1] * Value.Y + M[2][2] * Value.Z + M[2][3] * Value.W,
            M[3][0] * Value.X + M[3][1] * Value.Y + M[3][2] * Value.Z + M[3][3] * Value.W);
    }

    [[nodiscard]] FVector3 TransformPoint(const FVector3& Value) const noexcept
    {
        const FVector4 Result = TransformVector4(FVector4(Value.X, Value.Y, Value.Z, 1.0f));
        if (!FMath::IsNearlyZero(Result.W) && !FMath::IsNearlyEqual(Result.W, 1.0f))
        {
            return FVector3(Result.X / Result.W, Result.Y / Result.W, Result.Z / Result.W);
        }
        return FVector3(Result.X, Result.Y, Result.Z);
    }

    [[nodiscard]] FVector3 TransformVector(const FVector3& Value) const noexcept
    {
        const FVector4 Result = TransformVector4(FVector4(Value.X, Value.Y, Value.Z, 0.0f));
        return FVector3(Result.X, Result.Y, Result.Z);
    }

    [[nodiscard]] bool TryInverse(FMatrix4x4& OutInverse, float Tolerance = FMath::DefaultTolerance) const noexcept
    {
        OutInverse = Identity();
        if (!IsFinite() || !FMath::IsFinite(Tolerance) || Tolerance < 0.0f)
        {
            return false;
        }

        float Augmented[4][8] = {};
        for (int Row = 0; Row < 4; ++Row)
        {
            for (int Column = 0; Column < 4; ++Column)
            {
                Augmented[Row][Column] = M[Row][Column];
                Augmented[Row][Column + 4] = Row == Column ? 1.0f : 0.0f;
            }
        }

        for (int PivotColumn = 0; PivotColumn < 4; ++PivotColumn)
        {
            int PivotRow = PivotColumn;
            float PivotAbs = FMath::Abs(Augmented[PivotRow][PivotColumn]);
            for (int Row = PivotColumn + 1; Row < 4; ++Row)
            {
                const float CandidateAbs = FMath::Abs(Augmented[Row][PivotColumn]);
                if (CandidateAbs > PivotAbs)
                {
                    PivotAbs = CandidateAbs;
                    PivotRow = Row;
                }
            }

            if (!FMath::IsFinite(PivotAbs) || PivotAbs <= Tolerance)
            {
                return false;
            }

            if (PivotRow != PivotColumn)
            {
                for (int Column = 0; Column < 8; ++Column)
                {
                    std::swap(Augmented[PivotColumn][Column], Augmented[PivotRow][Column]);
                }
            }

            const float Pivot = Augmented[PivotColumn][PivotColumn];
            for (int Column = 0; Column < 8; ++Column)
            {
                Augmented[PivotColumn][Column] /= Pivot;
                if (!FMath::IsFinite(Augmented[PivotColumn][Column]))
                {
                    return false;
                }
            }

            for (int Row = 0; Row < 4; ++Row)
            {
                if (Row == PivotColumn)
                {
                    continue;
                }

                const float Factor = Augmented[Row][PivotColumn];
                for (int Column = 0; Column < 8; ++Column)
                {
                    Augmented[Row][Column] -= Factor * Augmented[PivotColumn][Column];
                    if (!FMath::IsFinite(Augmented[Row][Column]))
                    {
                        return false;
                    }
                }
            }
        }

        for (int Row = 0; Row < 4; ++Row)
        {
            for (int Column = 0; Column < 4; ++Column)
            {
                OutInverse.M[Row][Column] = Augmented[Row][Column + 4];
            }
        }
        if (!OutInverse.IsFinite())
        {
            OutInverse = Identity();
            return false;
        }
        return true;
    }
};

} // namespace Stoner::Core
