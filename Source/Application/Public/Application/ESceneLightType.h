#pragma once

namespace Stoner::Application
{

enum class ESceneLightType
{
    Directional,
    Point
};

[[nodiscard]] const char* ToString(ESceneLightType Type) noexcept;

} // namespace Stoner::Application
