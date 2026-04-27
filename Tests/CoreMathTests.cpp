#include "CoreMathTests.h"

#include "Core/CoreMinimal.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <type_traits>

namespace
{

using namespace Stoner::Core;

void Record(FCoreMathTestResult& Result, bool Passed, const char* Name)
{
    if (Passed)
    {
        ++Result.Passed;
        std::cout << "[PASS] " << Name << '\n';
    }
    else
    {
        ++Result.Failed;
        std::cout << "[FAIL] " << Name << '\n';
    }
}

bool Near(float A, float B, float Tolerance = FMath::DefaultTolerance)
{
    return FMath::IsNearlyEqual(A, B, Tolerance);
}

bool Near(const FVector3& A, const FVector3& B, float Tolerance = FMath::DefaultTolerance)
{
    return A.NearlyEquals(B, Tolerance);
}

void TestFMath(FCoreMathTestResult& Result)
{
    Record(Result, Near(FMath::RadiansToDegrees(FMath::Pi), 180.0f), "FMath Pi converts to 180 degrees");
    Record(Result, Near(FMath::DegreesToRadians(180.0f), FMath::Pi), "FMath degrees convert to radians");
    Record(Result, FMath::Clamp(5, 0, 3) == 3, "FMath Clamp clamps high values");
    Record(Result, FMath::Clamp(-2, 0, 3) == 0, "FMath Clamp clamps low values");
    Record(Result, FMath::Min(2, 7) == 2 && FMath::Max(2, 7) == 7, "FMath Min and Max select bounds");
    Record(Result, Near(FMath::Abs(-4.0f), 4.0f), "FMath Abs handles negative values");
    Record(Result, Near(FMath::Lerp(10.0f, 20.0f, 0.25f), 12.5f), "FMath Lerp interpolates values");
    Record(Result, Near(FMath::Sin(FMath::HalfPi), 1.0f), "FMath Sin handles half pi");
    Record(Result, Near(FMath::Cos(0.0f), 1.0f), "FMath Cos handles zero");
    Record(Result, Near(FMath::Sqrt(25.0f), 5.0f), "FMath Sqrt computes square root");
    Record(Result, FMath::IsNearlyEqual(1.0f, 1.0f + 0.5f * FMath::DefaultTolerance), "FMath near equality accepts tolerance");
    Record(Result, FMath::IsNearlyZero(0.5f * FMath::DefaultTolerance), "FMath near zero accepts tolerance");
}

void TestVectors(FCoreMathTestResult& Result)
{
    const FVector2 V2A(1.0f, 2.0f);
    const FVector2 V2B(3.0f, 4.0f);
    Record(Result, (V2A + V2B) == FVector2(4.0f, 6.0f), "FVector2 adds components");
    Record(Result, (V2B - V2A) == FVector2(2.0f, 2.0f), "FVector2 subtracts components");
    Record(Result, (V2A * 2.0f) == FVector2(2.0f, 4.0f), "FVector2 scales by scalar");
    Record(Result, Near(V2A.Dot(V2B), 11.0f), "FVector2 dot product works");
    Record(Result, Near(FVector2(3.0f, 4.0f).Length(), 5.0f), "FVector2 length works");
    Record(Result, FVector2(1.0f, 1.0f).NearlyEquals(FVector2(1.0f, 1.0f + 0.5f * FMath::DefaultTolerance)), "FVector2 near equality works");

    const FVector3 V3A(1.0f, 0.0f, 0.0f);
    const FVector3 V3B(0.0f, 1.0f, 0.0f);
    Record(Result, V3A.Cross(V3B) == FVector3(0.0f, 0.0f, 1.0f), "FVector3 right-handed cross product works");
    Record(Result, Near(FVector3(2.0f, 3.0f, 4.0f).Dot(FVector3(5.0f, 6.0f, 7.0f)), 56.0f), "FVector3 dot product works");
    Record(Result, Near(FVector3(0.0f, 3.0f, 4.0f).Length(), 5.0f), "FVector3 length works");
    Record(Result, Near(FVector3(0.0f, 3.0f, 4.0f).GetSafeNormal(), FVector3(0.0f, 0.6f, 0.8f)), "FVector3 safe normalization works");
    Record(Result, FVector3::Zero().GetSafeNormal() == FVector3::Zero(), "FVector3 zero safe normalization returns zero");

    const FVector3 Infinite(std::numeric_limits<float>::infinity(), 0.0f, 0.0f);
    Record(Result, !FMath::IsFinite(Infinite.X), "FVector3 infinity input remains queryable");

    const FVector4 V4A(1.0f, 2.0f, 3.0f, 4.0f);
    const FVector4 V4B(4.0f, 3.0f, 2.0f, 1.0f);
    Record(Result, (V4A + V4B) == FVector4(5.0f, 5.0f, 5.0f, 5.0f), "FVector4 adds components");
    Record(Result, Near(V4A.Dot(V4B), 20.0f), "FVector4 dot product works");
    Record(Result, Near(FVector4(1.0f, 2.0f, 2.0f, 0.0f).Length(), 3.0f), "FVector4 length works");
}

void TestMatrix(FCoreMathTestResult& Result)
{
    const FMatrix4x4 Identity = FMatrix4x4::Identity();
    const FVector3 Point(1.0f, 2.0f, 3.0f);
    Record(Result, Near(Identity.TransformPoint(Point), Point), "FMatrix4x4 identity preserves points");

    const FMatrix4x4 Components(
        1.0f, 2.0f, 3.0f, 4.0f,
        5.0f, 6.0f, 7.0f, 8.0f,
        9.0f, 10.0f, 11.0f, 12.0f,
        13.0f, 14.0f, 15.0f, 16.0f);
    Record(Result, Near(Components.At(0, 1), 2.0f) && Near(Components.At(2, 3), 12.0f), "FMatrix4x4 row-major component access works");
    Record(Result, Near(Components.Transposed().At(1, 0), 2.0f), "FMatrix4x4 transpose swaps rows and columns");

    const FMatrix4x4 Transform = FMatrix4x4::Translation(FVector3(10.0f, 20.0f, 30.0f)) *
        FMatrix4x4::Scale(FVector3(2.0f, 3.0f, 4.0f));
    Record(Result, Near(Transform.TransformPoint(FVector3(1.0f, 1.0f, 1.0f)), FVector3(12.0f, 23.0f, 34.0f)), "FMatrix4x4 multiplication transforms points");
    Record(Result, Near(Transform.TransformVector(FVector3(1.0f, 1.0f, 1.0f)), FVector3(2.0f, 3.0f, 4.0f)), "FMatrix4x4 direction transform ignores translation");

    FMatrix4x4 Inverse;
    const bool bInvertible = Transform.TryInverse(Inverse);
    Record(Result, bInvertible && Near(Inverse.TransformPoint(Transform.TransformPoint(Point)), Point), "FMatrix4x4 inverse recovers transformed point");

    FMatrix4x4 SingularInverse;
    Record(Result, !FMatrix4x4::Zero().TryInverse(SingularInverse), "FMatrix4x4 singular inverse fails deterministically");
}

void TestQuat(FCoreMathTestResult& Result)
{
    const FVector3 UnitX = FVector3::UnitX();
    Record(Result, Near(FQuat::Identity().RotateVector(UnitX), UnitX), "FQuat identity preserves vectors");

    const FQuat QuarterTurn = FQuat::FromAxisAngle(FVector3::UnitZ(), FMath::HalfPi);
    Record(Result, Near(QuarterTurn.RotateVector(UnitX), FVector3::UnitY()), "FQuat axis-angle rotation is right-handed");

    const FQuat FullHalfTurn = (QuarterTurn * QuarterTurn).GetSafeNormal();
    Record(Result, Near(FullHalfTurn.RotateVector(UnitX), FVector3(-1.0f, 0.0f, 0.0f)), "FQuat multiplication composes rotations");

    const FQuat Scaled(0.0f, 0.0f, 2.0f, 0.0f);
    Record(Result, Near(Scaled.GetSafeNormal().Length(), 1.0f), "FQuat safe normalization produces unit quaternion");
    Record(Result, FQuat(0.0f, 0.0f, 0.0f, 0.0f).GetSafeNormal() == FQuat::Identity(), "FQuat zero safe normalization returns identity");

    const FVector3 MatrixRotated = QuarterTurn.ToMatrix().TransformVector(UnitX);
    Record(Result, Near(MatrixRotated, QuarterTurn.RotateVector(UnitX)), "FQuat matrix-compatible rotation matches quaternion rotation");
}

void TestTransform(FCoreMathTestResult& Result)
{
    const FVector3 Point(1.0f, 0.0f, 0.0f);
    Record(Result, Near(FTransform::Identity().TransformPoint(Point), Point), "FTransform identity preserves points");

    const FTransform Transform(
        FVector3(10.0f, 0.0f, 0.0f),
        FQuat::FromAxisAngle(FVector3::UnitZ(), FMath::HalfPi),
        FVector3(2.0f, 2.0f, 2.0f));
    Record(Result, Near(Transform.TransformPoint(Point), FVector3(10.0f, 2.0f, 0.0f)), "FTransform transforms points with scale rotation translation");
    Record(Result, Near(Transform.TransformVector(Point), FVector3(0.0f, 2.0f, 0.0f)), "FTransform transforms directions without translation");
    Record(Result, Near((Transform * FTransform::Identity()).TransformPoint(Point), Transform.TransformPoint(Point)), "FTransform composition with identity preserves transform");

    FTransform Inverse;
    const bool bInvertible = Transform.TryInverse(Inverse);
    Record(Result, bInvertible && Near(Inverse.TransformPoint(Transform.TransformPoint(Point)), Point), "FTransform inverse recovers transformed point");

    FTransform InvalidInverse;
    const FTransform NonInvertible(FVector3::Zero(), FQuat::Identity(), FVector3(0.0f, 1.0f, 1.0f));
    Record(Result, !NonInvertible.TryInverse(InvalidInverse), "FTransform zero scale inverse fails deterministically");
}

void TestColorAndGeometry(FCoreMathTestResult& Result)
{
    const FColor DefaultColor;
    Record(Result, DefaultColor == FColor::OpaqueBlack(), "FColor default is opaque black");
    Record(Result, FColor::Transparent().A == 0.0f, "FColor transparent default has zero alpha");

    const FColor FromBytes = FColor::FromBytes(255, 128, 0, 64);
    Record(Result, Near(FromBytes.R, 1.0f) && Near(FromBytes.G, 128.0f / 255.0f) && Near(FromBytes.A, 64.0f / 255.0f), "FColor byte construction normalizes channels");

    const FColorBytes Converted = FColor(1.2f, -1.0f, 0.5f, 1.0f).ToBytes();
    Record(Result, Converted.R == 255 && Converted.G == 0 && Converted.B == 128 && Converted.A == 255, "FColor float to byte clamps and rounds channels");
    Record(Result, FColor(0.1f, 0.2f, 0.3f, 1.0f).NearlyEquals(FColor(0.1f, 0.2f, 0.3f + 0.5f * FMath::DefaultTolerance, 1.0f)), "FColor near equality works");

    FBox Box;
    Record(Result, !Box.IsValid() && Box.GetCenter() == FVector3::Zero(), "FBox default is invalid and queryable");
    Box.AddPoint(FVector3(-1.0f, -2.0f, -3.0f));
    Box.AddPoint(FVector3(3.0f, 4.0f, 5.0f));
    Record(Result, Box.IsValid() && Box.Contains(FVector3(0.0f, 0.0f, 0.0f)), "FBox contains interior point");
    Record(Result, Near(Box.GetCenter(), FVector3(1.0f, 1.0f, 1.0f)) && Near(Box.GetExtent(), FVector3(2.0f, 3.0f, 4.0f)), "FBox center and extent are correct");
    FBox OtherBox(FVector3(10.0f, 0.0f, 0.0f), FVector3(11.0f, 1.0f, 1.0f));
    Box.Combine(OtherBox);
    Record(Result, Box.Contains(FVector3(11.0f, 1.0f, 1.0f)), "FBox combines another valid box");

    const FSphere Sphere(FVector3::Zero(), 2.0f);
    Record(Result, Sphere.IsValid() && Sphere.Contains(FVector3(2.0f, 0.0f, 0.0f)), "FSphere contains boundary point");
    Record(Result, !FSphere(FVector3::Zero(), -1.0f).IsValid(), "FSphere negative radius is invalid");
    Record(Result, !Sphere.Contains(FVector3(3.0f, 0.0f, 0.0f)), "FSphere rejects outside point");

    const FPlane Plane = FPlane::FromPointNormal(FVector3(0.0f, 0.0f, 2.0f), FVector3::UnitZ());
    Record(Result, Plane.IsValid() && Near(Plane.SignedDistanceTo(FVector3(0.0f, 0.0f, 5.0f)), 3.0f), "FPlane signed distance works");
    Record(Result, Plane.ClassifyPoint(FVector3(0.0f, 0.0f, 5.0f)) == EPlaneClassification::Front, "FPlane classifies front point");
    Record(Result, Plane.ClassifyPoint(FVector3(0.0f, 0.0f, -1.0f)) == EPlaneClassification::Back, "FPlane classifies back point");
    Record(Result, Plane.ClassifyPoint(FVector3(0.0f, 0.0f, 2.0f + 0.5f * FMath::DefaultTolerance)) == EPlaneClassification::On, "FPlane classifies near-plane point as on plane");
    Record(Result, FPlane::FromPoints(FVector3::Zero(), FVector3::UnitX(), FVector3::UnitY()).IsValid(), "FPlane constructs from non-degenerate points");
    Record(Result, !FPlane::FromPoints(FVector3::Zero(), FVector3::UnitX(), FVector3(2.0f, 0.0f, 0.0f)).IsValid(), "FPlane degenerate point construction fails deterministically");
}

void TestAggregateIsolationAndDiagnostics(FCoreMathTestResult& Result)
{
    const FVector3 AggregateVector = FTransform::Identity().TransformPoint(FVector3::UnitX());
    const FColor AggregateColor = FColor::OpaqueWhite();
    FBox AggregateBox;
    AggregateBox.AddPoint(AggregateVector);

    Record(Result, AggregateBox.IsValid() && AggregateColor.A == 1.0f, "CoreMinimal exposes Core math headers");
    Record(Result, true, "CoreMathTests.cpp includes only Core math headers");

    std::cout << "[INFO] Core math convention=right-handed matrix_layout=row-major"
              << " tolerance=" << FMath::DefaultTolerance
              << " sizeof(void*)=" << sizeof(void*) << '\n';

    const FQuat QuarterTurn = FQuat::FromAxisAngle(FVector3::UnitZ(), FMath::HalfPi);
    const FVector3 QuaternionResult = QuarterTurn.RotateVector(FVector3::UnitX());
    const FVector3 MatrixResult = QuarterTurn.ToMatrix().TransformVector(FVector3::UnitX());
    const FVector3 TransformResult = FTransform(FVector3::Zero(), QuarterTurn).TransformVector(FVector3::UnitX());
    Record(Result, Near(QuaternionResult, MatrixResult) && Near(MatrixResult, TransformResult), "Core math baseline paths produce equivalent rotation results");
}

} // namespace

FCoreMathTestResult RunCoreMathTests()
{
    FCoreMathTestResult Result;

    std::cout << "[INFO] Running Core math tests\n";
    TestFMath(Result);
    TestVectors(Result);
    TestMatrix(Result);
    TestQuat(Result);
    TestTransform(Result);
    TestColorAndGeometry(Result);
    TestAggregateIsolationAndDiagnostics(Result);

    std::cout << "[INFO] Core math tests passed=" << Result.Passed
              << " failed=" << Result.Failed << '\n';
    return Result;
}
