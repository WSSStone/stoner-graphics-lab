#include "FImGuiInputAdapter.h"
#include <cfloat>
#include <cmath>

namespace Stoner::Application
{
namespace
{
ImGuiKey Translate(EKey Key)
{
    if (Key >= EKey::A && Key <= EKey::Z)
        return static_cast<ImGuiKey>(ImGuiKey_A + static_cast<int>(Key) - static_cast<int>(EKey::A));
    if (Key >= EKey::Num0 && Key <= EKey::Num9)
        return static_cast<ImGuiKey>(ImGuiKey_0 + static_cast<int>(Key) - static_cast<int>(EKey::Num0));
    if (Key >= EKey::F1 && Key <= EKey::F12)
        return static_cast<ImGuiKey>(ImGuiKey_F1 + static_cast<int>(Key) - static_cast<int>(EKey::F1));
    switch (Key)
    {
#define KEY(Engine, Native) case EKey::Engine: return ImGuiKey_##Native;
        KEY(Escape, Escape) KEY(Space, Space) KEY(Enter, Enter) KEY(Tab, Tab)
        KEY(Backspace, Backspace) KEY(Left, LeftArrow) KEY(Right, RightArrow)
        KEY(Up, UpArrow) KEY(Down, DownArrow) KEY(Home, Home) KEY(End, End)
        KEY(PageUp, PageUp) KEY(PageDown, PageDown) KEY(Insert, Insert) KEY(Delete, Delete)
        KEY(LeftShift, LeftShift) KEY(RightShift, RightShift)
        KEY(LeftControl, LeftCtrl) KEY(RightControl, RightCtrl)
        KEY(LeftAlt, LeftAlt) KEY(RightAlt, RightAlt)
        KEY(LeftSuper, LeftSuper) KEY(RightSuper, RightSuper)
#undef KEY
    default: return ImGuiKey_None;
    }
}
}

void FImGuiInputAdapter::Feed(ImGuiIO& IO,
    const Stoner::Core::TArray<FInputEvent>& Events, bool bFocused)
{
    bool Overflow = Events.size() > 4096;
    if (!Overflow)
        for (const auto& Event : Events) Overflow |= Event.EventType == EInputEventType::Overflow;
    if (Overflow)
    {
        IO.ClearEventsQueue();
        IO.ClearInputKeys();
        IO.ClearInputMouse();
        Held.fill(false);
        IO.AddFocusEvent(false);
        IO.AddFocusEvent(bFocused);
        return;
    }
    IO.AddFocusEvent(bFocused);
    auto Ordered = Events;
    SortInputEventsStable(Ordered);
    std::size_t TextBytes = 0;
    for (const auto& Event : Ordered)
        if (Event.EventType == EInputEventType::Text)
        {
            const auto C = Event.UnicodeScalar;
            if (C != 0 && C <= 0x10FFFF && !(C >= 0xD800 && C <= 0xDFFF))
                TextBytes += C <= 0x7F ? 1 : C <= 0x7FF ? 2 : C <= 0xFFFF ? 3 : 4;
        }
    for (const auto& Event : Ordered)
    {
        switch (Event.EventType)
        {
        case EInputEventType::KeyDown:
        case EInputEventType::KeyUp:
            if (IsKnownKey(Event.Key))
            {
                const bool Down = Event.EventType == EInputEventType::KeyDown;
                Held[static_cast<std::size_t>(Event.Key)] = Down;
                const auto Key = Translate(Event.Key);
                if (Key != ImGuiKey_None) IO.AddKeyEvent(Key, Down);
                auto IsHeld = [&](EKey K) { return Held[static_cast<std::size_t>(K)]; };
                IO.AddKeyEvent(ImGuiMod_Ctrl, IsHeld(EKey::LeftControl) || IsHeld(EKey::RightControl));
                IO.AddKeyEvent(ImGuiMod_Shift, IsHeld(EKey::LeftShift) || IsHeld(EKey::RightShift));
                IO.AddKeyEvent(ImGuiMod_Alt, IsHeld(EKey::LeftAlt) || IsHeld(EKey::RightAlt));
                IO.AddKeyEvent(ImGuiMod_Super, IsHeld(EKey::LeftSuper) || IsHeld(EKey::RightSuper));
            }
            break;
        case EInputEventType::Text:
            if (TextBytes <= 4096 && Event.UnicodeScalar != 0 && Event.UnicodeScalar <= 0x10FFFF &&
                !(Event.UnicodeScalar >= 0xD800 && Event.UnicodeScalar <= 0xDFFF))
                IO.AddInputCharacter(Event.UnicodeScalar);
            break;
        case EInputEventType::MouseButtonDown:
        case EInputEventType::MouseButtonUp:
            if (Event.MouseButton >= EMouseButton::Left && Event.MouseButton <= EMouseButton::X2)
                IO.AddMouseButtonEvent(static_cast<int>(Event.MouseButton) - 1,
                    Event.EventType == EInputEventType::MouseButtonDown);
            break;
        case EInputEventType::PointerMove:
            if (std::isfinite(Event.PointerX) && std::isfinite(Event.PointerY))
                IO.AddMousePosEvent(Event.PointerX, Event.PointerY);
            break;
        case EInputEventType::Scroll:
            if (std::isfinite(Event.DeltaX) && std::isfinite(Event.DeltaY))
                IO.AddMouseWheelEvent(Event.DeltaX, Event.DeltaY);
            break;
        case EInputEventType::FocusLost:
            IO.AddFocusEvent(false); Held.fill(false); break;
        case EInputEventType::FocusGained:
            IO.AddFocusEvent(true); break;
        case EInputEventType::CursorEntered:
            if (!Event.bCursorEntered) IO.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
            break;
        case EInputEventType::Overflow:
        case EInputEventType::Unknown: break;
        }
    }
}
}
