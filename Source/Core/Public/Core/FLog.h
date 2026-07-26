// FLog.h — Central log coordinator with thread-safe message dispatch.
#pragma once

#include "Core/ELogSeverity.h"

#include <atomic>
#include <cstdarg>

namespace Stoner::Core
{

struct FLogCategory;

// Assertion handler function pointer type.
// Parameters: File, Line, Expression, FormattedMessage (may be nullptr).
using FAssertionHandler = void(*)(const char* File, int Line, const char* Expression, const char* Message);

// Central logging coordinator.
// Receives log messages, applies filtering, formats output, and dispatches to the console sink.
// Thread-safe: concurrent LogMessage calls produce non-interleaved output lines.
struct FLog
{
    // Log a message through the logging system.
    // Called by the SG_LOG macro — do not call directly.
    static void LogMessage(FLogCategory& Category, ELogSeverity Severity,
                           const char* File, int Line,
                           const char* Format, ...);

    // Set the global minimum severity threshold.
    // Messages below this threshold are suppressed even if the category allows them.
    static void SetGlobalMinSeverity(ELogSeverity Severity) noexcept
    {
        GlobalMinSeverity.store(Severity, std::memory_order_relaxed);
    }

    // Get the current global minimum severity threshold.
    [[nodiscard]] static ELogSeverity GetGlobalMinSeverity() noexcept
    {
        return GlobalMinSeverity.load(std::memory_order_relaxed);
    }

    // Set a custom assertion handler (for testing).
    // Pass nullptr to restore the default handler (SG_DEBUG_BREAK).
    static void SetAssertionHandler(FAssertionHandler Handler);

    // Handle an assertion failure. Called by SG_CHECK/SG_VERIFY/SG_CHECKF macros.
    // Logs the failure details and invokes the assertion handler.
    static void HandleAssertionFailure(const char* File, int Line,
                                       const char* Expression,
                                       const char* Format = nullptr, ...);

private:
    inline static std::atomic<ELogSeverity> GlobalMinSeverity{ELogSeverity::Verbose};
};

} // namespace Stoner::Core
