#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FForwardDiagnostics.h"

namespace Stoner::Renderer
{

struct FForwardExtent2D
{
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
};

struct FForwardViewport
{
    Stoner::Core::uint32 X = 0;
    Stoner::Core::uint32 Y = 0;
    FForwardExtent2D Extent;
};

struct FForwardViewData
{
    Stoner::Core::FString ViewName;
    Stoner::Core::FMatrix4x4 ViewMatrix = Stoner::Core::FMatrix4x4::Identity();
    Stoner::Core::FMatrix4x4 ViewProjectionMatrix = Stoner::Core::FMatrix4x4::Identity();
    Stoner::Core::FVector3 CameraPosition = Stoner::Core::FVector3::Zero();
    FForwardViewport Viewport;

    [[nodiscard]] bool IsValid(FForwardDiagnosticLog* Diagnostics = nullptr) const;
    [[nodiscard]] float ComputeCameraSpaceDepth(Stoner::Core::FVector3 WorldPosition) const noexcept;
};

[[nodiscard]] bool IsForwardFinite(float Value) noexcept;
[[nodiscard]] bool IsForwardFinite(const Stoner::Core::FVector3& Value) noexcept;
[[nodiscard]] bool IsForwardFinite(const Stoner::Core::FVector4& Value) noexcept;
[[nodiscard]] bool IsForwardFinite(const Stoner::Core::FColor& Value) noexcept;
[[nodiscard]] bool IsForwardFinite(const Stoner::Core::FMatrix4x4& Value) noexcept;
[[nodiscard]] bool IsPositiveForwardExtent(FForwardExtent2D Extent) noexcept;
[[nodiscard]] bool IsStableForwardId(Stoner::Core::uint32 Id) noexcept;
[[nodiscard]] Stoner::Core::FString FormatForwardVector(Stoner::Core::FVector3 Value);
[[nodiscard]] Stoner::Core::FVector3 TransformWorldPositionToView(
    const Stoner::Core::FMatrix4x4& ViewMatrix,
    Stoner::Core::FVector3 WorldPosition) noexcept;
// The active world convention defines forward depth as view-space +X.
[[nodiscard]] float ComputeViewSpaceForwardDepth(
    const Stoner::Core::FMatrix4x4& ViewMatrix,
    Stoner::Core::FVector3 WorldPosition) noexcept;

} // namespace Stoner::Renderer
