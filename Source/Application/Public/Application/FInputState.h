#pragma once

#include "Application/FApplicationDiagnostics.h"
#include "Application/FInputEvent.h"

namespace Stoner::Application
{

class FInputState
{
public:
    void BeginFrame();
    void ClearAll();
    void SetFocused(bool bInFocused) noexcept;
    void ApplyEvent(const FInputEvent& Event, FApplicationDiagnosticLog* Diagnostics = nullptr);

    [[nodiscard]] bool IsKeyHeld(EKey Key) const;
    [[nodiscard]] bool WasKeyPressed(EKey Key) const;
    [[nodiscard]] bool WasKeyReleased(EKey Key) const;
    [[nodiscard]] bool IsMouseButtonHeld(EMouseButton Button) const;
    [[nodiscard]] bool WasMouseButtonPressed(EMouseButton Button) const;
    [[nodiscard]] bool WasMouseButtonReleased(EMouseButton Button) const;

    [[nodiscard]] float GetPointerX() const noexcept { return PointerX; }
    [[nodiscard]] float GetPointerY() const noexcept { return PointerY; }
    [[nodiscard]] float GetPointerDeltaX() const noexcept { return PointerDeltaX; }
    [[nodiscard]] float GetPointerDeltaY() const noexcept { return PointerDeltaY; }
    [[nodiscard]] bool HasPointerPosition() const noexcept { return bHasPointerPosition; }
    [[nodiscard]] bool IsFocused() const noexcept { return bFocused; }

    [[nodiscard]] const Stoner::Core::TArray<EKey>& GetHeldKeys() const noexcept { return HeldKeys; }
    [[nodiscard]] const Stoner::Core::TArray<EKey>& GetPressedKeys() const noexcept { return PressedKeys; }
    [[nodiscard]] const Stoner::Core::TArray<EKey>& GetReleasedKeys() const noexcept { return ReleasedKeys; }
    [[nodiscard]] const Stoner::Core::TArray<EMouseButton>& GetHeldMouseButtons() const noexcept { return HeldMouseButtons; }
    [[nodiscard]] const Stoner::Core::TArray<EMouseButton>& GetPressedMouseButtons() const noexcept { return PressedMouseButtons; }
    [[nodiscard]] const Stoner::Core::TArray<EMouseButton>& GetReleasedMouseButtons() const noexcept { return ReleasedMouseButtons; }

private:
    void ClearKeyAndMouseState();

    Stoner::Core::TArray<EKey> HeldKeys;
    Stoner::Core::TArray<EKey> PressedKeys;
    Stoner::Core::TArray<EKey> ReleasedKeys;
    Stoner::Core::TArray<EMouseButton> HeldMouseButtons;
    Stoner::Core::TArray<EMouseButton> PressedMouseButtons;
    Stoner::Core::TArray<EMouseButton> ReleasedMouseButtons;
    float PointerX = 0.0f;
    float PointerY = 0.0f;
    float PointerDeltaX = 0.0f;
    float PointerDeltaY = 0.0f;
    bool bHasPointerPosition = false;
    bool bFocused = true;
};

} // namespace Stoner::Application
