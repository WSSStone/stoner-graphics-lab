#pragma once

#include "Application/ESceneLightType.h"
#include "Core/CoreMinimal.h"

namespace Stoner::Application
{

struct FLightComponent
{
    ESceneLightType LightType = ESceneLightType::Point;
    Stoner::Core::FColor Color = Stoner::Core::FColor::OpaqueWhite();
    float Intensity = 1.0f;
    float Range = 1.0f;
    Stoner::Core::int32 OptionalSortKey = 0;
    bool bHasSortKey = false;

    [[nodiscard]] bool IsValid() const noexcept;
};

} // namespace Stoner::Application
