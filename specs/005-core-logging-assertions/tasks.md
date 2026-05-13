# Tasks: Core Foundation — Logging & Assertions

**Input**: Design documents from `/specs/005-core-logging-assertions/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/logging-assertions-api.md, quickstart.md

**Tests**: Required by the feature specification (SC-009 requires 100% coverage of all public API entry points). Test tasks are listed before their corresponding implementation tasks.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1, US2, US3, US4)
- Each task includes exact file paths

## Path Conventions

- **Core public headers**: `Source/Core/Public/Core/`
- **Core implementation**: `Source/Core/Private/`
- **Tests**: `Tests/`
- **Feature docs**: `specs/005-core-logging-assertions/`

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Prepare the logging/assertion test harness and verify existing Core layer is ready.

- [ ] T001 Create `Tests/LoggingAssertionTests.h` with a `RunLoggingAssertionTests()` declaration and shared test result type
- [ ] T002 Create `Tests/LoggingAssertionTests.cpp` with a minimal logging/assertion test harness scaffold that compiles but contains no passing feature assertions yet
- [ ] T003 Update `Tests/Main.cpp` to include `LoggingAssertionTests.h` and call `RunLoggingAssertionTests()`, returning nonzero when logging/assertion verification fails
- [ ] T004 Run `scons` from the repository root and fix any scaffold build errors in `Tests/LoggingAssertionTests.cpp` or `Tests/Main.cpp`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Create the platform abstraction and severity enum that ALL user stories depend on.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [ ] T005 [P] Create `Source/Core/Public/Core/ELogSeverity.h` with `enum class ELogSeverity : uint8 { Verbose = 0, Info = 1, Warning = 2, Error = 3, Fatal = 4 }` inside `namespace Stoner::Core`, including a `SeverityToString()` helper function declaration
- [ ] T006 [P] Create `Source/Core/Public/Core/SGPlatformBreak.h` with `SG_DEBUG_BREAK()` macro using `__debugbreak()` for MSVC, `__builtin_debugtrap()` for GCC/Clang, and `std::abort()` as fallback; guarded by `_DEBUG` (expands to nothing in Release)
- [ ] T007 Run `scons` from the repository root and fix any foundational build errors in `Source/Core/Public/Core/ELogSeverity.h` or `Source/Core/Public/Core/SGPlatformBreak.h`

**Checkpoint**: Foundational headers build. User story implementation can begin.

---

## Phase 3: User Story 1 — Engine Developer Logs Diagnostic Messages (Priority: P1) 🎯 MVP

**Goal**: Developers can emit structured diagnostic messages at all five severity levels using `SG_LOG` and see formatted console output with timestamp, category, severity, and message.

**Independent Test**: Call `SG_LOG(LogCore, Info, "Engine initialized version %d.%d", 1, 0)` and verify the formatted output appears on the console with correct timestamp, category, and severity prefix.

### Tests for User Story 1

- [ ] T008 [US1] Add failing verification cases in `Tests/LoggingAssertionTests.cpp` for `ELogSeverity` enum values: confirm `Verbose < Info < Warning < Error < Fatal` ordering and that `SeverityToString()` returns correct labels for all five levels
- [ ] T009 [US1] Add failing verification cases in `Tests/LoggingAssertionTests.cpp` for `FLogCategory` construction: confirm `GetName()` returns the category name, `GetMinSeverity()` returns the default, and `SetMinSeverity()` updates the threshold
- [ ] T010 [US1] Add failing verification cases in `Tests/LoggingAssertionTests.cpp` for `FLogConsoleSink` output format: capture sink output and verify it matches `[HH:MM:SS.mmm] CategoryName: SeverityLabel: Message\n` pattern
- [ ] T011 [US1] Add failing verification cases in `Tests/LoggingAssertionTests.cpp` for `FLog::LogMessage`: verify all five severity levels produce correctly labeled output, verify stdout vs stderr routing (Verbose/Info → stdout, Warning/Error/Fatal → stderr)
- [ ] T012 [US1] Add failing verification cases in `Tests/LoggingAssertionTests.cpp` for `SG_LOG` macro: verify macro-level early-out by confirming a side-effect counter is NOT incremented when the message is filtered out
- [ ] T013 [US1] Add failing verification case in `Tests/LoggingAssertionTests.cpp` for Fatal log behavior: install a custom assertion handler via `FLog::SetAssertionHandler()`, call `SG_LOG(LogCore, Fatal, ...)`, and verify the handler is invoked (do not actually abort in tests)

### Implementation for User Story 1

- [ ] T014 [P] [US1] Create `Source/Core/Public/Core/FLogCategory.h` with `struct FLogCategory` inside `namespace Stoner::Core`: constructor taking `const char* Name` and `ELogSeverity DefaultMinSeverity`, `GetName()`, `GetMinSeverity()`, `SetMinSeverity()`, self-registration into global category list, and `SG_DECLARE_LOG_CATEGORY_EXTERN` / `SG_DEFINE_LOG_CATEGORY` declaration macros
- [ ] T015 [P] [US1] Create `Source/Core/Public/Core/FLogConsoleSink.h` with `struct FLogConsoleSink` inside `namespace Stoner::Core`: `Write(ELogSeverity, const char* FormattedMessage)` method that routes to stdout (Verbose/Info) or stderr (Warning/Error/Fatal)
- [ ] T016 [P] [US1] Create `Source/Core/Public/Core/FLog.h` with `struct FLog` inside `namespace Stoner::Core`: static `LogMessage(FLogCategory&, ELogSeverity, const char* File, int Line, const char* Format, ...)`, `SetGlobalMinSeverity()`, `GetGlobalMinSeverity()`, `SetAssertionHandler()`, and `HandleAssertionFailure()` declarations
- [ ] T017 [US1] Create `Source/Core/Private/FLogCategory.cpp` implementing `FLogCategory` constructor with self-registration into a static `TArray<FLogCategory*>` global category list, and `SeverityToString()` helper
- [ ] T018 [US1] Create `Source/Core/Private/FLogConsoleSink.cpp` implementing `FLogConsoleSink::Write()` with `[HH:MM:SS.mmm]` timestamp formatting using `std::chrono::system_clock`, 1024-byte stack buffer via `vsnprintf`, truncation with `...`, and stdout/stderr routing
- [ ] T019 [US1] Create `Source/Core/Private/FLog.cpp` implementing `FLog::LogMessage()` with `std::mutex` serialization (format on caller stack, lock only for sink write), Fatal severity handling (log → `SG_DEBUG_BREAK()` in Debug → `std::abort()`), global severity filtering, and replaceable assertion handler
- [ ] T020 [US1] Create `Source/Core/Public/Core/SGLog.h` with `SG_LOG(Category, Severity, Format, ...)` macro: `do { if (static_cast<int>(ELogSeverity::Severity) >= static_cast<int>(Category.GetMinSeverity())) { FLog::LogMessage(...); } } while(0)` pattern with `__FILE__` and `__LINE__` passthrough
- [ ] T021 [US1] Define pre-defined log categories: declare `LogCore` in `Source/Core/Public/Core/FLogCategory.h` via `SG_DECLARE_LOG_CATEGORY_EXTERN`, define `LogCore` in `Source/Core/Private/FLogCategory.cpp` via `SG_DEFINE_LOG_CATEGORY`; declare `LogRHI`, `LogRenderer`, `LogBackend`, `LogApplication` similarly
- [ ] T022 [US1] Update `Source/Core/Public/Core/CoreMinimal.h` to include `ELogSeverity.h`, `FLogCategory.h`, `FLog.h`, `FLogConsoleSink.h`, `SGLog.h`, and `SGPlatformBreak.h`
- [ ] T023 [US1] Run `scons` and `StonerTest` locally, then fix any US1 failures in `Source/Core/Public/Core/`, `Source/Core/Private/`, or `Tests/LoggingAssertionTests.cpp`

**Checkpoint**: User Story 1 should pass — developers can log at all five severity levels with formatted console output.

---

## Phase 4: User Story 2 — Engine Developer Catches Invalid State with Assertions (Priority: P1)

**Goal**: Developers can validate runtime invariants using `SG_CHECK`, `SG_VERIFY`, and `SG_CHECKF` macros with clear failure reporting and Debug/Release behavior differentiation.

**Independent Test**: Trigger `SG_CHECK(false)` in a Debug build and verify it reports file, line, and expression; call `SG_VERIFY(SomeFunction())` and confirm the function executes in both Debug and Release builds.

### Tests for User Story 2

- [ ] T024 [US2] Add failing verification cases in `Tests/LoggingAssertionTests.cpp` for `SG_CHECK(Expr)`: install custom assertion handler, trigger `SG_CHECK(false)`, verify handler receives file path, line number, and expression text `"false"`
- [ ] T025 [US2] Add failing verification cases in `Tests/LoggingAssertionTests.cpp` for `SG_CHECKF(Expr, Format, ...)`: trigger `SG_CHECKF(false, "Index %d out of range", 42)`, verify handler receives the formatted message in addition to file/line/expression
- [ ] T026 [US2] Add failing verification cases in `Tests/LoggingAssertionTests.cpp` for `SG_VERIFY(Expr)`: call `SG_VERIFY(IncrementCounter())` and verify the counter is always incremented (both Debug and Release behavior), and that assertion handler is invoked when expression is falsy (Debug only)

### Implementation for User Story 2

- [ ] T027 [US2] Create `Source/Core/Public/Core/SGAssert.h` with `SG_CHECK(Expr)`, `SG_VERIFY(Expr)`, and `SG_CHECKF(Expr, Format, ...)` macros: `SG_CHECK` uses `#Expr` stringification and calls `FLog::HandleAssertionFailure()` on failure, guarded by `_DEBUG`; `SG_VERIFY` always evaluates `Expr` but only checks in Debug; `SG_CHECKF` adds printf-formatted message
- [ ] T028 [US2] Update `FLog::HandleAssertionFailure()` implementation in `Source/Core/Private/FLog.cpp` to log assertion details (file, line, expression, optional message) via the logging system and then invoke the assertion handler (default: `SG_DEBUG_BREAK()`)
- [ ] T029 [US2] Update `Source/Core/Public/Core/CoreMinimal.h` to include `SGAssert.h`
- [ ] T030 [US2] Run `scons` and `StonerTest` locally, then fix any US2 failures in `Source/Core/Public/Core/SGAssert.h`, `Source/Core/Private/FLog.cpp`, or `Tests/LoggingAssertionTests.cpp`

**Checkpoint**: User Stories 1 and 2 should both pass — the minimum viable diagnostic infrastructure is complete.

---

## Phase 5: User Story 3 — Engine Developer Filters Log Output by Category (Priority: P2)

**Goal**: Developers can configure per-category and global severity thresholds to suppress unwanted log output, enabling focused debugging.

**Independent Test**: Set `LogCore` minimum severity to `Warning`, log an `Info` message to `LogCore` and verify it is suppressed, while an `Info` message to `LogRHI` still appears.

### Tests for User Story 3

- [ ] T031 [US3] Add failing verification cases in `Tests/LoggingAssertionTests.cpp` for per-category filtering: set `LogCore.SetMinSeverity(ELogSeverity::Warning)`, verify `SG_LOG(LogCore, Info, ...)` is suppressed (no output captured), verify `SG_LOG(LogCore, Error, ...)` still produces output
- [ ] T032 [US3] Add failing verification cases in `Tests/LoggingAssertionTests.cpp` for global severity filtering: set `FLog::SetGlobalMinSeverity(ELogSeverity::Info)`, verify `SG_LOG(LogCore, Verbose, ...)` is suppressed
- [ ] T033 [US3] Add failing verification case in `Tests/LoggingAssertionTests.cpp` for early-out zero-overhead: use a side-effect counter in the format argument, verify the counter is NOT incremented when the message is filtered by category threshold

### Implementation for User Story 3

- [ ] T034 [US3] Ensure `FLog::LogMessage()` in `Source/Core/Private/FLog.cpp` applies global severity filtering in addition to the macro-level per-category check: if global min severity is higher than the message severity, suppress even if the category allows it
- [ ] T035 [US3] Run `scons` and `StonerTest` locally, then fix any US3 failures in `Source/Core/Private/FLog.cpp` or `Tests/LoggingAssertionTests.cpp`

**Checkpoint**: User Stories 1, 2, and 3 should all pass — filtering is operational.

---

## Phase 6: User Story 4 — Engine Developer Declares Custom Log Categories (Priority: P2)

**Goal**: Developers can declare new log categories for their subsystems using simple macros, with automatic registration and immediate usability.

**Independent Test**: Declare a new category `LogTest` and use it in `SG_LOG(LogTest, Info, "message")`, verifying the output shows "LogTest".

### Tests for User Story 4

- [ ] T036 [US4] Add failing verification cases in `Tests/LoggingAssertionTests.cpp` for custom category declaration: declare `LogTestCustom` using `SG_DECLARE_LOG_CATEGORY_EXTERN` / `SG_DEFINE_LOG_CATEGORY`, call `SG_LOG(LogTestCustom, Info, "custom test")`, verify output contains "LogTestCustom"
- [ ] T037 [US4] Add failing verification case in `Tests/LoggingAssertionTests.cpp` for category self-registration: after declaring and defining `LogTestCustom`, verify it appears in the global category registry

### Implementation for User Story 4

- [ ] T038 [US4] Verify `SG_DECLARE_LOG_CATEGORY_EXTERN` and `SG_DEFINE_LOG_CATEGORY` macros in `Source/Core/Public/Core/FLogCategory.h` work correctly for custom categories declared in test files — fix any cross-TU linkage or registration issues
- [ ] T039 [US4] Add `FLogCategory::GetAllCategories()` static method in `Source/Core/Public/Core/FLogCategory.h` and `Source/Core/Private/FLogCategory.cpp` that returns a const reference to the global category list for runtime enumeration
- [ ] T040 [US4] Run `scons` and `StonerTest` locally, then fix any US4 failures in `Source/Core/Public/Core/FLogCategory.h`, `Source/Core/Private/FLogCategory.cpp`, or `Tests/LoggingAssertionTests.cpp`

**Checkpoint**: All four user stories should pass — the complete logging and assertion system is functional.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Final cleanup, contract validation, and cross-platform readiness.

- [ ] T041 [P] Add thread-safety verification in `Tests/LoggingAssertionTests.cpp`: launch multiple threads each logging messages concurrently, verify no interleaved characters within any single log line (FR-013)
- [ ] T042 [P] Add zero-configuration startup verification in `Tests/LoggingAssertionTests.cpp`: verify `SG_LOG` works without any explicit initialization call (FR-014)
- [ ] T043 Verify public names follow UE5-style `E`, `F`, and `SG_` prefixes in `Source/Core/Public/Core/`
- [ ] T044 Verify no Core public header includes RHI, Backend, Renderer, Application, or graphics API headers in `Source/Core/Public/Core/`
- [ ] T045 Verify `specs/005-core-logging-assertions/contracts/logging-assertions-api.md` is satisfied by the implemented headers in `Source/Core/Public/Core/`
- [ ] T046 Run the quickstart build and verification flow from `specs/005-core-logging-assertions/quickstart.md`
- [ ] T047 Run `scons` from the repository root and confirm `Build/<Platform>/<Config>/Tests/StonerTest` exits with code 0
- [ ] T048 Review `Tests/LoggingAssertionTests.cpp` to ensure every public API entry point from `specs/005-core-logging-assertions/spec.md` has verification coverage (SC-009)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion — blocks all user stories
- **User Story 1 (Phase 3)**: Depends on Foundational phase completion — MVP
- **User Story 2 (Phase 4)**: Depends on Foundational phase completion; uses `FLog::HandleAssertionFailure()` from US1 implementation, so should follow US1
- **User Story 3 (Phase 5)**: Depends on US1 completion (needs working `SG_LOG` and categories to test filtering)
- **User Story 4 (Phase 6)**: Depends on US1 completion (needs working category macros to test custom declarations)
- **Polish (Phase 7)**: Depends on all user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: MVP. Establishes the complete logging pipeline: severity → category → coordinator → sink → macro.
- **User Story 2 (P1)**: Adds assertion macros. Depends on `FLog` from US1 for failure reporting.
- **User Story 3 (P2)**: Adds filtering behavior. Depends on working `SG_LOG` from US1.
- **User Story 4 (P2)**: Adds custom category extensibility. Depends on working category macros from US1.

### Within Each User Story

- Tests must be written before matching implementation tasks.
- Public headers can be implemented in parallel when they touch different files (T014, T015, T016).
- `.cpp` implementations follow their corresponding `.h` declarations.
- `CoreMinimal.h` aggregate updates follow the headers they include.
- Build and verification tasks follow implementation tasks.

### Parallel Opportunities

- T005 and T006 can run in parallel (different foundational headers).
- T014, T015, and T016 can run in parallel (different public headers for US1).
- T041 and T042 can run in parallel (different polish verification concerns).
- US3 and US4 can run in parallel after US1 is complete (independent filtering vs. custom category concerns).

---

## Parallel Example: User Story 1

```bash
# Launch all US1 public headers together (different files):
Task: "Create FLogCategory.h in Source/Core/Public/Core/FLogCategory.h"
Task: "Create FLogConsoleSink.h in Source/Core/Public/Core/FLogConsoleSink.h"
Task: "Create FLog.h in Source/Core/Public/Core/FLog.h"
```

---

## Parallel Example: User Stories 3 & 4

```bash
# After US1 is complete, US3 and US4 can proceed in parallel:
Task: "US3 — Per-category and global severity filtering"
Task: "US4 — Custom category declaration and registration"
```

---

## Implementation Strategy

### MVP First (User Stories 1 + 2)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational (`ELogSeverity`, `SGPlatformBreak`)
3. Complete Phase 3: User Story 1 (full logging pipeline)
4. Complete Phase 4: User Story 2 (assertion macros)
5. **STOP and VALIDATE**: Test the minimum viable diagnostic infrastructure
6. Developers can now use `SG_LOG` and `SG_CHECK` in all engine code

### Incremental Delivery

1. Deliver US1 + US2 → Minimum viable diagnostic infrastructure (MVP)
2. Add US3 → Category-based filtering for focused debugging
3. Add US4 → Custom category extensibility for new subsystems
4. Finish with polish checks against the spec, contract, and quickstart

### Solo Agent Strategy

Work sequentially by task ID. Run `scons` and `StonerTest` at each checkpoint before moving to the next story. The explicitly marked parallel tasks (T014/T015/T016) can be combined into a single implementation session since they touch different files.

---

## Notes

- Keep all new Core public deliverables inside `namespace Stoner::Core`.
- Macros (`SG_LOG`, `SG_CHECK`, `SG_VERIFY`, `SG_CHECKF`, `SG_DEBUG_BREAK`) are in the global macro namespace.
- Do not add dependencies outside the C++ standard library for this feature.
- Do not implement file-based sinks, async logging, color output, or profiling.
- Mark tasks complete in this file only after the implementation and relevant verification for that task are done.
- The test harness uses a custom assertion handler (`FLog::SetAssertionHandler()`) to intercept assertion failures without terminating the test process.
