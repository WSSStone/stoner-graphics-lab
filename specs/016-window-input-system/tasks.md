# Tasks: Window & Input System

**Input**: Design documents from `/specs/016-window-input-system/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/window-input-contract.md, quickstart.md

**Tests**: Required by the feature specification through deterministic headless validation, optional real-window smoke validation, and measurable success criteria.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1, US2, US3, US4)
- Include exact file paths in descriptions

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish Application test harness and build entry points for the feature.

- [x] T001 Update `Source/Application/SConscript` to compile all new Application private `.cpp` files and keep optional real-window adapter build detection private
- [x] T002 [P] Create `Tests/ApplicationWindowInputTests.h` with declarations for the 016 Application window/input test suite
- [x] T003 [P] Create `Tests/ApplicationWindowInputTests.cpp` with a minimal test suite scaffold and local assertion helpers
- [x] T004 Register `ApplicationWindowInputTests` in `Tests/Main.cpp`
- [x] T005 Confirm `Tests/SConscript` auto-discovers `ApplicationWindowInputTests.cpp` without extra per-file wiring in `Tests/SConscript`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Shared public types, diagnostics, and private driver seams that all user stories depend on.

**CRITICAL**: No user story work can begin until this phase is complete.

- [x] T006 [P] Define `EWindowLifecycleState`, `EWindowDisplayMode`, and `EWindowRuntimeAvailability` in `Source/Application/Public/Application/FWindowDesc.h`
- [x] T007 [P] Define stable physical key identifiers including `Unknown` in `Source/Application/Public/Application/EKey.h`
- [x] T008 [P] Define stable physical mouse button identifiers including `Unknown` in `Source/Application/Public/Application/EMouseButton.h`
- [x] T009 [P] Define diagnostic severity, category, record, and collection types in `Source/Application/Public/Application/FApplicationDiagnostics.h`
- [x] T010 Implement deterministic diagnostic ordering and append helpers in `Source/Application/Private/FApplicationDiagnostics.cpp`
- [x] T011 [P] Define window event types, event records, and latest-size fields in `Source/Application/Public/Application/FWindowEvent.h`
- [x] T012 Implement window event construction and deterministic ordering helpers in `Source/Application/Private/FWindowEvent.cpp`
- [x] T013 [P] Define ordered physical input event records in `Source/Application/Public/Application/FInputEvent.h`
- [x] T014 [P] Define per-frame input snapshot fields and query helpers in `Source/Application/Public/Application/FInputState.h`
- [x] T015 Create private `IWindowDriver` strategy seam for headless and real-window drivers in `Source/Application/Private/FWindowDriver.h`
- [x] T016 Create deterministic headless driver event queue skeleton in `Source/Application/Private/FHeadlessWindowDriver.cpp`
- [x] T017 Create private real-window adapter skeleton with unavailable-runtime fallback in `Source/Application/Private/FGlfwWindowDriver.cpp`
- [x] T018 Export foundational Application headers through `Source/Application/Public/Application/ApplicationMinimal.h`

**Checkpoint**: Foundation ready - user story implementation can now begin in parallel.

---

## Phase 3: User Story 1 - Create and Control the Primary Window (Priority: P1) MVP

**Goal**: Create, observe, close, and destroy one primary window through deterministic lifecycle validation and real-window smoke behavior when available.

**Independent Test**: Create a primary window from a valid configuration, observe Active state, request close, destroy it, and confirm repeated destroy remains safe without rendering.

### Tests for User Story 1

- [x] T019 [US1] Add valid primary window create/close/destroy lifecycle tests in `Tests/ApplicationWindowInputTests.cpp`
- [x] T020 [US1] Add invalid and boundary window description validation tests, including the 16384x16384 v1 safe maximum and above-limit rejection, in `Tests/ApplicationWindowInputTests.cpp`
- [x] T021 [US1] Add optional real-window smoke or unavailable-runtime lifecycle test coverage in `Tests/ApplicationWindowInputTests.cpp`

### Implementation for User Story 1

- [x] T022 [US1] Implement `FWindowDesc` defaults, `MaxClientWidth`/`MaxClientHeight` v1 safe bounds of 16384, and validation helpers in `Source/Application/Public/Application/FWindowDesc.h`
- [x] T023 [P] [US1] Define public primary window contract and lifecycle queries in `Source/Application/Public/Application/FWindow.h`
- [x] T024 [US1] Implement primary window lifecycle, close request, idempotent destroy, and state queries in `Source/Application/Private/FWindow.cpp`
- [x] T025 [P] [US1] Implement headless create, close-request, destroy, and lifecycle event playback in `Source/Application/Private/FHeadlessWindowDriver.cpp`
- [x] T026 [P] [US1] Implement real-window adapter create/close/destroy smoke path and dependency/display unavailable results in `Source/Application/Private/FGlfwWindowDriver.cpp`
- [x] T027 [P] [US1] Add stable window lifecycle diagnostic codes and messages in `Source/Application/Private/FApplicationDiagnostics.cpp`
- [x] T028 [US1] Export `FWindowDesc`, `FWindow`, and `FWindowEvent` through `Source/Application/Public/Application/ApplicationMinimal.h`
- [x] T029 [US1] Validate User Story 1 lifecycle tests pass through the Application test suite in `Tests/ApplicationWindowInputTests.cpp`

**Checkpoint**: User Story 1 is fully functional and testable independently.

---

## Phase 4: User Story 2 - Read Frame-Based Keyboard and Mouse State (Priority: P1)

**Goal**: Poll deterministic physical keyboard and mouse state once per frame, including pressed, held, released, pointer position, pointer delta, focus-loss clearing, and unknown input handling.

**Independent Test**: Feed known keyboard and mouse event sequences across frames and verify the resulting `FInputState` snapshot after each poll.

### Tests for User Story 2

- [x] T030 [US2] Add physical key pressed/held/released transition tests in `Tests/ApplicationWindowInputTests.cpp`
- [x] T031 [US2] Add mouse button pressed/held/released and pointer delta tests in `Tests/ApplicationWindowInputTests.cpp`
- [x] T032 [US2] Add focus-loss held-state clearing tests in `Tests/ApplicationWindowInputTests.cpp`
- [x] T033 [US2] Add unknown key/button non-corruption tests in `Tests/ApplicationWindowInputTests.cpp`

### Implementation for User Story 2

- [x] T034 [P] [US2] Complete physical keyboard vocabulary and helper names in `Source/Application/Public/Application/EKey.h`
- [x] T035 [P] [US2] Complete physical mouse-button vocabulary and helper names in `Source/Application/Public/Application/EMouseButton.h`
- [x] T036 [US2] Implement transient and held input state transitions in `Source/Application/Private/FInputState.cpp`
- [x] T037 [US2] Define public input manager polling and snapshot API in `Source/Application/Public/Application/FInputManager.h`
- [x] T038 [US2] Implement input event ingestion, frame-boundary reset, focus-loss clearing, and safe empty state in `Source/Application/Private/FInputManager.cpp`
- [x] T039 [US2] Extend headless driver with deterministic keyboard, mouse-button, pointer, focus, and unknown-input event injection in `Source/Application/Private/FHeadlessWindowDriver.cpp`
- [x] T040 [US2] Add stable input diagnostic codes for unknown inputs and unsafe polling in `Source/Application/Private/FApplicationDiagnostics.cpp`
- [x] T041 [US2] Export `EKey`, `EMouseButton`, `FInputEvent`, `FInputState`, and `FInputManager` through `Source/Application/Public/Application/ApplicationMinimal.h`
- [x] T042 [US2] Validate User Story 2 input transition tests pass through the Application test suite in `Tests/ApplicationWindowInputTests.cpp`

**Checkpoint**: User Stories 1 and 2 are independently functional and testable.

---

## Phase 5: User Story 3 - React to Window Events in the Application Loop (Priority: P2)

**Goal**: Run a minimal loop that polls events, updates input state, observes resize/close notifications, pauses presentation for minimized or zero drawable windows, and exits cleanly.

**Independent Test**: Run deterministic simulated window events through the loop and verify poll/update/window-state/decision ordering, latest resize size, presentation-paused state, and close exit.

### Tests for User Story 3

- [x] T043 [US3] Add resize coalescing and latest client-size loop tests in `Tests/ApplicationWindowInputTests.cpp`
- [x] T044 [US3] Add minimized and zero drawable presentation-paused loop tests in `Tests/ApplicationWindowInputTests.cpp`
- [x] T045 [US3] Add close-request loop exit ordering tests in `Tests/ApplicationWindowInputTests.cpp`
- [x] T046 [US3] Add 300-frame representative loop stability test in `Tests/ApplicationWindowInputTests.cpp`

### Implementation for User Story 3

- [x] T047 [P] [US3] Define public loop configuration, callback, and decision types in `Source/Application/Public/Application/FApplicationLoop.h`
- [x] T048 [US3] Implement minimal loop poll/update/window-state/decision order in `Source/Application/Private/FApplicationLoop.cpp`
- [x] T049 [US3] Extend `FWindow` with latest resize, minimized, drawable, and presentation-paused state handling in `Source/Application/Private/FWindow.cpp`
- [x] T050 [P] [US3] Extend `FWindowEvent` handling for resize coalescing, minimized, restored, focus, and close events in `Source/Application/Private/FWindowEvent.cpp`
- [x] T051 [P] [US3] Extend headless driver with resize, minimize, restore, focus, and close event playback scenarios in `Source/Application/Private/FHeadlessWindowDriver.cpp`
- [x] T052 [US3] Integrate `FInputManager` with `FApplicationLoop` frame polling in `Source/Application/Private/FInputManager.cpp`
- [x] T053 [P] [US3] Add stable loop and presentation-paused diagnostics in `Source/Application/Private/FApplicationDiagnostics.cpp`
- [x] T054 [US3] Export `FApplicationLoop` through `Source/Application/Public/Application/ApplicationMinimal.h`
- [x] T055 [US3] Validate User Story 3 loop and resize tests pass through the Application test suite in `Tests/ApplicationWindowInputTests.cpp`

**Checkpoint**: User Stories 1, 2, and 3 are independently functional and testable.

---

## Phase 6: User Story 4 - Fail Safely on Unsupported Runtime Conditions (Priority: P3)

**Goal**: Report clear diagnostics for invalid configurations, unavailable display/dependency, unsupported mode changes, unknown input identifiers, and out-of-scope behavior without crashes or partial state corruption.

**Independent Test**: Request invalid window configurations, unsupported display modes, unknown input identifiers, polling before/after lifecycle boundaries, and no-display real-window creation; verify diagnostics and safe fallback behavior.

### Tests for User Story 4

- [x] T056 [US4] Add invalid lifecycle call tests for poll-before-create, poll-after-destroy, and double-destroy in `Tests/ApplicationWindowInputTests.cpp`
- [x] T057 [US4] Add successful windowed/fullscreen-style mode switch tests plus unsupported fullscreen-style mode preservation tests in `Tests/ApplicationWindowInputTests.cpp`
- [x] T058 [US4] Add no-display and dependency-unavailable real-window diagnostic tests in `Tests/ApplicationWindowInputTests.cpp`
- [x] T059 [US4] Add deterministic debug dump stability test across 20 repeated headless runs in `Tests/ApplicationWindowInputTests.cpp`

### Implementation for User Story 4

- [x] T060 [P] [US4] Implement runtime availability and unsupported mode result mapping in `Source/Application/Private/FWindowDriver.h`
- [x] T061 [US4] Implement successful display-mode transitions, fullscreen-style mode failure preservation, safe dimension-boundary handling, and lifecycle boundary diagnostics in `Source/Application/Private/FWindow.cpp`
- [x] T062 [P] [US4] Implement no-display and dependency-unavailable reporting in `Source/Application/Private/FGlfwWindowDriver.cpp`
- [x] T063 [P] [US4] Implement deterministic Application debug dump generation in `Source/Application/Private/FApplicationDiagnostics.cpp`
- [x] T064 [US4] Ensure safe empty input state diagnostics before create and after destroy in `Source/Application/Private/FInputManager.cpp`
- [x] T065 [US4] Validate User Story 4 failure-mode tests pass through the Application test suite in `Tests/ApplicationWindowInputTests.cpp`

**Checkpoint**: All user stories are independently functional and testable.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Verification, documentation alignment, and architectural boundary checks across all stories.

- [x] T066 [P] Update `specs/016-window-input-system/quickstart.md` with actual verification results after implementation
- [x] T067 [P] Update `specs/016-window-input-system/spec.md` status and implementation notes after all tasks complete
- [x] T068 Run public Application boundary check from quickstart and resolve any public dependency leaks in `Source/Application/Public/Application`
- [x] T069 Run `conda run -n godot scons` locally and record outcome in `specs/016-window-input-system/quickstart.md`
- [x] T070 Run the local platform `StonerTest` executable, such as `Build/Mac/Debug/Tests/StonerTest` on macOS, and record outcome in `specs/016-window-input-system/quickstart.md`
- [x] T071 Create or update `.github/workflows/ci.yml` with a Windows/macOS/Linux GitHub Actions or equivalent CI matrix that builds with SCons and runs deterministic headless tests while keeping real-window smoke optional or unavailable-safe
- [x] T072 Validate the CI workflow includes `ubuntu-latest`, `macos-latest`, and `windows-latest`, uses platform-appropriate test executable paths, and records the expected matrix behavior in `specs/016-window-input-system/quickstart.md`; if any platform automation is temporarily unavailable, document the fallback manual command and follow-up task
- [x] T073 Review UE5-style naming, one-primary-window scope, and no text/gamepad/multi-window leakage in `Source/Application/Public/Application`
- [x] T074 Review deterministic diagnostics and debug dumps for pointer/native-handle leakage in `Source/Application/Private/FApplicationDiagnostics.cpp`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion - blocks all user stories
- **User Stories (Phase 3+)**: All depend on Foundational completion
- **Polish (Phase 7)**: Depends on all desired user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Foundational - no dependency on other stories
- **User Story 2 (P1)**: Can start after Foundational for key, mouse, pointer, and unknown-input semantics; focus-loss validation depends on the US1 window focus/lifecycle state or an equivalent focus-state seam landed in Foundation
- **User Story 3 (P2)**: Can start after Foundational, but final loop integration benefits from US1 window lifecycle and US2 input manager behavior
- **User Story 4 (P3)**: Can start after Foundational, but validates failure paths across US1-US3 behavior

### Within Each User Story

- Tests are written before implementation tasks.
- Public contracts before private implementation.
- Private implementation before story validation task.
- Story complete before advancing to the next priority checkpoint.

### Parallel Opportunities

- Setup scaffold tasks T002 and T003 can run in parallel.
- Foundational public type tasks T006-T009, T011, T013, and T014 can run in parallel.
- User story test tasks are intentionally sequential because they edit `Tests/ApplicationWindowInputTests.cpp`.
- Public enum/helper tasks T034 and T035 can run in parallel.
- Final documentation updates T066 and T067 can run in parallel.
- CI workflow task T071 can proceed after deterministic headless test commands are stable, then T072 validates the matrix definition.

---

## Parallel Example: User Story 1

```text
Task: "Define public primary window contract and lifecycle queries in Source/Application/Public/Application/FWindow.h"
Task: "Implement headless create, close-request, destroy, and lifecycle event playback in Source/Application/Private/FHeadlessWindowDriver.cpp"
Task: "Implement real-window adapter create/close/destroy smoke path and dependency/display unavailable results in Source/Application/Private/FGlfwWindowDriver.cpp"
Task: "Add stable window lifecycle diagnostic codes and messages in Source/Application/Private/FApplicationDiagnostics.cpp"
```

## Parallel Example: User Story 2

```text
Task: "Complete physical keyboard vocabulary and helper names in Source/Application/Public/Application/EKey.h"
Task: "Complete physical mouse-button vocabulary and helper names in Source/Application/Public/Application/EMouseButton.h"
```

## Parallel Example: User Story 3

```text
Task: "Define public loop configuration, callback, and decision types in Source/Application/Public/Application/FApplicationLoop.h"
Task: "Extend FWindowEvent handling for resize coalescing, minimized, restored, focus, and close events in Source/Application/Private/FWindowEvent.cpp"
Task: "Extend headless driver with resize, minimize, restore, focus, and close event playback scenarios in Source/Application/Private/FHeadlessWindowDriver.cpp"
Task: "Add stable loop and presentation-paused diagnostics in Source/Application/Private/FApplicationDiagnostics.cpp"
```

## Parallel Example: User Story 4

```text
Task: "Implement runtime availability and unsupported mode result mapping in Source/Application/Private/FWindowDriver.h"
Task: "Implement no-display and dependency-unavailable reporting in Source/Application/Private/FGlfwWindowDriver.cpp"
Task: "Implement deterministic Application debug dump generation in Source/Application/Private/FApplicationDiagnostics.cpp"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup.
2. Complete Phase 2: Foundational.
3. Complete Phase 3: User Story 1.
4. Stop and validate primary window lifecycle independently through headless tests and optional real-window smoke.

### Incremental Delivery

1. Complete Setup + Foundational -> shared contracts and driver seam ready.
2. Add User Story 1 -> primary window lifecycle MVP.
3. Add User Story 2 -> deterministic physical input state.
4. Add User Story 3 -> application loop, resize, and presentation-paused semantics.
5. Add User Story 4 -> failure diagnostics, display-mode success/failure behavior, unavailable runtime, and debug dump stability.
6. Complete Polish -> quickstart verification, cross-platform CI matrix, and boundary checks.

### Team Parallel Strategy

With multiple developers:

1. Team completes Setup + Foundational together.
2. Developer A implements US1 lifecycle.
3. Developer B implements US2 input state after foundational types land.
4. Developer C writes US3 loop tests while US1/US2 implementation stabilizes.
5. Developer D writes US4 failure-mode tests and diagnostics once shared diagnostics are available.

---

## Notes

- [P] tasks use different files or clearly separable sections and have no dependency on incomplete tasks.
- [US1]-[US4] labels map to user stories in `specs/016-window-input-system/spec.md`.
- Keep public Application headers free of GLFW, native handles, graphics API, swapchain, and renderer presentation types.
- Real-window smoke behavior must skip or report unavailable deterministically when display access or dependency support is absent.
- Commit after each task or logical group using the conventional commit style documented in `AGENTS.md`.
