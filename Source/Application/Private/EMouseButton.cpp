#include "Application/EMouseButton.h"

namespace Stoner::Application
{

bool IsKnownMouseButton(EMouseButton Button) noexcept
{
    return Button != EMouseButton::Unknown;
}

const char* ToString(EMouseButton Button) noexcept
{
    switch (Button)
    {
    case EMouseButton::Unknown: return "Unknown";
    case EMouseButton::Left: return "Left";
    case EMouseButton::Right: return "Right";
    case EMouseButton::Middle: return "Middle";
    case EMouseButton::X1: return "X1";
    case EMouseButton::X2: return "X2";
    }
    return "Unknown";
}

} // namespace Stoner::Application
