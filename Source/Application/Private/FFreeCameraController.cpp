#include "Application/FFreeCameraController.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Stoner::Application
{
namespace
{

using namespace Stoner::Core;

constexpr float LookRadiansPerLogicalPixel = 0.003f;
constexpr float ScrollRadiansPerUnit = 0.035f;
constexpr float FastMovementMultiplier = 4.0f;
constexpr float MinimumPitch = FMath::DegreesToRadians(-89.0f);
constexpr float MaximumPitch = FMath::DegreesToRadians(89.0f);
constexpr float MinimumFov = FMath::DegreesToRadians(20.0f);
constexpr float MaximumFov = FMath::DegreesToRadians(90.0f);
constexpr double MaximumDeltaSeconds = 0.25;
constexpr uint64 MaximumRevision = std::numeric_limits<uint64>::max();

void Fail(FString* OutReason, const char* Reason)
{
    if (OutReason) *OutReason = Reason;
}

[[nodiscard]] bool IsInteractiveDisplay(
    const FWindowDisplayState& Display) noexcept
{
    return Display.bFocused && !Display.bMinimized &&
        Display.DrawableExtent.IsPositive();
}

[[nodiscard]] FMatrix4x4 MakeView(
    const FVector3& Position,
    float YawRadians,
    float PitchRadians) noexcept
{
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

[[nodiscard]] FMatrix4x4 MakeProjection(
    float VerticalFovRadians,
    float Aspect,
    float NearPlane,
    float FarPlane) noexcept
{
    const float VerticalScale =
        1.0f / std::tan(VerticalFovRadians * 0.5f);
    const float HorizontalScale = VerticalScale / Aspect;
    const float DepthScale = FarPlane / (FarPlane - NearPlane);
    return FMatrix4x4(
        0.0f, HorizontalScale, 0.0f, 0.0f,
        0.0f, 0.0f, -VerticalScale, 0.0f,
        DepthScale, 0.0f, 0.0f, -NearPlane * DepthScale,
        1.0f, 0.0f, 0.0f, 0.0f);
}

[[nodiscard]] bool RebuildDerivedCamera(
    FFreeCameraState& Camera,
    FWindowExtent DrawableExtent) noexcept
{
    if (!DrawableExtent.IsPositive()) return false;
    const float Aspect = static_cast<float>(DrawableExtent.Width) /
        static_cast<float>(DrawableExtent.Height);
    if (!FMath::IsFinite(Aspect) || Aspect <= 0.0f) return false;
    Camera.DrawableExtent = DrawableExtent;
    Camera.View = MakeView(Camera.Position, Camera.YawRadians,
        Camera.PitchRadians);
    Camera.Projection = MakeProjection(Camera.VerticalFovRadians, Aspect,
        Camera.NearPlane, Camera.FarPlane);
    Camera.ViewProjection = Camera.Projection * Camera.View;
    return Camera.IsValid();
}

[[nodiscard]] uint64 NextRevision(uint64 Revision) noexcept
{
    if (Revision == 0 || Revision == MaximumRevision) return 0;
    return Revision + 1;
}

[[nodiscard]] bool Different(float Left, float Right) noexcept
{
    // State changes are exact value changes.  The matrix validation tolerance
    // must not hide a small legal lens adjustment from the projection flag.
    return Left != Right;
}

} // namespace

bool FFreeCameraActions::IsValid() const noexcept
{
    return FMath::IsFinite(ForwardAxis) && ForwardAxis >= -1.0f &&
        ForwardAxis <= 1.0f && FMath::IsFinite(RightAxis) &&
        RightAxis >= -1.0f && RightAxis <= 1.0f &&
        FMath::IsFinite(UpAxis) && UpAxis >= -1.0f && UpAxis <= 1.0f &&
        FMath::IsFinite(LookDeltaX) && FMath::IsFinite(LookDeltaY) &&
        FMath::IsFinite(ScrollDeltaY);
}

bool FFreeCameraController::Initialize(
    const FFreeCameraState& InInitialState,
    const FWindowDisplayState& CurrentDisplay,
    FString* OutReason)
{
    if (OutReason) OutReason->Clear();
    if (!CurrentDisplay.IsValid() ||
        !CurrentDisplay.DrawableExtent.IsPositive() ||
        !InInitialState.IsValid())
    {
        Fail(OutReason, "free camera controller initialization is invalid");
        return false;
    }

    FFreeCameraState Candidate = InInitialState;
    if (Candidate.DrawableExtent != CurrentDisplay.DrawableExtent &&
        !RebuildDerivedCamera(Candidate, CurrentDisplay.DrawableExtent))
    {
        Fail(OutReason, "free camera controller current aspect is invalid");
        return false;
    }

    InitialState = InInitialState;
    State = Candidate;
    CurrentDrawableExtent = CurrentDisplay.DrawableExtent;
    bInitialized = true;
    bNeedsZeroInterval = true;
    return true;
}

bool FFreeCameraController::Commit(
    FFreeCameraState Candidate,
    ECameraChangeFlags Flags,
    FWindowExtent PreviousExtent,
    FFreeCameraUpdateResult& OutResult) noexcept
{
    const uint64 Revision = NextRevision(State.CameraRevision);
    if (Revision == 0) return false;
    Candidate.CameraRevision = Revision;
    if (!Candidate.IsValid()) return false;

    OutResult.bCameraChanged = true;
    OutResult.ChangeSet.Flags = Flags;
    OutResult.ChangeSet.CameraRevision = Candidate.CameraRevision;
    OutResult.ChangeSet.PreviousDrawableExtent = PreviousExtent;
    OutResult.ChangeSet.NewDrawableExtent = Candidate.DrawableExtent;
    State = std::move(Candidate);
    CurrentDrawableExtent = State.DrawableExtent;
    return true;
}

FFreeCameraUpdateResult FFreeCameraController::Update(
    const FFreeCameraActions& Actions,
    const FWindowDisplayState& CurrentDisplay,
    double DeltaSeconds) noexcept
{
    FFreeCameraUpdateResult Result;
    if (!bInitialized || !CurrentDisplay.IsValid())
        return Result;

    if (!IsInteractiveDisplay(CurrentDisplay))
    {
        bNeedsZeroInterval = true;
        return Result;
    }

    if (!Actions.IsValid()) return Result;

    const bool bSuppressMovement = bNeedsZeroInterval;
    bNeedsZeroInterval = false;
    const FWindowExtent PreviousExtent = State.DrawableExtent;
    const bool bExtentChanged =
        CurrentDisplay.DrawableExtent != State.DrawableExtent;

    FFreeCameraState Candidate = State;
    ECameraChangeFlags Flags = ECameraChangeFlags::None;
    if (bExtentChanged)
    {
        if (!RebuildDerivedCamera(Candidate,
                CurrentDisplay.DrawableExtent))
            return Result;
        Flags |= ECameraChangeFlags::ExtentChanged;
        Flags |= ECameraChangeFlags::ProjectionChanged;
    }

    if (Actions.bReset)
    {
        const float PreviousFov = Candidate.VerticalFovRadians;
        const float PreviousNear = Candidate.NearPlane;
        const float PreviousFar = Candidate.FarPlane;
        Candidate = InitialState;
        if (!RebuildDerivedCamera(Candidate,
                CurrentDisplay.DrawableExtent))
            return Result;
        Flags |= ECameraChangeFlags::Reset;
        Flags |= ECameraChangeFlags::Cut;
        if (Different(PreviousFov, Candidate.VerticalFovRadians) ||
            Different(PreviousNear, Candidate.NearPlane) ||
            Different(PreviousFar, Candidate.FarPlane))
            Flags |= ECameraChangeFlags::ProjectionChanged;
        return Commit(std::move(Candidate), Flags, PreviousExtent, Result)
            ? Result : FFreeCameraUpdateResult{};
    }

    if (Actions.bLookCaptured &&
        (Actions.LookDeltaX != 0.0f || Actions.LookDeltaY != 0.0f))
    {
        Candidate.YawRadians +=
            Actions.LookDeltaX * LookRadiansPerLogicalPixel;
        Candidate.PitchRadians = std::clamp(
            Candidate.PitchRadians -
                Actions.LookDeltaY * LookRadiansPerLogicalPixel,
            MinimumPitch, MaximumPitch);
        Flags |= ECameraChangeFlags::ContinuousMotion;
    }

    if (Actions.ScrollDeltaY != 0.0f)
    {
        Candidate.VerticalFovRadians = std::clamp(
            Candidate.VerticalFovRadians -
                Actions.ScrollDeltaY * ScrollRadiansPerUnit,
            MinimumFov, MaximumFov);
        Flags |= ECameraChangeFlags::ProjectionChanged;
    }

    const bool bValidDelta = std::isfinite(DeltaSeconds) &&
        DeltaSeconds >= 0.0;
    const double ClampedDelta = bValidDelta
        ? std::min(DeltaSeconds, MaximumDeltaSeconds) : 0.0;
    const FVector3 Forward(
        std::cos(Candidate.PitchRadians) * std::cos(Candidate.YawRadians),
        std::cos(Candidate.PitchRadians) * std::sin(Candidate.YawRadians),
        std::sin(Candidate.PitchRadians));
    const FVector3 Right(
        -std::sin(Candidate.YawRadians),
        std::cos(Candidate.YawRadians), 0.0f);
    const FVector3 Movement = Forward * Actions.ForwardAxis +
        Right * Actions.RightAxis + FVector3::UnitZ() * Actions.UpAxis;
    if (!bSuppressMovement && ClampedDelta > 0.0 &&
        Movement.Length() > 0.0f)
    {
        const float Speed = Candidate.MovementSpeed *
            (Actions.bFast ? FastMovementMultiplier : 1.0f);
        Candidate.Position = Candidate.Position + Movement.GetSafeNormal() *
            (Speed * static_cast<float>(ClampedDelta));
        Flags |= ECameraChangeFlags::ContinuousMotion;
    }

    const bool bPoseChanged = Candidate.Position != State.Position ||
        Candidate.YawRadians != State.YawRadians ||
        Candidate.PitchRadians != State.PitchRadians ||
        Candidate.VerticalFovRadians != State.VerticalFovRadians;
    if (!bPoseChanged && !bExtentChanged) return Result;

    if (Candidate.Position != State.Position ||
        Candidate.YawRadians != State.YawRadians ||
        Candidate.PitchRadians != State.PitchRadians)
    {
        Candidate.View = MakeView(Candidate.Position, Candidate.YawRadians,
            Candidate.PitchRadians);
    }
    if (Candidate.VerticalFovRadians != State.VerticalFovRadians ||
        bExtentChanged)
    {
        if (!RebuildDerivedCamera(Candidate,
                CurrentDisplay.DrawableExtent))
            return Result;
    }
    else
    {
        Candidate.ViewProjection = Candidate.Projection * Candidate.View;
    }

    return Commit(std::move(Candidate), Flags, PreviousExtent, Result)
        ? Result : FFreeCameraUpdateResult{};
}

bool FFreeCameraController::SetNavigationParameters(float MovementSpeed, float VerticalFovRadians,
    const FWindowDisplayState& Display, FCameraChangeSet* OutChangeSet) noexcept
{
    if (OutChangeSet) *OutChangeSet = {};
    if (!bInitialized || !Display.IsValid() || Display.bMinimized || !Display.DrawableExtent.IsPositive() ||
        !std::isfinite(MovementSpeed) || MovementSpeed < 0.01f || MovementSpeed > 100.0f ||
        !std::isfinite(VerticalFovRadians) || VerticalFovRadians < MinimumFov || VerticalFovRadians > MaximumFov)
        return false;
    if (State.MovementSpeed == MovementSpeed && State.VerticalFovRadians == VerticalFovRadians &&
        State.DrawableExtent == Display.DrawableExtent) return true;
    auto Candidate = State;
    Candidate.MovementSpeed = MovementSpeed;
    Candidate.VerticalFovRadians = VerticalFovRadians;
    auto Flags = ECameraChangeFlags::None;
    if (VerticalFovRadians != State.VerticalFovRadians) Flags |= ECameraChangeFlags::ProjectionChanged;
    if (State.DrawableExtent != Display.DrawableExtent)
        Flags |= ECameraChangeFlags::ExtentChanged | ECameraChangeFlags::ProjectionChanged;
    if (HasCameraChangeFlag(Flags,ECameraChangeFlags::ProjectionChanged) &&
        !RebuildDerivedCamera(Candidate,Display.DrawableExtent)) return false;
    FFreeCameraUpdateResult Result;
    if (!Commit(std::move(Candidate),Flags,State.DrawableExtent,Result)) return false;
    if (OutChangeSet) *OutChangeSet = Result.ChangeSet;
    return true;
}

bool FFreeCameraController::RestorePreset(const FFreeCameraState& Preset,
    const FWindowDisplayState& Display, FCameraChangeSet* OutChangeSet) noexcept
{
    if (OutChangeSet) *OutChangeSet = {};
    if (!bInitialized || !Display.IsValid() || Display.bMinimized || !Display.DrawableExtent.IsPositive() ||
        !std::isfinite(Preset.YawRadians) || !std::isfinite(Preset.PitchRadians) ||
        Preset.PitchRadians < MinimumPitch || Preset.PitchRadians > MaximumPitch ||
        !std::isfinite(Preset.VerticalFovRadians) || Preset.VerticalFovRadians < MinimumFov || Preset.VerticalFovRadians > MaximumFov ||
        !std::isfinite(Preset.NearPlane) || !std::isfinite(Preset.FarPlane) ||
        Preset.NearPlane < 0.0001f || Preset.NearPlane >= Preset.FarPlane || Preset.FarPlane > 1000000.0f ||
        !std::isfinite(Preset.MovementSpeed) || Preset.MovementSpeed < 0.01f || Preset.MovementSpeed > 100.0f)
        return false;
    auto Candidate = State;
    Candidate.Position = Preset.Position;
    Candidate.YawRadians = Preset.YawRadians; Candidate.PitchRadians = Preset.PitchRadians;
    Candidate.VerticalFovRadians = Preset.VerticalFovRadians;
    Candidate.NearPlane = Preset.NearPlane; Candidate.FarPlane = Preset.FarPlane;
    Candidate.MovementSpeed = Preset.MovementSpeed;
    if (!RebuildDerivedCamera(Candidate,Display.DrawableExtent)) return false;
    auto Flags = ECameraChangeFlags::PresetRestore | ECameraChangeFlags::Cut;
    if (State.VerticalFovRadians != Candidate.VerticalFovRadians || State.NearPlane != Candidate.NearPlane || State.FarPlane != Candidate.FarPlane)
        Flags |= ECameraChangeFlags::ProjectionChanged;
    if (State.DrawableExtent != Candidate.DrawableExtent)
        Flags |= ECameraChangeFlags::ExtentChanged | ECameraChangeFlags::ProjectionChanged;
    FFreeCameraUpdateResult Result;
    if (!Commit(std::move(Candidate),Flags,State.DrawableExtent,Result)) return false;
    bNeedsZeroInterval = true;
    if (OutChangeSet) *OutChangeSet = Result.ChangeSet;
    return true;
}

bool FFreeCameraController::Reset(
    const FWindowDisplayState& CurrentDisplay,
    FCameraChangeSet* OutChangeSet) noexcept
{
    if (OutChangeSet) *OutChangeSet = FCameraChangeSet{};
    if (!bInitialized || !CurrentDisplay.IsValid() || CurrentDisplay.bMinimized ||
        !CurrentDisplay.DrawableExtent.IsPositive()) return false;
    auto Candidate=InitialState;
    if (!RebuildDerivedCamera(Candidate,CurrentDisplay.DrawableExtent)) return false;
    auto Flags=ECameraChangeFlags::Reset | ECameraChangeFlags::Cut;
    if (State.VerticalFovRadians != Candidate.VerticalFovRadians ||
        State.NearPlane != Candidate.NearPlane || State.FarPlane != Candidate.FarPlane)
        Flags |= ECameraChangeFlags::ProjectionChanged;
    if (State.DrawableExtent != Candidate.DrawableExtent)
        Flags |= ECameraChangeFlags::ExtentChanged | ECameraChangeFlags::ProjectionChanged;
    FFreeCameraUpdateResult Result;
    if (!Commit(std::move(Candidate),Flags,State.DrawableExtent,Result)) return false;
    if (OutChangeSet && Result.bCameraChanged)
        *OutChangeSet = Result.ChangeSet;
    return Result.bCameraChanged;
}

} // namespace Stoner::Application
