#include "Application/FInputEvent.h"

#include <algorithm>

namespace Stoner::Application
{

FInputEvent FInputEvent::KeyDown(EKey Key, Stoner::Core::uint64 Sequence)
{
    FInputEvent Event;
    Event.EventType = EInputEventType::KeyDown;
    Event.Key = Key;
    Event.Sequence = Sequence;
    return Event;
}

FInputEvent FInputEvent::KeyUp(EKey Key, Stoner::Core::uint64 Sequence)
{
    FInputEvent Event;
    Event.EventType = EInputEventType::KeyUp;
    Event.Key = Key;
    Event.Sequence = Sequence;
    return Event;
}

FInputEvent FInputEvent::MouseDown(EMouseButton Button, Stoner::Core::uint64 Sequence)
{
    FInputEvent Event;
    Event.EventType = EInputEventType::MouseButtonDown;
    Event.MouseButton = Button;
    Event.Sequence = Sequence;
    return Event;
}

FInputEvent FInputEvent::MouseUp(EMouseButton Button, Stoner::Core::uint64 Sequence)
{
    FInputEvent Event;
    Event.EventType = EInputEventType::MouseButtonUp;
    Event.MouseButton = Button;
    Event.Sequence = Sequence;
    return Event;
}

FInputEvent FInputEvent::PointerMove(float X, float Y, Stoner::Core::uint64 Sequence)
{
    FInputEvent Event;
    Event.EventType = EInputEventType::PointerMove;
    Event.PointerX = X;
    Event.PointerY = Y;
    Event.Sequence = Sequence;
    return Event;
}

FInputEvent FInputEvent::Scroll(float DeltaX, float DeltaY, Stoner::Core::uint64 Sequence)
{
    FInputEvent Event;
    Event.EventType = EInputEventType::Scroll;
    Event.DeltaX = DeltaX;
    Event.DeltaY = DeltaY;
    Event.Sequence = Sequence;
    return Event;
}

FInputEvent FInputEvent::FocusLost(Stoner::Core::uint64 Sequence)
{
    FInputEvent Event;
    Event.EventType = EInputEventType::FocusLost;
    Event.Sequence = Sequence;
    return Event;
}

FInputEvent FInputEvent::Unknown(Stoner::Core::uint64 Sequence)
{
    FInputEvent Event;
    Event.EventType = EInputEventType::Unknown;
    Event.Sequence = Sequence;
    return Event;
}

const char* ToString(EInputEventType Type) noexcept
{
    switch (Type)
    {
    case EInputEventType::KeyDown: return "KeyDown";
    case EInputEventType::KeyUp: return "KeyUp";
    case EInputEventType::MouseButtonDown: return "MouseButtonDown";
    case EInputEventType::MouseButtonUp: return "MouseButtonUp";
    case EInputEventType::PointerMove: return "PointerMove";
    case EInputEventType::Scroll: return "Scroll";
    case EInputEventType::FocusLost: return "FocusLost";
    case EInputEventType::Unknown: return "Unknown";
    }
    return "Unknown";
}

void SortInputEventsStable(Stoner::Core::TArray<FInputEvent>& Events)
{
    std::stable_sort(Events.begin(), Events.end(), [](const FInputEvent& Left, const FInputEvent& Right) {
        return Left.Sequence < Right.Sequence;
    });
}

} // namespace Stoner::Application
