#pragma once

namespace Stoner::Application
{

enum class ESceneComponentType
{
    Transform,
    Mesh,
    Light,
    Camera
};

[[nodiscard]] const char* ToString(ESceneComponentType Type) noexcept;

} // namespace Stoner::Application
