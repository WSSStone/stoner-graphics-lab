#include "Application/FInputState.h"

#include <algorithm>

namespace Stoner::Application
{

namespace
{

template <typename T>
bool Contains(const Stoner::Core::TArray<T>& Values, T Value)
{
    return std::find(Values.begin(), Values.end(), Value) != Values.end();
}

template <typename T>
void AddUnique(Stoner::Core::TArray<T>& Values, T Value)
{
    if (!Contains(Values, Value))
    {
        Values.push_back(Value);
    }
}

template <typename T>
void Remove(Stoner::Core::TArray<T>& Values, T Value)
{
    Values.erase(std::remove(Values.begin(), Values.end(), Value), Values.end());
}

} // namespace

void FInputState::BeginFrame()
{
    PressedKeys.clear();
    ReleasedKeys.clear();
    PressedMouseButtons.clear();
    ReleasedMouseButtons.clear();
    PointerDeltaX = 0.0f;
    PointerDeltaY = 0.0f;
}

void FInputState::ClearAll()
{
    HeldKeys.clear();
    PressedKeys.clear();
    ReleasedKeys.clear();
    HeldMouseButtons.clear();
    PressedMouseButtons.clear();
    ReleasedMouseButtons.clear();
}

void FInputState::ApplyEvent(const FInputEvent& Event, FApplicationDiagnosticLog* Diagnostics)
{
    switch (Event.EventType)
    {
    case EInputEventType::KeyDown:
        if (!IsKnownKey(Event.Key))
        {
            if (Diagnostics != nullptr)
            {
                Diagnostics->Add(EApplicationDiagnosticSeverity::Warning, EApplicationDiagnosticCategory::Input,
                    EApplicationResult::InvalidInput, "APP-INPUT-UNKNOWN-KEY", "Key", "Unknown key input ignored");
            }
            return;
        }
        if (!Contains(HeldKeys, Event.Key))
        {
            AddUnique(PressedKeys, Event.Key);
        }
        AddUnique(HeldKeys, Event.Key);
        break;
    case EInputEventType::KeyUp:
        if (!IsKnownKey(Event.Key))
        {
            if (Diagnostics != nullptr)
            {
                Diagnostics->Add(EApplicationDiagnosticSeverity::Warning, EApplicationDiagnosticCategory::Input,
                    EApplicationResult::InvalidInput, "APP-INPUT-UNKNOWN-KEY", "Key", "Unknown key input ignored");
            }
            return;
        }
        if (Contains(HeldKeys, Event.Key))
        {
            AddUnique(ReleasedKeys, Event.Key);
        }
        Remove(HeldKeys, Event.Key);
        break;
    case EInputEventType::MouseButtonDown:
        if (!IsKnownMouseButton(Event.MouseButton))
        {
            if (Diagnostics != nullptr)
            {
                Diagnostics->Add(EApplicationDiagnosticSeverity::Warning, EApplicationDiagnosticCategory::Input,
                    EApplicationResult::InvalidInput, "APP-INPUT-UNKNOWN-MOUSE", "Mouse", "Unknown mouse input ignored");
            }
            return;
        }
        if (!Contains(HeldMouseButtons, Event.MouseButton))
        {
            AddUnique(PressedMouseButtons, Event.MouseButton);
        }
        AddUnique(HeldMouseButtons, Event.MouseButton);
        break;
    case EInputEventType::MouseButtonUp:
        if (!IsKnownMouseButton(Event.MouseButton))
        {
            if (Diagnostics != nullptr)
            {
                Diagnostics->Add(EApplicationDiagnosticSeverity::Warning, EApplicationDiagnosticCategory::Input,
                    EApplicationResult::InvalidInput, "APP-INPUT-UNKNOWN-MOUSE", "Mouse", "Unknown mouse input ignored");
            }
            return;
        }
        if (Contains(HeldMouseButtons, Event.MouseButton))
        {
            AddUnique(ReleasedMouseButtons, Event.MouseButton);
        }
        Remove(HeldMouseButtons, Event.MouseButton);
        break;
    case EInputEventType::PointerMove:
        if (bHasPointerPosition)
        {
            PointerDeltaX += Event.PointerX - PointerX;
            PointerDeltaY += Event.PointerY - PointerY;
        }
        else
        {
            PointerDeltaX += Event.DeltaX;
            PointerDeltaY += Event.DeltaY;
            bHasPointerPosition = true;
        }
        PointerX = Event.PointerX;
        PointerY = Event.PointerY;
        break;
    case EInputEventType::Scroll:
        break;
    case EInputEventType::FocusLost:
        bFocused = false;
        ClearAll();
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EApplicationDiagnosticSeverity::Info, EApplicationDiagnosticCategory::Input,
                EApplicationResult::Success, "APP-INPUT-FOCUS-CLEAR", "Focus", "Focus loss cleared held input state");
        }
        break;
    case EInputEventType::Unknown:
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EApplicationDiagnosticSeverity::Warning, EApplicationDiagnosticCategory::Input,
                EApplicationResult::InvalidInput, "APP-INPUT-UNKNOWN-EVENT", "Input", "Unknown input event ignored");
        }
        break;
    }
}

bool FInputState::IsKeyHeld(EKey Key) const
{
    return Contains(HeldKeys, Key);
}

bool FInputState::WasKeyPressed(EKey Key) const
{
    return Contains(PressedKeys, Key);
}

bool FInputState::WasKeyReleased(EKey Key) const
{
    return Contains(ReleasedKeys, Key);
}

bool FInputState::IsMouseButtonHeld(EMouseButton Button) const
{
    return Contains(HeldMouseButtons, Button);
}

bool FInputState::WasMouseButtonPressed(EMouseButton Button) const
{
    return Contains(PressedMouseButtons, Button);
}

bool FInputState::WasMouseButtonReleased(EMouseButton Button) const
{
    return Contains(ReleasedMouseButtons, Button);
}

} // namespace Stoner::Application
