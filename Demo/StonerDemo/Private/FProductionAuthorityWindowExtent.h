#pragma once

#include "Application/FWindow.h"

namespace Stoner::Demo
{

struct FProductionAuthorityClientExtent
{
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
};

[[nodiscard]] bool CalculateProductionAuthorityClientExtent(
    Stoner::Core::uint32 CurrentClientWidth,
    Stoner::Core::uint32 CurrentClientHeight,
    Stoner::Core::uint32 CurrentDrawableWidth,
    Stoner::Core::uint32 CurrentDrawableHeight,
    Stoner::Core::uint32 TargetDrawableWidth,
    Stoner::Core::uint32 TargetDrawableHeight,
    FProductionAuthorityClientExtent& OutExtent) noexcept;

[[nodiscard]] bool ConvergeProductionAuthorityDrawableExtent(
    Stoner::Application::FWindow& Window,
    Stoner::Core::uint32 TargetWidth,
    Stoner::Core::uint32 TargetHeight);

} // namespace Stoner::Demo
