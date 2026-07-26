# Feature Specification: Vulkan Device & Swapchain Backend

**Feature Branch**: `009-vulkan-backend-device`  
**Created**: 2026-06-30  
**Status**: Draft  
**Input**: User description: "根据roadmap制定下一个大feature的spec"

## Clarifications

### Session 2026-06-30

- Q: Should swapchain/presentation validation be mandatory in every supported environment, or conditional on having a valid Core platform window wrapper? → A: Headless device initialization is mandatory; swapchain and presentation validation are required only when a valid Core platform window wrapper is available.
- Q: How should the backend choose among multiple compatible graphics adapters? → A: Apply a required capability gate first, then use deterministic scoring that prefers discrete GPUs, stronger queue support, presentation support, and required formats.
- Q: Should missing validation-layer support fail backend initialization in debug/development environments? → A: Request validation in debug/development when available; missing validation support must be reported diagnostically but must not fail device initialization.
- Q: How much queue submission behavior belongs in this phase before real command buffer recording exists? → A: Implement queue creation and wait-idle; submission must explicitly reject missing or non-executable command buffers until the command recording phase.
- Q: What should be the canonical presentation input for surface and swapchain creation? → A: Use the existing Core platform window wrapper as the canonical presentation input; null or invalid handles must be rejected explicitly.

### CR-001 Amendment 2026-07-26

- Runtime truthfulness: `FVulkanDevice` defaults to a real-runtime request. Until it owns native Vulkan instance/device resources, that request MUST return explicit unsupported status; deterministic fallback is test-only, requires explicit opt-in, and MUST be distinguishable in diagnostics and backend availability.
- Adapter identity: candidate names, rejection reasons, and the selected identity MUST be owned values. Empty or null source identities MUST be rejected before deterministic ordering.
- Format truthfulness: every adapter candidate MUST carry a concrete supported-format set. Device capability queries and resource factories MUST consume the selected candidate's same set rather than a backend-wide default.
- Presentation abstraction: after Feature 018 introduced the current backend-neutral presentation contracts, `FVulkanDevice` MUST implement `IRHIPresentationSurface`, descriptor-based swapchain creation, imported swapchain image access, and semaphore-aware acquire/present behavior. Backend-specific surface helpers may remain only as compatibility adapters.
- Presentation provenance: every surface MUST retain shared device-owned lifecycle and provenance. Stale, invalid, or foreign-device surfaces MUST be rejected, device shutdown MUST invalidate its surfaces and dependent swapchains, and surface loss MUST prevent image access, acquire/present, or successful recreation.
- Failure atomicity: failed surface creation MUST clear the output. Malformed descriptors and zero frame counts return invalid-state; valid requests exceeding a supported capability return unsupported. Deterministic imported images MUST remain explicit fallback objects and MUST NOT count as native presentation proof.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Initialize a Usable Vulkan Backend Device (Priority: P1)

An engine developer can start the Vulkan backend in a supported runtime environment and obtain an active rendering device that reports capabilities through the existing RHI contracts.

**Why this priority**: This is the first real graphics backend milestone. Without a usable backend device, later resource management, command recording, pipeline creation, and rendering milestones cannot be validated against real hardware.

**Independent Test**: Can be tested by requesting a Vulkan backend device in an environment with a compatible graphics runtime and verifying that headless device creation succeeds, capabilities are reported, and unsupported environments return explicit failure without crashing.

**Acceptance Scenarios**:

1. **Given** a supported environment with at least one compatible graphics adapter, **When** a developer initializes the Vulkan backend, **Then** the system returns an active device that exposes queue, format, presentation, synchronization, and frame-in-flight capabilities through RHI-visible state.
2. **Given** an environment with multiple compatible graphics adapters, **When** the backend selects a device, **Then** incompatible adapters are filtered out first and the chosen device is deterministic, preferring discrete GPUs, stronger queue support, presentation support, and required formats.
3. **Given** no compatible graphics adapter or required runtime support is available, **When** initialization is requested, **Then** the system returns an explicit unsupported or failed result and leaves no partially usable backend device.

---

### User Story 2 - Discover and Use Backend Queues (Priority: P1)

An engine developer can request graphics, compute, transfer, and present-capable queues from the Vulkan backend through the existing RHI queue contracts.

**Why this priority**: Queues are required for every later GPU operation. Device creation is not useful unless the backend can expose queue capabilities consistently and reject unsupported queue requests.

**Independent Test**: Can be tested by creating a backend device, requesting each supported queue type, checking queue compatibility metadata, verifying wait-idle succeeds, and verifying unsupported queue requests or non-executable submissions return explicit status values.

**Acceptance Scenarios**:

1. **Given** a backend device with graphics and present support, **When** graphics and present queues are requested, **Then** the system returns queue objects whose type and compatibility can be queried through the RHI contract and whose idle wait can complete safely.
2. **Given** a device that lacks a requested queue capability, **When** that queue is requested, **Then** the system reports unsupported without returning a usable queue object.
3. **Given** a queue request after device shutdown, **When** the request is made, **Then** the system returns invalid-state consistently.
4. **Given** real command buffer recording is outside this phase, **When** a queue submission receives a missing or non-executable command buffer, **Then** the system rejects the submission with an explicit status value.

---

### User Story 3 - Create and Recreate a Presentation Swapchain (Priority: P1)

An engine developer can create a presentation target from the existing Core platform window wrapper, acquire frames, present frames, and handle resize or unavailable presentation states through the existing RHI swapchain contract.

**Why this priority**: Swapchain support is the bridge from device initialization to visible rendering. The triangle demo and all real frame rendering paths depend on stable frame acquisition and presentation behavior.

**Independent Test**: Can be tested in an environment with a valid Core platform window wrapper by creating a swapchain, acquiring and presenting frames, simulating or triggering resize, and confirming state transitions are explicit and recoverable. Environments without a valid Core platform window wrapper may skip presentation validation but must still pass headless backend device validation.

**Acceptance Scenarios**:

1. **Given** a valid Core platform window wrapper and active backend device with presentation support, **When** a swapchain is created, **Then** the system returns a ready swapchain with a valid frame count and current frame index.
2. **Given** a ready swapchain, **When** a frame is acquired and then presented, **Then** the swapchain advances its current frame according to the RHI presentation contract.
3. **Given** a surface resize, presentation loss, or outdated presentation target, **When** acquire or present is attempted, **Then** the system returns a resize-required or unavailable result and allows the caller to recreate the presentation target.

---

### User Story 4 - Use Backend Synchronization Objects (Priority: P2)

An engine developer can create and use backend fence and semaphore objects that satisfy the existing RHI synchronization contracts.

**Why this priority**: Synchronization is needed for safe queue submission, frame pacing, and swapchain presentation. It can be validated after device and queue creation are stable.

**Independent Test**: Can be tested by creating synchronization objects, observing their initial state, waiting or signaling where applicable, and verifying invalid device states reject creation.

**Acceptance Scenarios**:

1. **Given** an active backend device with synchronization support, **When** a fence or semaphore is created, **Then** the object reports a valid initial state consistent with the RHI synchronization contract.
2. **Given** a fence or semaphore operation that cannot complete immediately, **When** the operation is requested, **Then** the system returns an explicit not-ready, timeout, or invalid-state result rather than blocking indefinitely.
3. **Given** device shutdown, **When** synchronization object creation is requested, **Then** the system returns invalid-state without creating usable objects.

---

### User Story 5 - Validate Clean Shutdown and Failure Recovery (Priority: P2)

An engine developer can repeatedly create and destroy the Vulkan backend device and swapchain in tests without leaked ownership, stale state, or crashes.

**Why this priority**: Real backend initialization touches external runtime resources. Deterministic cleanup and failure recovery are necessary before adding resource allocation, command buffers, and pipelines.

**Independent Test**: Can be tested by running repeated initialization, swapchain creation, acquire/present, recreation, and shutdown flows while checking that every object transitions to an explicit terminal or unavailable state.

**Acceptance Scenarios**:

1. **Given** an active backend device with created queues, synchronization objects, and swapchain, **When** shutdown is requested, **Then** the device releases or invalidates owned backend objects and rejects subsequent creation requests.
2. **Given** initialization fails partway through required backend setup, **When** the failure is reported, **Then** no partially initialized device, queue, sync object, surface, or swapchain remains usable.
3. **Given** repeated create/destroy cycles, **When** the test flow completes, **Then** each cycle reports deterministic success or explicit unsupported status with no crash.

### Edge Cases

- What happens when no compatible graphics adapter is available?
- What happens when required runtime support, validation support, or presentation support is unavailable?
- What happens when multiple adapters are available with different queue, format, and presentation capabilities?
- What happens when the Core platform window wrapper is missing, null, invalid, resized, minimized, or becomes unavailable during presentation?
- What happens when swapchain frame acquisition is requested twice before presentation?
- What happens when present is requested for a frame that was not acquired or is no longer valid?
- What happens when queue, fence, semaphore, or swapchain creation is requested after device shutdown?
- What happens when queue submission is requested before real command buffer recording support exists?
- What happens when initialization fails after some backend runtime objects have already been created?
- What happens in environments where a headless device is supported but presentation is not?
- What happens when repeated initialization and shutdown cycles occur in the same process?

## Architecture & Design Constraints *(mandatory)*

- **RHI Abstraction**: Renderer and Application-facing behavior MUST remain expressed through existing RHI device, queue, synchronization, and swapchain contracts. Backend-specific details MUST NOT leak into Renderer-facing contracts.
- **Design Patterns**: Instance/device selection, queue ownership, surface presentation, swapchain lifecycle, and synchronization responsibilities MUST remain separate. The feature MUST avoid a single catch-all backend object that owns unrelated behavior without clear boundaries.
- **Advanced Graphics**: Device capability reporting MUST leave room for future resource, pipeline, render graph, ray tracing, meshlet, and global illumination phases without implementing those features in this phase.
- **Naming Conventions**: Public project-facing concepts introduced by this feature MUST follow the project's UE5-style naming conventions.
- **Cross-Platform Compatibility**: The backend MUST support the project's target desktop platforms where the required graphics runtime and presentation bridge are available, and MUST report explicit unsupported status where they are not.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST allow a developer to request initialization of the Vulkan backend and receive an explicit success, unsupported, invalid-state, or failed result.
- **FR-002**: System MUST discover available graphics adapters, reject candidates that fail required capability gates, and select one compatible adapter using deterministic scoring that prefers discrete GPUs, stronger queue support, presentation support, and required formats.
- **FR-002a**: System MUST own adapter identity and rejection data, reject empty identities before scoring, and keep selection deterministic without borrowing caller storage.
- **FR-003**: System MUST expose selected device capabilities through the existing RHI capability model, including queue support, presentation support, synchronization support, supported frame count limits, and supported formats.
- **FR-003a**: System MUST derive public supported formats and format-gated resource creation from the same concrete format set retained by the selected adapter.
- **FR-004**: System MUST create an active backend device only when all required initialization steps complete successfully.
- **FR-004a**: System MUST require explicit opt-in for deterministic fallback and MUST NOT report that fallback as an available real Vulkan runtime.
- **FR-005**: System MUST reject or cleanly fail initialization when required runtime support, required extensions, compatible adapters, or required queue capabilities are unavailable.
- **FR-005a**: System MUST request validation support in debug/development environments when available, and MUST report missing validation support diagnostically without failing backend device initialization.
- **FR-006**: System MUST expose graphics, compute, transfer, and present queue availability according to the selected device capabilities.
- **FR-007**: System MUST allow creation of RHI command queues for supported queue types and reject unsupported queue types through explicit status values.
- **FR-008**: System MUST preserve the existing RHI queue contract behavior for queue type query, submitted command count query, explicit submission rejection, and wait-idle behavior where applicable to this phase.
- **FR-008a**: System MUST support queue wait-idle for created backend queues and MUST reject missing or non-executable command buffer submissions explicitly until real command buffer recording is delivered in a later phase.
- **FR-009**: System MUST create synchronization objects that satisfy the existing RHI fence and semaphore lifecycle and status contracts.
- **FR-010**: System MUST reject fence, semaphore, queue, and swapchain creation after device shutdown with invalid-state status.
- **FR-011**: System MUST allow creation of a presentation surface from the existing Core platform window wrapper when it contains a supported native window handle.
- **FR-012**: System MUST reject missing, null, invalid, or unsupported Core platform window handles without creating a usable surface or swapchain.
- **FR-013**: System MUST create a swapchain only when the selected device, surface, presentation mode, image count, and format are mutually compatible.
- **FR-014**: System MUST expose swapchain frame count, current frame index, acquire-next-frame, and present behavior through the existing RHI swapchain contract.
- **FR-015**: System MUST report resize-required, unavailable, invalid-state, timeout, or failed outcomes explicitly when frame acquisition or presentation cannot proceed normally.
- **FR-016**: System MUST support swapchain recreation after resize-required or unavailable presentation state once valid presentation inputs are restored.
- **FR-017**: System MUST ensure backend device shutdown transitions owned queues, synchronization objects, and swapchains into states that cannot be used for new work.
- **FR-018**: System MUST preserve existing RHI core and RHI resource/pipeline contract test outcomes while adding backend device and swapchain validation.
- **FR-019**: System MUST provide deterministic test coverage for success paths and negative paths for backend initialization, adapter selection, queue creation, synchronization creation, swapchain creation, frame acquire/present, resize recovery, and shutdown.
- **FR-020**: System MUST keep buffer creation, texture creation, shader module creation, descriptor sets, pipeline creation, command buffer recording, and resource upload scheduling outside this feature's delivered scope.
- **FR-021**: System MUST treat headless backend device initialization as mandatory for the feature MVP, while swapchain and presentation validation are mandatory only in test environments that provide a valid Core platform window wrapper.

### Key Entities *(include if feature involves data)*

- **Backend Instance**: Represents the initialized backend runtime context required before selecting devices and creating presentation resources.
- **Physical Adapter Candidate**: Represents one discoverable graphics adapter with required capability gate results, queue support, format support, presentation support, device type, and deterministic suitability score.
- **Backend Device**: Represents the selected logical device that implements the existing RHI device behavior for queues, synchronization, and swapchain creation in this phase.
- **Backend Queue**: Represents a queue exposed through an RHI queue type, including graphics, compute, transfer, or present capability, explicit non-executable submission rejection, and wait-idle behavior.
- **Presentation Surface**: Represents a platform presentation target created from the existing Core platform window wrapper when it contains a valid supported native handle.
- **Swapchain**: Represents the presentation frame sequence, including frame count, current frame index, acquire/present state, and resize/unavailable transitions.
- **Fence**: Represents completion observation for backend work using the existing RHI fence contract.
- **Semaphore**: Represents ordering between backend operations using the existing RHI semaphore contract.
- **Initialization Result**: Represents explicit success, unsupported, invalid-state, unavailable, timeout, or failed outcomes for backend operations.
- **Validation Diagnostics**: Represents whether optional development validation support was enabled, unavailable, or disabled, without changing successful device initialization into failure.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In a supported development environment, a developer can initialize the Vulkan backend without a presentation surface, create an active device, query capabilities, and shut it down in under 30 seconds using the project verification flow.
- **SC-002**: In unsupported environments, backend initialization reports an explicit unsupported or failed result in under 10 seconds without crashing or leaving a usable partial device.
- **SC-002a**: In debug/development environments without validation-layer support, backend initialization can still succeed and reports validation unavailability as a diagnostic outcome.
- **SC-003**: Backend validation covers at least one success path and one negative path each for initialization, deterministic adapter selection, required capability rejection, queue creation, synchronization creation, swapchain creation, acquire/present behavior, resize or unavailable presentation, and shutdown.
- **SC-004**: Existing Core, RHI core, and RHI resource/pipeline tests continue to pass after the Vulkan backend device and swapchain feature is added.
- **SC-005**: A supported presentation environment with a valid Core platform window wrapper can create a swapchain, acquire a frame, present that frame, and recreate the swapchain after a resize-required condition with zero process crashes.
- **SC-006**: Renderer-facing code can interact with backend device, queues, synchronization objects, and swapchain through RHI contracts without depending on backend-specific object details.

## Assumptions

- The previous RHI core and RHI resource/pipeline interface features are complete enough to define device, queue, synchronization, swapchain, result, format, and lifecycle expectations.
- This feature targets the roadmap's Phase 008, while the Speckit feature directory uses `009` because existing spec directories already occupy numbers through `008`.
- Real buffer, texture, descriptor, shader, pipeline, command buffer, and render graph execution are deferred to later roadmap phases.
- Real command buffer allocation and recording are deferred; this phase only requires queues to reject unsupported submissions explicitly and support idle waiting.
- Platform presentation is expected to support the project's desktop targets where a compatible graphics runtime and presentation bridge are available; unsupported targets must fail explicitly rather than silently degrading.
- Validation layer behavior is treated as an optional debug/development aid; feature correctness must be observable through explicit results and tests even when validation layers are unavailable.
- Headless initialization is the mandatory MVP path; presentation swapchain success requires a valid Core platform window wrapper and may be skipped only in environments that cannot provide one.
