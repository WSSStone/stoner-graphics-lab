#include "FProductionCameraPreview.h"

#include "Asset/FAssetDigest.h"
#include "Core/FPlatformFileSystem.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <span>

namespace Stoner::Demo
{
namespace
{

using namespace Stoner::Application;
using namespace Stoner::Core;

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

[[nodiscard]] FWindowDisplayState MakeDisplayState(
    Core::uint32 Width,
    Core::uint32 Height) noexcept
{
    FWindowDisplayState Display;
    Display.LogicalExtent = {Width, Height};
    Display.DrawableExtent = {Width, Height};
    Display.ContentScale = {1.0f, 1.0f};
    Display.FramebufferScale = {1.0f, 1.0f};
    Display.DisplayGeneration = 1;
    Display.bFocused = true;
    Display.bMinimized = false;
    return Display;
}

void AppendMatrix(std::ostringstream& Stream, const FMatrix4x4& Matrix)
{
    Stream << '[';
    bool bFirst = true;
    for (int Row = 0; Row < 4; ++Row)
    {
        for (int Column = 0; Column < 4; ++Column)
        {
            if (!bFirst) Stream << ',';
            bFirst = false;
            Stream << Matrix.M[Row][Column];
        }
    }
    Stream << ']';
}

FString MatrixPayload(
    const FMatrix4x4& View,
    const FMatrix4x4& Projection)
{
    std::ostringstream Stream;
    Stream.imbue(std::locale::classic());
    Stream << std::setprecision(std::numeric_limits<float>::max_digits10);
    Stream << "{\"projection\":";
    AppendMatrix(Stream, Projection);
    Stream << ",\"view\":";
    AppendMatrix(Stream, View);
    Stream << '}';
    return FString(Stream.str());
}

} // namespace

bool FProductionCameraCandidate::IsValid() const noexcept
{
    return !WorkloadRevision.IsEmpty() && !Backend.IsEmpty() &&
        Width != 0 && Height != 0 && Camera.IsValid() &&
        MatrixSha256.Len() == 64 && !CanonicalJson.IsEmpty() &&
        CanonicalJson.Len() <= 64 * 1024;
}

bool FProductionCameraPreviewController::Initialize(
    const FProductionCameraPreset& Preset,
    Core::uint32 InWidth,
    Core::uint32 InHeight,
    Core::FString* OutReason)
{
    if (OutReason) OutReason->Clear();
    if (!Preset.IsValid() || InWidth == 0 || InHeight == 0)
    {
        Fail(OutReason, "camera preview initialization is invalid");
        return false;
    }
    Application::FFreeCameraState InitialState;
    if (!BuildInitialCameraState(
            Preset, InWidth, InHeight, InitialState, OutReason))
        return false;

    InitialCamera = Preset;
    Camera = Preset;
    InitialCameraState = InitialState;
    Width = InWidth;
    Height = InHeight;
    DisplayState = MakeDisplayState(InWidth, InHeight);
    ClearInputState();
    if (!CameraController.Initialize(
            InitialCameraState, DisplayState, OutReason))
        return false;

    if (!CameraController.GetState().Projection.NearlyEquals(
            InitialCamera.Projection, 1.0e-6f) &&
        !SyncCameraFromController())
    {
        Fail(OutReason, "camera preview current-aspect projection is invalid");
        return false;
    }

    // The legacy calibration loop historically integrated its first queued
    // input interval.  Consume the Application controller's required first
    // active interval explicitly at zero delta so that compatibility callers
    // retain that cadence without bypassing controller time validation.
    (void)CameraController.Update(
        Application::FFreeCameraActions{}, DisplayState, 0.0);
    return true;
}

bool FProductionCameraPreviewController::BuildInitialCameraState(
    const FProductionCameraPreset& Preset,
    Core::uint32 InWidth,
    Core::uint32 InHeight,
    Application::FFreeCameraState& OutState,
    Core::FString* OutReason) const
{
    if (!Preset.IsValid() || InWidth == 0 || InHeight == 0)
    {
        Fail(OutReason, "camera preview initial free-camera state is invalid");
        return false;
    }

    const Core::FVector3 Forward(
        Preset.View.M[0][0], Preset.View.M[0][1], Preset.View.M[0][2]);
    const float VerticalScale = -Preset.Projection.M[1][2];
    if (!Core::FMath::IsFinite(VerticalScale) || VerticalScale <= 0.0f)
    {
        Fail(OutReason, "camera preview preset has invalid vertical scale");
        return false;
    }

    OutState = {};
    OutState.CameraRevision = 1;
    OutState.Position = Preset.CameraPosition;
    OutState.YawRadians = std::atan2(Forward.Y, Forward.X);
    OutState.PitchRadians = std::asin(std::clamp(Forward.Z, -1.0f, 1.0f));
    OutState.VerticalFovRadians =
        2.0f * std::atan(1.0f / VerticalScale);
    OutState.NearPlane = 0.1f;
    OutState.FarPlane = 100.0f;
    OutState.MovementSpeed = 1.5f;
    OutState.DrawableExtent = {InWidth, InHeight};
    OutState.View = Preset.View;
    OutState.Projection = Preset.Projection;
    const float PresetAspect = VerticalScale / Preset.Projection.M[0][1];
    const float CurrentAspect = static_cast<float>(InWidth) /
        static_cast<float>(InHeight);
    if (!Core::FMath::IsNearlyEqual(
            PresetAspect, CurrentAspect, 1.0e-6f))
    {
        OutState.Projection = MakeProductionPerspective(
            OutState.VerticalFovRadians, CurrentAspect,
            OutState.NearPlane, OutState.FarPlane);
    }
    OutState.ViewProjection = Preset.ViewProjection;
    if (!OutState.Projection.NearlyEquals(Preset.Projection, 1.0e-6f))
        OutState.ViewProjection = OutState.Projection * OutState.View;
    if (!OutState.IsValid())
    {
        Fail(OutReason, "camera preview preset cannot seed free-camera state");
        return false;
    }
    return true;
}

bool FProductionCameraPreviewController::SyncCameraFromController() noexcept
{
    if (!CameraController.IsInitialized()) return false;
    FProductionCameraPreset Updated;
    if (!BuildProductionCameraPreset(
            InitialCamera.WorkloadRevision,
            CameraController.GetState().View,
            CameraController.GetState().Projection,
            Updated))
        return false;
    Camera = std::move(Updated);
    return true;
}

void FProductionCameraPreviewController::ClearInputState() noexcept
{
    bRightMouseHeld = false;
    bHasPointer = false;
    bInputQuarantined = false;
    PointerX = 0.0f;
    PointerY = 0.0f;
    HeldKeys.clear();
}

bool FProductionCameraPreviewController::IsHeld(EKey Key) const
{
    return HeldKeys.find(Key) != HeldKeys.end();
}

bool FProductionCameraPreviewController::IsNavigationKey(EKey Key) noexcept
{
    switch (Key)
    {
    case EKey::A:
    case EKey::D:
    case EKey::E:
    case EKey::Q:
    case EKey::S:
    case EKey::W:
    case EKey::LeftShift:
    case EKey::RightShift:
    case EKey::R:
        return true;
    default:
        return false;
    }
}

void FProductionCameraPreviewController::Reset() noexcept
{
    ClearInputState();
    if (!CameraController.IsInitialized())
    {
        Camera = InitialCamera;
        return;
    }
    if (!CameraController.Reset(DisplayState))
    {
        Camera = InitialCamera;
        return;
    }
    if (!SyncCameraFromController()) return;

    const float VerticalScale = -InitialCamera.Projection.M[1][2];
    const float PresetAspect =
        VerticalScale / InitialCamera.Projection.M[0][1];
    const float CurrentAspect = static_cast<float>(
        DisplayState.DrawableExtent.Width) /
        static_cast<float>(DisplayState.DrawableExtent.Height);
    if (Core::FMath::IsNearlyEqual(PresetAspect, CurrentAspect, 1.0e-6f))
        Camera = InitialCamera;
}

FProductionCameraPreviewUpdate FProductionCameraPreviewController::Update(
    const Core::TArray<FInputEvent>& Events,
    double DeltaSeconds)
{
    return Update(Events, DeltaSeconds, DisplayState);
}

FProductionCameraPreviewUpdate FProductionCameraPreviewController::Update(
    const Core::TArray<FInputEvent>& Events,
    double DeltaSeconds,
    const FWindowDisplayState& CurrentDisplay)
{
    FProductionCameraPreviewUpdate Result;
    DisplayState = CurrentDisplay;
    const bool bInteractiveDisplay = IsInteractiveDisplay(DisplayState);
    if (bInteractiveDisplay && bInputQuarantined)
        bInputQuarantined = false;
    if (!bInteractiveDisplay)
    {
        ClearInputState();
        bInputQuarantined = true;
        bNeedsZeroInterval = true;
    }

    Core::TArray<Application::FFreeCameraActions> OrderedActions;
    bool bSawFocusLoss = false;
    for (const FInputEvent& Event : Events)
    {
        switch (Event.EventType)
        {
        case EInputEventType::KeyDown:
            if (Event.Key == EKey::Enter)
                Result.bSnapshotRequested = true;
            else if (Event.Key == EKey::Escape)
            {
                Result.bExitRequested = true;
                ClearInputState();
                bInputQuarantined = true;
                OrderedActions.clear();
            }
            else if (Event.Key == EKey::R && !bInputQuarantined)
            {
                OrderedActions.push_back(
                    Application::FFreeCameraActions{.bReset = true});
                ClearInputState();
            }
            else if (!bInputQuarantined && IsNavigationKey(Event.Key))
                HeldKeys.insert(Event.Key);
            break;
        case EInputEventType::KeyUp:
            HeldKeys.erase(Event.Key);
            break;
        case EInputEventType::MouseButtonDown:
            if (Event.MouseButton == EMouseButton::Right &&
                !bInputQuarantined)
                bRightMouseHeld = true;
            break;
        case EInputEventType::MouseButtonUp:
            if (Event.MouseButton == EMouseButton::Right)
            {
                bRightMouseHeld = false;
                bHasPointer = false;
            }
            break;
        case EInputEventType::PointerMove:
            if (!bInputQuarantined && bHasPointer && bRightMouseHeld)
            {
                OrderedActions.push_back(Application::FFreeCameraActions{
                    .LookDeltaX = Event.PointerX - PointerX,
                    .LookDeltaY = Event.PointerY - PointerY,
                    .bLookCaptured = true});
            }
            if (!bInputQuarantined)
            {
                PointerX = Event.PointerX;
                PointerY = Event.PointerY;
                bHasPointer = true;
            }
            break;
        case EInputEventType::Scroll:
            if (!bInputQuarantined)
                OrderedActions.push_back(
                    Application::FFreeCameraActions{
                        .ScrollDeltaY = Event.DeltaY});
            break;
        case EInputEventType::FocusLost:
            ClearInputState();
            bInputQuarantined = true;
            bNeedsZeroInterval = true;
            OrderedActions.clear();
            bSawFocusLoss = true;
            break;
        case EInputEventType::Unknown:
            break;
        }
    }

    Application::FFreeCameraActions MovementActions;
    MovementActions.ForwardAxis = (IsHeld(EKey::W) ? 1.0f : 0.0f) -
        (IsHeld(EKey::S) ? 1.0f : 0.0f);
    MovementActions.RightAxis = (IsHeld(EKey::D) ? 1.0f : 0.0f) -
        (IsHeld(EKey::A) ? 1.0f : 0.0f);
    MovementActions.UpAxis = (IsHeld(EKey::E) ? 1.0f : 0.0f) -
        (IsHeld(EKey::Q) ? 1.0f : 0.0f);
    MovementActions.bFast = IsHeld(EKey::LeftShift) || IsHeld(EKey::RightShift);
    // A release can arrive in the same batch as its final pointer move.  That
    // already-captured delta remains in OrderedActions even though the
    // persistent capture state is now normal.
    MovementActions.bLookCaptured = bRightMouseHeld;

    Application::FWindowDisplayState ControllerDisplay = DisplayState;
    // PollInputEvents may discover a lifecycle reset after PollEvents took
    // its display snapshot.  Convey that inactive interval to the reusable
    // controller so its next focused update receives the required zero-time
    // resume interval.
    if (bSawFocusLoss) ControllerDisplay.bFocused = false;
    bool bCameraChanged = false;
    for (const Application::FFreeCameraActions& Action : OrderedActions)
    {
        const Application::FFreeCameraUpdateResult CameraUpdate =
            CameraController.Update(Action, ControllerDisplay, 0.0);
        bCameraChanged = bCameraChanged || CameraUpdate.bCameraChanged;
    }
    const bool bSuppressMovement = bNeedsZeroInterval || bSawFocusLoss ||
        !bInteractiveDisplay;
    const Application::FFreeCameraUpdateResult MovementUpdate =
        CameraController.Update(
            MovementActions, ControllerDisplay,
            bSuppressMovement ? 0.0 : DeltaSeconds);
    bCameraChanged = bCameraChanged || MovementUpdate.bCameraChanged;
    const bool bResetOnly = !OrderedActions.empty() &&
        OrderedActions.back().bReset &&
        MovementActions.ForwardAxis == 0.0f &&
        MovementActions.RightAxis == 0.0f &&
        MovementActions.UpAxis == 0.0f;
    if (bCameraChanged && SyncCameraFromController())
    {
        Result.bCameraChanged = true;
        if (bResetOnly && IsInteractiveDisplay(DisplayState))
        {
            const float VerticalScale = -InitialCamera.Projection.M[1][2];
            const float PresetAspect =
                VerticalScale / InitialCamera.Projection.M[0][1];
            const float CurrentAspect = static_cast<float>(
                DisplayState.DrawableExtent.Width) /
                static_cast<float>(DisplayState.DrawableExtent.Height);
            if (Core::FMath::IsNearlyEqual(
                    PresetAspect, CurrentAspect, 1.0e-6f))
                Camera = InitialCamera;
        }
    }
    if (bInteractiveDisplay && !bSawFocusLoss)
        bNeedsZeroInterval = false;
    return Result;
}

FProductionCameraCandidate FProductionCameraPreviewController::BuildCandidate(
    const char* Backend,
    const Core::FString& WorkloadRevision) const
{
    FProductionCameraCandidate Candidate;
    if (!Backend || Backend[0] == '\0' || WorkloadRevision.IsEmpty() ||
        !Camera.IsValid())
        return Candidate;
    Candidate.WorkloadRevision = WorkloadRevision;
    Candidate.Backend = Backend;
    Candidate.Width = Width;
    Candidate.Height = Height;
    Candidate.Camera = Camera;
    Candidate.Camera.WorkloadRevision = WorkloadRevision;
    const FString Matrices = MatrixPayload(Camera.View, Camera.Projection);
    const std::string MatrixText = Matrices.ToStdString();
    Candidate.MatrixSha256 = Asset::FAssetDigest::FromBytes(
        std::span<const Core::uint8>(
            reinterpret_cast<const Core::uint8*>(MatrixText.data()),
            MatrixText.size())).ToLowerHex();

    std::ostringstream Stream;
    Stream.imbue(std::locale::classic());
    Stream << std::setprecision(std::numeric_limits<float>::max_digits10);
    Stream << "{\"schema\":\"stoner.production-camera-candidate\",";
    Stream << "\"schemaVersion\":1,\"workloadRevision\":\""
           << WorkloadRevision.CStr() << "\",\"backend\":\""
           << Backend << "\",\"width\":" << Width
           << ",\"height\":" << Height << ",\"view\":";
    AppendMatrix(Stream, Camera.View);
    Stream << ",\"projection\":";
    AppendMatrix(Stream, Camera.Projection);
    Stream << ",\"matrixSha256\":\"" << Candidate.MatrixSha256.CStr()
           << "\"}\n";
    Candidate.CanonicalJson = FString(Stream.str());
    return Candidate;
}

bool WriteProductionCameraCandidate(
    const Core::FString& Path,
    const FProductionCameraCandidate& Candidate,
    Core::FString* OutReason)
{
    if (OutReason) OutReason->Clear();
    if (Path.IsEmpty() || !Candidate.IsValid())
    {
        Fail(OutReason, "camera candidate output is invalid");
        return false;
    }
    std::error_code Error;
    const std::filesystem::path Native(Path.CStr());
    if (!Native.parent_path().empty())
        std::filesystem::create_directories(Native.parent_path(), Error);
    if (Error)
    {
        Fail(OutReason, "camera candidate directory creation failed");
        return false;
    }
    const std::string Text = Candidate.CanonicalJson.ToStdString();
    const Core::TArray<Core::uint8> Bytes(Text.begin(), Text.end());
    if (!Core::FPlatformFileSystem::WriteFile(Path, Bytes))
    {
        Fail(OutReason, "camera candidate write failed");
        return false;
    }
    return true;
}

} // namespace Stoner::Demo
