#pragma once
#include "Application/FFreeCameraController.h"
#include "Application/FInputEvent.h"
#include "Application/FInputOwnershipSnapshot.h"

namespace Stoner::Application
{
// Current-frame capture/activation, evaluated after building UI. Hover alone
// must not assert ownership here. Raw events always reach UI before Resolve.
struct FUILabCapture
{
    bool bKeyboard = false;
    bool bPointer = false;
    bool bScroll = false;
    bool bTextEditing = false;
    bool bVisibilityChanged = false;
    bool bHideUIRequested = false;
};
struct FLabRoutedInput
{
    FFreeCameraActions Actions;
    bool bToggleUI = false;
    bool bCancelInteraction = false;
};
class FLabInputRouter
{
public:
    [[nodiscard]] FLabRoutedInput Resolve(
        const Stoner::Core::TArray<FInputEvent>& Events,
        const FUILabCapture& Capture, bool bFocused, bool bOverflow = false);
    void CancelInteraction() noexcept { Quarantine(); }
    void InvalidatePointerBaseline() noexcept { Ownership.bPointerBaselineValid = false; }
    [[nodiscard]] const FInputOwnershipSnapshot& GetOwnership() const noexcept { return Ownership; }
private:
    void Quarantine() noexcept;
    FInputOwnershipSnapshot Ownership;
    std::array<bool, GInputKeyCount> HeldKeys{};
    std::array<bool, GInputMouseButtonCount> HeldButtons{};
    float PointerX = 0;
    float PointerY = 0;
    bool bInitialized = false;
};
}
