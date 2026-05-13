// ELogSeverity.h — Five-level severity enumeration for the logging system.
#pragma once

#include "Core/FPlatformTypes.h"

namespace Stoner::Core
{

// Ordered severity levels for log messages.
// Verbose < Info < Warning < Error < Fatal.
enum class ELogSeverity : uint8
{
    Verbose = 0,
    Info    = 1,
    Warning = 2,
    Error   = 3,
    Fatal   = 4
};

// Convert a severity value to its human-readable label.
inline const char* SeverityToString(ELogSeverity Severity)
{
    switch (Severity)
    {
        case ELogSeverity::Verbose: return "Verbose";
        case ELogSeverity::Info:    return "Info";
        case ELogSeverity::Warning: return "Warning";
        case ELogSeverity::Error:   return "Error";
        case ELogSeverity::Fatal:   return "Fatal";
        default:                    return "Unknown";
    }
}

} // namespace Stoner::Core
