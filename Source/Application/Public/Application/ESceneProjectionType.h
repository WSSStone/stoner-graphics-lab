#pragma once

namespace Stoner::Application
{

enum class ESceneProjectionType
{
    Perspective,
    Orthographic
};

[[nodiscard]] const char* ToString(ESceneProjectionType Type) noexcept;

} // namespace Stoner::Application
