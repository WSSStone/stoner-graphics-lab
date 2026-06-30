# Tasks: Vulkan Device & Swapchain Backend

**Input**: Design documents from `/specs/009-vulkan-device-swapchain/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/vulkan-device-swapchain-contract.md, quickstart.md

**Tests**: Test tasks are included because the feature specification requires deterministic success and negative coverage for backend initialization, adapter selection, queues, synchronization, swapchain behavior, unsupported paths, and shutdown.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Path Conventions

- Vulkan backend public headers: `Source/Backend/Vulkan/Public/VulkanRHI/`
- Vulkan backend private sources: `Source/Backend/Vulkan/Private/`
- Vulkan backend build script: `Source/Backend/Vulkan/SConscript`
- Tests: `Tests/`
- Feature documentation: `specs/009-vulkan-device-swapchain/`

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish source/test file shells, build detection scaffolding, and test entry points for the Vulkan backend slice without implementing behavior yet.

- [X] T001 [P] Create public header shell `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDiagnostics.h` for backend availability and validation diagnostic contracts
- [X] T002 [P] Create public header shell `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanInstance.h` for backend runtime initialization contracts
- [X] T003 [P] Create public header shell `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanPhysicalDevice.h` for adapter candidate and selection contracts
- [X] T004 [P] Create public header shell `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDevice.h` for RHI device implementation contracts
- [X] T005 [P] Create public header shell `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanQueue.h` for queue implementation contracts
- [X] T006 [P] Create public header shell `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanSurface.h` for Core platform-window-backed surface contracts
- [X] T007 [P] Create public header shell `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanSwapchain.h` for swapchain lifecycle contracts
- [X] T008 [P] Create public header shells `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanFence.h` and `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanSemaphore.h` for synchronization contracts
- [X] T009 [P] Create private source shells `Source/Backend/Vulkan/Private/FVulkanDiagnostics.cpp`, `Source/Backend/Vulkan/Private/FVulkanInstance.cpp`, and `Source/Backend/Vulkan/Private/FVulkanPhysicalDevice.cpp`
- [X] T010 [P] Create private source shells `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`, `Source/Backend/Vulkan/Private/FVulkanQueue.cpp`, and `Source/Backend/Vulkan/Private/FVulkanSurface.cpp`
- [X] T011 [P] Create private source shells `Source/Backend/Vulkan/Private/FVulkanSwapchain.cpp`, `Source/Backend/Vulkan/Private/FVulkanFence.cpp`, and `Source/Backend/Vulkan/Private/FVulkanSemaphore.cpp`
- [X] T012 Update aggregate backend header `Source/Backend/Vulkan/Public/VulkanRHI/VulkanDevice.h` to include the new Vulkan backend public headers
- [X] T013 Add Vulkan backend test header shell `Tests/VulkanBackendTests.h`
- [X] T014 Add Vulkan backend test source shell `Tests/VulkanBackendTests.cpp`
- [X] T015 Register Vulkan backend tests in `Tests/Main.cpp`
- [X] T016 Add Vulkan SDK/header/library detection placeholders and unsupported-build fallback variables in `Source/Backend/Vulkan/SConscript`
- [X] T017 Run `conda run -n godot scons` and fix scaffold build errors in `Source/Backend/Vulkan/` or `Tests/`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Implement shared backend availability, diagnostics, adapter candidate data, result mapping, and build/runtime guard rails used by every user story.

**CRITICAL**: No user story work can begin until this phase is complete.

- [X] T018 Implement backend availability enum and validation state enum in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDiagnostics.h`
- [X] T019 Implement diagnostic snapshot fields for runtime availability, validation availability, selected adapter reason, unsupported runtime reason, and presentation skip reason in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDiagnostics.h`
- [X] T020 Implement diagnostic helper functions for unsupported runtime, validation unavailable, selected adapter, and presentation skip in `Source/Backend/Vulkan/Private/FVulkanDiagnostics.cpp`
- [X] T021 Implement adapter candidate fields for identity, device type, gate result, queue support, presentation support, format support, score, and rejection reason in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanPhysicalDevice.h`
- [X] T022 Implement deterministic adapter scoring declarations in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanPhysicalDevice.h`
- [X] T023 Implement deterministic adapter scoring and required capability gate helpers in `Source/Backend/Vulkan/Private/FVulkanPhysicalDevice.cpp`
- [X] T024 Add test helper declarations for synthetic adapter candidates and diagnostic assertions in `Tests/VulkanBackendTests.h`
- [X] T025 Add test helper implementations for synthetic adapter candidates and diagnostic assertions in `Tests/VulkanBackendTests.cpp`
- [X] T026 Implement build-time Vulkan availability constants or fallback macros consumed by backend sources in `Source/Backend/Vulkan/SConscript`
- [X] T027 Update `Source/Backend/Vulkan/Private/VulkanDevice.cpp` to remove placeholder-only initialization and route through new backend initialization scaffolding
- [X] T028 Run `conda run -n godot scons` and fix foundational build errors in `Source/Backend/Vulkan/` or `Tests/VulkanBackendTests.cpp`

**Checkpoint**: Foundation ready - user story implementation can now begin.

---

## Phase 3: User Story 1 - Initialize a Usable Vulkan Backend Device (Priority: P1) MVP

**Goal**: Developers can initialize a headless Vulkan backend device, query RHI-visible capabilities, receive deterministic adapter selection diagnostics, and get explicit unsupported status when the runtime or compatible adapter is unavailable.

**Independent Test**: Request backend initialization without a presentation surface; supported environments produce an active RHI device and capabilities, while unsupported environments return explicit status and diagnostics without crashing.

### Tests for User Story 1

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T029 [P] [US1] Add failing test for supported or explicitly unsupported headless backend initialization in `Tests/VulkanBackendTests.cpp`
- [X] T030 [P] [US1] Add failing test for selected device capabilities exposure through RHI fields in `Tests/VulkanBackendTests.cpp`
- [X] T031 [P] [US1] Add failing test for no-compatible-adapter unsupported result in `Tests/VulkanBackendTests.cpp`
- [X] T032 [P] [US1] Add failing test for deterministic multi-adapter selection and rejected candidate diagnostics in `Tests/VulkanBackendTests.cpp`
- [X] T033 [P] [US1] Add failing test for optional validation unavailable diagnostics not failing initialization in `Tests/VulkanBackendTests.cpp`
- [X] T034 [US1] Add failing test that partial initialization failure leaves no usable backend device in `Tests/VulkanBackendTests.cpp`

### Implementation for User Story 1

- [X] T035 [P] [US1] Implement `FVulkanInstance` lifecycle fields and diagnostics query API in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanInstance.h`
- [X] T036 [US1] Implement runtime initialization, validation request handling, and unsupported-runtime fallback in `Source/Backend/Vulkan/Private/FVulkanInstance.cpp`
- [X] T037 [P] [US1] Implement `FVulkanPhysicalDevice` adapter enumeration result fields and selected-candidate query API in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanPhysicalDevice.h`
- [X] T038 [US1] Implement adapter candidate filtering, deterministic scoring, and selected adapter diagnostics in `Source/Backend/Vulkan/Private/FVulkanPhysicalDevice.cpp`
- [X] T039 [P] [US1] Implement `FVulkanDevice` RHI device class declaration with lifecycle, capabilities, diagnostics, and unsupported out-of-scope factory declarations in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDevice.h`
- [X] T040 [US1] Implement headless `FVulkanDevice` initialization and capability mapping in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T041 [US1] Implement explicit unsupported return behavior for out-of-scope resource, descriptor, shader, pipeline, command-buffer, and upload-related factories in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T042 [US1] Expose backend creation helper through `Source/Backend/Vulkan/Public/VulkanRHI/VulkanDevice.h`
- [X] T043 [US1] Implement backend creation helper in `Source/Backend/Vulkan/Private/VulkanDevice.cpp`
- [X] T044 [US1] Wire Vulkan backend tests into `Tests/VulkanBackendTests.cpp` for supported, unsupported, and diagnostic paths
- [X] T045 [US1] Run `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`, then fix US1 failures in `Source/Backend/Vulkan/` or `Tests/VulkanBackendTests.cpp`

**Checkpoint**: User Story 1 is independently functional and testable as the MVP.

---

## Phase 4: User Story 2 - Discover and Use Backend Queues (Priority: P1)

**Goal**: Developers can request supported backend queues through RHI queue contracts, observe queue type and submitted count, wait for idle, and get explicit rejections for unsupported queues or non-executable submissions.

**Independent Test**: Create a backend device, request supported and unsupported queue types, verify metadata and wait-idle, and verify submit rejects missing or non-executable command buffers until command recording exists.

### Tests for User Story 2

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T046 [P] [US2] Add failing queue creation success tests for graphics, compute, transfer, and present capability paths in `Tests/VulkanBackendTests.cpp`
- [X] T047 [P] [US2] Add failing unsupported queue request tests using synthetic capability fixtures in `Tests/VulkanBackendTests.cpp`
- [X] T048 [P] [US2] Add failing queue metadata tests for queue type and submitted command count in `Tests/VulkanBackendTests.cpp`
- [X] T049 [P] [US2] Add failing wait-idle success tests for created backend queues in `Tests/VulkanBackendTests.cpp`
- [X] T050 [P] [US2] Add failing non-executable command buffer submission rejection tests in `Tests/VulkanBackendTests.cpp`
- [X] T051 [US2] Add failing post-shutdown queue creation rejection tests in `Tests/VulkanBackendTests.cpp`

### Implementation for User Story 2

- [X] T052 [P] [US2] Implement `FVulkanQueue` class declaration with queue type, submitted count, submit, and wait-idle APIs in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanQueue.h`
- [X] T053 [US2] Implement queue construction, queue type query, submitted count query, and wait-idle behavior in `Source/Backend/Vulkan/Private/FVulkanQueue.cpp`
- [X] T054 [US2] Implement explicit missing or non-executable command buffer submit rejection in `Source/Backend/Vulkan/Private/FVulkanQueue.cpp`
- [X] T055 [US2] Integrate queue family/capability mapping into `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T056 [US2] Implement `FVulkanDevice::CreateCommandQueue` support and unsupported queue rejection in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T057 [US2] Add queue capability diagnostics to `Source/Backend/Vulkan/Private/FVulkanDiagnostics.cpp`
- [X] T058 [US2] Run `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`, then fix US2 failures in `Source/Backend/Vulkan/` or `Tests/VulkanBackendTests.cpp`

**Checkpoint**: User Stories 1 and 2 validate headless device initialization plus queue readiness.

---

## Phase 5: User Story 3 - Create and Recreate a Presentation Swapchain (Priority: P1)

**Goal**: Developers can create a presentation surface from a valid Core platform window wrapper, create a compatible swapchain, acquire and present frames, handle resize/unavailable states, and explicitly skip presentation when no valid wrapper exists.

**Independent Test**: With a valid Core platform window wrapper, create a swapchain and exercise acquire/present/recreate; without one, verify presentation tests skip explicitly while headless device validation still runs.

### Tests for User Story 3

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T059 [P] [US3] Add failing invalid/null Core platform window wrapper surface rejection tests in `Tests/VulkanBackendTests.cpp`
- [X] T060 [P] [US3] Add failing presentation-unavailable explicit skip tests in `Tests/VulkanBackendTests.cpp`
- [X] T061 [P] [US3] Add failing surface creation success test guarded by valid Core platform window wrapper availability in `Tests/VulkanBackendTests.cpp`
- [X] T062 [P] [US3] Add failing swapchain creation success test for compatible device, surface, frame count, format, and present queue in `Tests/VulkanBackendTests.cpp`
- [X] T063 [P] [US3] Add failing acquire-then-present frame advancement tests in `Tests/VulkanBackendTests.cpp`
- [X] T064 [P] [US3] Add failing acquire twice, present without acquire, and stale frame present rejection tests in `Tests/VulkanBackendTests.cpp`
- [X] T065 [P] [US3] Add failing resize-required, unavailable, and recreate flow tests in `Tests/VulkanBackendTests.cpp`
- [X] T066 [US3] Add failing post-shutdown surface and swapchain creation rejection tests in `Tests/VulkanBackendTests.cpp`

### Implementation for User Story 3

- [X] T067 [P] [US3] Implement `FVulkanSurface` class declaration with Core platform window wrapper validation and lifecycle query in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanSurface.h`
- [X] T068 [US3] Implement Core platform window wrapper validation, unsupported platform bridge handling, and surface diagnostics in `Source/Backend/Vulkan/Private/FVulkanSurface.cpp`
- [X] T069 [P] [US3] Implement `FVulkanSwapchain` class declaration with frame count, current frame index, acquire, present, resize-required, unavailable, and recreate APIs in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanSwapchain.h`
- [X] T070 [US3] Implement swapchain compatibility checks for device, surface, frame count, format, presentation mode, and present queue in `Source/Backend/Vulkan/Private/FVulkanSwapchain.cpp`
- [X] T071 [US3] Implement acquire-next-frame, present, invalid acquire/present rejection, and frame index advancement in `Source/Backend/Vulkan/Private/FVulkanSwapchain.cpp`
- [X] T072 [US3] Implement resize-required, unavailable, and recreation state transitions in `Source/Backend/Vulkan/Private/FVulkanSwapchain.cpp`
- [X] T073 [US3] Integrate surface and swapchain factory behavior into `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T074 [US3] Add presentation availability and skip diagnostics to `Source/Backend/Vulkan/Private/FVulkanDiagnostics.cpp`
- [X] T075 [US3] Run `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`, then fix US3 failures in `Source/Backend/Vulkan/` or `Tests/VulkanBackendTests.cpp`

**Checkpoint**: User Stories 1, 2, and 3 complete the P1 Vulkan backend device, queue, and presentation MVP.

---

## Phase 6: User Story 4 - Use Backend Synchronization Objects (Priority: P2)

**Goal**: Developers can create backend fence and semaphore objects that satisfy existing RHI synchronization contracts and reject invalid transitions or post-shutdown creation.

**Independent Test**: Create fences and semaphores from an active backend device, query initial state, exercise signal/wait/reset/consume behavior, and verify invalid-state outcomes.

### Tests for User Story 4

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T076 [P] [US4] Add failing fence initial unsignaled and initially signaled creation tests in `Tests/VulkanBackendTests.cpp`
- [X] T077 [P] [US4] Add failing fence wait success, not-ready, timeout, reset, and signal tests in `Tests/VulkanBackendTests.cpp`
- [X] T078 [P] [US4] Add failing semaphore initial state, signal, consume, double-signal, consume-not-ready, and reset tests in `Tests/VulkanBackendTests.cpp`
- [X] T079 [US4] Add failing post-shutdown fence and semaphore creation rejection tests in `Tests/VulkanBackendTests.cpp`

### Implementation for User Story 4

- [X] T080 [P] [US4] Implement `FVulkanFence` class declaration with state, wait, reset, signal, and lifecycle APIs in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanFence.h`
- [X] T081 [US4] Implement fence state transitions and explicit timeout/not-ready/invalid-state behavior in `Source/Backend/Vulkan/Private/FVulkanFence.cpp`
- [X] T082 [P] [US4] Implement `FVulkanSemaphore` class declaration with state, signal, consume, reset, and lifecycle APIs in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanSemaphore.h`
- [X] T083 [US4] Implement semaphore state transitions and invalid transition rejection in `Source/Backend/Vulkan/Private/FVulkanSemaphore.cpp`
- [X] T084 [US4] Integrate fence and semaphore factory behavior into `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T085 [US4] Run `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`, then fix US4 failures in `Source/Backend/Vulkan/` or `Tests/VulkanBackendTests.cpp`

**Checkpoint**: Backend synchronization contracts are independently functional and ready for future queue submission work.

---

## Phase 7: User Story 5 - Validate Clean Shutdown and Failure Recovery (Priority: P2)

**Goal**: Developers can repeatedly create and destroy backend devices, queues, sync objects, surfaces, and swapchains without stale usability, partial initialization leaks, or crashes.

**Independent Test**: Run repeated initialization, object creation, acquire/present or presentation-skip, recreation, and shutdown flows while checking deterministic success or explicit unsupported states.

### Tests for User Story 5

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T086 [P] [US5] Add failing clean shutdown lifecycle matrix tests for device, queues, fences, semaphores, surfaces, and swapchains in `Tests/VulkanBackendTests.cpp`
- [X] T087 [P] [US5] Add failing repeated create/destroy cycle tests for supported and unsupported runtime modes in `Tests/VulkanBackendTests.cpp`
- [X] T088 [P] [US5] Add failing partial initialization cleanup tests using synthetic failure injection in `Tests/VulkanBackendTests.cpp`
- [X] T089 [P] [US5] Add failing diagnostic coverage tests for runtime unavailable, validation unavailable, selected adapter, rejected adapter, and presentation skip in `Tests/VulkanBackendTests.cpp`
- [X] T090 [US5] Add failing verification that every contract section in `specs/009-vulkan-device-swapchain/contracts/vulkan-device-swapchain-contract.md` has success and negative coverage in `Tests/VulkanBackendTests.cpp`

### Implementation for User Story 5

- [X] T091 [US5] Implement owned-object invalidation and release tracking in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T092 [US5] Implement safe repeated shutdown and invalid repeated transition behavior in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T093 [US5] Implement partial initialization failure cleanup paths in `Source/Backend/Vulkan/Private/FVulkanInstance.cpp`
- [X] T094 [US5] Implement partial device creation failure cleanup paths in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T095 [US5] Complete diagnostic query coverage across `Source/Backend/Vulkan/Private/FVulkanDiagnostics.cpp`
- [X] T096 [US5] Implement full backend smoke flow helper in `Tests/VulkanBackendTests.cpp`
- [X] T097 [US5] Update `Tests/VulkanBackendTests.h` with final test result aggregation declarations
- [X] T098 [US5] Run `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`, then fix US5 failures in `Source/Backend/Vulkan/` or `Tests/VulkanBackendTests.cpp`

**Checkpoint**: All user stories are independently functional and collectively validate the Vulkan backend device/swapchain slice.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Verify contract coverage, build portability, architecture isolation, quickstart readiness, and roadmap state.

- [X] T099 Verify `specs/009-vulkan-device-swapchain/contracts/vulkan-device-swapchain-contract.md` is satisfied by `Source/Backend/Vulkan/` and `Tests/VulkanBackendTests.cpp`
- [X] T100 Verify no Renderer or Application public dependency is introduced into `Source/Backend/Vulkan/Public/VulkanRHI/`
- [X] T101 Verify public names follow UE5-style `F*`, `E*`, `I*`, and `T*` naming conventions in `Source/Backend/Vulkan/Public/VulkanRHI/`
- [X] T102 Verify Vulkan-specific includes remain isolated to `Source/Backend/Vulkan/` and do not leak into `Source/RHI/Public/RHI/`
- [X] T103 Review `Tests/VulkanBackendTests.cpp` to ensure supported runtime, unsupported runtime, presentation skip, and presentation available modes are all deterministic
- [X] T104 Run quickstart build and verification flow from `specs/009-vulkan-device-swapchain/quickstart.md`
- [X] T105 Run `conda run -n godot scons` from the repository root and confirm the build exits with code 0
- [X] T106 Run `Build/Mac/Debug/Tests/StonerTest` from the repository root and confirm the executable exits with code 0
- [X] T107 Update `doc/roadmap.md` Phase 008 status only after implementation and verification are complete
- [X] T108 Review `specs/009-vulkan-device-swapchain/tasks.md` for completed task checkboxes and consistency before closing the feature

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion - BLOCKS all user stories
- **User Story 1 (Phase 3)**: Depends on Foundational - MVP
- **User Story 2 (Phase 4)**: Depends on Foundational and benefits from US1 device readiness
- **User Story 3 (Phase 5)**: Depends on Foundational and benefits from US1 device plus US2 queue/present capability
- **User Story 4 (Phase 6)**: Depends on Foundational and benefits from US1 device readiness
- **User Story 5 (Phase 7)**: Depends on all earlier user stories for lifecycle and failure recovery coverage
- **Polish (Phase 8)**: Depends on all desired user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Foundational; recommended MVP because it proves headless backend device initialization.
- **User Story 2 (P1)**: Can start after Foundational, but full queue factory validation uses US1 device creation.
- **User Story 3 (P1)**: Can start after Foundational, but full swapchain validation uses US1 device and US2 present queue behavior.
- **User Story 4 (P2)**: Can start after Foundational, but full factory validation uses US1 device creation.
- **User Story 5 (P2)**: Integrates all prior stories for clean shutdown, partial failure, and repeated lifecycle validation.

### Within Each User Story

- Tests MUST be written and fail before implementation.
- Public headers before private source implementations.
- Diagnostics and state models before device integration.
- Factory integration before full story smoke tests.
- Story complete before moving to the next priority unless working in parallel after Foundation.

### Parallel Opportunities

- Setup header/source shell tasks T001-T011 can run in parallel.
- Foundational diagnostic and adapter model tasks T018-T025 can run in parallel after Setup.
- Test tasks inside each user story marked [P] can be written in parallel.
- Public header tasks for queues, sync, surfaces, and swapchains can be written in parallel once Foundation is complete.
- US2 and US4 can be implemented in parallel after US1 factory shape is stable.

---

## Parallel Example: User Story 1

```bash
# Launch tests for User Story 1 in parallel:
Task: "Add failing test for supported or explicitly unsupported headless backend initialization in Tests/VulkanBackendTests.cpp"
Task: "Add failing test for selected device capabilities exposure through RHI fields in Tests/VulkanBackendTests.cpp"
Task: "Add failing test for deterministic multi-adapter selection and rejected candidate diagnostics in Tests/VulkanBackendTests.cpp"

# Launch independent public declarations in parallel:
Task: "Implement FVulkanInstance lifecycle fields and diagnostics query API in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanInstance.h"
Task: "Implement FVulkanPhysicalDevice adapter enumeration result fields and selected-candidate query API in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanPhysicalDevice.h"
Task: "Implement FVulkanDevice RHI device class declaration with lifecycle, capabilities, diagnostics, and unsupported out-of-scope factory declarations in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDevice.h"
```

## Parallel Example: User Story 3

```bash
# Launch tests for User Story 3 in parallel:
Task: "Add failing invalid/null Core platform window wrapper surface rejection tests in Tests/VulkanBackendTests.cpp"
Task: "Add failing swapchain creation success test for compatible device, surface, frame count, format, and present queue in Tests/VulkanBackendTests.cpp"
Task: "Add failing acquire twice, present without acquire, and stale frame present rejection tests in Tests/VulkanBackendTests.cpp"

# Launch declarations in parallel:
Task: "Implement FVulkanSurface class declaration with Core platform window wrapper validation and lifecycle query in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanSurface.h"
Task: "Implement FVulkanSwapchain class declaration with frame count, current frame index, acquire, present, resize-required, unavailable, and recreate APIs in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanSwapchain.h"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational
3. Complete Phase 3: User Story 1
4. Stop and validate: `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`
5. Confirm supported runtime or explicit unsupported runtime behavior is deterministic

### Incremental Delivery

1. Setup + Foundational -> backend scaffolding and diagnostics ready
2. US1 -> headless backend device MVP
3. US2 -> queue exposure and explicit non-executable submit rejection
4. US3 -> presentation surface and swapchain lifecycle when available
5. US4 -> synchronization objects
6. US5 -> shutdown, partial failure cleanup, and repeated lifecycle validation
7. Polish -> quickstart, roadmap, isolation, and final verification

### Parallel Team Strategy

After Foundation:

- Developer A: US1 device/runtime initialization
- Developer B: US2 queue contracts after US1 factory shape is stable
- Developer C: US4 synchronization contracts after US1 device shape is stable
- Developer D: US3 surface/swapchain contracts after US2 present queue behavior is available

## Notes

- `[P]` tasks = different files or independent test additions with no dependency on incomplete implementation tasks.
- `[US#]` label maps tasks to user stories in `specs/009-vulkan-device-swapchain/spec.md`.
- Unsupported runtime and presentation-skip outcomes are valid explicit test outcomes, not silent passes.
- Keep all Vulkan-specific runtime detail inside `Source/Backend/Vulkan/`.
- Do not implement real buffer/texture allocation, shader modules, descriptor sets, pipelines, command recording, or resource upload in this feature.
