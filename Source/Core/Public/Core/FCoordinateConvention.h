#pragma once

#include "Core/FVector3.h"

namespace Stoner::Core
{

// Engine-world convention. Component algebra remains conventional vector and
// Hamilton algebra; these named axes define the only supported world meaning.
struct FCoordinateConvention
{
    static constexpr const char* Name = "UnrealLH_ZUp_XForward_YRight_Meters_CW";

    [[nodiscard]] static constexpr FVector3 Forward() noexcept { return FVector3::UnitX(); }
    [[nodiscard]] static constexpr FVector3 Right() noexcept { return FVector3::UnitY(); }
    [[nodiscard]] static constexpr FVector3 Up() noexcept { return FVector3::UnitZ(); }
};

} // namespace Stoner::Core
