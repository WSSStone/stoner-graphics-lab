# Quickstart: Core Foundation — Logging & Assertions

**Feature**: 005-core-logging-assertions
**Date**: 2026-05-13

## What This Feature Produces

This feature adds the engine's diagnostic infrastructure to the Core layer. It provides:

- `ELogSeverity` — five-level severity enumeration (Verbose, Info, Warning, Error, Fatal)
- `FLogCategory` — named log categories with per-category severity filtering
- `FLog` — central log coordinator with thread-safe message dispatch
- `FLogConsoleSink` — console output sink (stdout/stderr)
- `SG_LOG(Category, Severity, Format, ...)` — structured logging macro with early-out filtering
- `SG_CHECK(Expr)` / `SG_VERIFY(Expr)` / `SG_CHECKF(Expr, Format, ...)` — assertion macros
- `SG_DEBUG_BREAK()` — platform-abstracted debugger break
- 5 pre-defined log categories: `LogCore`, `LogRHI`, `LogRenderer`, `LogBackend`, `LogApplication`
- Logging and assertion verification in the existing `StonerTest` executable

## Prerequisites

- Phase 002 (Core Types & Memory) and Phase 003 (Core Math) are complete.
- Current branch is `005-core-logging-assertions`.
- A supported C++20 compiler is available.
- SCons 4.10.1 or newer is available.

## Expected Public Include Usage

After implementation, any engine source file can use logging:

```cpp
#include "Core/SGLog.h"       // SG_LOG macro + categories
#include "Core/SGAssert.h"    // SG_CHECK / SG_VERIFY / SG_CHECKF macros
```

Or via the aggregate header:

```cpp
#include "Core/CoreMinimal.h" // Includes everything including logging & assertions
```

### Basic Logging Usage

```cpp
// Log at various severity levels
SG_LOG(LogCore, Info, "Engine initialized version %d.%d", 1, 0);
SG_LOG(LogRHI, Warning, "Buffer size %zu exceeds recommended limit", bufferSize);
SG_LOG(LogRenderer, Error, "Failed to compile shader: %s", shaderName);
SG_LOG(LogCore, Fatal, "Unrecoverable error in subsystem %s", subsystem);
// ^ Fatal: logs, breaks (Debug), then aborts
```

### Basic Assertion Usage

```cpp
// Simple assertion (Debug only, zero cost in Release)
SG_CHECK(ptr != nullptr);

// Assertion with formatted message (Debug only)
SG_CHECKF(index < count, "Index %d out of range [0, %d)", index, count);

// Verify: expression always evaluates, check only in Debug
SG_VERIFY(Initialize());
```

### Category Filtering

```cpp
// Set per-category filter to suppress verbose messages
LogCore.SetMinSeverity(ELogSeverity::Warning);

// This message is now suppressed (Info < Warning) — zero overhead
SG_LOG(LogCore, Info, "This will not appear");

// This message still appears (Error >= Warning)
SG_LOG(LogCore, Error, "This will appear");
```

### Custom Category Declaration

```cpp
// In MySubsystem.h:
SG_DECLARE_LOG_CATEGORY_EXTERN(LogMeshlet, ELogSeverity::Verbose)

// In MySubsystem.cpp:
SG_DEFINE_LOG_CATEGORY(LogMeshlet)

// Usage:
SG_LOG(LogMeshlet, Info, "Processing cluster %d", clusterId);
```

## Build

From the repository root:

```bash
scons
```

Expected result:

- Core static library builds successfully with new logging/assertion source files.
- Existing downstream skeleton layers still build.
- `StonerTest` builds successfully with new test suite.

## Run Verification

Run the generated test executable from the build output directory:

```bash
# macOS Debug
Build/Mac/Debug/Tests/StonerTest

# Windows Debug
Build\Win64\Debug\Tests\StonerTest.exe

# Linux Debug
Build/Linux/Debug/Tests/StonerTest
```

Expected result:

- Process exits with code `0`.
- Logging and assertion verification reports no failures.
- Console output shows formatted log messages during test execution.

## Manual Validation Checklist

1. Verify `Core/CoreMinimal.h` includes the new logging and assertion headers.
2. Verify `SG_LOG` produces formatted output with timestamp, category, severity, and message.
3. Verify all five severity levels produce distinct labels.
4. Verify `SG_LOG` with `Fatal` severity terminates the process (test in isolation).
5. Verify `SG_CHECK(false)` triggers debugger break in Debug build (test in debugger).
6. Verify `SG_CHECK(false)` is completely absent in Release build (check disassembly or observe no effect).
7. Verify `SG_VERIFY(SideEffectFunction())` always calls the function in both Debug and Release.
8. Verify per-category severity filtering suppresses messages below threshold.
9. Verify global severity filtering works when no per-category override is set.
10. Verify concurrent logging from multiple threads produces non-interleaved output.
11. Verify custom category declaration works across translation units.

## Cross-Platform Verification

Run the build and verification flow on:

- Windows with MSVC
- macOS with Apple Clang or Clang
- Linux with GCC or Clang

The feature is complete only when the public behavior is consistent across all supported platforms.

## Troubleshooting

- If includes fail, confirm new headers are under `Source/Core/Public/Core/` and included as `Core/<Header>.h`.
- If downstream layers fail, confirm `CoreMinimal.h` remains compatible with existing includes.
- If assertion macros don't compile out in Release, verify `NDEBUG` is defined in the Release build configuration.
- If debug break doesn't trigger, verify `_DEBUG` is defined and a debugger is attached.
- If log output is interleaved, verify the mutex is correctly acquired before sink writes.
- If tests don't link, confirm new `.cpp` files are in directories discovered by SConscript.
