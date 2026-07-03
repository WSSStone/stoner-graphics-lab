#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FForwardFramePlan.h"

namespace Stoner::Renderer
{

enum class EForwardTransparentSortPolicy
{
    CameraDepthDescendingMaterialObject
};

struct FForwardRendererConfiguration
{
    int PointLightLimit = 4;
    bool bEnableAmbientFallback = true;
    bool bEnableSkyBackground = true;
    EForwardTransparentSortPolicy TransparentSortPolicy = EForwardTransparentSortPolicy::CameraDepthDescendingMaterialObject;
};

struct FForwardFrameInputs
{
    Stoner::Core::FString FrameName = "ForwardFrame";
    FForwardViewData View;
    FForwardOutputTarget Output;
    Stoner::Core::TArray<FMeshDrawCandidate> DrawCandidates;
    Stoner::Core::TArray<FForwardDirectionalLight> DirectionalLights;
    Stoner::Core::TArray<FForwardPointLight> PointLights;
    FForwardEnvironmentBackground Environment;
};

class FForwardRenderer
{
public:
    FForwardRenderer() = default;
    explicit FForwardRenderer(FForwardRendererConfiguration InConfiguration);

    [[nodiscard]] EForwardResult PrepareFrame(const FForwardFrameInputs& Inputs, FForwardFramePlan& OutPlan);
    void Reset();
    void Invalidate();

    [[nodiscard]] const FForwardRendererConfiguration& GetConfiguration() const noexcept;
    void SetConfiguration(FForwardRendererConfiguration InConfiguration) noexcept;
    [[nodiscard]] EForwardValidationState GetValidationState() const noexcept;
    [[nodiscard]] const FForwardFramePlan& GetLastFramePlan() const noexcept;
    [[nodiscard]] const FForwardDiagnosticLog& GetDiagnostics() const noexcept;

private:
    FForwardRendererConfiguration Configuration;
    FForwardFramePlan LastFramePlan;
    FForwardDiagnosticLog Diagnostics;
    EForwardValidationState ValidationState = EForwardValidationState::Draft;
    Stoner::Core::uint32 NextFrameId = 1;
};

[[nodiscard]] const char* ToString(EForwardTransparentSortPolicy Policy) noexcept;

} // namespace Stoner::Renderer
