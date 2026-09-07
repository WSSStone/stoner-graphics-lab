#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::Application
{

enum class ECursorMode : Stoner::Core::uint8
{
    Normal,
    Disabled
};

// A client or drawable size. A zero pair is the explicit paused display
// state; a single zero axis is never a usable extent.
struct FWindowExtent
{
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;

    [[nodiscard]] constexpr bool IsZero() const noexcept
    {
        return Width == 0 && Height == 0;
    }

    [[nodiscard]] constexpr bool IsPositive() const noexcept
    {
        return Width > 0 && Height > 0;
    }

    [[nodiscard]] friend constexpr bool operator==(
        const FWindowExtent& Left,
        const FWindowExtent& Right) noexcept = default;
};

struct FWindowDisplayState
{
    FWindowExtent LogicalExtent;
    FWindowExtent DrawableExtent;
    Stoner::Core::FVector2 ContentScale = Stoner::Core::FVector2(1.0f, 1.0f);
    Stoner::Core::FVector2 FramebufferScale =
        Stoner::Core::FVector2(1.0f, 1.0f);
    Stoner::Core::uint64 DisplayGeneration = 0;
    bool bFocused = false;
    bool bMinimized = false;

    [[nodiscard]] bool IsValid() const noexcept;
};

} // namespace Stoner::Application
