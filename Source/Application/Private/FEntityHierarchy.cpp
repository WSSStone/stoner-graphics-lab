#include "Application/FEntityHierarchy.h"

namespace Stoner::Application
{

const char* ToString(EReparentTransformPreservation Preservation) noexcept
{
    switch (Preservation)
    {
    case EReparentTransformPreservation::PreserveWorld: return "PreserveWorld";
    case EReparentTransformPreservation::PreserveLocal: return "PreserveLocal";
    }
    return "Unknown";
}

} // namespace Stoner::Application
