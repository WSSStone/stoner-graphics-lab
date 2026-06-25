# Tasks: RHI Core Interfaces

**Input**: Design documents from `/specs/007-rhi-core-interfaces/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/rhi-core-api.md, quickstart.md

**Tests**: Test tasks are included because the feature specification requires mock-based lifecycle-state matrix coverage and negative-path validation for every public RHI core contract.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Path Conventions

- RHI public headers: `Source/RHI/Public/RHI/`
- RHI private sources: `Source/RHI/Private/`
- Tests: `Tests/`
- Feature documentation: `specs/007-rhi-core-interfaces/`

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create the RHI core test harness and public header shells without implementing behavior yet.

- [X] T001 Create `Tests/RHICoreTests.h` declaring `FRHICoreTestResult` and `RunRHICoreTests()`
- [X] T002 Create `Tests/RHICoreTests.cpp` with a compiling empty RHI core test harness returning zero failures
- [X] T003 Update `Tests/Main.cpp` to include `RHICoreTests.h`, call `RunRHICoreTests()`, and include its failure count in the process exit code
- [X] T004 [P] Create public header shell `Source/RHI/Public/RHI/ERHIResult.h` with result/status enum placeholder
- [X] T005 [P] Create public header shells `Source/RHI/Public/RHI/ERHIQueueType.h` and `Source/RHI/Public/RHI/ERHIFormat.h` with enum placeholders
- [X] T006 [P] Create public header shell `Source/RHI/Public/RHI/FRHIDeviceCapabilities.h` with struct placeholder
- [X] T007 [P] Create public interface header shells `Source/RHI/Public/RHI/IRHIDevice.h`, `Source/RHI/Public/RHI/IRHICommandBuffer.h`, and `Source/RHI/Public/RHI/IRHICommandQueue.h`
- [X] T008 [P] Create public interface header shells `Source/RHI/Public/RHI/IRHIFence.h`, `Source/RHI/Public/RHI/IRHISemaphore.h`, and `Source/RHI/Public/RHI/IRHISwapchain.h`
- [X] T009 Update `Source/RHI/Public/RHI/RHIMinimal.h` to include all new RHI core public headers
- [X] T010 Run `conda run -n godot scons` from the repository root and fix scaffold build errors in `Source/RHI/Public/RHI/` or `Tests/`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Establish shared result/status values, enum identities, capability data, and mock test utilities needed by all user stories.

**CRITICAL**: No user story work can begin until this phase is complete.

- [X] T011 Implement `ERHIResult` values Success, InvalidState, Unsupported, Timeout, NotReady, ResizeRequired, Unavailable, and Failed in `Source/RHI/Public/RHI/ERHIResult.h`
- [X] T012 Implement `ERHIQueueType` values Graphics, Compute, Transfer, and Present in `Source/RHI/Public/RHI/ERHIQueueType.h`
- [X] T013 Implement `ERHIFormat` unknown, common color, depth, stencil, and depth-stencil identities in `Source/RHI/Public/RHI/ERHIFormat.h`
- [X] T014 Implement `FRHIDeviceCapabilities` fields for supported queue types, feature flags, limits, and supported formats in `Source/RHI/Public/RHI/FRHIDeviceCapabilities.h`
- [X] T015 Add shared test assertion helpers and result counters for RHI core tests in `Tests/RHICoreTests.cpp`
- [X] T016 Add mock symbolic command representation, lifecycle enum helpers, and queue compatibility helpers in `Tests/RHICoreTests.cpp`
- [X] T017 Add header isolation smoke checks for `RHI/RHIMinimal.h` in `Tests/RHICoreTests.cpp`
- [X] T018 Run `conda run -n godot scons` from the repository root and fix foundational build errors in `Source/RHI/Public/RHI/` or `Tests/RHICoreTests.cpp`

**Checkpoint**: Foundation ready - user story implementation can now begin.

---

## Phase 3: User Story 1 - Query Rendering Device Capabilities Uniformly (Priority: P1) MVP

**Goal**: Developers can query device lifecycle, queue support, capabilities, and device-owned creation behavior through backend-neutral RHI contracts.

**Independent Test**: Use a mock rendering device to query deterministic capabilities, verify supported and unsupported queues, validate safe shutdown-state behavior, and exercise device-owned object creation/rejection without any real graphics API.

### Tests for User Story 1

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T019 [P] [US1] Add failing `ERHIResult`, `ERHIQueueType`, `ERHIFormat`, and `FRHIDeviceCapabilities` value verification in `Tests/RHICoreTests.cpp`
- [X] T020 [US1] Add failing `IRHIDevice` lifecycle and capability query verification in `Tests/RHICoreTests.cpp`
- [X] T021 [US1] Add failing supported and unsupported queue capability verification in `Tests/RHICoreTests.cpp`
- [X] T022 [US1] Add failing device-owned object creation and shutdown rejection verification in `Tests/RHICoreTests.cpp`
- [X] T023 [US1] Add failing device lifecycle-state matrix negative-path verification in `Tests/RHICoreTests.cpp`

### Implementation for User Story 1

- [X] T024 [US1] Declare `IRHIDevice` lifecycle, capabilities, and device-owned factory methods in `Source/RHI/Public/RHI/IRHIDevice.h`
- [X] T025 [US1] Declare minimal forward references and ownership-safe pointer aliases for RHI core object interfaces in `Source/RHI/Public/RHI/IRHIDevice.h`
- [X] T026 [US1] Implement mock rendering device lifecycle and deterministic capability behavior in `Tests/RHICoreTests.cpp`
- [X] T027 [US1] Implement mock device queue support, unsupported queue rejection, and shutdown-state rejection in `Tests/RHICoreTests.cpp`
- [X] T028 [US1] Implement mock device-owned creation stubs for queues, command buffers, fences, semaphores, and swapchains in `Tests/RHICoreTests.cpp`
- [X] T029 [US1] Run `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`, then fix US1 failures in `Source/RHI/Public/RHI/` or `Tests/RHICoreTests.cpp`

**Checkpoint**: User Story 1 is independently functional and testable as the MVP.

---

## Phase 4: User Story 2 - Record and Submit Work Through Core RHI Flow (Priority: P1)

**Goal**: Developers can record symbolic draw, dispatch, and barrier commands, complete command buffers, submit them to compatible queues, and observe idle/completion behavior through RHI contracts.

**Independent Test**: Use mock command buffers and queues to exercise the full lifecycle matrix, symbolic command ordering, compatible submissions, incompatible submissions, and invalid lifecycle transitions.

### Tests for User Story 2

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T030 [US2] Add failing `IRHICommandBuffer` begin/end/reset lifecycle matrix verification in `Tests/RHICoreTests.cpp`
- [X] T031 [US2] Add failing symbolic draw, dispatch, and barrier command ordering verification in `Tests/RHICoreTests.cpp`
- [X] T032 [US2] Add failing `IRHICommandQueue` compatible submit and wait-idle verification in `Tests/RHICoreTests.cpp`
- [X] T033 [US2] Add failing negative-path verification for double begin, end without begin, record after end, submit while recording, incomplete submit, and incompatible queue submit in `Tests/RHICoreTests.cpp`
- [X] T034 [US2] Add failing renderer-facing smoke flow verification that records, completes, submits, and observes a mock frame without resources or pipelines in `Tests/RHICoreTests.cpp`

### Implementation for User Story 2

- [X] T035 [US2] Declare command buffer lifecycle, symbolic draw/dispatch/barrier recording, reset, and state query APIs in `Source/RHI/Public/RHI/IRHICommandBuffer.h`
- [X] T036 [US2] Declare command queue type query, submit, and wait-idle APIs in `Source/RHI/Public/RHI/IRHICommandQueue.h`
- [X] T037 [US2] Implement mock command buffer lifecycle state machine and explicit result/status returns in `Tests/RHICoreTests.cpp`
- [X] T038 [US2] Implement mock symbolic command storage and ordering checks in `Tests/RHICoreTests.cpp`
- [X] T039 [US2] Implement mock command queue submission, compatibility checks, submitted sequence tracking, and wait-idle behavior in `Tests/RHICoreTests.cpp`
- [X] T040 [US2] Integrate mock device-created command buffers and queues with US2 lifecycle behavior in `Tests/RHICoreTests.cpp`
- [X] T041 [US2] Run `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`, then fix US2 failures in `Source/RHI/Public/RHI/` or `Tests/RHICoreTests.cpp`

**Checkpoint**: User Stories 1 and 2 both work independently and together as the RHI core MVP.

---

## Phase 5: User Story 3 - Coordinate Work With Synchronization Primitives (Priority: P2)

**Goal**: Developers can represent fence and semaphore synchronization through backend-neutral RHI contracts and verify deterministic signal, wait, reset, and ordering behavior.

**Independent Test**: Use mock fences and semaphores to transition through unsignaled, signaled, waited, reset, and consumed states while queues observe those objects during mock submission.

### Tests for User Story 3

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T042 [US3] Add failing `IRHIFence` signaled, unsignaled, wait, timeout/not-ready, and reset state matrix verification in `Tests/RHICoreTests.cpp`
- [X] T043 [US3] Add failing `IRHISemaphore` unsignaled, signaled, consumed, and invalid-use verification in `Tests/RHICoreTests.cpp`
- [X] T044 [US3] Add failing queue submission verification that signals fences and observes semaphore dependencies without backend handles in `Tests/RHICoreTests.cpp`

### Implementation for User Story 3

- [X] T045 [US3] Declare fence status query, wait, signal/mock-completion, and reset APIs in `Source/RHI/Public/RHI/IRHIFence.h`
- [X] T046 [US3] Declare semaphore status query, signal, consume, and validity APIs in `Source/RHI/Public/RHI/IRHISemaphore.h`
- [X] T047 [US3] Implement mock fence state transitions and explicit result/status returns in `Tests/RHICoreTests.cpp`
- [X] T048 [US3] Implement mock semaphore state transitions and invalid-use reporting in `Tests/RHICoreTests.cpp`
- [X] T049 [US3] Integrate mock queue submission with fence signaling and semaphore dependency observation in `Tests/RHICoreTests.cpp`
- [X] T050 [US3] Run `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`, then fix US3 failures in `Source/RHI/Public/RHI/` or `Tests/RHICoreTests.cpp`

**Checkpoint**: Synchronization behavior is usable independently after device and queue contracts.

---

## Phase 6: User Story 4 - Present Frames Through a Swapchain Contract (Priority: P2)

**Goal**: Developers can model headless/mock frame acquisition, presentation, unavailable states, invalid states, and resize-required behavior without a native window or graphics backend surface.

**Independent Test**: Use a headless mock swapchain to acquire frames, present acquired frames, reject invalid presentation order, and report resize-required/unavailable statuses under controlled conditions.

### Tests for User Story 4

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T051 [US4] Add failing `IRHISwapchain` acquire and present success flow verification in `Tests/RHICoreTests.cpp`
- [X] T052 [US4] Add failing invalid present without acquire and repeated present negative-path verification in `Tests/RHICoreTests.cpp`
- [X] T053 [US4] Add failing resize-required, unavailable, frame count, and current frame behavior verification in `Tests/RHICoreTests.cpp`
- [X] T054 [US4] Add failing verification that swapchain tests require no native window, platform surface, graphics API, Backend, Renderer, or Application dependency in `Tests/RHICoreTests.cpp`

### Implementation for User Story 4

- [X] T055 [US4] Declare headless swapchain frame count, acquire, present, resize-required, unavailable, and state query APIs in `Source/RHI/Public/RHI/IRHISwapchain.h`
- [X] T056 [US4] Implement mock swapchain acquire/present state machine and frame index tracking in `Tests/RHICoreTests.cpp`
- [X] T057 [US4] Implement mock swapchain resize-required, unavailable, and invalid-state result/status behavior in `Tests/RHICoreTests.cpp`
- [X] T058 [US4] Integrate mock device-owned swapchain creation with headless swapchain behavior in `Tests/RHICoreTests.cpp`
- [X] T059 [US4] Run `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`, then fix US4 failures in `Source/RHI/Public/RHI/` or `Tests/RHICoreTests.cpp`

**Checkpoint**: All user stories are independently functional and collectively validate the RHI core contracts.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Verify contract coverage, architecture isolation, naming, quickstart readiness, and roadmap state.

- [X] T060 Verify `specs/007-rhi-core-interfaces/contracts/rhi-core-api.md` is satisfied by public headers in `Source/RHI/Public/RHI/`
- [X] T061 Verify no public RHI core header includes Backend, Renderer, Application, graphics API, native windowing, or platform surface headers in `Source/RHI/Public/RHI/`
- [X] T062 Verify public names follow UE5-style `I*`, `F*`, and `E*` naming conventions in `Source/RHI/Public/RHI/`
- [X] T063 Review `Tests/RHICoreTests.cpp` to ensure every public RHI core contract has lifecycle-state matrix and negative-path coverage
- [X] T064 Run the quickstart build and verification flow from `specs/007-rhi-core-interfaces/quickstart.md`
- [X] T065 Run `conda run -n godot scons` from the repository root and confirm `Build/<Platform>/Debug/Tests/StonerTest` exits with code 0
- [X] T066 Update `doc/roadmap.md` Phase 006 status only after implementation and verification are complete
- [X] T067 Review `specs/007-rhi-core-interfaces/tasks.md` for completed task checkboxes and consistency before closing the feature

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion - BLOCKS all user stories
- **User Story 1 (Phase 3)**: Depends on Foundational - MVP
- **User Story 2 (Phase 4)**: Depends on Foundational and benefits from US1 mock device ownership, but command buffer and queue contracts remain independently testable
- **User Story 3 (Phase 5)**: Depends on Foundational and queue submission behavior from US2 for full integration tests
- **User Story 4 (Phase 6)**: Depends on Foundational and device ownership from US1 for full creation tests
- **Polish (Phase 7)**: Depends on all desired user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: No dependency on other stories; recommended MVP because it establishes device ownership and capability contracts
- **User Story 2 (P1)**: Can start after Foundational; integrates best after US1 but remains testable with direct mocks
- **User Story 3 (P2)**: Can start after Foundational; full queue integration depends on US2 submission behavior
- **User Story 4 (P2)**: Can start after Foundational; full device creation integration depends on US1 device ownership behavior

### Within Each User Story

- Tests MUST be written and fail before implementation
- Public header declarations before mock behavior implementation
- Result/status values before lifecycle matrices
- Device-owned creation before story integration through `IRHIDevice`
- Story complete before moving to next priority in solo-agent mode
- Run `conda run -n godot scons` and `StonerTest` at each story checkpoint

### Parallel Opportunities

- T004, T005, T006, T007, and T008 can run in parallel during Setup
- T011, T012, and T013 can run in parallel during Foundational
- Tests inside a story can be drafted before implementation and many are independent within `Tests/RHICoreTests.cpp`, but coordinate same-file edits carefully
- Interface declarations in separate headers can run in parallel after Foundational
- User Stories 3 and 4 can proceed in parallel after US1/US2 integration points are stable
- Polish checks T060, T061, T062, and T063 can run in parallel after implementation

---

## Parallel Example: User Story 1

```bash
Task: "Add failing ERHIResult, ERHIQueueType, ERHIFormat, and FRHIDeviceCapabilities value verification in Tests/RHICoreTests.cpp"
Task: "Declare IRHIDevice lifecycle, capabilities, and device-owned factory methods in Source/RHI/Public/RHI/IRHIDevice.h"
```

---

## Parallel Example: User Story 2

```bash
Task: "Declare command buffer lifecycle, symbolic draw/dispatch/barrier recording, reset, and state query APIs in Source/RHI/Public/RHI/IRHICommandBuffer.h"
Task: "Declare command queue type query, submit, and wait-idle APIs in Source/RHI/Public/RHI/IRHICommandQueue.h"
```

---

## Parallel Example: User Story 3

```bash
Task: "Declare fence status query, wait, signal/mock-completion, and reset APIs in Source/RHI/Public/RHI/IRHIFence.h"
Task: "Declare semaphore status query, signal, consume, and validity APIs in Source/RHI/Public/RHI/IRHISemaphore.h"
```

---

## Parallel Example: User Story 4

```bash
Task: "Add failing IRHISwapchain acquire and present success flow verification in Tests/RHICoreTests.cpp"
Task: "Declare headless swapchain frame count, acquire, present, resize-required, unavailable, and state query APIs in Source/RHI/Public/RHI/IRHISwapchain.h"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational
3. Complete Phase 3: User Story 1
4. Stop and validate device lifecycle, capabilities, queue support, and device-owned creation behavior independently
5. Confirm `conda run -n godot scons` and `Build/<Platform>/Debug/Tests/StonerTest` pass

### Incremental Delivery

1. Setup + Foundational: RHI result/status, queue/format identities, capabilities, test harness
2. US1: Device lifecycle, capabilities, and device-owned creation MVP
3. US2: Command buffer and queue recording/submission flow
4. US3: Fence and semaphore synchronization contracts
5. US4: Headless swapchain presentation contract
6. Polish: contract coverage, include isolation, naming, quickstart, roadmap update

### Parallel Team Strategy

With multiple developers:

1. Team completes Setup + Foundational together
2. Developer A: US1 device/capability contracts and mock ownership
3. Developer B: US2 command buffer and queue contracts after foundational headers settle
4. Developer C: US3 synchronization contracts after queue API shape is stable
5. Developer D: US4 headless swapchain contract after device-owned creation shape is stable

---

## Notes

- [P] tasks = different files, no dependencies
- [US1]-[US4] labels map tasks to specific user stories for traceability
- Tests are included because the spec requires lifecycle-state matrix and negative-path coverage
- Public RHI headers must remain backend-neutral and must not include graphics API headers
- Command recording is symbolic in this feature; concrete resources, descriptors, shaders, and pipelines belong to the next RHI feature
- Swapchain behavior is headless/mockable in this feature; native windows and backend surfaces belong to later phases
