#pragma once

#include "Application/EKey.h"
#include "Application/EMouseButton.h"
#include "Application/FWindowDisplayState.h"
#include "Core/CoreMinimal.h"

#include <array>

namespace Stoner::Application
{

enum class EInputOwner : Stoner::Core::uint8
{
    None,
    UI,
    Viewport
};

inline constexpr std::size_t GInputKeyCount =
    static_cast<std::size_t>(EKey::F12) + 1;
inline constexpr std::size_t GInputMouseButtonCount =
    static_cast<std::size_t>(EMouseButton::X2) + 1;

struct FInputOwnershipSnapshot
{
    Stoner::Core::uint64 EventSequence = 0;
    Stoner::Core::uint64 FocusGeneration = 0;
    EInputOwner KeyboardOwner = EInputOwner::None;
    EInputOwner PointerGestureOwner = EInputOwner::None;
    EInputOwner ScrollOwner = EInputOwner::None;
    std::array<EInputOwner, GInputKeyCount> PressOwnerByKey{};
    std::array<EInputOwner, GInputMouseButtonCount> PressOwnerByButton{};
    std::array<bool, GInputKeyCount> QuarantinedHeldKeys{};
    std::array<bool, GInputMouseButtonCount> QuarantinedHeldButtons{};
    ECursorMode CursorMode = ECursorMode::Normal;
    bool bPointerBaselineValid = false;
    bool bFocused = false;

    [[nodiscard]] bool IsValid() const noexcept;
};

} // namespace Stoner::Application
