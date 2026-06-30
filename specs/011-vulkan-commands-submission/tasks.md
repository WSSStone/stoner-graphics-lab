# Tasks: Vulkan Command Recording & Submission

**Input**: Design documents from `specs/011-vulkan-commands-submission/`
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`, `contracts/vulkan-command-submission-contract.md`, `quickstart.md`
**Tests**: Required by the specification for deterministic lifecycle, recording, submission, render pass/framebuffer, upload scheduling, and regression coverage.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel because it touches a different file and has no dependency on incomplete tasks in the same phase.
- **[Story]**: Maps the task to a specific user story phase.
- Every task includes an exact repository-relative file path.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Prepare command-submission files, aggregate includes, and test scaffolding before story work begins.

- [X] T001 Review existing RHI command, queue, render pass, framebuffer, upload, and Vulkan device contracts in `Source/RHI/Public/RHI/IRHICommandBuffer.h`
- [X] T002 [P] Create command pool public header shell in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandPool.h`
- [X] T003 [P] Create command buffer public header shell in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandBuffer.h`
- [X] T004 [P] Create command submission public header shell in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandSubmission.h`
- [X] T005 [P] Create backend render pass public header shell in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanRenderPass.h`
- [X] T006 [P] Create backend framebuffer public header shell in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanFramebuffer.h`
- [X] T007 [P] Create command pool private source shell in `Source/Backend/Vulkan/Private/FVulkanCommandPool.cpp`
- [X] T008 [P] Create command buffer private source shell in `Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp`
- [X] T009 [P] Create command submission private source shell in `Source/Backend/Vulkan/Private/FVulkanCommandSubmission.cpp`
- [X] T010 [P] Create backend render pass private source shell in `Source/Backend/Vulkan/Private/FVulkanRenderPass.cpp`
- [X] T011 [P] Create backend framebuffer private source shell in `Source/Backend/Vulkan/Private/FVulkanFramebuffer.cpp`
- [X] T012 Add new command, render pass, framebuffer, and submission headers to the Vulkan aggregate include in `Source/Backend/Vulkan/Public/VulkanRHI/VulkanDevice.h`
- [X] T013 Add command-related owned object forward declarations and includes to `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDevice.h`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Extend shared command vocabulary and Vulkan diagnostics used by every user story.

**CRITICAL**: No user story work can begin until this phase is complete.

- [X] T014 Extend backend-neutral command kinds for draw indexed, buffer copy, texture copy, layout transition, render pass begin/end, and upload scheduling in `Source/RHI/Public/RHI/IRHICommandBuffer.h`
- [X] T015 Define backend-neutral command buffer helper structs for copy ranges, texture regions, and barrier/layout intent in `Source/RHI/Public/RHI/IRHICommandBuffer.h`
- [X] T016 Add backend-neutral command recording method declarations for draw indexed, copy, layout transition, and render pass scope in `Source/RHI/Public/RHI/IRHICommandBuffer.h`
- [X] T017 Update existing RHI mock command buffer test doubles for the expanded command interface in `Tests/RHICoreTests.cpp`
- [X] T018 Preserve existing RHI command buffer and queue behavior with expanded command vocabulary in `Tests/RHICoreTests.cpp`
- [X] T019 Extend Vulkan diagnostics fields for command allocation, recording, render pass, framebuffer, submission, completion, and upload scheduling reasons in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDiagnostics.h`
- [X] T020 Implement Vulkan diagnostics marker helpers for command allocation, recording, render pass, framebuffer, submission, completion, and upload scheduling in `Source/Backend/Vulkan/Private/FVulkanDiagnostics.cpp`
- [X] T021 Add command pool, command buffer, render pass, framebuffer, submission, and upload scheduling ownership arrays to the device in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDevice.h`
- [X] T022 Add command buffer capacity and fallback completion injection configuration APIs to the device in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDevice.h`
- [X] T023 Initialize command capacity and fallback completion defaults in the device constructor path in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T024 Add lifecycle invalidation hooks for future command objects to device shutdown in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T025 [P] Add shared Vulkan command test helper builders for valid buffers, textures, uploads, render pass descriptions, and framebuffer descriptions in `Tests/VulkanBackendTests.cpp`
- [X] T026 Verify foundational RHI and Vulkan tests still compile after interface expansion in `Tests/VulkanBackendTests.cpp`

**Checkpoint**: Foundation ready - user story implementation can now begin in priority order or in parallel where dependencies allow.

---

## Phase 3: User Story 1 - Allocate and Reuse Command Buffers (Priority: P1) MVP

**Goal**: Developers can allocate command buffers for supported queues, observe lifecycle transitions, reset or recycle safe buffers, and receive explicit failures for invalid allocation or lifecycle operations.

**Independent Test**: Create an active backend device with supported queues, allocate command buffers, begin/end/reset them, and verify unsupported queue, capacity exhaustion, shutdown, and invalid lifecycle transitions return explicit results.

### Tests for User Story 1

- [X] T027 [US1] Add failing command buffer allocation success and query tests in `Tests/VulkanBackendTests.cpp`
- [X] T028 [US1] Add failing unsupported queue type and command buffer capacity exhaustion tests in `Tests/VulkanBackendTests.cpp`
- [X] T029 [US1] Add failing Begin/End/Reset lifecycle transition tests in `Tests/VulkanBackendTests.cpp`
- [X] T030 [US1] Add failing invalid lifecycle transition tests for double Begin, End before Begin, record after End, Reset while Recording, and Reset while Submitted in `Tests/VulkanBackendTests.cpp`
- [X] T031 [US1] Add failing command pool and command buffer shutdown invalidation tests in `Tests/VulkanBackendTests.cpp`
- [X] T032 [US1] Add failing command buffer recycle and stale command clearing tests in `Tests/VulkanBackendTests.cpp`

### Implementation for User Story 1

- [X] T033 [P] [US1] Implement command pool state, capacity, allocation count, queue type, and invalidation API in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandPool.h`
- [X] T034 [P] [US1] Implement command buffer lifecycle fields, queue compatibility query, command count query, and invalidation API in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandBuffer.h`
- [X] T035 [US1] Implement command pool construction, capacity checks, allocation, and shutdown invalidation in `Source/Backend/Vulkan/Private/FVulkanCommandPool.cpp`
- [X] T036 [US1] Implement command buffer Begin, End, Reset, stale command clearing, and invalid lifecycle rejection in `Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp`
- [X] T037 [US1] Integrate command pool creation and command buffer allocation into `FVulkanDevice::CreateCommandBuffer` in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T038 [US1] Store created command pools and command buffers on the device for ownership and shutdown invalidation in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T039 [US1] Implement command buffer capacity configuration and exhaustion diagnostics in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T040 [US1] Expose command buffer allocation and lifecycle diagnostics through device diagnostics in `Source/Backend/Vulkan/Private/FVulkanDiagnostics.cpp`
- [X] T041 [US1] Add command headers to the Vulkan backend aggregate include after implementations compile in `Source/Backend/Vulkan/Public/VulkanRHI/VulkanDevice.h`
- [X] T042 [US1] Run and fix US1 allocation, lifecycle, capacity, recycle, and shutdown tests in `Tests/VulkanBackendTests.cpp`

**Checkpoint**: User Story 1 is independently functional and testable as the MVP.

---

## Phase 4: User Story 2 - Record Graphics, Compute, Transfer, and Barrier Commands (Priority: P1)

**Goal**: Developers can record command categories with deterministic validation of ordering, queue compatibility, resource lifecycle, render pass scope, placeholder pipeline diagnostics, and declarative barrier/layout intent.

**Independent Test**: Create compatible render pass/framebuffer objects, record draw, indexed draw, dispatch, transfer, barrier/layout, and render pass begin/end commands, then verify negative cases for invalid ordering, invalid resources, incompatible queues, and invalid render pass scope.

### Tests for User Story 2

- [X] T043 [US2] Add failing minimal backend render pass creation success and invalid description tests in `Tests/VulkanBackendTests.cpp`
- [X] T044 [US2] Add failing minimal backend framebuffer creation success and attachment compatibility failure tests in `Tests/VulkanBackendTests.cpp`
- [X] T045 [US2] Add failing BeginRenderPass and EndRenderPass scope success and invalid ordering tests in `Tests/VulkanBackendTests.cpp`
- [X] T046 [US2] Add failing draw and indexed draw placeholder command tests with missing-pipeline diagnostics in `Tests/VulkanBackendTests.cpp`
- [X] T047 [US2] Add failing dispatch placeholder command tests with missing-pipeline diagnostics and queue compatibility rejection in `Tests/VulkanBackendTests.cpp`
- [X] T048 [US2] Add failing buffer copy and texture copy recording tests with range, region, resource lifecycle, and queue compatibility coverage in `Tests/VulkanBackendTests.cpp`
- [X] T049 [US2] Add failing declarative barrier and layout transition tests for usage compatibility and before/after state consistency in `Tests/VulkanBackendTests.cpp`
- [X] T050 [US2] Add failing command ordering and failed-recording-does-not-append tests in `Tests/VulkanBackendTests.cpp`
- [X] T051 [US2] Add failing End rejection test when a render pass scope remains unclosed in `Tests/VulkanBackendTests.cpp`

### Implementation for User Story 2

- [X] T052 [P] [US2] Implement backend render pass fields, attachment queries, lifecycle state, and invalidation API in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanRenderPass.h`
- [X] T053 [P] [US2] Implement backend framebuffer fields, render pass query, dimensions, attachment count, lifecycle state, and invalidation API in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanFramebuffer.h`
- [X] T054 [US2] Implement render pass description validation and lifecycle behavior in `Source/Backend/Vulkan/Private/FVulkanRenderPass.cpp`
- [X] T055 [US2] Implement framebuffer attachment compatibility validation and lifecycle behavior in `Source/Backend/Vulkan/Private/FVulkanFramebuffer.cpp`
- [X] T056 [US2] Replace unsupported render pass and framebuffer factories with minimal backend object creation in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T057 [US2] Store backend render pass and framebuffer objects on the device for shutdown invalidation in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T058 [US2] Add recorded command structures for draw, indexed draw, dispatch, copy, barrier, layout transition, render pass boundary, and upload scheduling in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandBuffer.h`
- [X] T059 [US2] Implement BeginRenderPass and EndRenderPass validation, active scope tracking, nested rejection, and unclosed-scope End rejection in `Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp`
- [X] T060 [US2] Implement draw and indexed draw placeholder recording with missing-pipeline diagnostics and graphics scope validation in `Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp`
- [X] T061 [US2] Implement compute dispatch placeholder recording with missing-pipeline diagnostics and queue compatibility validation in `Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp`
- [X] T062 [US2] Implement buffer copy and texture copy recording validation for ranges, regions, lifecycle, usage, and queue capability in `Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp`
- [X] T063 [US2] Implement declarative barrier and layout transition recording with lifecycle, usage compatibility, and before/after consistency validation in `Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp`
- [X] T064 [US2] Ensure failed command recording preserves previous recorded command count and diagnostics in `Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp`
- [X] T065 [US2] Add render pass, framebuffer, command recording, and barrier diagnostics to `Source/Backend/Vulkan/Private/FVulkanDiagnostics.cpp`
- [X] T066 [US2] Run and fix US2 render pass, framebuffer, recording, ordering, and diagnostics tests in `Tests/VulkanBackendTests.cpp`

**Checkpoint**: User Story 2 is independently functional after US1 foundation and can validate command recording without real pipelines.

---

## Phase 5: User Story 3 - Submit Recorded Work and Observe Completion (Priority: P1)

**Goal**: Developers can submit executable command buffers to compatible queues, observe real or fallback submission diagnostics, wait for completion or idle state, and reset completed command buffers.

**Independent Test**: Record a valid command buffer, submit it to a compatible queue, verify submitted count and completion behavior, then reject invalid, incompatible, still-recording, reset, already submitted, or invalidated command buffers.

### Tests for User Story 3

- [X] T067 [US3] Add failing compatible queue submission success and submitted-count tests in `Tests/VulkanBackendTests.cpp`
- [X] T068 [US3] Add failing submission rejection tests for missing, still-recording, never-recorded, reset, already submitted, incompatible, and invalidated command buffers in `Tests/VulkanBackendTests.cpp`
- [X] T069 [US3] Add failing deterministic fallback submission diagnostics and immediate completion tests in `Tests/VulkanBackendTests.cpp`
- [X] T070 [US3] Add failing fallback not-ready and timeout injection tests for completion observation in `Tests/VulkanBackendTests.cpp`
- [X] T071 [US3] Add failing wait semaphore consumption, signal semaphore signaling, and optional fence signaling tests in `Tests/VulkanBackendTests.cpp`
- [X] T072 [US3] Add failing queue WaitIdle and command buffer resettable-after-completion tests in `Tests/VulkanBackendTests.cpp`
- [X] T073 [US3] Add failing queue, command buffer, submission batch, and completion invalidation-on-shutdown tests in `Tests/VulkanBackendTests.cpp`

### Implementation for User Story 3

- [X] T074 [P] [US3] Define submission mode, completion state, completion injection config, and submission batch data in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandSubmission.h`
- [X] T075 [P] [US3] Add submission state transition helpers to the command buffer API in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandBuffer.h`
- [X] T076 [US3] Implement submission batch construction, fallback completion defaults, injected not-ready/timeout behavior, and diagnostics in `Source/Backend/Vulkan/Private/FVulkanCommandSubmission.cpp`
- [X] T077 [US3] Implement command buffer MarkSubmitted and MarkCompletedOrResettable transitions in `Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp`
- [X] T078 [US3] Replace queue submission placeholder behavior with executable command buffer validation in `Source/Backend/Vulkan/Private/FVulkanQueue.cpp`
- [X] T079 [US3] Implement deterministic fallback submission diagnostics and successful submitted-count updates in `Source/Backend/Vulkan/Private/FVulkanQueue.cpp`
- [X] T080 [US3] Implement wait semaphore consumption, signal semaphore signaling, optional fence signaling, and failure rollback behavior in `Source/Backend/Vulkan/Private/FVulkanQueue.cpp`
- [X] T081 [US3] Implement WaitIdle completion handling that makes submitted command buffers resettable in `Source/Backend/Vulkan/Private/FVulkanQueue.cpp`
- [X] T082 [US3] Add queue-level completion injection controls wired through the device configuration API in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T083 [US3] Store submission batches on the device or queue for diagnostics and shutdown invalidation in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T084 [US3] Add submission and completion diagnostics markers in `Source/Backend/Vulkan/Private/FVulkanDiagnostics.cpp`
- [X] T085 [US3] Run and fix US3 queue submission, fallback completion, synchronization, wait-idle, and shutdown tests in `Tests/VulkanBackendTests.cpp`

**Checkpoint**: User Story 3 is independently functional after US1/US2 recording support and validates queue submission behavior.

---

## Phase 6: User Story 4 - Integrate Pending Resource Uploads with Command Recording (Priority: P2)

**Goal**: Developers can schedule existing pending buffer and texture upload records into compatible command buffers without claiming execution before submission completion.

**Independent Test**: Create pending buffer and texture uploads, record compatible upload scheduling commands, verify scheduled state transitions and command summaries, and reject missing, non-pending, already scheduled, invalidated-destination, invalid range/region, or incompatible queue cases.

### Tests for User Story 4

- [X] T086 [US4] Add failing buffer upload scheduling success and destination range preservation tests in `Tests/VulkanBackendTests.cpp`
- [X] T087 [US4] Add failing texture upload scheduling success and destination region preservation tests in `Tests/VulkanBackendTests.cpp`
- [X] T088 [US4] Add failing upload scheduling rejection tests for missing, non-pending, already scheduled, and invalidated-destination upload records in `Tests/VulkanBackendTests.cpp`
- [X] T089 [US4] Add failing upload scheduling rejection tests for invalid ranges, invalid regions, and unavailable queue capabilities in `Tests/VulkanBackendTests.cpp`
- [X] T090 [US4] Add failing upload scheduled-but-not-executed-before-submission-completion tests in `Tests/VulkanBackendTests.cpp`
- [X] T091 [US4] Add failing upload scheduling invalidation-on-device-shutdown tests in `Tests/VulkanBackendTests.cpp`

### Implementation for User Story 4

- [X] T092 [P] [US4] Extend upload lifecycle with scheduled state and scheduling query helpers in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanUploadStaging.h`
- [X] T093 [P] [US4] Add upload scheduling record fields to command buffer recorded command storage in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandBuffer.h`
- [X] T094 [US4] Implement upload lifecycle transition, scheduled state, repeated scheduling rejection, and invalidation behavior in `Source/Backend/Vulkan/Private/FVulkanUploadStaging.cpp`
- [X] T095 [US4] Implement buffer upload scheduling validation and command recording in `Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp`
- [X] T096 [US4] Implement texture upload scheduling validation and command recording in `Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp`
- [X] T097 [US4] Ensure upload scheduling preserves staging-data association and does not claim execution before completion in `Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp`
- [X] T098 [US4] Add upload scheduling diagnostics markers in `Source/Backend/Vulkan/Private/FVulkanDiagnostics.cpp`
- [X] T099 [US4] Wire upload scheduling shutdown invalidation through the device-owned upload list in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T100 [US4] Run and fix US4 upload scheduling, lifecycle, diagnostics, and shutdown tests in `Tests/VulkanBackendTests.cpp`

**Checkpoint**: User Story 4 is independently functional after command recording support and validates upload scheduling without pretending GPU execution happened.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Validate the whole feature, preserve architecture boundaries, and update roadmap context.

- [X] T101 Run full project build with conda environment in `SConstruct`
- [X] T102 Run full test executable and record pass/fail outcome in `Tests/Main.cpp`
- [X] T103 Verify no Renderer or Application includes are introduced into Vulkan public command headers in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandBuffer.h`
- [X] T104 Verify public RHI headers remain Vulkan-free after command interface updates in `Source/RHI/Public/RHI/IRHICommandBuffer.h`
- [X] T105 Verify shader compilation, pipeline creation, full pipeline binding, full resource state tracking, render graph scheduling, multi-threaded recording, and visible rendering remain out of scope in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T106 [P] Update roadmap Phase 010 status for Vulkan Commands & Submission in `doc/roadmap.md`
- [X] T107 [P] Add or update implementation documentation placeholder for this feature in `doc/011-vulkan-commands-submission.html`
- [X] T108 Update agent context recent changes for implemented command submission work in `AGENTS.md`
- [X] T109 Mark completed tasks during implementation in `specs/011-vulkan-commands-submission/tasks.md`
- [X] T110 Run quickstart verification flow and fix any deviations documented in `specs/011-vulkan-commands-submission/quickstart.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately.
- **Foundational (Phase 2)**: Depends on Setup completion - blocks all user stories.
- **US1 (Phase 3)**: Depends on Foundational - MVP command buffer allocation and lifecycle.
- **US2 (Phase 4)**: Depends on US1 for command buffer lifecycle and recording storage.
- **US3 (Phase 5)**: Depends on US1 and US2 because submission requires executable recorded command buffers.
- **US4 (Phase 6)**: Depends on US2 because upload scheduling records into command buffers; US3 is needed only to validate scheduled work through submission.
- **Polish (Phase 7)**: Depends on all desired user stories being complete.

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Foundational; no dependency on other stories.
- **User Story 2 (P1)**: Starts after US1; independently testable for recording once command buffers exist.
- **User Story 3 (P1)**: Starts after US1 and US2; independently testable for submission once executable command buffers exist.
- **User Story 4 (P2)**: Starts after US2; can be validated with submission once US3 is complete.

### Within Each User Story

- Tests are written first and should fail before implementation.
- Header/API shape precedes source implementation.
- Source implementation precedes device integration.
- Device integration precedes full story validation.
- Story checkpoint must pass before moving to the next priority unless deliberately parallelizing.

### Parallel Opportunities

- Setup header/source shell tasks T002-T011 can run in parallel.
- Foundational diagnostics and interface tasks T014-T020 can be split by file once RHI method names are agreed.
- US1 header tasks T033-T034 can run in parallel before source implementation.
- US2 render pass/framebuffer header tasks T052-T053 can run in parallel before source implementation.
- US3 submission header tasks T074-T075 can run in parallel before source implementation.
- US4 upload staging and command buffer header tasks T092-T093 can run in parallel before source implementation.
- Polish documentation and roadmap tasks T106-T107 can run in parallel.

---

## Parallel Example: User Story 1

```text
Task: "T033 [P] [US1] Implement command pool state, capacity, allocation count, queue type, and invalidation API in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandPool.h"
Task: "T034 [P] [US1] Implement command buffer lifecycle fields, queue compatibility query, command count query, and invalidation API in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandBuffer.h"
```

## Parallel Example: User Story 2

```text
Task: "T052 [P] [US2] Implement backend render pass fields, attachment queries, lifecycle state, and invalidation API in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanRenderPass.h"
Task: "T053 [P] [US2] Implement backend framebuffer fields, render pass query, dimensions, attachment count, lifecycle state, and invalidation API in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanFramebuffer.h"
```

## Parallel Example: User Story 3

```text
Task: "T074 [P] [US3] Define submission mode, completion state, completion injection config, and submission batch data in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandSubmission.h"
Task: "T075 [P] [US3] Add submission state transition helpers to the command buffer API in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandBuffer.h"
```

## Parallel Example: User Story 4

```text
Task: "T092 [P] [US4] Extend upload lifecycle with scheduled state and scheduling query helpers in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanUploadStaging.h"
Task: "T093 [P] [US4] Add upload scheduling record fields to command buffer recorded command storage in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandBuffer.h"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup.
2. Complete Phase 2: Foundational.
3. Complete Phase 3: User Story 1.
4. Stop and validate command buffer allocation, lifecycle, reset/recycle, capacity, and shutdown tests.

### Incremental Delivery

1. Add US1 command buffers and lifecycle.
2. Add US2 recording, render pass/framebuffer scope, copy, and barrier validation.
3. Add US3 queue submission and completion observation.
4. Add US4 upload scheduling into command buffers.
5. Run full quickstart and regression checks.

### Parallel Team Strategy

1. One developer prepares foundational RHI command extensions.
2. One developer prepares Vulkan command pool/buffer skeletons.
3. One developer prepares render pass/framebuffer validation.
4. After US1 lands, US2 and US3 tests can be drafted while implementation proceeds in dependency order.

---

## Notes

- Tests are included because the spec explicitly requires deterministic coverage.
- `[P]` tasks touch different files or non-overlapping setup work.
- Story labels map to spec user stories for traceability.
- Commit after each phase or coherent task group using the project conventional commit style.
- Do not implement shader compilation, real graphics/compute pipeline creation, full pipeline binding validation, full resource state tracking, render graph scheduling, multi-threaded command recording, or visible frame rendering in this feature.
