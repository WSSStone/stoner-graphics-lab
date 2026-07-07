#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::Application
{

struct FTransformComponent
{
    Stoner::Core::FTransform LocalTransform = Stoner::Core::FTransform::Identity();
    Stoner::Core::FTransform WorldTransform = Stoner::Core::FTransform::Identity();
    bool bWorldTransformValid = false;

    FTransformComponent() noexcept = default;
    explicit FTransformComponent(const Stoner::Core::FTransform& InLocalTransform) noexcept
        : LocalTransform(InLocalTransform)
    {
    }

    [[nodiscard]] static FTransformComponent Identity() noexcept;
    [[nodiscard]] bool IsValid() const noexcept;
};

} // namespace Stoner::Application
