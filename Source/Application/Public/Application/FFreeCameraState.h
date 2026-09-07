#pragma once

#include "Application/FWindowDisplayState.h"
#include "Core/CoreMinimal.h"

namespace Stoner::Application
{

enum class ECameraChangeFlags : Stoner::Core::uint32
{
    None = 0,
    ContinuousMotion = 1u << 0,
    Cut = 1u << 1,
    Reset = 1u << 2,
    PresetRestore = 1u << 3,
    ProjectionChanged = 1u << 4,
    ExtentChanged = 1u << 5
};

[[nodiscard]] constexpr ECameraChangeFlags operator|(
    ECameraChangeFlags Left,
    ECameraChangeFlags Right) noexcept
{
    return static_cast<ECameraChangeFlags>(
        static_cast<Stoner::Core::uint32>(Left) |
        static_cast<Stoner::Core::uint32>(Right));
}

[[nodiscard]] constexpr ECameraChangeFlags operator&(
    ECameraChangeFlags Left,
    ECameraChangeFlags Right) noexcept
{
    return static_cast<ECameraChangeFlags>(
        static_cast<Stoner::Core::uint32>(Left) &
        static_cast<Stoner::Core::uint32>(Right));
}

constexpr ECameraChangeFlags& operator|=(
    ECameraChangeFlags& Left,
    ECameraChangeFlags Right) noexcept
{
    Left = Left | Right;
    return Left;
}

[[nodiscard]] constexpr bool HasCameraChangeFlag(
    ECameraChangeFlags Flags,
    ECameraChangeFlags Flag) noexcept
{
    return (Flags & Flag) == Flag;
}

struct FCameraChangeSet
{
    ECameraChangeFlags Flags = ECameraChangeFlags::None;
    Stoner::Core::uint64 CameraRevision = 0;
    FWindowExtent PreviousDrawableExtent;
    FWindowExtent NewDrawableExtent;

    [[nodiscard]] bool HasFlag(ECameraChangeFlags Flag) const noexcept
    {
        return HasCameraChangeFlag(Flags, Flag);
    }

    [[nodiscard]] bool IsValid() const noexcept;
};

struct FFreeCameraState
{
    Stoner::Core::uint64 CameraRevision = 0;
    Stoner::Core::FVector3 Position = Stoner::Core::FVector3::Zero();
    float YawRadians = 0.0f;
    float PitchRadians = 0.0f;
    float VerticalFovRadians =
        Stoner::Core::FMath::DegreesToRadians(60.0f);
    float NearPlane = 0.1f;
    float FarPlane = 100.0f;
    float MovementSpeed = 1.5f;
    FWindowExtent DrawableExtent;
    Stoner::Core::FMatrix4x4 View = Stoner::Core::FMatrix4x4::Identity();
    Stoner::Core::FMatrix4x4 Projection =
        Stoner::Core::FMatrix4x4::Identity();
    Stoner::Core::FMatrix4x4 ViewProjection =
        Stoner::Core::FMatrix4x4::Identity();

    [[nodiscard]] bool IsValid() const noexcept;
};

} // namespace Stoner::Application
