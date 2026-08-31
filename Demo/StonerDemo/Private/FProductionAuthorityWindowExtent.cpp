#include "FProductionAuthorityWindowExtent.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

namespace Stoner::Demo
{
namespace
{

Stoner::Core::uint32 FitClientDimension(
    Stoner::Core::uint32 CurrentClient,
    Stoner::Core::uint32 CurrentDrawable,
    Stoner::Core::uint32 TargetDrawable) noexcept
{
    const double Scaled = static_cast<double>(CurrentClient) *
        static_cast<double>(TargetDrawable) /
        static_cast<double>(CurrentDrawable);
    auto Proposed = static_cast<Stoner::Core::uint32>(std::clamp(
        std::llround(Scaled), 1LL,
        static_cast<long long>(
            Stoner::Application::FWindowDesc::MaxClientWidth)));
    if (Proposed == CurrentClient && CurrentDrawable != TargetDrawable)
    {
        if (CurrentDrawable < TargetDrawable &&
            Proposed < Stoner::Application::FWindowDesc::MaxClientWidth)
            ++Proposed;
        else if (CurrentDrawable > TargetDrawable && Proposed > 1u)
            --Proposed;
    }
    return Proposed;
}

} // namespace

bool CalculateProductionAuthorityClientExtent(
    Stoner::Core::uint32 CurrentClientWidth,
    Stoner::Core::uint32 CurrentClientHeight,
    Stoner::Core::uint32 CurrentDrawableWidth,
    Stoner::Core::uint32 CurrentDrawableHeight,
    Stoner::Core::uint32 TargetDrawableWidth,
    Stoner::Core::uint32 TargetDrawableHeight,
    FProductionAuthorityClientExtent& OutExtent) noexcept
{
    OutExtent = {};
    if (CurrentClientWidth == 0 || CurrentClientHeight == 0 ||
        CurrentDrawableWidth == 0 || CurrentDrawableHeight == 0 ||
        TargetDrawableWidth == 0 || TargetDrawableHeight == 0)
        return false;
    OutExtent.Width = FitClientDimension(
        CurrentClientWidth, CurrentDrawableWidth, TargetDrawableWidth);
    OutExtent.Height = FitClientDimension(
        CurrentClientHeight, CurrentDrawableHeight, TargetDrawableHeight);
    return OutExtent.Width > 0 && OutExtent.Height > 0;
}

bool ConvergeProductionAuthorityDrawableExtent(
    Stoner::Application::FWindow& Window,
    Stoner::Core::uint32 TargetWidth,
    Stoner::Core::uint32 TargetHeight)
{
    constexpr Stoner::Core::uint32 MaximumAttempts = 8;
    constexpr auto AttemptTimeout = std::chrono::milliseconds(250);
    for (Stoner::Core::uint32 Attempt = 0;
         Attempt < MaximumAttempts; ++Attempt)
    {
        (void)Window.PollEvents();
        if (Window.GetDrawableWidth() == TargetWidth &&
            Window.GetDrawableHeight() == TargetHeight)
            return true;
        FProductionAuthorityClientExtent ClientExtent;
        if (!CalculateProductionAuthorityClientExtent(
                Window.GetClientWidth(), Window.GetClientHeight(),
                Window.GetDrawableWidth(), Window.GetDrawableHeight(),
                TargetWidth, TargetHeight, ClientExtent) ||
            Window.SetClientSize(ClientExtent.Width, ClientExtent.Height) !=
                Stoner::Application::EApplicationResult::Success)
            return false;
        const auto Deadline =
            std::chrono::steady_clock::now() + AttemptTimeout;
        do
        {
            (void)Window.PollEvents();
            if (Window.GetDrawableWidth() == TargetWidth &&
                Window.GetDrawableHeight() == TargetHeight)
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        while (std::chrono::steady_clock::now() < Deadline);
    }
    return false;
}

} // namespace Stoner::Demo
