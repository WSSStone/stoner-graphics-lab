// Feature 030 T014 camera/controller fixtures.
//
// These tests use already-arbitrated action values.  Input ownership,
// press-ledger and pointer quarantine behavior belongs to the later router;
// this suite verifies only the reusable camera math and display seam.

#include "Application/FFreeCameraController.h"
#include "Application/FFreeCameraState.h"
#include "Application/FWindowDisplayState.h"

#include <cmath>
#include <iostream>
#include <limits>

struct FApplicationFreeCameraTestResult
{
    int Passed = 0;
    int Failed = 0;
};

namespace
{

using namespace Stoner::Application;
using namespace Stoner::Core;

constexpr float MatrixTolerance = 1.0e-4f;

void Record(FApplicationFreeCameraTestResult& Result, bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

// These values are frozen numerical fixtures for the +X StandardZ camera.
// They intentionally do not call the controller's matrix construction code.
FMatrix4x4 ExpectedProjection(float HorizontalScale)
{
    return FMatrix4x4(
        0.0f, HorizontalScale, 0.0f, 0.0f,
        0.0f, 0.0f, -1.7320508f, 0.0f,
        1.0010010f, 0.0f, 0.0f, -0.1001001f,
        1.0f, 0.0f, 0.0f, 0.0f);
}

FFreeCameraState MakeInitialCamera()
{
    FFreeCameraState Camera;
    Camera.CameraRevision = 1;
    Camera.Position = FVector3::Zero();
    Camera.YawRadians = 0.0f;
    Camera.PitchRadians = 0.0f;
    Camera.VerticalFovRadians = FMath::DegreesToRadians(60.0f);
    Camera.NearPlane = 0.1f;
    Camera.FarPlane = 100.0f;
    Camera.MovementSpeed = 1.5f;
    Camera.DrawableExtent = {1024, 1024};
    Camera.View = FMatrix4x4::Identity();
    Camera.Projection = ExpectedProjection(1.7320508f);
    Camera.ViewProjection = Camera.Projection * Camera.View;
    return Camera;
}

FWindowDisplayState MakeDisplay(FWindowExtent DrawableExtent = {1920, 1080})
{
    FWindowDisplayState Display;
    Display.LogicalExtent = DrawableExtent;
    Display.DrawableExtent = DrawableExtent;
    Display.ContentScale = FVector2(1.0f, 1.0f);
    Display.FramebufferScale = FVector2(1.0f, 1.0f);
    Display.DisplayGeneration = 1;
    Display.bFocused = true;
    Display.bMinimized = false;
    return Display;
}

FFreeCameraUpdateResult WarmController(
    FFreeCameraController& Controller,
    const FWindowDisplayState& Display)
{
    return Controller.Update({}, Display, 0.0);
}

bool NearlyEqual(float Left, float Right, float Tolerance = MatrixTolerance)
{
    return FMath::IsNearlyEqual(Left, Right, Tolerance);
}

void TestInitializationAndAspect(FApplicationFreeCameraTestResult& Result)
{
    const FFreeCameraState Initial = MakeInitialCamera();
    const FWindowDisplayState Display = MakeDisplay();
    FFreeCameraController Controller;
    FString Reason;
    const bool bInitialized = Controller.Initialize(Initial, Display, &Reason);
    const FFreeCameraState& State = Controller.GetState();
    const FMatrix4x4 ExpectedCurrentProjection =
        ExpectedProjection(0.97427857f);
    Record(Result, bInitialized && State.IsValid() &&
            State.DrawableExtent == Display.DrawableExtent &&
            State.Position == Initial.Position &&
            NearlyEqual(State.VerticalFovRadians, Initial.VerticalFovRadians) &&
            State.Projection.NearlyEquals(ExpectedCurrentProjection),
        "controller initializes a valid camera at the current drawable aspect");

    Record(Result, bInitialized && Controller.Update(
            FFreeCameraActions{}, Display, 0.0).ChangeSet.CameraRevision == 0,
        "controller leaves the initial revision untouched for a no-op interval");

    FWindowDisplayState ChangedAspect = MakeDisplay({1600, 1000});
    const auto ExtentResult = Controller.Update({}, ChangedAspect, 0.0);
    Record(Result, ExtentResult.bCameraChanged &&
            ExtentResult.ChangeSet.HasFlag(ECameraChangeFlags::ExtentChanged) &&
            ExtentResult.ChangeSet.HasFlag(
                ECameraChangeFlags::ProjectionChanged) &&
            Controller.GetState().DrawableExtent ==
                ChangedAspect.DrawableExtent &&
            Controller.GetState().IsValid() &&
            Controller.GetState().Projection.NearlyEquals(
                ExpectedProjection(1.0825317f)),
        "drawable extent changes rebuild projection at the new aspect");
}

void TestMovementAndSpeed(FApplicationFreeCameraTestResult& Result)
{
    const FWindowDisplayState Display = MakeDisplay();
    FFreeCameraController Controller;
    Record(Result, Controller.Initialize(MakeInitialCamera(), Display),
        "controller accepts the known finite camera fixture");

    (void)WarmController(Controller, Display);
    FFreeCameraActions Actions;
    Actions.ForwardAxis = 1.0f;
    const auto Forward = Controller.Update(Actions, Display, 0.25);
    Record(Result, Forward.bCameraChanged &&
            NearlyEqual(Controller.GetState().Position.X, 0.375f) &&
            NearlyEqual(Controller.GetState().Position.Y, 0.0f) &&
            NearlyEqual(Controller.GetState().Position.Z, 0.0f) &&
            Forward.ChangeSet.HasFlag(
                ECameraChangeFlags::ContinuousMotion),
        "forward action follows +X at the default speed");

    (void)Controller.Reset(Display);
    Actions = {};
    Actions.RightAxis = 1.0f;
    (void)Controller.Update(Actions, Display, 0.25);
    Record(Result, NearlyEqual(Controller.GetState().Position.Y, 0.375f),
        "right action follows +Y without changing height");

    (void)Controller.Reset(Display);
    Actions = {};
    Actions.UpAxis = 1.0f;
    (void)Controller.Update(Actions, Display, 0.25);
    Record(Result, NearlyEqual(Controller.GetState().Position.Z, 0.375f),
        "up action follows world +Z");

    (void)Controller.Reset(Display);
    Actions = {};
    Actions.ForwardAxis = 1.0f;
    Actions.RightAxis = 1.0f;
    const auto Diagonal = Controller.Update(Actions, Display, 0.25);
    const FVector3 DiagonalPosition = Controller.GetState().Position;
    Record(Result, Diagonal.bCameraChanged &&
            NearlyEqual(DiagonalPosition.X, 0.375f / std::sqrt(2.0f)) &&
            NearlyEqual(DiagonalPosition.Y, 0.375f / std::sqrt(2.0f)) &&
            NearlyEqual(DiagonalPosition.Length(), 0.375f),
        "diagonal movement is normalized before applying speed");

    (void)Controller.Reset(Display);
    Actions = {};
    Actions.ForwardAxis = 1.0f;
    Actions.bFast = true;
    (void)Controller.Update(Actions, Display, 0.25);
    Record(Result, NearlyEqual(Controller.GetState().Position.X, 1.5f),
        "Shift action multiplies movement speed by four");
}

void TestDeltaAndResume(FApplicationFreeCameraTestResult& Result)
{
    const FWindowDisplayState Display = MakeDisplay();
    FFreeCameraController Controller;
    Record(Result, Controller.Initialize(MakeInitialCamera(), Display),
        "controller initializes before first active interval");

    FFreeCameraActions Forward;
    Forward.ForwardAxis = 1.0f;
    const auto First = Controller.Update(Forward, Display, 1.0);
    Record(Result, !First.bCameraChanged &&
            Controller.GetState().Position == FVector3::Zero(),
        "first active interval suppresses translation time");

    const auto Second = Controller.Update(Forward, Display, 1.0);
    Record(Result, Second.bCameraChanged &&
            NearlyEqual(Controller.GetState().Position.X, 0.375f),
        "translation begins after the first active interval");

    (void)Controller.Reset(Display);
    (void)WarmController(Controller, Display);
    const FVector3 BeforeInvalidDelta = Controller.GetState().Position;
    Record(Result, !Controller.Update(Forward, Display, -1.0).bCameraChanged &&
            Controller.GetState().Position == BeforeInvalidDelta &&
            !Controller.Update(Forward, Display,
                std::numeric_limits<double>::quiet_NaN()).bCameraChanged,
        "negative and non-finite elapsed time produce no movement");

    (void)Controller.Reset(Display);
    (void)WarmController(Controller, Display);
    (void)Controller.Update(Forward, Display, 1.0);
    Record(Result, NearlyEqual(Controller.GetState().Position.X, 0.375f),
        "elapsed time clamps to the quarter-second maximum");

    FWindowDisplayState Unfocused = Display;
    Unfocused.bFocused = false;
    const FVector3 BeforeFocusLoss = Controller.GetState().Position;
    FFreeCameraActions Malformed = Forward;
    Malformed.ForwardAxis = std::numeric_limits<float>::quiet_NaN();
    Record(Result, !Controller.Update(Malformed, Unfocused, 1.0).bCameraChanged &&
            Controller.GetState().Position == BeforeFocusLoss,
        "unfocused display state stops movement before action validation");

    const auto Resumed = Controller.Update(Forward, Display, 1.0);
    Record(Result, !Resumed.bCameraChanged &&
            Controller.GetState().Position == BeforeFocusLoss,
        "first interval after focus restore has zero elapsed movement");
    Record(Result, Controller.Update(Forward, Display, 1.0).bCameraChanged,
        "fresh active interval resumes camera movement");

    (void)Controller.Reset(Display);
    (void)WarmController(Controller, Display);
    const auto LargeFiniteDelta = Controller.Update(
        Forward, Display, std::numeric_limits<double>::max());
    Record(Result, LargeFiniteDelta.bCameraChanged &&
            NearlyEqual(Controller.GetState().Position.X, 0.375f),
        "finite elapsed time above float range still clamps to a quarter second");

    FWindowDisplayState Paused = Display;
    Paused.DrawableExtent = {};
    Paused.bMinimized = false;
    const FVector3 BeforePause = Controller.GetState().Position;
    Record(Result, Controller.Update(Forward, Paused, 1.0).bCameraChanged == false &&
            Controller.GetState().Position == BeforePause,
        "zero drawable pauses movement without invalidating the camera state");
}

void TestLookAndFov(FApplicationFreeCameraTestResult& Result)
{
    const FWindowDisplayState Display = MakeDisplay();
    FFreeCameraController Controller;
    Record(Result, Controller.Initialize(MakeInitialCamera(), Display),
        "controller initializes before look and FOV actions");

    FFreeCameraActions Look;
    Look.bLookCaptured = true;
    Look.LookDeltaX = 32.0f;
    Look.LookDeltaY = -16.0f;
    const auto LookResult = Controller.Update(Look, Display, 0.0);
    Record(Result, LookResult.bCameraChanged &&
            NearlyEqual(Controller.GetState().YawRadians, 0.096f) &&
            NearlyEqual(Controller.GetState().PitchRadians, 0.048f),
        "captured look applies logical-pixel yaw and pitch deltas");

    Look = {};
    Look.LookDeltaX = 100.0f;
    Look.LookDeltaY = 100.0f;
    const float YawBeforeUncaptured = Controller.GetState().YawRadians;
    Record(Result, !Controller.Update(Look, Display, 0.0).bCameraChanged &&
            NearlyEqual(Controller.GetState().YawRadians,
                YawBeforeUncaptured),
        "uncaptured look deltas do not rotate the camera");

    (void)Controller.Reset(Display);
    Look = {};
    Look.bLookCaptured = true;
    Look.LookDeltaY = 10000.0f;
    (void)Controller.Update(Look, Display, 0.0);
    Record(Result, NearlyEqual(Controller.GetState().PitchRadians,
            FMath::DegreesToRadians(-89.0f)),
        "look pitch clamps at the negative eighty-nine degree limit");

    (void)Controller.Reset(Display);
    FFreeCameraActions Scroll;
    Scroll.ScrollDeltaY = 1.0f;
    const auto FovResult = Controller.Update(Scroll, Display, 0.0);
    Record(Result, FovResult.bCameraChanged &&
            NearlyEqual(Controller.GetState().VerticalFovRadians,
                FMath::DegreesToRadians(60.0f) - 0.035f) &&
            FovResult.ChangeSet.HasFlag(
                ECameraChangeFlags::ProjectionChanged),
        "scroll changes vertical FOV and marks projection change");

    Scroll.ScrollDeltaY = -100.0f;
    (void)Controller.Update(Scroll, Display, 0.0);
    Record(Result, NearlyEqual(Controller.GetState().VerticalFovRadians,
            FMath::DegreesToRadians(90.0f)),
        "negative wheel input clamps FOV at ninety degrees");

    Scroll.ScrollDeltaY = 100.0f;
    (void)Controller.Update(Scroll, Display, 0.0);
    Record(Result, NearlyEqual(Controller.GetState().VerticalFovRadians,
            FMath::DegreesToRadians(20.0f)),
        "positive wheel input clamps FOV at twenty degrees");

    Scroll.ScrollDeltaY = std::numeric_limits<float>::infinity();
    Record(Result, !Controller.Update(Scroll, Display, 0.0).bCameraChanged,
        "non-finite wheel input is rejected");
}

void TestResetAndCadence(FApplicationFreeCameraTestResult& Result)
{
    const FWindowDisplayState Display = MakeDisplay();
    FFreeCameraController Controller;
    const FFreeCameraState Initial = MakeInitialCamera();
    Record(Result, Controller.Initialize(Initial, Display),
        "controller initializes before reset and cadence checks");

    (void)WarmController(Controller, Display);
    FFreeCameraActions Move;
    Move.ForwardAxis = 1.0f;
    Move.RightAxis = 1.0f;
    Move.bFast = true;
    (void)Controller.Update(Move, Display, 0.1);
    FFreeCameraActions Reset;
    Reset.bReset = true;
    const auto ResetResult = Controller.Update(Reset, Display, 0.0);
    const FFreeCameraState& ResetState = Controller.GetState();
    Record(Result, ResetResult.bCameraChanged &&
            ResetResult.ChangeSet.HasFlag(ECameraChangeFlags::Reset) &&
            ResetResult.ChangeSet.HasFlag(ECameraChangeFlags::Cut) &&
            ResetState.Position == Initial.Position &&
            NearlyEqual(ResetState.VerticalFovRadians,
                Initial.VerticalFovRadians) &&
            NearlyEqual(ResetState.NearPlane, Initial.NearPlane) &&
            NearlyEqual(ResetState.FarPlane, Initial.FarPlane) &&
            ResetState.Projection.NearlyEquals(
                ExpectedProjection(0.97427857f)),
        "reset restores pose and lens values using the current aspect");

    FFreeCameraActions TinyScroll;
    TinyScroll.ScrollDeltaY = 0.0001f;
    const auto TinyFovResult = Controller.Update(TinyScroll, Display, 0.0);
    FCameraChangeSet TinyResetChange;
    const bool bTinyReset = Controller.Reset(Display, &TinyResetChange);
    Record(Result, TinyFovResult.bCameraChanged && bTinyReset &&
            TinyResetChange.HasFlag(ECameraChangeFlags::ProjectionChanged),
        "reset reports a projection change for a small legal FOV change");

    const auto RunCadence = [&](int Steps)
    {
        FFreeCameraController Candidate;
        (void)Candidate.Initialize(Initial, Display);
        (void)WarmController(Candidate, Display);
        for (int Step = 0; Step < Steps; ++Step)
            (void)Candidate.Update(
                FFreeCameraActions{.ForwardAxis = 1.0f},
                Display, 1.0 / static_cast<double>(Steps));
        return Candidate.GetState().Position;
    };
    const FVector3 At30 = RunCadence(30);
    const FVector3 At60 = RunCadence(60);
    const FVector3 At120 = RunCadence(120);
    Record(Result, At30.NearlyEquals(At60, 1.0e-4f) &&
            At60.NearlyEquals(At120, 1.0e-4f) &&
            NearlyEqual(At60.X, 1.5f, 1.0e-4f),
        "thirty sixty and one-hundred-twenty hertz integrate identically");
}

} // namespace

int RunApplicationFreeCameraTests()
{
    FApplicationFreeCameraTestResult Result;
    std::cout << "[INFO] Running application free-camera tests\n";
    TestInitializationAndAspect(Result);
    TestMovementAndSpeed(Result);
    TestDeltaAndResume(Result);
    TestLookAndFov(Result);
    TestResetAndCadence(Result);
    std::cout << "[INFO] Application free-camera tests passed="
              << Result.Passed << " failed=" << Result.Failed << '\n';
    return Result.Failed == 0 ? 0 : 1;
}
