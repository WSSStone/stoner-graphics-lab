#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FDeferredDiagnostics.h"

namespace Stoner::Renderer
{

struct FDeferredEntityIdentity
{
    Stoner::Core::uint32 Slot = 0;
    Stoner::Core::uint32 Generation = 0;

    [[nodiscard]] bool IsValid() const noexcept { return Slot != 0 && Generation != 0; }
    [[nodiscard]] friend bool operator==(FDeferredEntityIdentity Left, FDeferredEntityIdentity Right) noexcept
    {
        return Left.Slot == Right.Slot && Left.Generation == Right.Generation;
    }
    [[nodiscard]] friend bool operator<(FDeferredEntityIdentity Left, FDeferredEntityIdentity Right) noexcept
    {
        return Left.Slot < Right.Slot || (Left.Slot == Right.Slot && Left.Generation < Right.Generation);
    }
};

enum class EDeferredLightType
{
    Directional,
    Point,
    Spot
};

enum class EDeferredLightAcceptance
{
    AcceptedFullView,
    AcceptedVolumeOutsideCamera,
    AcceptedVolumeCameraInside,
    AcceptedVolumeNearPlaneIntersection,
    CulledOutsideView,
    RejectedInvalid
};

struct FDeferredDirectionalLight
{
    FDeferredEntityIdentity Identity;
    Stoner::Core::FString Name;
    Stoner::Core::FVector3 Direction = Stoner::Core::FVector3(0.0f, -1.0f, 0.0f);
    Stoner::Core::FColor Color = Stoner::Core::FColor::OpaqueWhite();
    float Intensity = 1.0f;
};

struct FDeferredPointLight
{
    FDeferredEntityIdentity Identity;
    Stoner::Core::FString Name;
    Stoner::Core::FVector3 Position = Stoner::Core::FVector3::Zero();
    Stoner::Core::FColor Color = Stoner::Core::FColor::OpaqueWhite();
    float Intensity = 1.0f;
    float Range = 1.0f;
};

struct FDeferredSpotLight : FDeferredPointLight
{
    Stoner::Core::FVector3 Direction = Stoner::Core::FVector3(0.0f, -1.0f, 0.0f);
    float InnerConeAngleRadians = 0.35f;
    float OuterConeAngleRadians = 0.5f;
};

struct FDeferredLightRecord
{
    FDeferredEntityIdentity Identity;
    Stoner::Core::FString Name;
    EDeferredLightType Type = EDeferredLightType::Directional;
    EDeferredLightAcceptance Acceptance = EDeferredLightAcceptance::RejectedInvalid;
    Stoner::Core::FVector3 Position = Stoner::Core::FVector3::Zero();
    Stoner::Core::FVector3 Direction = Stoner::Core::FVector3(0.0f, -1.0f, 0.0f);
    Stoner::Core::FColor Color = Stoner::Core::FColor::OpaqueWhite();
    float Intensity = 0.0f;
    float Range = 0.0f;
    float InnerConeAngleRadians = 0.0f;
    float OuterConeAngleRadians = 0.0f;
    Stoner::Core::FString Reason;
};

struct FDeferredLightSet
{
    Stoner::Core::TArray<FDeferredLightRecord> Accepted;
    Stoner::Core::TArray<FDeferredLightRecord> Culled;
    Stoner::Core::TArray<FDeferredLightRecord> Rejected;

    [[nodiscard]] Stoner::Core::uint32 GetAcceptedCount(EDeferredLightType Type) const noexcept;
};

[[nodiscard]] bool IsValidDeferredDirectionalLight(const FDeferredDirectionalLight& Light,
    FDeferredDiagnosticLog* Diagnostics = nullptr);
[[nodiscard]] bool IsValidDeferredPointLight(const FDeferredPointLight& Light,
    FDeferredDiagnosticLog* Diagnostics = nullptr);
[[nodiscard]] bool IsValidDeferredSpotLight(const FDeferredSpotLight& Light,
    FDeferredDiagnosticLog* Diagnostics = nullptr);
[[nodiscard]] FDeferredLightSet PrepareDeferredLightSet(
    const Stoner::Core::TArray<FDeferredDirectionalLight>& DirectionalLights,
    const Stoner::Core::TArray<FDeferredPointLight>& PointLights,
    const Stoner::Core::TArray<FDeferredSpotLight>& SpotLights,
    FDeferredDiagnosticLog* Diagnostics = nullptr);
void SortDeferredLightRecords(Stoner::Core::TArray<FDeferredLightRecord>& Records);
[[nodiscard]] const char* ToString(EDeferredLightType Type) noexcept;
[[nodiscard]] const char* ToString(EDeferredLightAcceptance Acceptance) noexcept;

} // namespace Stoner::Renderer
