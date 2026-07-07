#include "Application/FCameraComponent.h"

namespace Stoner::Application
{

bool FCameraComponent::IsValid() const noexcept
{
    if (!Stoner::Core::FMath::IsFinite(NearPlane) ||
        !Stoner::Core::FMath::IsFinite(FarPlane) ||
        NearPlane <= 0.0f ||
        FarPlane <= NearPlane)
    {
        return false;
    }

    if (ProjectionType == ESceneProjectionType::Perspective)
    {
        return Stoner::Core::FMath::IsFinite(FieldOfViewRadians) &&
            FieldOfViewRadians > 0.0f &&
            FieldOfViewRadians < Stoner::Core::FMath::Pi;
    }

    return Stoner::Core::FMath::IsFinite(OrthographicExtent) &&
        OrthographicExtent > 0.0f;
}

} // namespace Stoner::Application
