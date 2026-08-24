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

constexpr float LookRadiansPerPixel = 0.003f;
constexpr float BaseMovementPerSecond = 1.5f;
constexpr float FastMovementMultiplier = 4.0f;
constexpr float MinimumPitch = -1.55334306f;
constexpr float MaximumPitch = 1.55334306f;
constexpr float MinimumFov = 0.34906585f;
constexpr float MaximumFov = 1.57079633f;

void Fail(FString* OutReason, const char* Reason)
{
    if (OutReason) *OutReason = Reason;
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
    InitialCamera = Preset;
    Camera = Preset;
    Width = InWidth;
    Height = InHeight;
    Aspect = static_cast<float>(InWidth) / static_cast<float>(InHeight);
    Reset();
    return true;
}

bool FProductionCameraPreviewController::IsHeld(EKey Key) const
{
    return HeldKeys.find(Key) != HeldKeys.end();
}

void FProductionCameraPreviewController::Reset() noexcept
{
    Camera = InitialCamera;
    Position = Camera.CameraPosition;
    const FVector3 Forward(
        Camera.View.M[0][0], Camera.View.M[0][1], Camera.View.M[0][2]);
    YawRadians = std::atan2(Forward.Y, Forward.X);
    PitchRadians = std::asin(std::clamp(Forward.Z, -1.0f, 1.0f));
    const float VerticalScale = -Camera.Projection.M[1][2];
    VerticalFovRadians = 2.0f * std::atan(1.0f / VerticalScale);
    Aspect = VerticalScale / Camera.Projection.M[0][1];
    bRightMouseHeld = false;
    bHasPointer = false;
    HeldKeys.clear();
}

void FProductionCameraPreviewController::RebuildCamera()
{
    FProductionCameraPreset Updated;
    if (BuildProductionCameraPreset(
            Camera.WorkloadRevision,
            MakeProductionCameraView(Position, YawRadians, PitchRadians),
            MakeProductionPerspective(VerticalFovRadians, Aspect),
            Updated))
        Camera = std::move(Updated);
}

FProductionCameraPreviewUpdate FProductionCameraPreviewController::Update(
    const Core::TArray<FInputEvent>& Events,
    double DeltaSeconds)
{
    FProductionCameraPreviewUpdate Result;
    bool bLookOrFovChanged = false;
    for (const FInputEvent& Event : Events)
    {
        switch (Event.EventType)
        {
        case EInputEventType::KeyDown:
            HeldKeys.insert(Event.Key);
            if (Event.Key == EKey::R)
            {
                Reset();
                Result.bCameraChanged = true;
            }
            else if (Event.Key == EKey::Enter)
                Result.bSnapshotRequested = true;
            else if (Event.Key == EKey::Escape)
                Result.bExitRequested = true;
            break;
        case EInputEventType::KeyUp:
            HeldKeys.erase(Event.Key);
            break;
        case EInputEventType::MouseButtonDown:
            if (Event.MouseButton == EMouseButton::Right)
                bRightMouseHeld = true;
            break;
        case EInputEventType::MouseButtonUp:
            if (Event.MouseButton == EMouseButton::Right)
                bRightMouseHeld = false;
            break;
        case EInputEventType::PointerMove:
            if (bHasPointer && bRightMouseHeld)
            {
                YawRadians += (Event.PointerX - PointerX) *
                    LookRadiansPerPixel;
                PitchRadians = std::clamp(
                    PitchRadians - (Event.PointerY - PointerY) *
                        LookRadiansPerPixel,
                    MinimumPitch, MaximumPitch);
                bLookOrFovChanged = true;
            }
            PointerX = Event.PointerX;
            PointerY = Event.PointerY;
            bHasPointer = true;
            break;
        case EInputEventType::Scroll:
            VerticalFovRadians = std::clamp(
                VerticalFovRadians - Event.DeltaY * 0.035f,
                MinimumFov, MaximumFov);
            bLookOrFovChanged = true;
            break;
        case EInputEventType::FocusLost:
            HeldKeys.clear();
            bRightMouseHeld = false;
            bHasPointer = false;
            break;
        case EInputEventType::Unknown:
            break;
        }
    }

    const double ClampedDelta = std::clamp(DeltaSeconds, 0.0, 0.25);
    FVector3 Movement = FVector3::Zero();
    const float CosPitch = std::cos(PitchRadians);
    const FVector3 Forward(
        CosPitch * std::cos(YawRadians),
        CosPitch * std::sin(YawRadians),
        std::sin(PitchRadians));
    const FVector3 Right(-std::sin(YawRadians), std::cos(YawRadians), 0.0f);
    if (IsHeld(EKey::W)) Movement = Movement + Forward;
    if (IsHeld(EKey::S)) Movement = Movement - Forward;
    if (IsHeld(EKey::D)) Movement = Movement + Right;
    if (IsHeld(EKey::A)) Movement = Movement - Right;
    if (IsHeld(EKey::E)) Movement = Movement + FVector3::UnitZ();
    if (IsHeld(EKey::Q)) Movement = Movement - FVector3::UnitZ();
    if (Movement.Length() > 0.0f && ClampedDelta > 0.0)
    {
        const float Speed = BaseMovementPerSecond *
            (IsHeld(EKey::LeftShift) || IsHeld(EKey::RightShift)
                ? FastMovementMultiplier : 1.0f);
        Position = Position + Movement.Normalized() *
            (Speed * static_cast<float>(ClampedDelta));
        Result.bCameraChanged = true;
    }
    if (bLookOrFovChanged) Result.bCameraChanged = true;
    if (Result.bCameraChanged) RebuildCamera();
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
