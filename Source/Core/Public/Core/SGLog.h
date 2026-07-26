// SGLog.h — SG_LOG macro with macro-level early-out severity filtering.
#pragma once

#include "Core/ELogSeverity.h"
#include "Core/FLog.h"
#include "Core/FLogCategory.h"

namespace Stoner::Core::Detail
{

[[nodiscard]] constexpr uint8 LogSeverityBit(ELogSeverity Severity) noexcept
{
    return static_cast<uint8>(1u << static_cast<uint8>(Severity));
}

[[nodiscard]] constexpr uint8 LogSeverityThresholdMask(ELogSeverity Severity) noexcept
{
    return static_cast<uint8>(0x1Fu << static_cast<uint8>(Severity));
}

} // namespace Stoner::Core::Detail

// SG_LOG(Category, Severity, Format, ...)
//
// Structured logging macro with macro-level early-out filtering.
// When the message severity is below the category or global minimum threshold,
// the format string and arguments are never evaluated. The two threshold masks
// are combined before a single integer comparison.
//
// Usage:
//   SG_LOG(LogCore, Info, "Engine initialized version %d.%d", 1, 0);
//
#define SG_LOG(Category, Severity, Format, ...) \
    do { \
        constexpr auto SGLogSeverity = ::Stoner::Core::ELogSeverity::Severity; \
        const auto SGLogEnabledMask = static_cast<::Stoner::Core::uint8>( \
            ::Stoner::Core::Detail::LogSeverityThresholdMask(Category.GetMinSeverity()) & \
            ::Stoner::Core::Detail::LogSeverityThresholdMask( \
                ::Stoner::Core::FLog::GetGlobalMinSeverity())); \
        if ((::Stoner::Core::Detail::LogSeverityBit(SGLogSeverity) & \
             SGLogEnabledMask) != 0u) \
        { \
            ::Stoner::Core::FLog::LogMessage( \
                Category, SGLogSeverity, __FILE__, __LINE__, \
                Format __VA_OPT__(,) __VA_ARGS__); \
        } \
    } while (0)
