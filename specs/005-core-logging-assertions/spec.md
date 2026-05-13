# Feature Specification: Core Foundation — Logging & Assertions

**Feature Branch**: `005-core-logging-assertions`  
**Created**: 2026-05-13  
**Status**: Draft  
**Input**: User description: "Core logging and assertion system: FLog with severity levels (Verbose/Info/Warning/Error/Fatal), SG_LOG macro with category and printf-style formatting, SG_CHECK/SG_VERIFY/SG_CHECKF assertion macros, category-based filtering. Console output sink. UE5 naming style. Cross-platform. Unit tests."

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Engine Developer Logs Diagnostic Messages (Priority: P1)

An engine developer working on any layer (Core, RHI, Backend, Renderer, Application) needs to emit structured diagnostic messages during development and debugging. They use the `SG_LOG` macro with a category and severity level to record what the engine is doing at runtime. Messages appear on the console with a consistent format showing timestamp, category, severity, and the message body.

**Why this priority**: Logging is the most fundamental diagnostic tool. Without it, developers cannot observe engine behavior, trace execution flow, or diagnose issues. Every subsequent phase depends on having a working logging system.

**Independent Test**: Can be fully tested by calling `SG_LOG(LogCore, Info, "Engine initialized version %d.%d", major, minor)` and verifying the formatted output appears on the console with correct timestamp, category, and severity prefix.

**Acceptance Scenarios**:

1. **Given** the logging system is initialized, **When** a developer calls `SG_LOG(LogCore, Info, "Hello %s", "World")`, **Then** a formatted message appears on the console containing the timestamp, "LogCore", "Info", and "Hello World".
2. **Given** the logging system is initialized, **When** a developer calls `SG_LOG(LogRHI, Warning, "Buffer size %zu exceeds limit", size)`, **Then** the message appears with "Warning" severity and "LogRHI" category.
3. **Given** the logging system is initialized, **When** a developer logs messages at all five severity levels (Verbose, Info, Warning, Error, Fatal), **Then** each message displays with its correct severity label.

---

### User Story 2 — Engine Developer Catches Invalid State with Assertions (Priority: P1)

An engine developer needs to validate assumptions and invariants at runtime. They use `SG_CHECK(Expr)` to assert conditions that must be true, `SG_VERIFY(Expr)` for assertions where the expression has side effects that must always execute, and `SG_CHECKF(Expr, Format, ...)` for assertions with descriptive failure messages. When an assertion fails in a Debug build, the engine reports the failure location and message clearly.

**Why this priority**: Assertions are equally critical as logging — they catch programming errors early and prevent silent corruption. The combination of logging + assertions forms the minimum viable diagnostic infrastructure.

**Independent Test**: Can be fully tested by triggering `SG_CHECK(false)` in a Debug build and verifying it reports the file, line, and expression; and by calling `SG_VERIFY(SomeFunction())` and confirming the function executes in both Debug and Release builds.

**Acceptance Scenarios**:

1. **Given** a Debug build, **When** `SG_CHECK(ptr != nullptr)` is called with a null pointer, **Then** the assertion fires, reporting the file name, line number, and the expression `ptr != nullptr`.
2. **Given** a Release build, **When** `SG_CHECK(ptr != nullptr)` is called with a null pointer, **Then** the check is stripped and has zero runtime cost.
3. **Given** any build configuration, **When** `SG_VERIFY(Initialize())` is called, **Then** `Initialize()` always executes regardless of build type, and the return value is checked only in Debug builds.
4. **Given** a Debug build, **When** `SG_CHECKF(index < count, "Index %d out of range [0, %d)", index, count)` fails, **Then** the formatted message is included in the assertion report.

---

### User Story 3 — Engine Developer Filters Log Output by Category (Priority: P2)

An engine developer working on the Vulkan backend is overwhelmed by log output from all engine subsystems. They configure the logging system to show only messages from specific categories (e.g., `LogVulkan`) or to suppress verbose messages below a certain severity threshold. This allows focused debugging without noise from unrelated subsystems.

**Why this priority**: Category-based filtering is important for productivity but the engine can function without it initially. Developers can always grep console output as a workaround.

**Independent Test**: Can be fully tested by setting the minimum severity for `LogCore` to `Warning`, logging an `Info` message to `LogCore`, and verifying it is suppressed, while an `Info` message to `LogRHI` still appears.

**Acceptance Scenarios**:

1. **Given** the minimum severity for category `LogCore` is set to `Warning`, **When** `SG_LOG(LogCore, Info, "test")` is called, **Then** the message is suppressed and does not appear on the console.
2. **Given** the minimum severity for category `LogCore` is set to `Warning`, **When** `SG_LOG(LogCore, Error, "test")` is called, **Then** the message appears on the console.
3. **Given** a global minimum severity of `Info` is set, **When** `SG_LOG(LogCore, Verbose, "test")` is called, **Then** the message is suppressed.

---

### User Story 4 — Engine Developer Declares Custom Log Categories (Priority: P2)

An engine developer creating a new subsystem (e.g., a Meshlet renderer) needs to declare a new log category specific to their module. They use a simple declaration mechanism to define `LogMeshlet` and immediately use it with `SG_LOG`. The category is automatically registered and available for filtering.

**Why this priority**: Extensibility of the category system is needed for all future phases but is a straightforward extension of the core logging infrastructure.

**Independent Test**: Can be fully tested by declaring a new category `LogTest` and using it in `SG_LOG(LogTest, Info, "message")`, verifying the output shows "LogTest".

**Acceptance Scenarios**:

1. **Given** a developer declares a new log category `LogMeshlet`, **When** they call `SG_LOG(LogMeshlet, Info, "Processing cluster %d", id)`, **Then** the message appears with "LogMeshlet" as the category.
2. **Given** multiple categories are declared across different translation units, **When** the engine starts, **Then** all categories are available and no duplicate registration errors occur.

---

### Edge Cases

- What happens when a `Fatal` severity message is logged? The engine should log the message and then terminate the process (or trigger a debugger break in Debug builds).
- What happens when the format string has mismatched arguments? The system should behave safely (no undefined behavior), ideally producing a best-effort output or a clear error.
- What happens when logging is called from multiple threads simultaneously? The console output must not produce garbled/interleaved characters within a single log line.
- What happens when `SG_LOG` is called before the logging system is explicitly initialized? It should still produce output (using a default/fallback configuration).

## Architecture & Design Constraints *(mandatory)*

- **RHI Abstraction**: This feature is in the Core layer and does not interact with graphics APIs. It MUST NOT have any dependencies on RHI or higher layers.
- **Design Patterns**: The logging system MUST use a clean separation between the log macro interface, the category registry, and the output sink. No god-classes.
- **Advanced Graphics**: Not directly applicable, but the logging infrastructure MUST be lightweight enough to be used in performance-sensitive rendering code paths without significant overhead.
- **Naming Conventions**: All types MUST follow UE5-style naming: `FLog` (struct), `ELogSeverity` (enum), `SG_LOG` / `SG_CHECK` / `SG_VERIFY` / `SG_CHECKF` (macros).
- **Cross-Platform Compatibility**: The logging and assertion system MUST compile and run identically on Windows, macOS, and Linux. Console output MUST use platform-appropriate mechanisms. Debug break behavior in assertions MUST use platform-specific intrinsics (`__debugbreak()` on MSVC, `__builtin_trap()` on GCC/Clang, etc.) behind conditional compilation.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST provide an `ELogSeverity` enumeration with five levels: Verbose, Info, Warning, Error, Fatal.
- **FR-002**: System MUST provide an `SG_LOG(Category, Severity, Format, ...)` macro that formats a message using printf-style formatting and dispatches it to the active output sink.
- **FR-003**: System MUST provide a log category declaration mechanism that allows defining named categories (e.g., `LogCore`, `LogRHI`, `LogRenderer`, `LogVulkan`) usable across translation units.
- **FR-004**: System MUST provide a console output sink that writes formatted log messages to `stdout` (for Verbose/Info) and `stderr` (for Warning/Error/Fatal).
- **FR-005**: Each log message MUST include: timestamp, category name, severity label, and the user-provided formatted message.
- **FR-006**: System MUST support per-category minimum severity filtering, allowing suppression of messages below a configured threshold.
- **FR-007**: System MUST support a global minimum severity threshold that applies when no per-category override is set.
- **FR-008**: System MUST provide `SG_CHECK(Expr)` — a runtime assertion macro that is active in Debug builds and stripped (zero cost) in Release builds.
- **FR-009**: System MUST provide `SG_VERIFY(Expr)` — an assertion macro that always evaluates the expression but only checks the result in Debug builds.
- **FR-010**: System MUST provide `SG_CHECKF(Expr, Format, ...)` — an assertion macro with a printf-style formatted failure message, active in Debug builds.
- **FR-011**: When an assertion fails (Debug build), the system MUST report the source file, line number, the failed expression text, and any formatted message.
- **FR-012**: When a `Fatal` severity log message is emitted, the system MUST log the message and then terminate the process.
- **FR-013**: Log output for a single message MUST be atomic — concurrent logging from multiple threads MUST NOT produce interleaved characters within one log line.
- **FR-014**: The logging system MUST function with a default configuration even if no explicit initialization call is made (zero-configuration startup).
- **FR-015**: System MUST provide pre-defined log categories for existing engine layers: `LogCore`, `LogRHI`, `LogRenderer`, `LogBackend`, `LogApplication`.

### Key Entities

- **ELogSeverity**: Enumeration representing message importance levels (Verbose, Info, Warning, Error, Fatal).
- **FLogCategory**: Represents a named log category with an associated minimum severity filter. Categories are declared statically and registered automatically.
- **FLog**: The central logging coordinator that receives log messages, applies filtering, and dispatches to output sinks.
- **FLogConsoleSink**: The console output sink that formats and writes messages to stdout/stderr.
- **SG_LOG / SG_CHECK / SG_VERIFY / SG_CHECKF**: Macro-based interfaces that provide file/line context and compile-time stripping.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer can add a log statement to any engine source file and see formatted output on the console within one compilation cycle — no additional setup or initialization code required.
- **SC-002**: All five severity levels produce visually distinguishable output (different labels) so a developer can scan console output and immediately identify warnings and errors.
- **SC-003**: Assertion macros catch 100% of violated preconditions in Debug builds, reporting exact source location (file + line) for every failure.
- **SC-004**: `SG_CHECK` assertions have zero measurable runtime cost in Release builds (completely compiled out).
- **SC-005**: `SG_VERIFY` always executes its expression in all build configurations, ensuring side-effect-dependent code is never silently removed.
- **SC-006**: Category-based filtering allows a developer to reduce visible log output by at least 80% when focusing on a single subsystem, without modifying any log call sites.
- **SC-007**: Concurrent logging from multiple threads produces correctly formatted, non-interleaved output lines 100% of the time.
- **SC-008**: The logging and assertion system compiles and passes all unit tests on Windows (MSVC), macOS (Clang), and Linux (GCC) without platform-specific workarounds at call sites.
- **SC-009**: Unit test suite achieves 100% coverage of all public API entry points (every macro, every severity level, every filtering path).

## Assumptions

- The Core layer's `FPlatformTypes.h` and basic type system (from Phase 002 / specs 003) are available and complete.
- The engine does not yet have a threading/concurrency library; thread safety for log output will use standard library primitives (`std::mutex`) as an interim solution.
- File-based log sinks are explicitly out of scope and will be added in a future enhancement phase.
- Remote/network logging is out of scope.
- Profiling and performance timing instrumentation are out of scope (separate future phase).
- The assertion macros will use platform-specific debug break intrinsics; the exact behavior on assertion failure (break into debugger vs. abort) may vary by platform, which is acceptable.
- Printf-style formatting is chosen over `std::format` / `<format>` for broader compiler compatibility and familiarity; this may be revisited in a future phase.
