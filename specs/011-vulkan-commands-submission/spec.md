# Feature Specification: Vulkan Command Recording & Submission

**Feature Branch**: `011-vulkan-commands-submission`  
**Created**: 2026-06-30  
**Status**: Draft  
**Input**: User description: "根据roadmap制定下一个大feature的spec"

## Clarifications

### Session 2026-06-30

- Q: Should queue submission require a real runtime, be disabled until later, or support deterministic fallback submission state? → A: Real runtime uses real queue submission; unavailable runtime may use deterministic fallback submission state with diagnostics that no real GPU execution occurred.
- Q: How should draw and dispatch behave before pipeline and shader binding exists? → A: Draw and dispatch may be recorded as placeholder commands, but submission diagnostics must report missing pipeline binding and must not require real execution.
- Q: What is the minimum render pass/framebuffer scope for command recording? → A: Implement minimal single-subpass backend render pass/framebuffer validation to support Begin/EndRenderPass scope.
- Q: How deep should barrier and layout transition validation go in this phase? → A: Record declarative barrier/layout intent and validate resource lifecycle, usage compatibility, and basic before/after state consistency.
- Q: How should completion observation behave for deterministic fallback submissions? → A: Fallback submission completes immediately by default, with test-configurable not-ready and timeout injection.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Allocate and Reuse Command Buffers (Priority: P1)

An engine developer can request command buffers for supported backend queues, reset or recycle them after use, and observe clear lifecycle state changes without leaking stale recording state.

**Why this priority**: Command buffers are the required container for every later draw, dispatch, copy, barrier, render pass, and queue submission operation. Without allocation and lifecycle guarantees, no GPU work can be represented safely.

**Independent Test**: Can be tested by creating an active backend device with supported queues, allocating command buffers, beginning and ending recording, resetting or recycling them, and verifying invalid queue types, exhausted capacity, shutdown state, or incorrect lifecycle transitions return explicit failures.

**Acceptance Scenarios**:

1. **Given** an active backend device and a supported queue type, **When** a developer allocates a command buffer, **Then** the system returns a valid command buffer whose queue compatibility and lifecycle state are queryable.
2. **Given** a completed or submitted command buffer that is safe to reuse, **When** a developer resets or recycles it, **Then** the command buffer returns to a recordable state without retaining stale commands.
3. **Given** an unsupported queue type, exhausted command buffer capacity, or a device that has shut down, **When** allocation is requested, **Then** the system returns an explicit failure without creating a usable partial command buffer.

---

### User Story 2 - Record Graphics, Compute, Transfer, and Barrier Commands (Priority: P1)

An engine developer can record supported command categories into a command buffer while the system validates recording order, queue compatibility, resource lifecycle, and required render pass context.

**Why this priority**: Resource management becomes useful only when work can be recorded against those resources. The feature must validate commands now so later pipeline and renderer features can rely on deterministic failures instead of undefined behavior.

**Independent Test**: Can be tested by creating compatible single-subpass backend render pass and framebuffer objects, then recording draw, indexed draw, compute dispatch, buffer copy, texture copy or transition, resource barriers, and render pass begin/end commands across supported command buffer states, including negative cases for out-of-order commands and invalid resources.

**Acceptance Scenarios**:

1. **Given** a newly allocated command buffer, **When** recording begins and supported transfer or barrier commands are added, **Then** the command buffer stores the commands and reports a recordable state until recording ends.
2. **Given** a command buffer inside a valid single-subpass render pass scope with a compatible framebuffer, **When** graphics draw commands are recorded, **Then** the commands are accepted only when the command buffer, resources, and render pass state are compatible.
3. **Given** a command buffer for a compute-capable queue, **When** compute dispatch commands are recorded, **Then** the commands are accepted only when the queue supports compute work and the recording state is valid.
4. **Given** commands are requested before begin-recording, after end-recording, after reset-required state, outside a required render pass scope, or against invalidated resources, **When** recording is attempted, **Then** the system returns explicit failures without changing unrelated recorded commands.

---

### User Story 3 - Submit Recorded Work and Observe Completion (Priority: P1)

An engine developer can submit executable command buffers to compatible backend queues, optionally associate completion observation, and wait for completion or idle state through existing queue and synchronization contracts.

**Why this priority**: Recording without submission cannot advance rendering. Queue submission is the bridge from backend command representation to executable GPU work and is required before pipeline and shader work can produce visible frames.

**Independent Test**: Can be tested by recording a valid command buffer, submitting it to a compatible queue, checking submitted-count changes, waiting for queue idle or completion observation, and confirming missing, unrecorded, already submitted, incompatible, or invalidated command buffers are rejected.

**Acceptance Scenarios**:

1. **Given** an ended command buffer compatible with a queue, **When** it is submitted, **Then** the queue accepts the work, increments observable submission state, and the command buffer transitions out of the editable recording state.
2. **Given** completion observation is requested for submitted work, **When** the work completes or is waited on, **Then** the system reports completion, timeout, not-ready, invalid-state, or failed outcomes explicitly.
3. **Given** a missing, still-recording, reset, already consumed, incompatible, or invalidated command buffer, **When** submission is requested, **Then** the system rejects it without changing the queue's successful submission count.

---

### User Story 4 - Integrate Pending Resource Uploads with Command Recording (Priority: P2)

An engine developer can consume pending buffer and texture upload requests from the resource management phase by recording validated transfer work and marking upload records as scheduled or rejected according to command compatibility.

**Why this priority**: The previous phase intentionally staged uploads without executing them. This feature should connect staged data to command recording while still keeping actual renderer scheduling and asset loading outside scope.

**Independent Test**: Can be tested by creating pending buffer and texture uploads, recording compatible transfer commands for them, verifying scheduled state transitions, and rejecting invalidated destinations, out-of-date upload records, unsupported queue types, or repeated scheduling attempts.

**Acceptance Scenarios**:

1. **Given** a valid pending buffer upload and a transfer-capable command buffer, **When** upload scheduling is recorded, **Then** the upload record transitions to a scheduled state and the command buffer retains the transfer work.
2. **Given** a valid pending texture upload and a transfer-capable command buffer, **When** upload scheduling is recorded, **Then** the upload record transitions to a scheduled state with preserved destination region information.
3. **Given** an invalidated destination resource, non-pending upload record, unsupported queue, or repeated scheduling attempt, **When** upload scheduling is requested, **Then** the system returns an explicit failure without claiming execution.

### Edge Cases

- What happens when command buffer allocation is requested for an unsupported or unavailable queue type?
- What happens when command buffer pool capacity is exhausted or reset is requested while work is still in flight?
- What happens when begin-recording is called twice, end-recording is called before begin-recording, or reset is called in an invalid lifecycle state?
- What happens when draw or dispatch is requested on a queue that does not support the corresponding work category?
- What happens when graphics commands are recorded outside a render pass scope, a render pass is ended without being active, or the framebuffer is incompatible with the render pass?
- What happens when resource barriers, layout transitions, or copy commands target invalidated, missing, incompatible, or out-of-bounds resources?
- What happens when a command buffer is submitted while still recording, never recorded, already submitted, reset, incompatible with the target queue, or invalidated by device shutdown?
- What happens when completion waiting times out or is requested after the related device, queue, or command buffer is no longer valid?
- What happens when staged upload records are missing, already scheduled, target invalidated resources, or require queue capabilities that are not available?
- What happens when repeated allocate, record, submit, wait, reset, and shutdown cycles happen in the same process?

## Architecture & Design Constraints *(mandatory)*

- **RHI Abstraction**: Renderer and Application-facing behavior MUST remain expressed through existing command buffer, queue, synchronization, resource, and render pass contracts. Backend-specific command recording details MUST NOT leak into Renderer-facing contracts.
- **Design Patterns**: Command pool ownership, command buffer lifecycle, command recording validation, queue submission, completion observation, and upload scheduling responsibilities MUST remain separate and testable. The feature MUST avoid a single catch-all backend command manager with hidden ownership rules.
- **Advanced Graphics**: Command categories and validation behavior MUST leave room for future graphics pipelines, compute pipelines, render graph scheduling, ray tracing, meshlet, and global illumination workloads without implementing those higher-level systems in this phase.
- **Naming Conventions**: Public project-facing concepts introduced by this feature MUST follow the project's UE5-style naming conventions.
- **Cross-Platform Compatibility**: The backend MUST support supported desktop environments where the required graphics runtime is available and MUST report explicit unsupported status where runtime, queue, command, synchronization, or presentation capabilities are absent.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST allow command buffer allocation for active backend devices and supported queue types.
- **FR-002**: System MUST reject command buffer allocation when the device is shut down, the queue type is unsupported, queue ownership is unavailable, or command buffer capacity is exhausted.
- **FR-003**: System MUST expose command buffer lifecycle states that distinguish allocated, recording, executable, submitted or pending completion, reset or reusable, invalidated, and failed outcomes.
- **FR-004**: System MUST allow command buffers to begin and end recording only through valid lifecycle transitions.
- **FR-005**: System MUST reject recording begin, recording end, reset, or recycle requests that conflict with the current command buffer lifecycle state.
- **FR-006**: System MUST support command buffer reset or recycling once previous work is safe to discard, and MUST prevent reset or recycle while work is still in flight.
- **FR-007**: System MUST record graphics draw and indexed draw commands only when command buffer state, queue capability, render pass scope, and referenced resource lifecycle are valid, and MUST treat them as placeholder commands with missing-pipeline diagnostics until pipeline binding exists.
- **FR-008**: System MUST record compute dispatch commands only when command buffer state, queue capability, and referenced resource lifecycle are valid, and MUST treat them as placeholder commands with missing-pipeline diagnostics until compute pipeline binding exists.
- **FR-009**: System MUST record transfer commands for buffer and texture copy operations only when source and destination resources, ranges, regions, and queue capabilities are valid.
- **FR-010**: System MUST record declarative resource barrier and texture layout transition intent with explicit validation of resource lifecycle, supported usage, and basic before/after state consistency.
- **FR-010a**: System MUST keep full per-resource state tracking across command buffers and render graph-level synchronization conflict detection outside this feature's delivered scope.
- **FR-011**: System MUST provide minimal backend validation for single-subpass render passes and compatible framebuffers so render pass begin and end commands can define a valid graphics recording scope.
- **FR-011a**: System MUST reject nested, missing, out-of-order, incompatible-framebuffer, invalidated-render-pass, and invalidated-framebuffer render pass transitions.
- **FR-012**: System MUST preserve recorded command summaries or diagnostics sufficient for tests and future renderer code to verify command categories, ordering, and rejection reasons deterministically.
- **FR-013**: System MUST submit only ended, executable command buffers to compatible queues and reject missing, still-recording, reset, already consumed, incompatible, or invalidated command buffers.
- **FR-013a**: System MUST use real queue submission when supported runtime execution is available and MAY use deterministic fallback submission state when runtime execution is unavailable, provided diagnostics explicitly report that no real GPU execution occurred.
- **FR-014**: System MUST update queue submission observability consistently, including successful submission count, last submission result, and wait-idle behavior.
- **FR-015**: System MUST provide completion observation for submitted work through existing synchronization and queue status contracts, including explicit completed, not-ready, timeout, invalid-state, and failed outcomes.
- **FR-015a**: System MUST treat deterministic fallback submissions as immediately complete by default and MUST allow validation to inject not-ready and timeout outcomes for completion-wait testing.
- **FR-016**: System MUST invalidate or release owned command pools, command buffers, and pending submission state when the backend device shuts down.
- **FR-017**: System MUST reject command allocation, recording, reset, recycle, submission, upload scheduling, and completion waiting after backend device shutdown with invalid-state status.
- **FR-018**: System MUST allow pending buffer upload records from the resource management phase to be scheduled into compatible command buffers without claiming upload execution before submission.
- **FR-019**: System MUST allow pending texture upload records from the resource management phase to be scheduled into compatible command buffers without claiming upload execution before submission.
- **FR-020**: System MUST reject upload scheduling when the upload record is missing, no longer pending, already scheduled, targets invalidated resources, has invalid ranges or regions, or requires unavailable queue capabilities.
- **FR-021**: System MUST preserve existing Core, RHI resource/pipeline, Vulkan device/swapchain, and Vulkan resource management test outcomes while adding backend command validation.
- **FR-022**: System MUST provide deterministic test coverage for command buffer allocation, lifecycle transitions, recording success and rejection paths, render pass scope validation, transfer and barrier validation, queue submission, completion waiting, upload scheduling, reset or recycling, and shutdown invalidation.
- **FR-023**: System MUST keep shader compilation, pipeline creation, full pipeline binding validation, full resource state tracking, multi-subpass render pass modeling, render graph scheduling, multi-threaded command recording, and visible frame rendering outside this feature's delivered scope.

### Key Entities *(include if feature involves data)*

- **Command Pool**: Represents queue-family-compatible ownership for command buffer allocation, reset, recycling, and shutdown invalidation.
- **Command Buffer**: Represents a recordable unit of backend work with queue compatibility, lifecycle state, recorded command summaries, and submission status.
- **Recorded Command**: Represents accepted work intent such as graphics draw, compute dispatch, transfer copy, declarative barrier, layout transition, or render pass boundary.
- **Render Pass Scope**: Represents the active graphics recording region required for draw commands and render pass ordering validation.
- **Backend Render Pass**: Represents a minimal single-subpass backend render pass compatible with existing render pass descriptions and command scope validation.
- **Backend Framebuffer**: Represents a minimal backend framebuffer with attachments compatible with a backend render pass for command scope validation.
- **Submission Batch**: Represents one or more executable command buffers accepted by a compatible queue for completion observation.
- **Completion Observation**: Represents the result of waiting for submitted work, including completed, not-ready, timeout, invalid-state, or failed outcomes.
- **Upload Scheduling Record**: Represents a pending buffer or texture upload that has been validated and recorded into command work but has not been treated as fully executed until the submitted work completes.
- **Command Diagnostics**: Represents rejection reasons for allocation, lifecycle, recording, queue compatibility, resource lifecycle, render pass scope, upload scheduling, submission, and completion waiting.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In a supported development environment, a developer can allocate a command buffer, begin and end recording, submit it to a compatible queue, wait for completion or idle state, and reset it for reuse in under 30 seconds using the project verification flow.
- **SC-002**: Command lifecycle validation rejects 100% of covered invalid transitions, including double begin, end-before-begin, record-after-end, reset-while-in-flight, submit-while-recording, and use-after-shutdown, without crashes or usable partial state.
- **SC-003**: Command recording validation covers at least one success path and one negative path each for draw, indexed draw, compute dispatch, missing-pipeline diagnostics, buffer copy, texture copy or transition, declarative barrier/layout validation, minimal single-subpass render pass/framebuffer validation, render pass begin/end, and out-of-scope render pass usage.
- **SC-004**: Queue submission validation covers at least one success path and one negative path each for compatible submission, deterministic fallback submission diagnostics when runtime execution is unavailable, immediate fallback completion, injected timeout or not-ready observation, incompatible queue submission, missing command buffer, unrecorded command buffer, already submitted command buffer, completion wait, and wait-idle behavior.
- **SC-005**: Upload scheduling validation covers at least one success path and one negative path each for pending buffer upload scheduling and pending texture upload scheduling, including preservation of destination ranges or regions.
- **SC-006**: Existing Core, RHI, Vulkan device/swapchain, and Vulkan resource management tests continue to pass after command recording and submission are added.
- **SC-007**: Renderer-facing code can interact with backend command buffers, queues, synchronization status, resource barriers, and upload scheduling through established contracts without depending on backend-specific command implementation details.

## Assumptions

- Vulkan resource management is complete enough to provide buffers, textures, descriptor-related objects, upload staging records, lifecycle state, and diagnostics needed by command recording validation.
- Vulkan device and queue support is complete enough to expose active queues, wait-idle behavior, synchronization objects, shutdown state, and explicit unsupported runtime outcomes.
- This feature targets the roadmap's Phase 010, while the Speckit feature directory uses `011` because existing spec directories already occupy numbers through `010`.
- Command recording should validate and retain deterministic command intent even in environments where a full runtime path is unavailable; queue submission should use real execution when available and deterministic fallback submission state with explicit diagnostics when unavailable.
- Deterministic fallback submission defaults to immediate completion so verification remains stable; not-ready and timeout outcomes are validation controls rather than implied asynchronous GPU behavior.
- Draw, dispatch, copy, barrier, and render pass commands are recorded and submitted as backend work intent in this phase; draw and dispatch do not require real pipeline execution until shader and pipeline support is delivered in a later phase.
- Minimal backend render pass and framebuffer support is included only to validate single-subpass command scope; multi-subpass behavior and full pipeline compatibility are deferred.
- Barrier and layout transition support is limited to declarative intent plus basic validation; complete resource state tracking is deferred to later renderer or synchronization work.
- Multi-threaded command recording is outside the first delivered scope; command buffer lifecycle behavior may assume single-threaded validation unless a later phase expands it.
