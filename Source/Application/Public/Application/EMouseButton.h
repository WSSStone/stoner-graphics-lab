#pragma once

namespace Stoner::Application
{

enum class EMouseButton
{
    Unknown,
    Left,
    Right,
    Middle,
    X1,
    X2
};

[[nodiscard]] bool IsKnownMouseButton(EMouseButton Button) noexcept;
[[nodiscard]] const char* ToString(EMouseButton Button) noexcept;

} // namespace Stoner::Application
