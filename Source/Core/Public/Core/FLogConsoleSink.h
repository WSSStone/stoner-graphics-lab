// FLogConsoleSink.h — Console output sink for the logging system.
#pragma once

#include "Core/ELogSeverity.h"

namespace Stoner::Core
{

// Console output sink that formats and writes log messages.
// Routes Verbose/Info to stdout and Warning/Error/Fatal to stderr.
struct FLogConsoleSink
{
    // Write a pre-formatted message to the appropriate console stream.
    // Severity determines stdout vs stderr routing.
    static void Write(ELogSeverity Severity, const char* FormattedMessage);
};

} // namespace Stoner::Core
