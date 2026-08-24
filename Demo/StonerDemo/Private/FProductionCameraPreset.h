#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::Demo
{

struct FProductionCameraPreset
{
    Core::FString WorkloadRevision;
    Core::FMatrix4x4 View = Core::FMatrix4x4::Identity();
    Core::FMatrix4x4 Projection = Core::FMatrix4x4::Identity();
    Core::FMatrix4x4 ViewProjection = Core::FMatrix4x4::Identity();
    Core::FMatrix4x4 InverseViewProjection = Core::FMatrix4x4::Identity();
    Core::FVector3 CameraPosition = Core::FVector3::Zero();

    [[nodiscard]] bool IsValid() const noexcept;
};

[[nodiscard]] Core::FMatrix4x4 MakeProductionPerspective(
    float VerticalFovRadians,
    float Aspect,
    float NearPlane = 0.1f,
    float FarPlane = 100.0f) noexcept;

[[nodiscard]] Core::FMatrix4x4 MakeProductionCameraView(
    const Core::FVector3& Position,
    float YawRadians,
    float PitchRadians) noexcept;

[[nodiscard]] bool BuildProductionCameraPreset(
    const Core::FString& WorkloadRevision,
    const Core::FMatrix4x4& View,
    const Core::FMatrix4x4& Projection,
    FProductionCameraPreset& OutPreset,
    Core::FString* OutReason = nullptr);

[[nodiscard]] bool ResolveProductionCameraPreset(
    const Core::FString& WorkloadRevision,
    FProductionCameraPreset& OutPreset,
    Core::FString* OutReason = nullptr);

} // namespace Stoner::Demo
