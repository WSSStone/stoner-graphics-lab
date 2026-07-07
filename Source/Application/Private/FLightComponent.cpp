#include "Application/FLightComponent.h"

namespace Stoner::Application
{

bool FLightComponent::IsValid() const noexcept
{
    const bool bFiniteColor = Stoner::Core::FMath::IsFinite(Color.R) &&
        Stoner::Core::FMath::IsFinite(Color.G) &&
        Stoner::Core::FMath::IsFinite(Color.B) &&
        Stoner::Core::FMath::IsFinite(Color.A);
    const bool bFiniteLighting = Stoner::Core::FMath::IsFinite(Intensity) &&
        Stoner::Core::FMath::IsFinite(Range);
    if (!bFiniteColor || !bFiniteLighting || Intensity < 0.0f)
    {
        return false;
    }
    return LightType != ESceneLightType::Point || Range > 0.0f;
}

} // namespace Stoner::Application
