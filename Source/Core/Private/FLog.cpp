// FLog.cpp — Central log coordinator implementation.
#include "Core/FLog.h"
#include "Core/FLogCategory.h"
#include "Core/FLogConsoleSink.h"
#include "Core/SGPlatformBreak.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace Stoner::Core
{

// Internal state — file-scoped statics for zero-configuration startup.
static ELogSeverity GGlobalMinSeverity = ELogSeverity::Verbose;
static FAssertionHandler GAssertionHandler = nullptr;
static std::mutex GLogMutex;

// Default assertion handler: triggers platform debug break.
static void DefaultAssertionHandler(const char* /*File*/, int /*Line*/,
                                    const char* /*Expression*/, const char* /*Message*/)
{
    SG_DEBUG_BREAK();
}

void FLog::LogMessage(FLogCategory& Category, ELogSeverity Severity,
                      const char* File, int Line,
                      const char* Format, ...)
{
    // Global severity filter (in addition to macro-level per-category check).
    if (static_cast<int>(Severity) < static_cast<int>(GGlobalMinSeverity))
    {
        return;
    }

    // Format the user message on the caller's stack.
    char UserBuffer[1024];
    va_list Args;
    va_start(Args, Format);
    const int Written = std::vsnprintf(UserBuffer, sizeof(UserBuffer), Format, Args);
    va_end(Args);

    // Truncation indicator.
    if (Written >= static_cast<int>(sizeof(UserBuffer)))
    {
        UserBuffer[sizeof(UserBuffer) - 4] = '.';
        UserBuffer[sizeof(UserBuffer) - 3] = '.';
        UserBuffer[sizeof(UserBuffer) - 2] = '.';
        UserBuffer[sizeof(UserBuffer) - 1] = '\0';
    }

    // Format timestamp [HH:MM:SS.mmm].
    const auto Now = std::chrono::system_clock::now();
    const auto TimeT = std::chrono::system_clock::to_time_t(Now);
    const auto Ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        Now.time_since_epoch()) % 1000;

    std::tm TimeParts{};
#if defined(_MSC_VER)
    localtime_s(&TimeParts, &TimeT);
#else
    localtime_r(&TimeT, &TimeParts);
#endif

    // Compose the final formatted log line.
    char FinalBuffer[1280];
    std::snprintf(FinalBuffer, sizeof(FinalBuffer),
                  "[%02d:%02d:%02d.%03d] %s: %s: %s\n",
                  TimeParts.tm_hour, TimeParts.tm_min, TimeParts.tm_sec,
                  static_cast<int>(Ms.count()),
                  Category.GetName(),
                  SeverityToString(Severity),
                  UserBuffer);

    // Lock only for the sink write (minimize contention).
    {
        std::lock_guard<std::mutex> Lock(GLogMutex);
        FLogConsoleSink::Write(Severity, FinalBuffer);
    }

    // Fatal severity: break (Debug) then abort (always).
    if (Severity == ELogSeverity::Fatal)
    {
        SG_DEBUG_BREAK();
        std::abort();
    }

    (void)File;
    (void)Line;
}

void FLog::SetGlobalMinSeverity(ELogSeverity Severity)
{
    GGlobalMinSeverity = Severity;
}

ELogSeverity FLog::GetGlobalMinSeverity()
{
    return GGlobalMinSeverity;
}

void FLog::SetAssertionHandler(FAssertionHandler Handler)
{
    GAssertionHandler = Handler;
}

void FLog::HandleAssertionFailure(const char* File, int Line,
                                  const char* Expression,
                                  const char* Format, ...)
{
    // Format the optional user message.
    char MessageBuffer[1024];
    MessageBuffer[0] = '\0';

    if (Format != nullptr)
    {
        va_list Args;
        va_start(Args, Format);
        std::vsnprintf(MessageBuffer, sizeof(MessageBuffer), Format, Args);
        va_end(Args);
    }

    // Log the assertion failure through the logging system.
    if (MessageBuffer[0] != '\0')
    {
        // Log with user message.
        char AssertBuffer[2048];
        std::snprintf(AssertBuffer, sizeof(AssertBuffer),
                      "Assertion failed: %s [%s] at %s:%d",
                      Expression, MessageBuffer, File, Line);

        // Format timestamp for direct sink write.
        const auto Now = std::chrono::system_clock::now();
        const auto TimeT = std::chrono::system_clock::to_time_t(Now);
        const auto Ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            Now.time_since_epoch()) % 1000;

        std::tm TimeParts{};
#if defined(_MSC_VER)
        localtime_s(&TimeParts, &TimeT);
#else
        localtime_r(&TimeT, &TimeParts);
#endif

        char FinalBuffer[2304];
        std::snprintf(FinalBuffer, sizeof(FinalBuffer),
                      "[%02d:%02d:%02d.%03d] Assertion: Fatal: %s\n",
                      TimeParts.tm_hour, TimeParts.tm_min, TimeParts.tm_sec,
                      static_cast<int>(Ms.count()),
                      AssertBuffer);

        std::lock_guard<std::mutex> Lock(GLogMutex);
        FLogConsoleSink::Write(ELogSeverity::Fatal, FinalBuffer);
    }
    else
    {
        // Log without user message.
        char AssertBuffer[2048];
        std::snprintf(AssertBuffer, sizeof(AssertBuffer),
                      "Assertion failed: %s at %s:%d",
                      Expression, File, Line);

        const auto Now = std::chrono::system_clock::now();
        const auto TimeT = std::chrono::system_clock::to_time_t(Now);
        const auto Ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            Now.time_since_epoch()) % 1000;

        std::tm TimeParts{};
#if defined(_MSC_VER)
        localtime_s(&TimeParts, &TimeT);
#else
        localtime_r(&TimeT, &TimeParts);
#endif

        char FinalBuffer[2304];
        std::snprintf(FinalBuffer, sizeof(FinalBuffer),
                      "[%02d:%02d:%02d.%03d] Assertion: Fatal: %s\n",
                      TimeParts.tm_hour, TimeParts.tm_min, TimeParts.tm_sec,
                      static_cast<int>(Ms.count()),
                      AssertBuffer);

        std::lock_guard<std::mutex> Lock(GLogMutex);
        FLogConsoleSink::Write(ELogSeverity::Fatal, FinalBuffer);
    }

    // Invoke the assertion handler (custom or default).
    FAssertionHandler Handler = GAssertionHandler;
    if (Handler != nullptr)
    {
        Handler(File, Line, Expression, MessageBuffer[0] != '\0' ? MessageBuffer : nullptr);
    }
    else
    {
        DefaultAssertionHandler(File, Line, Expression, MessageBuffer);
    }
}

} // namespace Stoner::Core
