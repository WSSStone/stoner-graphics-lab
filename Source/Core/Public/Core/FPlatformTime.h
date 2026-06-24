#pragma once

#include <chrono>

namespace Stoner::Core
{

struct FPlatformTime
{
    using FTimestamp = std::chrono::steady_clock::time_point;
    using FDuration = std::chrono::steady_clock::duration;

    [[nodiscard]] static FTimestamp Now() noexcept;
    [[nodiscard]] static double ToSeconds(FDuration Duration) noexcept;
    [[nodiscard]] static double ToMilliseconds(FDuration Duration) noexcept;
    [[nodiscard]] static double ToMicroseconds(FDuration Duration) noexcept;
};

} // namespace Stoner::Core
