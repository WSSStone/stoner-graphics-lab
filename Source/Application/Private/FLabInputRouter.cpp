#include "FLabInputRouter.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Stoner::Application
{
namespace
{
std::size_t Index(EKey Key) { return static_cast<std::size_t>(Key); }
std::size_t Index(EMouseButton Button) { return static_cast<std::size_t>(Button); }
}

void FLabInputRouter::Quarantine() noexcept
{
    for (std::size_t I = 0; I < HeldKeys.size(); ++I)
        if (HeldKeys[I]) Ownership.QuarantinedHeldKeys[I] = true;
    for (std::size_t I = 0; I < HeldButtons.size(); ++I)
        if (HeldButtons[I]) Ownership.QuarantinedHeldButtons[I] = true;
    Ownership.CursorMode = ECursorMode::Normal;
    Ownership.bPointerBaselineValid = false;
    Ownership.PointerGestureOwner = EInputOwner::None;
}

FLabRoutedInput FLabInputRouter::Resolve(
    const Stoner::Core::TArray<FInputEvent>& Events,
    const FUILabCapture& Capture, bool bFocused, bool bOverflow)
{
    FLabRoutedInput Result;
    for (const auto& Event : Events) bOverflow |= Event.EventType == EInputEventType::Overflow;
    if (!bInitialized)
    {
        Ownership.FocusGeneration = 1;
        Ownership.EventSequence = 1;
        Ownership.bFocused = bFocused;
        bInitialized = true;
    }
    auto ChangeFocus = [&](bool Focused)
    {
        if (Ownership.bFocused != Focused)
        {
            if (Ownership.FocusGeneration != std::numeric_limits<Stoner::Core::uint64>::max())
                ++Ownership.FocusGeneration;
            Ownership.bFocused = Focused;
            Quarantine();
        }
    };
    // The final window state may include focus events still in this interval.
    // Only use it as an initial override when there are no ordered focus events.
    bool HasFocusEvent = false;
    for (const auto& Event : Events)
        HasFocusEvent |= Event.EventType == EInputEventType::FocusLost ||
            Event.EventType == EInputEventType::FocusGained;
    if (!HasFocusEvent) ChangeFocus(bFocused);
    const bool KeyboardUI = Capture.bKeyboard || Capture.bTextEditing;
    if ((KeyboardUI && Ownership.KeyboardOwner != EInputOwner::UI) ||
        Capture.bVisibilityChanged || bOverflow) Quarantine();
    Ownership.KeyboardOwner = KeyboardUI ? EInputOwner::UI : EInputOwner::Viewport;
    if (bOverflow)
    {
        // Unknown releases cannot be reconstructed. Quarantine all navigation
        // until observed release; never interpret this incomplete interval.
        Ownership.QuarantinedHeldKeys.fill(true);
        Ownership.QuarantinedHeldButtons.fill(true);
        Ownership.QuarantinedHeldKeys[0] = false;
        Ownership.QuarantinedHeldButtons[0] = false;
        return Result;
    }
    if (Capture.bPointer && Ownership.PointerGestureOwner == EInputOwner::Viewport)
        Quarantine();
    for (const auto& Event : Events)
    {
        Ownership.EventSequence = std::max(Ownership.EventSequence, Event.Sequence);
        if (Event.EventType == EInputEventType::FocusLost)
        { ChangeFocus(false); Result.Actions = {}; continue; }
        if (Event.EventType == EInputEventType::FocusGained)
        { ChangeFocus(true); continue; }
        if (Event.EventType == EInputEventType::KeyUp && IsKnownKey(Event.Key))
        {
            const auto I = Index(Event.Key);
            HeldKeys[I] = false;
            Ownership.QuarantinedHeldKeys[I] = false;
            Ownership.PressOwnerByKey[I] = EInputOwner::None;
        }
        if (Event.EventType == EInputEventType::KeyDown && IsKnownKey(Event.Key))
        {
            const auto I = Index(Event.Key);
            if (Event.bRepeat || HeldKeys[I]) continue;
            HeldKeys[I] = true;
            Ownership.PressOwnerByKey[I] = KeyboardUI ? EInputOwner::UI : EInputOwner::Viewport;
            if (KeyboardUI || !Ownership.bFocused) Ownership.QuarantinedHeldKeys[I] = true;
            if (!Ownership.bFocused) continue;
            if (Event.Key == EKey::Escape)
            { Result.bCancelInteraction = true; Quarantine(); Result.Actions = {}; }
            if (Event.Key == EKey::F1 && !Capture.bTextEditing)
            { Result.bToggleUI = true; Quarantine(); Result.Actions = {}; }
        }
        if (Event.EventType == EInputEventType::MouseButtonUp && IsKnownMouseButton(Event.MouseButton) &&
            Index(Event.MouseButton) < HeldButtons.size())
        {
            const auto I = Index(Event.MouseButton);
            HeldButtons[I] = false;
            Ownership.QuarantinedHeldButtons[I] = false;
            Ownership.PressOwnerByButton[I] = EInputOwner::None;
            bool AnyHeld = false;
            for (bool Held : HeldButtons) AnyHeld |= Held;
            if (!AnyHeld) Ownership.PointerGestureOwner = EInputOwner::None;
            if (Event.MouseButton == EMouseButton::Right)
            { Ownership.CursorMode = ECursorMode::Normal; Ownership.bPointerBaselineValid = false; }
        }
        if (Event.EventType == EInputEventType::MouseButtonDown && IsKnownMouseButton(Event.MouseButton) &&
            Index(Event.MouseButton) < HeldButtons.size())
        {
            const auto I = Index(Event.MouseButton);
            if (HeldButtons[I]) continue;
            HeldButtons[I] = true;
            if (!Ownership.bFocused) Ownership.QuarantinedHeldButtons[I] = true;
            if (Ownership.PointerGestureOwner == EInputOwner::None)
                Ownership.PointerGestureOwner = Capture.bPointer ? EInputOwner::UI : EInputOwner::Viewport;
            Ownership.PressOwnerByButton[I] = Ownership.PointerGestureOwner;
            if (Event.MouseButton == EMouseButton::Right && Ownership.bFocused &&
                !Ownership.QuarantinedHeldButtons[I] && Ownership.PointerGestureOwner == EInputOwner::Viewport)
            { Ownership.CursorMode = ECursorMode::Disabled; Ownership.bPointerBaselineValid = false; }
        }
        if (Event.EventType == EInputEventType::CursorEntered)
            Ownership.bPointerBaselineValid = false;
        if (Event.EventType == EInputEventType::PointerMove && Ownership.bFocused &&
            std::isfinite(Event.PointerX) && std::isfinite(Event.PointerY))
        {
            if (Ownership.bPointerBaselineValid && Ownership.CursorMode == ECursorMode::Disabled)
            {
                Result.Actions.LookDeltaX += Event.PointerX - PointerX;
                Result.Actions.LookDeltaY += Event.PointerY - PointerY;
            }
            PointerX = Event.PointerX;
            PointerY = Event.PointerY;
            Ownership.bPointerBaselineValid = true;
        }
        Ownership.ScrollOwner = Capture.bScroll || Ownership.PointerGestureOwner == EInputOwner::UI ?
            EInputOwner::UI : EInputOwner::Viewport;
        if (Event.EventType == EInputEventType::Scroll && Ownership.bFocused &&
            Ownership.ScrollOwner == EInputOwner::Viewport && std::isfinite(Event.DeltaY))
            Result.Actions.ScrollDeltaY += Event.DeltaY;
    }
    auto Down = [&](EKey Key) {
        return Ownership.bFocused && !KeyboardUI && HeldKeys[Index(Key)] &&
            !Ownership.QuarantinedHeldKeys[Index(Key)] &&
            Ownership.PressOwnerByKey[Index(Key)] == EInputOwner::Viewport;
    };
    Result.Actions.ForwardAxis = static_cast<float>(Down(EKey::W)) - static_cast<float>(Down(EKey::S));
    Result.Actions.RightAxis = static_cast<float>(Down(EKey::D)) - static_cast<float>(Down(EKey::A));
    Result.Actions.UpAxis = static_cast<float>(Down(EKey::E)) - static_cast<float>(Down(EKey::Q));
    Result.Actions.bFast = Down(EKey::LeftShift) || Down(EKey::RightShift);
    Result.Actions.bLookCaptured = Ownership.bFocused && Ownership.CursorMode == ECursorMode::Disabled;
    if (!Result.Actions.bLookCaptured) Result.Actions.LookDeltaX = Result.Actions.LookDeltaY = 0;
    if (!Result.Actions.IsValid()) { Result.Actions = {}; Quarantine(); }
    return Result;
}
}
