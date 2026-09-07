#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FOutputTransformSettings.h"

namespace Stoner::Renderer
{

// Renderer receives this copied value after the output profile has been
// resolved. It carries no native output object and no platform metadata.
struct FUICompositionSettings
{
    Stoner::Core::FString OutputProfileId;
    ERenderGraphColorDomain BlendDomain =
        ERenderGraphColorDomain::Unspecified;
    float UIWhiteMultiplier = 1.0f;
    float UIReferenceWhiteNits = 0.0f;
    float NativePackingWhiteNits = 0.0f;
    Stoner::Core::uint64 DisplayGeneration = 0;

    [[nodiscard]] bool IsValid() const noexcept;
};

} // namespace Stoner::Renderer
