#pragma once

#include "Renderer/FDeferredFramePlan.h"

namespace Stoner::Renderer
{

struct FDeferredLightScissor
{
    Stoner::Core::uint32 X = 0;
    Stoner::Core::uint32 Y = 0;
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
};

struct FDeferredLightVolumeClassification
{
    EDeferredLightAcceptance Acceptance = EDeferredLightAcceptance::RejectedInvalid;
    FDeferredLightScissor Scissor;
    bool bIntersectsView = false;
};

[[nodiscard]] FDeferredLightVolumeClassification ClassifyDeferredLightVolume(
    const FDeferredLightRecord& Light, const FDeferredViewData& View) noexcept;
void ApplyDeferredLightVolumeCulling(FDeferredLightSet& LightSet,
    const FDeferredViewData& View, bool bCullOutsideView);

} // namespace Stoner::Renderer
