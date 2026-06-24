#include "Core/FPlatformTime.h"

namespace Stoner::Core
{

FPlatformTime::FTimestamp FPlatformTime::Now() noexcept
{
    return std::chrono::steady_clock::now();
}

double FPlatformTime::ToSeconds(FDuration Duration) noexcept
{
    return std::chrono::duration<double>(Duration).count();
}

double FPlatformTime::ToMilliseconds(FDuration Duration) noexcept
{
    return std::chrono::duration<double, std::milli>(Duration).count();
}

double FPlatformTime::ToMicroseconds(FDuration Duration) noexcept
{
    return std::chrono::duration<double, std::micro>(Duration).count();
}

} // namespace Stoner::Core
