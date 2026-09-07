#include "Application/FFreeCameraState.h"
#include "Application/FInputOwnershipSnapshot.h"
#include "Application/FLabSettingsSnapshot.h"
#include "Application/FWindowDisplayState.h"
#include "Renderer/FOutputTransformSettings.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Stoner::Application
{
namespace
{

using namespace Stoner::Core;

constexpr uint32 MaximumExtentAxis = 4096;
constexpr uint64 MaximumDrawablePixels = 7864320;
constexpr float MatrixTolerance = 1.0e-4f;
constexpr float InverseTolerance = 5.0e-4f;
constexpr std::size_t MaximumIdentityStringLength = 4096;
constexpr std::size_t MaximumStageNameLength = 4096;

[[nodiscard]] bool IsBoundedExtent(
    const FWindowExtent& Extent,
    bool bAllowZero) noexcept
{
    if (Extent.IsZero()) return bAllowZero;
    if (!Extent.IsPositive() ||
        Extent.Width > MaximumExtentAxis ||
        Extent.Height > MaximumExtentAxis)
        return false;
    return static_cast<uint64>(Extent.Width) * Extent.Height <=
        MaximumDrawablePixels;
}

[[nodiscard]] bool IsFinite(const FVector3& Value) noexcept
{
    return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) &&
        FMath::IsFinite(Value.Z);
}

[[nodiscard]] bool IsFinite(const FVector2& Value) noexcept
{
    return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y);
}

[[nodiscard]] bool IsFinitePositive(const FVector2& Value) noexcept
{
    return IsFinite(Value) && Value.X > 0.0f && Value.Y > 0.0f;
}

[[nodiscard]] bool IsNearlyIdentity(const FMatrix4x4& Value) noexcept
{
    return Value.NearlyEquals(FMatrix4x4::Identity(), InverseTolerance);
}

[[nodiscard]] bool IsAffineOrthonormalView(
    const FMatrix4x4& View) noexcept
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

[[nodiscard]] bool IsPositiveXStandardZProjection(
    const FMatrix4x4& Projection) noexcept
{
    if (!Projection.IsFinite()) return false;
    return FMath::IsNearlyZero(Projection.M[0][0], MatrixTolerance) &&
        Projection.M[0][1] > 0.0f &&
        FMath::IsNearlyZero(Projection.M[0][2], MatrixTolerance) &&
        FMath::IsNearlyZero(Projection.M[0][3], MatrixTolerance) &&
        FMath::IsNearlyZero(Projection.M[1][0], MatrixTolerance) &&
        FMath::IsNearlyZero(Projection.M[1][1], MatrixTolerance) &&
        Projection.M[1][2] < 0.0f &&
        FMath::IsNearlyZero(Projection.M[1][3], MatrixTolerance) &&
        Projection.M[2][0] > 0.0f &&
        FMath::IsNearlyZero(Projection.M[2][1], MatrixTolerance) &&
        FMath::IsNearlyZero(Projection.M[2][2], MatrixTolerance) &&
        Projection.M[2][3] < 0.0f &&
        FMath::IsNearlyEqual(Projection.M[3][0], 1.0f, MatrixTolerance) &&
        FMath::IsNearlyZero(Projection.M[3][1], MatrixTolerance) &&
        FMath::IsNearlyZero(Projection.M[3][2], MatrixTolerance) &&
        FMath::IsNearlyZero(Projection.M[3][3], MatrixTolerance);
}

[[nodiscard]] FMatrix4x4 MakeCameraView(
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

[[nodiscard]] FMatrix4x4 MakeCameraProjection(
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

[[nodiscard]] bool IsValidIdentity(
    const FString& Value,
    std::size_t MaximumLength) noexcept
{
    const std::string_view Text = Value.View();
    if (Text.empty() || Text.size() > MaximumLength) return false;
    return std::all_of(Text.begin(), Text.end(), [](char Character)
    {
        return (Character >= 'A' && Character <= 'Z') ||
            (Character >= 'a' && Character <= 'z') ||
            (Character >= '0' && Character <= '9') ||
        Character == '.' || Character == '_' || Character == '-';
    });
}

[[nodiscard]] bool IsValidStageName(const FString& Value) noexcept
{
    return IsValidIdentity(Value, MaximumStageNameLength);
}

[[nodiscard]] bool IsValidColorDomain(
    Renderer::ERenderGraphColorDomain Domain) noexcept
{
    switch (Domain)
    {
    case Renderer::ERenderGraphColorDomain::SceneLinearRec709D65:
    case Renderer::ERenderGraphColorDomain::DisplayLinearRec709D65:
    case Renderer::ERenderGraphColorDomain::DisplayLinearRec2020D65:
    case Renderer::ERenderGraphColorDomain::EncodedSrgb:
    case Renderer::ERenderGraphColorDomain::EncodedBt709:
    case Renderer::ERenderGraphColorDomain::EncodedGamma22:
    case Renderer::ERenderGraphColorDomain::EncodedPqRec2020D65:
    case Renderer::ERenderGraphColorDomain::ExtendedSrgbLinear:
        return true;
    case Renderer::ERenderGraphColorDomain::Unspecified:
        return false;
    }
    return false;
}

[[nodiscard]] bool IsValidInputOwner(EInputOwner Owner) noexcept
{
    switch (Owner)
    {
    case EInputOwner::None:
    case EInputOwner::UI:
    case EInputOwner::Viewport:
        return true;
    }
    return false;
}

[[nodiscard]] bool IsValidCursorMode(ECursorMode Mode) noexcept
{
    switch (Mode)
    {
    case ECursorMode::Normal:
    case ECursorMode::Disabled:
        return true;
    }
    return false;
}

[[nodiscard]] bool IsValidCameraChangeFlags(
    ECameraChangeFlags Flags) noexcept
{
    constexpr uint32 ValidMask =
        static_cast<uint32>(ECameraChangeFlags::ContinuousMotion) |
        static_cast<uint32>(ECameraChangeFlags::Reset) |
        static_cast<uint32>(ECameraChangeFlags::PresetRestore) |
        static_cast<uint32>(ECameraChangeFlags::Cut) |
        static_cast<uint32>(ECameraChangeFlags::ProjectionChanged) |
        static_cast<uint32>(ECameraChangeFlags::ExtentChanged);
    return (static_cast<uint32>(Flags) & ~ValidMask) == 0;
}

} // namespace

bool FWindowDisplayState::IsValid() const noexcept
{
    if (DisplayGeneration == 0 ||
        !LogicalExtent.IsPositive() ||
        !IsBoundedExtent(DrawableExtent, true) ||
        !IsFinite(ContentScale) ||
        ContentScale.X < 0.5f || ContentScale.X > 4.0f ||
        ContentScale.Y < 0.5f || ContentScale.Y > 4.0f ||
        !IsFinitePositive(FramebufferScale))
        return false;

    if (DrawableExtent.IsZero()) return true;

    const float WidthRatio = static_cast<float>(DrawableExtent.Width) /
        static_cast<float>(LogicalExtent.Width);
    const float HeightRatio = static_cast<float>(DrawableExtent.Height) /
        static_cast<float>(LogicalExtent.Height);
    return FMath::IsNearlyEqual(FramebufferScale.X, WidthRatio,
            MatrixTolerance) &&
        FMath::IsNearlyEqual(FramebufferScale.Y, HeightRatio,
            MatrixTolerance);
}

bool FCameraChangeSet::IsValid() const noexcept
{
    return CameraRevision != 0 &&
        IsValidCameraChangeFlags(Flags) &&
        IsBoundedExtent(PreviousDrawableExtent, true) &&
        IsBoundedExtent(NewDrawableExtent, true);
}

bool FFreeCameraState::IsValid() const noexcept
{
    const float MinimumFov = FMath::DegreesToRadians(20.0f);
    const float MaximumFov = FMath::DegreesToRadians(90.0f);
    const float MinimumPitch = FMath::DegreesToRadians(-89.0f);
    const float MaximumPitch = FMath::DegreesToRadians(89.0f);
    if (CameraRevision == 0 || !IsFinite(Position) ||
        !FMath::IsFinite(YawRadians) || !FMath::IsFinite(PitchRadians) ||
        !FMath::IsFinite(VerticalFovRadians) ||
        !FMath::IsFinite(NearPlane) || !FMath::IsFinite(FarPlane) ||
        !FMath::IsFinite(MovementSpeed) ||
        PitchRadians < MinimumPitch || PitchRadians > MaximumPitch ||
        VerticalFovRadians < MinimumFov || VerticalFovRadians > MaximumFov ||
        NearPlane < 0.0001f || FarPlane <= NearPlane ||
        FarPlane > 1000000.0f || MovementSpeed < 0.01f ||
        MovementSpeed > 100.0f || !IsBoundedExtent(DrawableExtent, false) ||
        !IsAffineOrthonormalView(View) ||
        !IsPositiveXStandardZProjection(Projection) ||
        !ViewProjection.IsFinite())
        return false;

    if (!View.NearlyEquals(
            MakeCameraView(Position, YawRadians, PitchRadians),
            MatrixTolerance))
        return false;

    const float Aspect = static_cast<float>(DrawableExtent.Width) /
        static_cast<float>(DrawableExtent.Height);
    if (!FMath::IsFinite(Aspect) || Aspect <= 0.0f ||
        !Projection.NearlyEquals(
            MakeCameraProjection(VerticalFovRadians, Aspect,
                NearPlane, FarPlane), MatrixTolerance))
        return false;

    if (!ViewProjection.NearlyEquals(Projection * View, MatrixTolerance))
        return false;

    FMatrix4x4 Inverse;
    return ViewProjection.TryInverse(Inverse) &&
        Inverse.IsFinite() && IsNearlyIdentity(Inverse * ViewProjection);
}

bool FInputOwnershipSnapshot::IsValid() const noexcept
{
    if (EventSequence == 0 || FocusGeneration == 0 ||
        !IsValidInputOwner(KeyboardOwner) ||
        !IsValidInputOwner(PointerGestureOwner) ||
        !IsValidInputOwner(ScrollOwner) ||
        !IsValidCursorMode(CursorMode) ||
        (bPointerBaselineValid && !bFocused))
        return false;

    if (PressOwnerByKey[0] != EInputOwner::None ||
        PressOwnerByButton[0] != EInputOwner::None ||
        QuarantinedHeldKeys[0] || QuarantinedHeldButtons[0])
        return false;

    return std::all_of(PressOwnerByKey.begin(), PressOwnerByKey.end(),
            IsValidInputOwner) &&
        std::all_of(PressOwnerByButton.begin(), PressOwnerByButton.end(),
            IsValidInputOwner);
}

bool FLabDebugBypass::IsValid() const noexcept
{
    const float Difference = VisualizationMaximum - VisualizationMinimum;
    if (!FMath::IsFinite(VisualizationMinimum) ||
        !FMath::IsFinite(VisualizationMaximum) ||
        VisualizationMinimum >= VisualizationMaximum ||
        !FMath::IsFinite(Difference) || Difference <= 0.0f)
        return false;

    switch (Mode)
    {
    case Renderer::EOutputTransformDebugBypassMode::Disabled:
        return StageName.IsEmpty() &&
            SourceDomain == Renderer::ERenderGraphColorDomain::Unspecified;
    case Renderer::EOutputTransformDebugBypassMode::HDRPreservingReadback:
    case Renderer::EOutputTransformDebugBypassMode::BoundedVisualization:
        return IsValidStageName(StageName) && IsValidColorDomain(SourceDomain);
    }
    return false;
}

bool FLabDebugBypass::IsValidForResolvedStageDomain(
    const FString& ResolvedStageName,
    Renderer::ERenderGraphColorDomain ResolvedSourceDomain) const noexcept
{
    if (!IsValid()) return false;
    if (Mode == Renderer::EOutputTransformDebugBypassMode::Disabled)
        return true;
    return IsValidStageName(ResolvedStageName) &&
        IsValidColorDomain(ResolvedSourceDomain) &&
        StageName == ResolvedStageName && SourceDomain == ResolvedSourceDomain;
}

bool FLabSettingsSnapshot::IsValid() const noexcept
{
    Renderer::FOutputTransformSettingsValidator Validator;
    const Renderer::FOutputDeviceProfile* RequestedProfile =
        Validator.FindProfile(RequestedProfileId);
    const Renderer::FOutputDeviceProfile* EffectiveProfile =
        Validator.FindProfile(EffectiveProfileId);
    const Renderer::FOutputTransformStrategyIdentity* SdrStrategy =
        Validator.FindStrategy(SdrToneMapVersion);
    const Renderer::FOutputTransformStrategyIdentity* HdrStrategy =
        Validator.FindStrategy(HdrViewingVersion);

    const bool bKnownProfiles = RequestedProfile != nullptr &&
        EffectiveProfile != nullptr;
    const bool bValidIdentitySyntax =
        IsValidIdentity(RequestedProfileId, MaximumIdentityStringLength) &&
        IsValidIdentity(EffectiveProfileId, MaximumIdentityStringLength) &&
        IsValidIdentity(SdrToneMapVersion, MaximumIdentityStringLength) &&
        IsValidIdentity(HdrViewingVersion, MaximumIdentityStringLength);
    const bool bCorrectStrategies = SdrStrategy != nullptr &&
        SdrStrategy->Family == Renderer::EOutputTransformStrategyFamily::SDRToneMap &&
        HdrStrategy != nullptr &&
        HdrStrategy->Family == Renderer::EOutputTransformStrategyFamily::HDRViewing;

    return CameraRevision != 0 && SettingsRevision != 0 &&
        DisplayGeneration != 0 && OutputModeGeneration != 0 &&
        bValidIdentitySyntax && bKnownProfiles && bCorrectStrategies &&
        FMath::IsFinite(ExposureStops) && ExposureStops >= -16.0f &&
        ExposureStops <= 16.0f && DebugBypass.IsValid() &&
        FMath::IsFinite(UIWhiteMultiplier) &&
        UIWhiteMultiplier >= 0.25f && UIWhiteMultiplier <= 2.0f &&
        FMath::IsFinite(UIReferenceWhiteNits) &&
        UIReferenceWhiteNits > 0.0f &&
        FMath::IsFinite(NativePackingWhiteNits) &&
        NativePackingWhiteNits > 0.0f;
}

} // namespace Stoner::Application
