#pragma once

#include "Application/FEntity.h"

namespace Stoner::Application
{

enum class EReparentTransformPreservation
{
    PreserveWorld,
    PreserveLocal
};

struct FEntityHierarchyRecord
{
    FEntity Entity;
    FEntity Parent;
    Stoner::Core::TArray<FEntity> Children;
};

[[nodiscard]] const char* ToString(EReparentTransformPreservation Preservation) noexcept;

} // namespace Stoner::Application
