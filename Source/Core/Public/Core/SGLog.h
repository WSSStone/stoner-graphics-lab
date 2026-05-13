// SGLog.h — SG_LOG macro with macro-level early-out severity filtering.
#pragma once

#include "Core/ELogSeverity.h"
#include "Core/FLog.h"
#include "Core/FLogCategory.h"

// SG_LOG(Category, Severity, Format, ...)
//
// Structured logging macro with macro-level early-out filtering.
// When the message severity is below the category's minimum threshold,
// the format string and arguments are never evaluated — cost is a single
// integer comparison.
//
// Usage:
//   SG_LOG(LogCore, Info, "Engine initialized version %d.%d", 1, 0);
//
#define SG_LOG(Category, Severity, Format, ...) \
    do { \
        if (static_cast<int>(::Stoner::Core::ELogSeverity::Severity) >= \
            static_cast<int>(Category.GetMinSeverity())) \
        { \
            ::Stoner::Core::FLog::LogMessage( \
                Category, ::Stoner::Core::ELogSeverity::Severity, \
                __FILE__, __LINE__, Format, ##__VA_ARGS__); \
        } \
    } while (0)
