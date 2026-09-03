#include "FProductionCameraPreset.h"

#include <cmath>

namespace Stoner::Demo
{
namespace
{

using namespace Stoner::Core;

constexpr float MatrixTolerance = 1.0e-4f;

void Fail(FString* OutReason, const char* Reason)
{
    if (OutReason) *OutReason = Reason;
}

bool IsAffineOrthonormalView(const FMatrix4x4& View) noexcept
{
    if (!View.IsFinite() ||
        !FMath::IsNearlyZero(View.M[3][0], MatrixTolerance) ||
        !FMath::IsNearlyZero(View.M[3][1], MatrixTolerance) ||
        !FMath::IsNearlyZero(View.M[3][2], MatrixTolerance) ||
        !FMath::IsNearlyEqual(View.M[3][3], 1.0f, MatrixTolerance))
        return false;

    const FVector3 Forward(View.M[0][0], View.M[0][1], View.M[0][2]);
    const FVector3 Right(View.M[1][0], View.M[1][1], View.M[1][2]);
    const FVector3 Up(View.M[2][0], View.M[2][1], View.M[2][2]);
    const float Determinant = Forward.Dot(Right.Cross(Up));
    return FMath::IsNearlyEqual(Forward.Length(), 1.0f, MatrixTolerance) &&
        FMath::IsNearlyEqual(Right.Length(), 1.0f, MatrixTolerance) &&
        FMath::IsNearlyEqual(Up.Length(), 1.0f, MatrixTolerance) &&
        FMath::IsNearlyZero(Forward.Dot(Right), MatrixTolerance) &&
        FMath::IsNearlyZero(Forward.Dot(Up), MatrixTolerance) &&
        FMath::IsNearlyZero(Right.Dot(Up), MatrixTolerance) &&
        FMath::IsNearlyEqual(Determinant, 1.0f, MatrixTolerance);
}

bool IsForwardStandardZProjection(const FMatrix4x4& Projection) noexcept
{
    if (!Projection.IsFinite()) return false;
    const bool bShape =
        FMath::IsNearlyZero(Projection.M[0][0], MatrixTolerance) &&
        Projection.M[0][1] > 0.0f &&
        FMath::IsNearlyZero(Projection.M[0][2], MatrixTolerance) &&
        FMath::IsNearlyZero(Projection.M[0][3], MatrixTolerance) &&
        FMath::IsNearlyZero(Projection.M[1][0], MatrixTolerance) &&
        FMath::IsNearlyZero(Projection.M[1][1], MatrixTolerance) &&
        Projection.M[1][2] < 0.0f &&
        FMath::IsNearlyZero(Projection.M[1][3], MatrixTolerance) &&
        Projection.M[2][0] > 1.0f &&
        FMath::IsNearlyZero(Projection.M[2][1], MatrixTolerance) &&
        FMath::IsNearlyZero(Projection.M[2][2], MatrixTolerance) &&
        Projection.M[2][3] < 0.0f &&
        FMath::IsNearlyEqual(Projection.M[3][0], 1.0f, MatrixTolerance) &&
        FMath::IsNearlyZero(Projection.M[3][1], MatrixTolerance) &&
        FMath::IsNearlyZero(Projection.M[3][2], MatrixTolerance) &&
        FMath::IsNearlyZero(Projection.M[3][3], MatrixTolerance);
    if (!bShape) return false;
    FMatrix4x4 Inverse;
    return Projection.TryInverse(Inverse) && Inverse.IsFinite();
}

} // namespace

bool FProductionCameraPreset::IsValid() const noexcept
{
    if (WorkloadRevision.IsEmpty() ||
        !IsAffineOrthonormalView(View) ||
        !IsForwardStandardZProjection(Projection) ||
        !ViewProjection.IsFinite() ||
        !InverseViewProjection.IsFinite() ||
        !FMath::IsFinite(CameraPosition.X) ||
        !FMath::IsFinite(CameraPosition.Y) ||
        !FMath::IsFinite(CameraPosition.Z) ||
        !ViewProjection.NearlyEquals(Projection * View, MatrixTolerance))
        return false;
    return (InverseViewProjection * ViewProjection).NearlyEquals(
        FMatrix4x4::Identity(), 5.0e-4f);
}

FMatrix4x4 MakeProductionPerspective(
    float VerticalFovRadians,
    float Aspect,
    float NearPlane,
    float FarPlane) noexcept
{
    if (!FMath::IsFinite(VerticalFovRadians) ||
        !FMath::IsFinite(Aspect) ||
        !FMath::IsFinite(NearPlane) ||
        !FMath::IsFinite(FarPlane) ||
        VerticalFovRadians <= 0.0f || VerticalFovRadians >= 3.13f ||
        Aspect <= 0.0f || NearPlane <= 0.0f || FarPlane <= NearPlane)
        return FMatrix4x4::Zero();
    const float VerticalScale = 1.0f / std::tan(VerticalFovRadians * 0.5f);
    const float HorizontalScale = VerticalScale / Aspect;
    const float DepthScale = FarPlane / (FarPlane - NearPlane);
    return FMatrix4x4(
        0.0f, HorizontalScale, 0.0f, 0.0f,
        0.0f, 0.0f, -VerticalScale, 0.0f,
        DepthScale, 0.0f, 0.0f, -NearPlane * DepthScale,
        1.0f, 0.0f, 0.0f, 0.0f);
}

FMatrix4x4 MakeProductionCameraView(
    const FVector3& Position,
    float YawRadians,
    float PitchRadians) noexcept
{
    if (!FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y) ||
        !FMath::IsFinite(Position.Z) || !FMath::IsFinite(YawRadians) ||
        !FMath::IsFinite(PitchRadians))
        return FMatrix4x4::Zero();
    const float CosPitch = std::cos(PitchRadians);
    const FVector3 Forward(
        CosPitch * std::cos(YawRadians),
        CosPitch * std::sin(YawRadians),
        std::sin(PitchRadians));
    const FVector3 Right(-std::sin(YawRadians), std::cos(YawRadians), 0.0f);
    const FVector3 Up = Forward.Cross(Right);
    return FMatrix4x4(
        Forward.X, Forward.Y, Forward.Z, -Forward.Dot(Position),
        Right.X, Right.Y, Right.Z, -Right.Dot(Position),
        Up.X, Up.Y, Up.Z, -Up.Dot(Position),
        0.0f, 0.0f, 0.0f, 1.0f);
}

bool BuildProductionCameraPreset(
    const FString& WorkloadRevision,
    const FMatrix4x4& View,
    const FMatrix4x4& Projection,
    FProductionCameraPreset& OutPreset,
    FString* OutReason)
{
    OutPreset = {};
    if (OutReason) OutReason->Clear();
    if (WorkloadRevision.IsEmpty())
    {
        Fail(OutReason, "camera workload revision is empty");
        return false;
    }
    if (!IsAffineOrthonormalView(View))
    {
        Fail(OutReason, "camera View is not finite affine orthonormal");
        return false;
    }
    if (!IsForwardStandardZProjection(Projection))
    {
        Fail(OutReason, "camera Projection is not positive-X StandardZ perspective");
        return false;
    }
    FMatrix4x4 InverseView;
    FMatrix4x4 InverseViewProjection;
    const FMatrix4x4 ViewProjection = Projection * View;
    if (!View.TryInverse(InverseView) ||
        !ViewProjection.TryInverse(InverseViewProjection))
    {
        Fail(OutReason, "camera matrices are not invertible");
        return false;
    }
    FProductionCameraPreset Candidate;
    Candidate.WorkloadRevision = WorkloadRevision;
    Candidate.View = View;
    Candidate.Projection = Projection;
    Candidate.ViewProjection = ViewProjection;
    Candidate.InverseViewProjection = InverseViewProjection;
    Candidate.CameraPosition = InverseView.TransformPoint(FVector3::Zero());
    if (!Candidate.IsValid())
    {
        Fail(OutReason, "camera derived data is invalid");
        return false;
    }
    OutPreset = std::move(Candidate);
    return true;
}

bool ResolveProductionCameraPreset(
    const FString& WorkloadRevision,
    FProductionCameraPreset& OutPreset,
    FString* OutReason)
{
    if (WorkloadRevision == FString("production-content-sponza-v2") ||
        WorkloadRevision == FString("production-content-sponza-v3"))
    {
        return BuildProductionCameraPreset(
            WorkloadRevision,
            FMatrix4x4(
                -0.136053622f, -0.986105621f, -0.0953160524f, 1.61162198f,
                0.990615845f, -0.136675894f, 0.0f, -3.93525147f,
                -0.0130274063f, -0.0944215953f, 0.995447159f, 0.12801826f,
                0.0f, 0.0f, 0.0f, 1.0f),
            FMatrix4x4(
                0.0f, 1.7320509f, 0.0f, 0.0f,
                0.0f, 0.0f, -1.7320509f, 0.0f,
                1.001001f, 0.0f, 0.0f, -0.1001001f,
                1.0f, 0.0f, 0.0f, 0.0f),
            OutPreset, OutReason);
    }
    if (WorkloadRevision != FString("production-content-lantern-v2") &&
        WorkloadRevision != FString("production-content-lantern-v3") &&
        WorkloadRevision != FString("production-content-v1") &&
        WorkloadRevision != FString("production-content-sponza-v1"))
    {
        OutPreset = {};
        Fail(OutReason, "camera workload revision is not declared");
        return false;
    }
    return BuildProductionCameraPreset(
        WorkloadRevision,
        FMatrix4x4::Identity(),
        MakeProductionPerspective(1.0471975512f, 1.0f),
        OutPreset, OutReason);
}

} // namespace Stoner::Demo
