#include "Application/FEntity.h"

#include <sstream>

namespace Stoner::Application
{

Stoner::Core::FString FormatEntityIdentity(const FEntity& Entity)
{
    if (!Entity.IsSet())
    {
        return Stoner::Core::FString("entity[invalid]");
    }

    std::ostringstream Stream;
    Stream << "entity[" << Entity.SlotIndex << ':' << Entity.Generation << ']';
    return Stoner::Core::FString(Stream.str());
}

} // namespace Stoner::Application
