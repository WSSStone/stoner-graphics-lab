#include "CoordinateConventionTests.h"

#include "Core/CoreMinimal.h"
#include "RHI/FRHIGraphicsPipelineDesc.h"
#include "Renderer/FForwardViewData.h"
#include "Renderer/FShaderMatrixPacking.h"

#include <iostream>

namespace
{

using namespace Stoner::Core;

void Record(FCoordinateConventionTestResult& Result, bool Passed, const char* Name)
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

} // namespace

FCoordinateConventionTestResult RunCoordinateConventionTests()
{
    FCoordinateConventionTestResult Result;
    const FVector3 Forward = FCoordinateConvention::Forward();
    const FVector3 Right = FCoordinateConvention::Right();
    const FVector3 Up = FCoordinateConvention::Up();

    Record(Result,
        Forward == FVector3::UnitX() && Right == FVector3::UnitY() && Up == FVector3::UnitZ(),
        "Coordinate convention defines +X forward, +Y right, and +Z up");
    Record(Result, Forward.Cross(Right) == Up,
        "Coordinate convention preserves component cross-product algebra");
    Record(Result,
        FQuat::FromAxisAngle(Up, FMath::HalfPi).RotateVector(Forward).NearlyEquals(Right),
        "Positive yaw maps forward to right");
    const FTransform Srt(FVector3(2.0f, 3.0f, 4.0f),
        FQuat::FromAxisAngle(Up, FMath::HalfPi), FVector3(2.0f, 1.0f, 1.0f));
    Record(Result,
        Srt.TransformPoint(Forward).NearlyEquals(FVector3(2.0f, 5.0f, 4.0f)) &&
            Srt.ToMatrix().TransformPoint(Forward).NearlyEquals(FVector3(2.0f, 5.0f, 4.0f)),
        "Coordinate convention preserves scale-rotate-translate composition");
    Record(Result,
        Stoner::RHI::FRHIGraphicsPipelineDesc().Rasterizer.FrontFace ==
            Stoner::RHI::ERHIFrontFace::Clockwise,
        "Coordinate convention uses clockwise front-face culling by default");
    Record(Result,
        FTransform(FVector3(2.0f, 3.0f, 4.0f), FQuat::Identity(), FVector3(-1.0f, 1.0f, 1.0f))
            .TransformPoint(Forward) == FVector3(1.0f, 3.0f, 4.0f),
        "Negative scale remains explicit in SRT transforms");
    const FMatrix4x4 ShiftedView = FMatrix4x4::Translation(FVector3(-10.0f, 0.0f, 0.0f));
    Record(Result,
        Stoner::Renderer::TransformWorldPositionToView(ShiftedView, FVector3(12.0f, 4.0f, -3.0f)) ==
                FVector3(2.0f, 4.0f, -3.0f) &&
            Stoner::Renderer::ComputeViewSpaceForwardDepth(ShiftedView, FVector3(12.0f, 4.0f, -3.0f)) ==
                2.0f,
        "World-to-view conversion names +X as camera forward depth");
    const FMatrix4x4 Matrix(
        1.0f, 2.0f, 3.0f, 4.0f,
        5.0f, 6.0f, 7.0f, 8.0f,
        9.0f, 10.0f, 11.0f, 12.0f,
        13.0f, 14.0f, 15.0f, 16.0f);
    const Stoner::Renderer::FShaderMatrix4x4 Packed =
        Stoner::Renderer::PackRowMajorMatrixForShader(Matrix);
    Record(Result,
        Packed.Elements[0] == 1.0f && Packed.Elements[1] == 5.0f &&
            Packed.Elements[4] == 2.0f && Packed.Elements[15] == 16.0f,
        "Non-symmetric CPU matrices use explicit GLSL column-major packing");
    return Result;
}
