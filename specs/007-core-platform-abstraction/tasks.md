# Tasks: Core Foundation - Platform Abstraction Layer

**Input**: Design documents from `/specs/007-core-platform-abstraction/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/core-platform-api.md, quickstart.md

**Tests**: Test tasks are included because the specification requires unit-test coverage for platform identity, platform information, high-resolution timing, file operations, dynamic module failure handling, and native window handle validity.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Path Conventions

- Core public headers: `Source/Core/Public/Core/`
- Core implementation files: `Source/Core/Private/`
- Verification files: `Tests/`
- Feature documentation: `specs/007-core-platform-abstraction/`

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create platform abstraction test harness and public header shells without implementing behavior yet.

- [X] T001 Create `Tests/CorePlatformTests.h` declaring `FCorePlatformTestResult` and `RunCorePlatformTests()`
- [X] T002 Create `Tests/CorePlatformTests.cpp` with a compiling empty Core platform test harness that returns zero failures
- [X] T003 Update `Tests/Main.cpp` to include `CorePlatformTests.h`, call `RunCorePlatformTests()`, and include its failure count in the process exit code
- [X] T004 [P] Create public header shell `Source/Core/Public/Core/SGPlatform.h` with include guard/pragmas and an empty platform macro section
- [X] T005 [P] Create public header shells `Source/Core/Public/Core/FPlatformMisc.h`, `Source/Core/Public/Core/FPlatformTime.h`, and `Source/Core/Public/Core/FPlatformWindow.h` with `namespace Stoner::Core`
- [X] T006 [P] Create public header shells `Source/Core/Public/Core/FPlatformFileSystem.h` and `Source/Core/Public/Core/FPlatformProcess.h` with `namespace Stoner::Core`
- [X] T007 [P] Create private source shells `Source/Core/Private/FPlatformMisc.cpp` and `Source/Core/Private/FPlatformTime.cpp` including their matching public headers
- [X] T008 [P] Create private source shells `Source/Core/Private/FPlatformFileSystem.cpp` and `Source/Core/Private/FPlatformProcess.cpp` including their matching public headers
- [X] T009 Run `scons` from the repository root and fix scaffold build errors in `Tests/CorePlatformTests.cpp`, `Tests/Main.cpp`, or `Source/Core/Public/Core/`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Establish shared platform identity, public API shape, and aggregate include behavior needed by all user stories.

**CRITICAL**: No user story work can begin until this phase is complete.

- [X] T010 Implement mutually exclusive `SG_PLATFORM_WINDOWS`, `SG_PLATFORM_MAC`, and `SG_PLATFORM_LINUX` macros in `Source/Core/Public/Core/SGPlatform.h`
- [X] T011 Add compile-time unsupported-platform failure behavior in `Source/Core/Public/Core/SGPlatform.h`
- [X] T012 Declare the full `FPlatformMisc` public API in `Source/Core/Public/Core/FPlatformMisc.h` per `specs/007-core-platform-abstraction/contracts/core-platform-api.md`
- [X] T013 Declare the full `FPlatformTime` public API in `Source/Core/Public/Core/FPlatformTime.h` per `specs/007-core-platform-abstraction/contracts/core-platform-api.md`
- [X] T014 Declare the full `FPlatformFileSystem` public API in `Source/Core/Public/Core/FPlatformFileSystem.h` per `specs/007-core-platform-abstraction/contracts/core-platform-api.md`
- [X] T015 Declare the full `FPlatformProcess` public API and opaque module handle type in `Source/Core/Public/Core/FPlatformProcess.h` per `specs/007-core-platform-abstraction/contracts/core-platform-api.md`
- [X] T016 Declare the full `FPlatformWindow` public API and opaque native handle representation in `Source/Core/Public/Core/FPlatformWindow.h` per `specs/007-core-platform-abstraction/contracts/core-platform-api.md`
- [X] T017 Update `Source/Core/Public/Core/CoreMinimal.h` to include `SGPlatform.h`, `FPlatformMisc.h`, `FPlatformTime.h`, `FPlatformFileSystem.h`, `FPlatformProcess.h`, and `FPlatformWindow.h`
- [X] T018 Run `scons` from the repository root and fix foundational build errors in `Source/Core/Public/Core/` or `Source/Core/Private/`

**Checkpoint**: Foundation ready - user story implementation can now begin.

---

## Phase 3: User Story 1 - Engine Developer Uses Platform Capabilities Uniformly (Priority: P1) MVP

**Goal**: Developers can query platform identity, platform information, and monotonic time from Core-only code without OS-specific call sites.

**Independent Test**: Query platform identity, OS name, CPU count, available memory, and 1,000 monotonic timestamps from `Tests/CorePlatformTests.cpp`; verify values are present, sensible, and stable.

### Tests for User Story 1

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T019 [US1] Add failing platform macro exclusivity verification in `Tests/CorePlatformTests.cpp`
- [X] T020 [US1] Add failing `FPlatformMisc::GetOSName()` and `FPlatformMisc::GetCPUCoreCount()` verification cases in `Tests/CorePlatformTests.cpp`
- [X] T021 [US1] Add failing `FPlatformMisc::GetAvailableMemoryBytes()` safe-result verification case in `Tests/CorePlatformTests.cpp`
- [X] T022 [US1] Add failing `FPlatformTime` monotonic 1,000-sample and duration conversion verification cases in `Tests/CorePlatformTests.cpp`
- [X] T023 [US1] Add failing aggregate include verification for `Core/CoreMinimal.h` exposing `SGPlatform`, `FPlatformMisc`, and `FPlatformTime` APIs in `Tests/CorePlatformTests.cpp`

### Implementation for User Story 1

- [X] T024 [US1] Implement `FPlatformMisc::GetOSName()` using `SG_PLATFORM_*` branches in `Source/Core/Private/FPlatformMisc.cpp`
- [X] T025 [US1] Implement `FPlatformMisc::GetCPUCoreCount()` with a portable fallback of at least 1 in `Source/Core/Private/FPlatformMisc.cpp`
- [X] T026 [US1] Implement `FPlatformMisc::GetAvailableMemoryBytes()` with Windows/macOS/Linux guarded behavior and safe unavailable result in `Source/Core/Private/FPlatformMisc.cpp`
- [X] T027 [US1] Implement `FPlatformTime` timestamp type, `Now()`, and elapsed conversion helpers in `Source/Core/Public/Core/FPlatformTime.h`
- [X] T028 [US1] Implement platform-backed monotonic timestamp capture in `Source/Core/Private/FPlatformTime.cpp`
- [X] T029 [US1] Run `scons` and `Build/Mac/Debug/Tests/StonerTest` locally, then fix US1 failures in `Source/Core/Public/Core/`, `Source/Core/Private/`, or `Tests/CorePlatformTests.cpp`

**Checkpoint**: User Story 1 is independently functional and testable as the MVP.

---

## Phase 4: User Story 2 - Engine Developer Performs Basic File Operations Portably (Priority: P1)

**Goal**: Developers can check existence, create nested directories recursively, write local files, and read payloads back byte-for-byte through Core APIs.

**Independent Test**: Create a nested temporary directory, write text and binary payloads up to 1 MB, read them back, and verify clear failures for missing or invalid paths.

### Tests for User Story 2

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T030 [US2] Add failing `FPlatformFileSystem::CreateDirectory()` recursive nested directory verification in `Tests/CorePlatformTests.cpp`
- [X] T031 [US2] Add failing `FPlatformFileSystem::Exists()` file and directory verification cases in `Tests/CorePlatformTests.cpp`
- [X] T032 [US2] Add failing `FPlatformFileSystem::WriteFile()` and `ReadFile()` text payload roundtrip verification in `Tests/CorePlatformTests.cpp`
- [X] T033 [US2] Add failing `FPlatformFileSystem::WriteFile()` and `ReadFile()` binary payload up to 1 MB verification in `Tests/CorePlatformTests.cpp`
- [X] T034 [US2] Add failing missing file, directory-as-file, path-with-spaces, non-ASCII path, and inaccessible path failure handling verification in `Tests/CorePlatformTests.cpp`

### Implementation for User Story 2

- [X] T035 [US2] Implement `FPlatformFileSystem::Exists()` for files and directories in `Source/Core/Private/FPlatformFileSystem.cpp`
- [X] T036 [US2] Implement recursive `FPlatformFileSystem::CreateDirectory()` including missing parent directories in `Source/Core/Private/FPlatformFileSystem.cpp`
- [X] T037 [US2] Implement byte-preserving `FPlatformFileSystem::WriteFile()` in `Source/Core/Private/FPlatformFileSystem.cpp`
- [X] T038 [US2] Implement byte-preserving `FPlatformFileSystem::ReadFile()` with clear missing/invalid path failure behavior in `Source/Core/Private/FPlatformFileSystem.cpp`
- [X] T039 [US2] Run `scons` and `Build/Mac/Debug/Tests/StonerTest` locally, then fix US2 failures in `Source/Core/Private/FPlatformFileSystem.cpp` or `Tests/CorePlatformTests.cpp`

**Checkpoint**: User Stories 1 and 2 both work independently.

---

## Phase 5: User Story 3 - Engine Developer Loads Optional Runtime Modules (Priority: P2)

**Goal**: Developers can load optional runtime modules by explicit path, resolve symbols, and release handles while missing modules and symbols fail safely.

**Independent Test**: Attempt missing-module and missing-symbol cases without crashing, and verify invalid handle release is a safe no-op.

### Tests for User Story 3

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T040 [US3] Add failing invalid dynamic module handle default and safe release verification in `Tests/CorePlatformTests.cpp`
- [X] T041 [US3] Add failing successful explicit-path dynamic module load, symbol lookup, and release verification for a platform-supported system module in `Tests/CorePlatformTests.cpp`
- [X] T042 [US3] Add failing missing explicit module path load failure verification in `Tests/CorePlatformTests.cpp`
- [X] T043 [US3] Add failing missing symbol lookup failure verification for a loaded or invalid module in `Tests/CorePlatformTests.cpp`
- [X] T044 [US3] Add failing explicit-path-only behavior verification for `FPlatformProcess` in `Tests/CorePlatformTests.cpp`

### Implementation for User Story 3

- [X] T045 [US3] Implement `FPlatformProcess::LoadDynamicModule()` explicit-path validation and Windows/macOS/Linux load branches in `Source/Core/Private/FPlatformProcess.cpp`
- [X] T046 [US3] Implement `FPlatformProcess::GetSymbol()` with successful lookup and recoverable missing-symbol behavior in `Source/Core/Private/FPlatformProcess.cpp`
- [X] T047 [US3] Implement `FPlatformProcess::FreeDynamicModule()` with valid release and invalid-handle no-op behavior in `Source/Core/Private/FPlatformProcess.cpp`
- [X] T048 [US3] Run `scons` and `Build/Mac/Debug/Tests/StonerTest` locally, then fix US3 failures in `Source/Core/Public/Core/FPlatformProcess.h`, `Source/Core/Private/FPlatformProcess.cpp`, or `Tests/CorePlatformTests.cpp`

**Checkpoint**: Dynamic module behavior is usable independently after foundational APIs.

---

## Phase 6: User Story 4 - Engine Developer Passes Native Window Handles Safely (Priority: P2)

**Goal**: Developers can represent empty and wrapped native window handles through Core without public OS, windowing framework, or graphics API includes.

**Independent Test**: Construct empty and wrapped handle values in Core tests, verify validity behavior, and verify public headers compile without higher-layer dependencies.

### Tests for User Story 4

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T049 [US4] Add failing empty `FPlatformWindow` invalid-but-safe verification in `Tests/CorePlatformTests.cpp`
- [X] T050 [US4] Add failing wrapped native handle validity and value preservation verification in `Tests/CorePlatformTests.cpp`
- [X] T051 [US4] Add failing Core-only public header isolation verification for `FPlatformWindow.h` in `Tests/CorePlatformTests.cpp`

### Implementation for User Story 4

- [X] T052 [US4] Implement empty and wrapped native handle construction in `Source/Core/Public/Core/FPlatformWindow.h`
- [X] T053 [US4] Implement `FPlatformWindow` validity, value retrieval, copy, and clear behavior in `Source/Core/Public/Core/FPlatformWindow.h`
- [X] T054 [US4] Review `Source/Core/Public/Core/FPlatformWindow.h` to ensure it includes no OS, windowing framework, RHI, Backend, Renderer, Application, or graphics API headers
- [X] T055 [US4] Run `scons` and `Build/Mac/Debug/Tests/StonerTest` locally, then fix US4 failures in `Source/Core/Public/Core/FPlatformWindow.h` or `Tests/CorePlatformTests.cpp`

**Checkpoint**: All user stories are independently functional.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Verify contract coverage, constitution compliance, and roadmap readiness.

- [X] T056 Verify `specs/007-core-platform-abstraction/contracts/core-platform-api.md` is satisfied by public headers in `Source/Core/Public/Core/`
- [X] T057 Verify no Core platform public header includes RHI, Backend, Renderer, Application, windowing framework, or graphics API headers in `Source/Core/Public/Core/`
- [X] T058 Verify public names follow UE5-style `FPlatform*` and `SG_PLATFORM_*` naming in `Source/Core/Public/Core/`
- [X] T059 Review `Tests/CorePlatformTests.cpp` to ensure every public API entry point from `specs/007-core-platform-abstraction/spec.md` has verification coverage
- [X] T060 Run the quickstart build and verification flow from `specs/007-core-platform-abstraction/quickstart.md`
- [X] T061 Run `scons` from the repository root and confirm `Build/<Platform>/<Config>/Tests/StonerTest` exits with code 0
- [X] T062 Update `doc/roadmap.md` Phase 005 status only after implementation and verification are complete
- [X] T063 Review `specs/007-core-platform-abstraction/tasks.md` for completed task checkboxes and consistency before closing the feature

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion - BLOCKS all user stories
- **User Story 1 (Phase 3)**: Depends on Foundational - MVP
- **User Story 2 (Phase 4)**: Depends on Foundational; can proceed independently of US1 after shared API shape exists
- **User Story 3 (Phase 5)**: Depends on Foundational; can proceed independently of US1/US2 after shared API shape exists
- **User Story 4 (Phase 6)**: Depends on Foundational; can proceed independently of US1/US2/US3 after shared API shape exists
- **Polish (Phase 7)**: Depends on all desired user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: No dependency on other stories; recommended MVP because it validates platform identity and timing foundation
- **User Story 2 (P1)**: No dependency on other stories; can start after Phase 2 but should avoid conflicting edits to `Tests/CorePlatformTests.cpp`
- **User Story 3 (P2)**: No dependency on other stories; can start after Phase 2 but should avoid conflicting edits to `Tests/CorePlatformTests.cpp`
- **User Story 4 (P2)**: No dependency on other stories; can start after Phase 2 but should avoid conflicting edits to `Tests/CorePlatformTests.cpp`

### Within Each User Story

- Tests MUST be written and fail before implementation
- Public header declarations before private `.cpp` implementations
- Platform-specific branches behind `SG_PLATFORM_*`
- Story complete before moving to next priority in solo-agent mode
- Run `scons` and `StonerTest` at each story checkpoint

### Parallel Opportunities

- T004, T005, T006, T007, and T008 can run in parallel during Setup
- After T010-T018, implementation files for `FPlatformMisc`, `FPlatformTime`, `FPlatformFileSystem`, `FPlatformProcess`, and `FPlatformWindow` can be worked on by separate developers
- User Stories 1-4 can proceed in parallel after Phase 2 if teams coordinate edits to `Tests/CorePlatformTests.cpp`
- Polish checks T056, T057, T058, and T059 can run in parallel after implementation is complete

---

## Parallel Example: User Story 1

```bash
Task: "Implement FPlatformMisc::GetOSName() using SG_PLATFORM_* branches in Source/Core/Private/FPlatformMisc.cpp"
Task: "Implement FPlatformTime timestamp type, Now(), and elapsed conversion helpers in Source/Core/Public/Core/FPlatformTime.h"
```

---

## Parallel Example: User Story 2

```bash
Task: "Implement recursive FPlatformFileSystem::CreateDirectory() including missing parent directories in Source/Core/Private/FPlatformFileSystem.cpp"
Task: "Add failing FPlatformFileSystem::WriteFile() and ReadFile() binary payload up to 1 MB verification in Tests/CorePlatformTests.cpp"
```

---

## Parallel Example: User Story 3

```bash
Task: "Implement FPlatformProcess::LoadDynamicModule() explicit-path validation and Windows/macOS/Linux load branches in Source/Core/Private/FPlatformProcess.cpp"
Task: "Add failing invalid dynamic module handle default and safe release verification in Tests/CorePlatformTests.cpp"
```

---

## Parallel Example: User Story 4

```bash
Task: "Implement empty and wrapped native handle construction in Source/Core/Public/Core/FPlatformWindow.h"
Task: "Add failing Core-only public header isolation verification for FPlatformWindow.h in Tests/CorePlatformTests.cpp"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational
3. Complete Phase 3: User Story 1
4. Stop and validate platform identity, platform info, monotonic timing, and aggregate includes
5. Use this MVP as the base for filesystem, dynamic module, and native handle work

### Incremental Delivery

1. Complete Setup + Foundational -> shared platform public API compiles
2. Add User Story 1 -> platform identity/info/time works
3. Add User Story 2 -> filesystem operations work
4. Add User Story 3 -> dynamic module handling works
5. Add User Story 4 -> native window handle representation works
6. Complete Polish -> contract, quickstart, roadmap, and cross-platform checks complete

### Solo Agent Strategy

1. Work strictly in task order T001-T063
2. Keep `Tests/CorePlatformTests.cpp` changes sequential to avoid self-conflicts
3. Run `scons` and `Build/Mac/Debug/Tests/StonerTest` after each user story checkpoint
4. Update `doc/roadmap.md` only after all verification passes

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to the user story from `spec.md`
- Each user story should be independently completable and testable after Phase 2
- Verify tests fail before implementing story behavior
- Keep all public platform headers Core-only and graphics-API-free
- Commit after each task or logical group
