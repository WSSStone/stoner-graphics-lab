#include "Application/FWindowEvent.h"

#include <algorithm>

namespace Stoner::Application
{

FWindowEvent FWindowEvent::Created(Stoner::Core::uint32 WindowId,
    Stoner::Core::uint32 Width,
    Stoner::Core::uint32 Height,
    Stoner::Core::uint64 Sequence)
{
    FWindowEvent Event;
    Event.EventType = EWindowEventType::Created;
    Event.WindowId = WindowId;
    Event.ClientWidth = Width;
    Event.ClientHeight = Height;
    Event.Sequence = Sequence;
    return Event;
}

FWindowEvent FWindowEvent::Resized(Stoner::Core::uint32 Width,
    Stoner::Core::uint32 Height,
    Stoner::Core::uint64 Sequence)
{
    FWindowEvent Event;
    Event.EventType = EWindowEventType::Resized;
    Event.ClientWidth = Width;
    Event.ClientHeight = Height;
    Event.Sequence = Sequence;
    return Event;
}

FWindowEvent FWindowEvent::Minimized(Stoner::Core::uint64 Sequence)
{
    FWindowEvent Event;
    Event.EventType = EWindowEventType::Minimized;
    Event.Sequence = Sequence;
    return Event;
}

FWindowEvent FWindowEvent::Restored(Stoner::Core::uint32 Width,
    Stoner::Core::uint32 Height,
    Stoner::Core::uint64 Sequence)
{
    FWindowEvent Event;
    Event.EventType = EWindowEventType::Restored;
    Event.ClientWidth = Width;
    Event.ClientHeight = Height;
    Event.Sequence = Sequence;
    return Event;
}

FWindowEvent FWindowEvent::FocusGained(Stoner::Core::uint64 Sequence)
{
    FWindowEvent Event;
    Event.EventType = EWindowEventType::FocusGained;
    Event.Sequence = Sequence;
    return Event;
}

FWindowEvent FWindowEvent::FocusLost(Stoner::Core::uint64 Sequence)
{
    FWindowEvent Event;
    Event.EventType = EWindowEventType::FocusLost;
    Event.Sequence = Sequence;
    return Event;
}

FWindowEvent FWindowEvent::CloseRequested(Stoner::Core::uint64 Sequence)
{
    FWindowEvent Event;
    Event.EventType = EWindowEventType::CloseRequested;
    Event.Sequence = Sequence;
    return Event;
}

FWindowEvent FWindowEvent::Destroyed(Stoner::Core::uint64 Sequence)
{
    FWindowEvent Event;
    Event.EventType = EWindowEventType::Destroyed;
    Event.Sequence = Sequence;
    return Event;
}

FWindowEvent FWindowEvent::Unavailable(EWindowRuntimeAvailability Availability,
    Stoner::Core::FString Message,
    Stoner::Core::uint64 Sequence)
{
    FWindowEvent Event;
    Event.EventType = EWindowEventType::UnavailableRuntime;
    Event.RuntimeAvailability = Availability;
    Event.Message = std::move(Message);
    Event.Sequence = Sequence;
    return Event;
}

const char* ToString(EWindowEventType Type) noexcept
{
    switch (Type)
    {
    case EWindowEventType::Created: return "Created";
    case EWindowEventType::Resized: return "Resized";
    case EWindowEventType::Minimized: return "Minimized";
    case EWindowEventType::Restored: return "Restored";
    case EWindowEventType::FocusGained: return "FocusGained";
    case EWindowEventType::FocusLost: return "FocusLost";
    case EWindowEventType::CloseRequested: return "CloseRequested";
    case EWindowEventType::Destroyed: return "Destroyed";
    case EWindowEventType::UnavailableRuntime: return "UnavailableRuntime";
    }
    return "Unknown";
}

void SortWindowEventsStable(Stoner::Core::TArray<FWindowEvent>& Events)
{
    std::stable_sort(Events.begin(), Events.end(), [](const FWindowEvent& Left, const FWindowEvent& Right) {
        return Left.Sequence < Right.Sequence;
    });
}

} // namespace Stoner::Application
