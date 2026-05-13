# Research: Core Foundation — Logging & Assertions

**Feature**: 005-core-logging-assertions
**Date**: 2026-05-13
**Status**: Complete

## Research Tasks

This feature introduces the Core layer's diagnostic infrastructure: structured logging with category-based filtering and assertion macros with platform debug break support. The research phase resolves all implementation choices needed before designing tasks.

---

## Decision 1: Log Message Formatting Strategy

**Decision**: Use `vsnprintf` with a fixed 1024-byte stack buffer for message formatting. If the message exceeds the buffer, it is truncated with a trailing `...` indicator.

**Rationale**: Printf-style formatting is specified in the requirements. A stack buffer avoids heap allocation in the common case (most log messages are well under 1KB). Truncation is acceptable for diagnostic messages — extremely long messages indicate a caller issue, not a logging system failure. `vsnprintf` is safe (no buffer overflow) and available on all target platforms.

**Alternatives Considered**:

- Dynamic heap allocation with `vasprintf` or two-pass `vsnprintf` (rejected — unnecessary complexity and heap pressure for diagnostic messages; `vasprintf` is not standard C++).
- `std::format` / `<format>` (rejected — spec explicitly chose printf-style for broader compiler compatibility).
- `fmt::format` third-party library (rejected — adds external dependency for a learning-oriented project [[memory:9rx96rfq]]).

---

## Decision 2: Macro-Level Early-Out Filtering

**Decision**: The `SG_LOG` macro expands to an `if` statement that checks the category's current minimum severity against the message severity before evaluating any format arguments or calling any function.

**Rationale**: This was clarified in the spec (FR-016) — filtered messages must cost at most a single integer comparison. The macro reads the category's `MinSeverity` atomic field and compares it against the message severity. If the message would be filtered, the entire format string and arguments are never evaluated.

**Implementation Pattern**:
```cpp
#define SG_LOG(Category, Severity, Format, ...) \
    do { \
        if (static_cast<int>(ELogSeverity::Severity) >= \
            static_cast<int>(Category.GetMinSeverity())) \
        { \
            Stoner::Core::FLog::LogMessage( \
                Category, ELogSeverity::Severity, \
                __FILE__, __LINE__, Format, ##__VA_ARGS__); \
        } \
    } while (0)
```

**Alternatives Considered**:

- Filter inside `FLog::LogMessage` after formatting (rejected — violates FR-016 early-out requirement).
- Compile-time stripping via `SG_LOG_STRIP_BELOW_WARNING` (rejected during clarify — not needed for this phase; can be added later).

---

## Decision 3: Thread-Safe Log Output

**Decision**: Use a single `std::mutex` in `FLog` to serialize the format + write sequence for each log message. The mutex is locked after formatting (which happens on the caller's stack) but before writing to the sink.

**Rationale**: The spec assumes no custom threading library exists yet. `std::mutex` is the simplest correct solution. Formatting on the caller's stack before acquiring the lock minimizes contention — the critical section only covers the sink write, not the `snprintf` call.

**Implementation Detail**: Format the message into a thread-local or stack buffer first, then lock the mutex, write to sink, unlock. This ensures FR-013 (atomic log lines) while keeping the critical section as short as possible.

**Alternatives Considered**:

- Lock-free ring buffer with background writer thread (rejected — over-engineered for this phase; no threading library available).
- Per-sink mutex (rejected — only one sink exists; adds unnecessary abstraction).
- No synchronization (rejected — violates FR-013 atomic output requirement).

---

## Decision 4: Log Category Registration Pattern

**Decision**: Use static self-registration. Each `FLogCategory` instance registers itself with a global category list in its constructor. Categories are declared as global/static variables using declaration macros (`SG_DECLARE_LOG_CATEGORY_EXTERN` / `SG_DEFINE_LOG_CATEGORY`).

**Rationale**: This mirrors UE5's `DECLARE_LOG_CATEGORY_EXTERN` / `DEFINE_LOG_CATEGORY` pattern. Static initialization ensures categories are available before `main()` without explicit initialization calls (FR-014). The global list enables runtime enumeration for filtering configuration.

**Declaration Pattern**:
```cpp
// In header (cross-TU declaration):
SG_DECLARE_LOG_CATEGORY_EXTERN(LogCore, ELogSeverity::Verbose)

// In one .cpp (definition + registration):
SG_DEFINE_LOG_CATEGORY(LogCore)
```

**Alternatives Considered**:

- Explicit `FLog::RegisterCategory()` calls in initialization code (rejected — violates FR-014 zero-configuration startup).
- Compile-time category table via template metaprogramming (rejected — overly complex for the current scope).

---

## Decision 5: Platform Debug Break Abstraction

**Decision**: Provide a `SG_DEBUG_BREAK()` macro that expands to the platform-appropriate debugger break intrinsic, guarded by `_DEBUG`.

**Platform Mapping**:
- MSVC (`_MSC_VER`): `__debugbreak()`
- GCC/Clang (`__GNUC__` or `__clang__`): `__builtin_debugtrap()`
- Fallback: `std::abort()`

**Rationale**: The spec requires platform-specific debug break for assertions (FR-011) and Fatal logs (FR-012). Isolating this behind a single macro keeps all platform-conditional code in one header (`SGPlatformBreak.h`). Using `__builtin_debugtrap()` instead of `__builtin_trap()` because `debugtrap` is resumable (the developer can continue in the debugger), while `trap` is not — this matches the Q1 clarification decision.

**Alternatives Considered**:

- `raise(SIGTRAP)` on Unix (rejected — not available on Windows; `__builtin_debugtrap()` is more portable across GCC/Clang).
- `std::abort()` everywhere (rejected — loses the debugger break capability that was explicitly clarified).

---

## Decision 6: Fatal Log vs Assertion Behavior Separation

**Decision**: Implement two distinct termination paths as clarified in the spec:

1. **Assertion failure** (`SG_CHECK` / `SG_CHECKF`): Log failure details → `SG_DEBUG_BREAK()` (Debug only). No `std::abort()`. Developer can continue.
2. **Fatal log** (`SG_LOG(Cat, Fatal, ...)`): Log message → `SG_DEBUG_BREAK()` (Debug only) → `std::abort()` (always).

**Rationale**: This matches the Q1 and Q3 clarification decisions. Assertions are "soft stops" for catching bugs during development. Fatal logs are "hard stops" for unrecoverable errors. The behavioral difference is intentional and important.

**Alternatives Considered**:

- Unified behavior for both (rejected — clarify phase explicitly differentiated them).

---

## Decision 7: Console Output Format

**Decision**: Use the format `[TIMESTAMP] CATEGORY: SEVERITY: MESSAGE\n` where:
- TIMESTAMP: `HH:MM:SS.mmm` (hours:minutes:seconds.milliseconds) from `std::chrono::system_clock`
- CATEGORY: The category name string (e.g., `LogCore`)
- SEVERITY: The severity label (e.g., `Info`, `Warning`)
- MESSAGE: The formatted user message

**Example**: `[14:32:07.123] LogCore: Info: Engine initialized version 1.0`

**Rationale**: Compact, scannable format. Millisecond precision is useful for timing-sensitive debugging. Category and severity are clearly labeled for grep/filter workflows. No date component — log sessions are typically short-lived during development.

**Alternatives Considered**:

- ISO 8601 full timestamp (rejected — too verbose for console output; date is rarely needed).
- Color/ANSI codes (rejected — not portable to all terminals; can be added as a future enhancement).
- UE5-style format with date (rejected — unnecessary for this phase).

---

## Decision 8: Verification Strategy

**Decision**: Add logging and assertion verification to the existing `StonerTest` executable. Tests capture log output by temporarily redirecting the sink to a test buffer, then verify content. Assertion tests use platform-specific signal/exception handling to verify failure behavior without terminating the test process.

**Rationale**: Follows the established pattern from Phase 002/003. No external test framework dependency. For assertion failure testing, the test harness can install a custom assertion handler that records the failure instead of breaking.

**Implementation Detail**: Provide a test-only `FLog::SetAssertionHandler()` that allows tests to intercept assertion failures. In production, the default handler triggers `SG_DEBUG_BREAK()`. In tests, a custom handler records the failure details for verification.

**Alternatives Considered**:

- Fork/exec child process for assertion tests (rejected — complex and platform-dependent).
- Skip assertion failure testing (rejected — SC-003 requires 100% coverage of assertion behavior).

---

## Open Questions

None. All planning decisions are resolved.
