# Contract: Logging & Assertions Public API

**Feature**: 005-core-logging-assertions
**Date**: 2026-05-13

## Overview

This contract defines the public logging and assertion surface that all engine layers may depend on. The exact implementation may evolve, but the names and observable behavior below must remain stable for this feature.

## Public Header Contract

All headers live under `Source/Core/Public/Core/` and are included as `Core/<Header>.h`.

| Header | Public Deliverable | Required Behavior |
|--------|--------------------|-------------------|
| `ELogSeverity.h` | `ELogSeverity` | Five-level severity enum: Verbose, Info, Warning, Error, Fatal |
| `FLogCategory.h` | `FLogCategory`, declaration macros | Named category with severity filter, self-registration, cross-TU declaration |
| `FLog.h` | `FLog` | Central coordinator: message dispatch, global severity, thread-safe output |
| `FLogConsoleSink.h` | `FLogConsoleSink` | Console output: stdout for Verbose/Info, stderr for Warning/Error/Fatal |
| `SGLog.h` | `SG_LOG` macro | Structured logging with macro-level early-out severity filtering |
| `SGAssert.h` | `SG_CHECK`, `SG_VERIFY`, `SG_CHECKF` | Assertion macros with Debug/Release behavior |
| `SGPlatformBreak.h` | `SG_DEBUG_BREAK` | Platform-abstracted debugger break intrinsic |
| `CoreMinimal.h` | Core public aggregate | Updated to include all logging and assertion headers |

## Namespace Contract

Public deliverables must be available through the Core namespace:

```cpp
namespace Stoner::Core
{
    enum class ELogSeverity : uint8;
    struct FLogCategory;
    struct FLog;
    struct FLogConsoleSink;
}
```

Macros (`SG_LOG`, `SG_CHECK`, `SG_VERIFY`, `SG_CHECKF`, `SG_DEBUG_BREAK`) are in the global macro namespace as is standard for C++ macros.

## Behavioral Contract

### `ELogSeverity`

- Five ordered values: `Verbose < Info < Warning < Error < Fatal`.
- Underlying type supports integer comparison for filtering.
- Usable in both macro contexts and function signatures.

### `FLogCategory`

- Construction requires a name string and default minimum severity.
- Self-registers into the global category registry on construction.
- `GetMinSeverity()` returns the current minimum severity threshold.
- `SetMinSeverity(ELogSeverity)` updates the per-category filter at runtime.
- `GetName()` returns the category name string.
- Multiple categories across different translation units coexist without conflict.

### Category Declaration Macros

- `SG_DECLARE_LOG_CATEGORY_EXTERN(CategoryName, DefaultSeverity)` — declares an extern category variable in a header.
- `SG_DEFINE_LOG_CATEGORY(CategoryName)` — defines the category variable in a source file.
- Pre-defined categories: `LogCore`, `LogRHI`, `LogRenderer`, `LogBackend`, `LogApplication`.

### `FLog`

- `LogMessage(Category, Severity, File, Line, Format, ...)` — formats and dispatches a log message.
- `SetGlobalMinSeverity(ELogSeverity)` — sets the global severity threshold.
- `GetGlobalMinSeverity()` — returns the current global threshold.
- Default global severity is `Verbose` (show everything).
- Thread-safe: concurrent calls produce non-interleaved output lines.
- Fatal messages: log → `SG_DEBUG_BREAK()` (Debug only) → `std::abort()` (always).
- Functions with default configuration before any explicit initialization.

### `FLogConsoleSink`

- Writes formatted messages to `stdout` (Verbose, Info) or `stderr` (Warning, Error, Fatal).
- Output format: `[HH:MM:SS.mmm] CategoryName: SeverityLabel: Message\n`
- Messages exceeding internal buffer are truncated with `...`.
- Each write produces exactly one complete line.

### `SG_LOG(Category, Severity, Format, ...)`

- Macro-level early-out: checks `Category.GetMinSeverity()` before any formatting.
- Filtered messages cost at most one integer comparison.
- Passes `__FILE__` and `__LINE__` to the log coordinator.
- Printf-style formatting via `vsnprintf`.

### `SG_CHECK(Expr)`

- **Debug build**: Evaluates `Expr`. On failure: logs file, line, expression text → triggers assertion handler (default: `SG_DEBUG_BREAK()`).
- **Release build**: Entire macro is compiled out. `Expr` is not evaluated. Zero cost.

### `SG_VERIFY(Expr)`

- **All builds**: Always evaluates `Expr`.
- **Debug build**: If `Expr` is falsy: logs file, line, expression text → triggers assertion handler.
- **Release build**: `Expr` is evaluated but result is discarded. No check, no failure handling.

### `SG_CHECKF(Expr, Format, ...)`

- Same as `SG_CHECK` but includes a printf-formatted failure message in the assertion report.
- **Debug build**: On failure: logs file, line, expression text, formatted message → triggers assertion handler.
- **Release build**: Entire macro is compiled out. Zero cost.

### `SG_DEBUG_BREAK()`

- **Debug build** (`_DEBUG` defined):
  - MSVC: `__debugbreak()`
  - GCC/Clang: `__builtin_debugtrap()`
  - Fallback: `std::abort()`
- **Release build** (`NDEBUG` defined): Expands to nothing.
- Resumable on supported platforms (developer can continue in debugger).

## Verification Contract

The logging and assertion verification suite must cover:

- All five severity levels produce correctly labeled output.
- Category-based filtering suppresses messages below threshold.
- Global severity filtering works when no per-category override is set.
- `SG_LOG` early-out does not evaluate format arguments for filtered messages.
- Fatal log writes the labeled message to `stderr` and terminates the process
  (tested in an isolated child process so the test runner survives).
- `SG_CHECK` reports file, line, and expression on failure.
- `SG_CHECKF` includes formatted message in failure report.
- `SG_VERIFY` always evaluates its expression regardless of build configuration.
- Concurrent logging from multiple threads produces non-interleaved output.
- Runtime category and global threshold reads/writes are free of data races.
- Custom category declaration and usage across translation units.
- Console output format matches the specified pattern.
- Logging works without explicit initialization (zero-configuration startup).

## Exclusions

- No file-based log sinks.
- No remote/network logging.
- No profiling or performance timing.
- No color/ANSI terminal output.
- No async/queued logging.
- No RHI, Backend, Renderer, or Application dependencies.
