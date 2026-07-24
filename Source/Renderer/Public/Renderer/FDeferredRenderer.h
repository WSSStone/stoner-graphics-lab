#pragma once

#include "Renderer/FDeferredFramePlan.h"

namespace Stoner::Renderer
{

enum class ERendererStrategy
{
    Forward,
    Deferred
};

struct FDeferredRendererConfiguration
{
    bool bEnableMaskedGeometry = true;
    bool bEnableForwardTransparencyHandoff = true;
    bool bCullLocalLightsOutsideView = true;
    bool bEnableValidationReadback = false;
    Stoner::RHI::ERHISampleCount SampleCount = Stoner::RHI::ERHISampleCount::One;
};

struct FDeferredFrameInputs
{
    Stoner::Core::FString FrameId = "DeferredFrame";
    FDeferredViewData View;
    FDeferredOutputTarget Output;
    Stoner::Core::TArray<FDeferredDrawCandidate> DrawCandidates;
    Stoner::Core::TArray<FDeferredDirectionalLight> DirectionalLights;
    Stoner::Core::TArray<FDeferredPointLight> PointLights;
    Stoner::Core::TArray<FDeferredSpotLight> SpotLights;
    Stoner::Core::FColor AmbientContribution = Stoner::Core::FColor::Transparent();
};

class FDeferredRenderer
{
public:
    FDeferredRenderer() = default;
    explicit FDeferredRenderer(FDeferredRendererConfiguration InConfiguration);

    [[nodiscard]] EDeferredResult PrepareFrame(const FDeferredFrameInputs& Inputs,
        FDeferredFramePlan& OutPlan);
    void Reset();
    void Invalidate();

    [[nodiscard]] const FDeferredRendererConfiguration& GetConfiguration() const noexcept;
    void SetConfiguration(FDeferredRendererConfiguration InConfiguration) noexcept;
    [[nodiscard]] const FDeferredFramePlan& GetLastFramePlan() const noexcept;
    [[nodiscard]] bool IsInvalidated() const noexcept { return bInvalidated; }

private:
    FDeferredRendererConfiguration Configuration;
    FDeferredFramePlan LastFramePlan;
    bool bInvalidated = false;
};

[[nodiscard]] ERendererStrategy GetDefaultRendererStrategy() noexcept;
[[nodiscard]] const char* ToString(ERendererStrategy Strategy) noexcept;

} // namespace Stoner::Renderer
