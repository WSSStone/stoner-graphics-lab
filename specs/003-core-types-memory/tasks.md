# Tasks: Core Foundation Types & Memory

**Input**: Design documents from `/specs/003-core-types-memory/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/core-foundation-api.md, quickstart.md

**Tests**: Required by the feature specification and roadmap. Test tasks are listed before their corresponding implementation tasks.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1, US2, US3)
- Each task includes exact file paths

## Path Conventions

- **Core public headers**: `Source/Core/Public/Core/`
- **Core implementation**: `Source/Core/Private/`
- **Tests**: `Tests/`
- **Feature docs**: `specs/003-core-types-memory/`

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Prepare the Core header and test harness locations used by every story.

- [X] T001 Create or verify Core public/private directories at `Source/Core/Public/Core/` and `Source/Core/Private/`
- [X] T002 Create `Tests/CoreFoundationTests.h` with a `RunCoreFoundationTests()` declaration and shared test result type
- [X] T003 Create `Tests/CoreFoundationTests.cpp` with a minimal Core foundation test harness scaffold that compiles but contains no passing feature assertions yet
- [X] T004 Update `Tests/Main.cpp` to call `RunCoreFoundationTests()` and return nonzero when Core foundation verification fails
- [X] T005 Verify `Source/Core/SConscript` discovers implementation files under `Source/Core/Private/`
- [X] T006 Verify `Tests/SConscript` discovers new test files under `Tests/`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Establish shared public API conventions before user story implementation.

**CRITICAL**: No user story work should begin until this phase is complete.

- [X] T007 Add Core namespace and public header style conventions to `Source/Core/Public/Core/CoreMinimal.h`
- [X] T008 Create empty public header shells with `#pragma once` and `namespace Stoner::Core` in `Source/Core/Public/Core/FPlatformTypes.h`, `Source/Core/Public/Core/FString.h`, `Source/Core/Public/Core/FName.h`, `Source/Core/Public/Core/TSharedPtr.h`, `Source/Core/Public/Core/TUniquePtr.h`, `Source/Core/Public/Core/TArray.h`, `Source/Core/Public/Core/TMap.h`, and `Source/Core/Public/Core/FMemory.h`
- [X] T009 Create `Source/Core/Private/FMemory.cpp` with the include for `Core/FMemory.h` and an empty `Stoner::Core` namespace block
- [X] T010 Run `scons` from the repository root and fix any scaffold build errors in `Source/Core/Public/Core/CoreMinimal.h`, `Source/Core/Private/FMemory.cpp`, `Tests/CoreFoundationTests.cpp`, or `Tests/Main.cpp`

**Checkpoint**: Shared scaffolding builds, and user story test-first work can begin.

---

## Phase 3: User Story 1 - Use Stable Core Types Across Engine Layers (Priority: P1) MVP

**Goal**: Developers can use stable Core type, string, name, ownership, and container vocabulary without inventing local substitutes.

**Independent Test**: Build and run Core-only verification that declares fixed-width types, strings, names, ownership pointers, arrays, and maps without depending on higher engine layers.

### Tests for User Story 1

- [X] T011 [US1] Add failing fixed-width type size and pointer-size verification cases in `Tests/CoreFoundationTests.cpp`
- [X] T012 [US1] Add failing `FString` default, text construction, copy, move, comparison, length, and empty verification cases in `Tests/CoreFoundationTests.cpp`
- [X] T013 [US1] Add failing `FName` empty, duplicate text equality, different text inequality, and collision-safe equality verification cases in `Tests/CoreFoundationTests.cpp`
- [X] T014 [US1] Add failing `TSharedPtr<T>` and `TUniquePtr<T>` null, copy or move ownership verification cases in `Tests/CoreFoundationTests.cpp`
- [X] T015 [US1] Add failing `TArray<T>` and `TMap<K, V>` empty, insert, retrieve, copy, and move verification cases in `Tests/CoreFoundationTests.cpp`

### Implementation for User Story 1

- [X] T016 [P] [US1] Implement fixed-width, character, boolean, size, and pointer-sized aliases in `Source/Core/Public/Core/FPlatformTypes.h`
- [X] T017 [P] [US1] Implement `FString` owning text value in `Source/Core/Public/Core/FString.h`
- [X] T018 [P] [US1] Implement `TSharedPtr<T>` shared ownership alias in `Source/Core/Public/Core/TSharedPtr.h`
- [X] T019 [P] [US1] Implement `TUniquePtr<T>` unique ownership alias in `Source/Core/Public/Core/TUniquePtr.h`
- [X] T020 [P] [US1] Implement `TArray<T>` dynamic array alias in `Source/Core/Public/Core/TArray.h`
- [X] T021 [P] [US1] Implement `TMap<K, V>` key-value map alias in `Source/Core/Public/Core/TMap.h`
- [X] T022 [US1] Implement immutable `FName` with stored text, hash fast path, and text fallback equality in `Source/Core/Public/Core/FName.h`
- [X] T023 [US1] Update `Source/Core/Public/Core/CoreMinimal.h` to include `FPlatformTypes.h`, `FString.h`, `FName.h`, `TSharedPtr.h`, `TUniquePtr.h`, `TArray.h`, and `TMap.h`
- [X] T024 [US1] Run `scons` and `StonerTest` locally, then fix any US1 failures in `Source/Core/Public/Core/` or `Tests/CoreFoundationTests.cpp`

**Checkpoint**: User Story 1 should pass its Core vocabulary verification independently.

---

## Phase 4: User Story 2 - Verify Memory Behavior Safely (Priority: P2)

**Goal**: Developers can allocate, align, release, copy, move, set, and zero memory through deterministic Core memory utilities.

**Independent Test**: Exercise zero-size, small, large, aligned, invalid-alignment, and byte-operation cases and confirm cleanup is safe.

### Tests for User Story 2

- [X] T025 [US2] Add failing `FMemory::Allocate` and `FMemory::Deallocate` verification cases for zero-size, small, and large allocations in `Tests/CoreFoundationTests.cpp`
- [X] T026 [US2] Add failing `FMemory::AllocateAligned` and aligned deallocation verification cases for valid and invalid alignments in `Tests/CoreFoundationTests.cpp`
- [X] T027 [US2] Add failing `FMemory::Copy`, `FMemory::Move`, `FMemory::Set`, and `FMemory::Zero` byte-range verification cases in `Tests/CoreFoundationTests.cpp`

### Implementation for User Story 2

- [X] T028 [US2] Declare the full `FMemory` public API in `Source/Core/Public/Core/FMemory.h`
- [X] T029 [US2] Implement `FMemory` allocation, aligned allocation, deallocation, and aligned deallocation in `Source/Core/Private/FMemory.cpp`
- [X] T030 [US2] Implement `FMemory` copy, move, set, and zero byte operations in `Source/Core/Private/FMemory.cpp`
- [X] T031 [US2] Update `Source/Core/Public/Core/CoreMinimal.h` to include `FMemory.h`
- [X] T032 [US2] Run `scons` and `StonerTest` locally, then fix any US2 failures in `Source/Core/Public/Core/FMemory.h`, `Source/Core/Private/FMemory.cpp`, or `Tests/CoreFoundationTests.cpp`

**Checkpoint**: User Stories 1 and 2 should both pass without higher-layer dependencies.

---

## Phase 5: User Story 3 - Confirm Cross-Platform Consistency (Priority: P3)

**Goal**: Maintainers can verify that the Core foundation behaves consistently across Windows, macOS, and Linux.

**Independent Test**: Run the Core foundation verification suite on each supported platform and compare type sizes, alignment behavior, string/name behavior, and container behavior.

### Tests for User Story 3

- [X] T033 [US3] Add aggregate include verification for `Core/CoreMinimal.h` in `Tests/CoreFoundationTests.cpp`
- [X] T034 [US3] Add higher-layer isolation verification that Core tests include no RHI, Backend, Renderer, or Application headers in `Tests/CoreFoundationTests.cpp`
- [X] T035 [US3] Add cross-platform expectation output for type sizes, pointer sizes, and alignment cases in `Tests/CoreFoundationTests.cpp`

### Implementation for User Story 3

- [X] T036 [US3] Ensure `Source/Core/Public/Core/CoreMinimal.h` exposes every public Core foundation header required by `specs/003-core-types-memory/contracts/core-foundation-api.md`
- [X] T037 [US3] Update `specs/003-core-types-memory/quickstart.md` with any exact local build output path discovered while running `scons`
- [X] T038 [US3] Run `scons` and `StonerTest` locally, then fix any US3 consistency failures in `Source/Core/Public/Core/CoreMinimal.h` or `Tests/CoreFoundationTests.cpp`

**Checkpoint**: All user stories should pass locally and be ready for cross-platform verification.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final cleanup and acceptance validation across all stories.

- [X] T039 Verify public names follow UE5-style `F` and `T` prefixes in `Source/Core/Public/Core/`
- [X] T040 Verify no Core public header includes RHI, Backend, Renderer, Application, or graphics API headers in `Source/Core/Public/Core/`
- [X] T041 Verify `specs/003-core-types-memory/contracts/core-foundation-api.md` is satisfied by the implemented headers in `Source/Core/Public/Core/`
- [X] T042 Run the quickstart build and verification flow from `specs/003-core-types-memory/quickstart.md`
- [X] T043 Run `scons` from the repository root and confirm `Build/<Platform>/<Config>/Tests/StonerTest` exits with code 0
- [X] T044 Review `Tests/CoreFoundationTests.cpp` to ensure every primitive category from `specs/003-core-types-memory/spec.md` has normal, boundary, and invalid-input coverage where applicable

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion - blocks all user stories
- **User Story 1 (Phase 3)**: Depends on Foundational phase completion
- **User Story 2 (Phase 4)**: Depends on Foundational phase completion; can use US1 type aliases but remains focused on `FMemory`
- **User Story 3 (Phase 5)**: Depends on US1 and US2 completion because it validates the complete public Core foundation
- **Polish (Phase 6)**: Depends on all desired user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: MVP. Establishes stable Core vocabulary for types, strings, names, ownership, arrays, and maps.
- **User Story 2 (P2)**: Adds deterministic memory utility behavior and depends on the shared Core scaffolding.
- **User Story 3 (P3)**: Validates complete Core foundation consistency after US1 and US2 are implemented.

### Within Each User Story

- Tests must be written before matching implementation tasks.
- Public headers can be implemented in parallel when they touch different files.
- `FName.h` follows `FString.h` and `FPlatformTypes.h`.
- `CoreMinimal.h` aggregate updates follow the headers it includes.
- Build and verification tasks follow implementation tasks.

### Parallel Opportunities

- T016 through T021 can run in parallel after US1 tests are written because they modify different public headers.
- T018 through T021 can run in parallel with T017 after shared naming and namespace conventions are established.
- Polish verification tasks T039 through T041 can be split across reviewers after all story phases pass.

---

## Parallel Example: User Story 1

```bash
Task: "Implement fixed-width, character, boolean, size, and pointer-sized aliases in Source/Core/Public/Core/FPlatformTypes.h"
Task: "Implement FString owning text value in Source/Core/Public/Core/FString.h"
Task: "Implement TSharedPtr<T> shared ownership alias in Source/Core/Public/Core/TSharedPtr.h"
Task: "Implement TUniquePtr<T> unique ownership alias in Source/Core/Public/Core/TUniquePtr.h"
Task: "Implement TArray<T> dynamic array alias in Source/Core/Public/Core/TArray.h"
Task: "Implement TMap<K, V> key-value map alias in Source/Core/Public/Core/TMap.h"
```

---

## Parallel Example: User Story 2

No User Story 2 implementation tasks are marked parallel because `FMemory.h`, `FMemory.cpp`, and shared test coverage depend on a single public API shape and should be edited sequentially.

---

## Parallel Example: User Story 3

No User Story 3 implementation tasks are marked parallel because they validate the completed aggregate public API and quickstart flow.

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational scaffolding
3. Write US1 tests in `Tests/CoreFoundationTests.cpp`
4. Implement US1 public headers in `Source/Core/Public/Core/`
5. Stop and validate that the Core vocabulary can be used without higher-layer dependencies

### Incremental Delivery

1. Deliver US1 for stable Core vocabulary
2. Add US2 for deterministic memory utilities
3. Add US3 for aggregate include and cross-platform consistency validation
4. Finish with polish checks against the spec, contract, and quickstart

### Solo Agent Strategy

Work sequentially by task ID except for the explicitly marked US1 header implementation tasks. Run `scons` and `StonerTest` at each checkpoint before moving to the next story.

---

## Notes

- Keep all new Core public deliverables inside `namespace Stoner::Core`.
- Do not add dependencies outside the C++ standard library for this feature.
- Do not implement math types, logging, assertions, platform file/process APIs, or graphics API behavior.
- Mark tasks complete in this file only after the implementation and relevant verification for that task are done.
