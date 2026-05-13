// FLogConsoleSink.cpp — Console output sink implementation.
#include "Core/FLogConsoleSink.h"

#include <cstdio>

namespace Stoner::Core
{

void FLogConsoleSink::Write(ELogSeverity Severity, const char* FormattedMessage)
{
    // Route Verbose/Info to stdout, Warning/Error/Fatal to stderr.
    FILE* Stream = (Severity <= ELogSeverity::Info) ? stdout : stderr;
    std::fputs(FormattedMessage, Stream);
    std::fflush(Stream);
}

} // namespace Stoner::Core
