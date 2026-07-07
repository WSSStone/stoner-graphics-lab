#pragma once

#include "Application/ESceneProjectionType.h"
#include "Core/CoreMinimal.h"

namespace Stoner::Application
{

struct FCameraComponent
{
    ESceneProjectionType ProjectionType = ESceneProjectionType::Perspective;
    float FieldOfViewRadians = Stoner::Core::FMath::DegreesToRadians(60.0f);
    float OrthographicExtent = 10.0f;
    float NearPlane = 0.1f;
    float FarPlane = 1000.0f;
    bool bActiveCamera = false;
    Stoner::Core::int32 OptionalSortKey = 0;
    bool bHasSortKey = false;

    [[nodiscard]] bool IsValid() const noexcept;
};

} // namespace Stoner::Application
