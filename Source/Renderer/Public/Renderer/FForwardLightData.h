#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FForwardDiagnostics.h"
#include "Renderer/FForwardViewData.h"

namespace Stoner::Renderer
{

struct FForwardDirectionalLight
{
    Stoner::Core::uint32 LightId = 0;
    Stoner::Core::FString Name;
    Stoner::Core::FVector3 Direction = Stoner::Core::FVector3(0.0f, -1.0f, 0.0f);
    Stoner::Core::FColor Color = Stoner::Core::FColor::OpaqueWhite();
    float Intensity = 1.0f;
    bool bPrimary = true;
};

struct FForwardPointLight
{
    Stoner::Core::uint32 LightId = 0;
    Stoner::Core::FString Name;
    Stoner::Core::FVector3 Position = Stoner::Core::FVector3::Zero();
    Stoner::Core::FColor Color = Stoner::Core::FColor::OpaqueWhite();
    float Intensity = 1.0f;
    float Range = 1.0f;
    float InfluenceScore = 0.0f;
};

struct FForwardRejectedLight
{
    Stoner::Core::uint32 LightId = 0;
    Stoner::Core::FString Name;
    Stoner::Core::FString ReasonCode;
};

struct FForwardLightSet
{
    bool bHasDirectionalLight = false;
    FForwardDirectionalLight DirectionalLight;
    Stoner::Core::TArray<FForwardPointLight> AcceptedPointLights;
    Stoner::Core::TArray<FForwardRejectedLight> RejectedLights;

    [[nodiscard]] bool HasAcceptedLights() const noexcept;
    [[nodiscard]] int GetAcceptedLightCount() const noexcept;
};

[[nodiscard]] bool IsValidForwardDirectionalLight(const FForwardDirectionalLight& Light,
    FForwardDiagnosticLog* Diagnostics = nullptr);
[[nodiscard]] bool IsValidForwardPointLight(const FForwardPointLight& Light,
    FForwardDiagnosticLog* Diagnostics = nullptr);
[[nodiscard]] float ComputeForwardPointLightInfluence(const FForwardPointLight& Light,
    const FForwardViewData& View) noexcept;
[[nodiscard]] FForwardLightSet PrepareForwardLightSet(const Stoner::Core::TArray<FForwardDirectionalLight>& DirectionalLights,
    const Stoner::Core::TArray<FForwardPointLight>& PointLights,
    const FForwardViewData& View,
    int PointLightLimit,
    FForwardDiagnosticLog* Diagnostics = nullptr);

} // namespace Stoner::Renderer
