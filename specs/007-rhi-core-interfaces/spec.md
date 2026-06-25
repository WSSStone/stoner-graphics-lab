# Feature Specification: RHI Core Interfaces

**Feature Branch**: `007-rhi-core-interfaces`  
**Created**: 2026-06-25  
**Status**: Draft  
**Input**: User description: "根据roadmap制定下一个大feature的spec"

## Clarifications

### Session 2026-06-25

- Q: How should RHI core contracts report recoverable operation failures and invalid lifecycle states? → A: Explicit result/status values for recoverable operations, with clear success/failure states.
- Q: Who owns creation and lifetime of RHI core objects such as queues, command buffers, synchronization primitives, and swapchains? → A: Device is the authoritative factory/owner for RHI core objects.
- Q: What command payload detail belongs in this phase without pulling in resource and pipeline interfaces? → A: Record symbolic draw/dispatch/barrier commands with no concrete resource or pipeline validation.
- Q: How much swapchain behavior belongs in RHI core before real windows and backend surfaces exist? → A: Include headless/mock swapchain contract only: acquire, present, resize-required statuses.
- Q: What test coverage depth is required for RHI core contracts in this feature? → A: Lifecycle-state matrix tests plus negative-path validation for each core contract.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Query Rendering Device Capabilities Uniformly (Priority: P1)

An engine developer can work against a stable rendering hardware interface to discover available device capabilities, queue support, core limits, and device-owned object creation flow without depending on a concrete graphics backend.

**Why this priority**: Device lifecycle and capability discovery are the first contract every backend, renderer, and test double must share. Without this, later resource, pipeline, and Vulkan work cannot agree on supported behavior.

**Independent Test**: Can be tested with a mock rendering device that reports deterministic capabilities and queue support; consumers can query those capabilities and make decisions without any real graphics API.

**Acceptance Scenarios**:

1. **Given** a valid mock rendering device, **When** an engine subsystem queries its capabilities, **Then** the subsystem receives a complete capability snapshot containing supported queues, limits, and feature flags.
2. **Given** a device with limited queue support, **When** a consumer asks whether graphics, compute, transfer, or present work is supported, **Then** the result accurately reflects the device's declared capabilities.
3. **Given** a device that has been shut down, **When** a consumer queries its state, **Then** the device reports a safe inactive state instead of exposing stale capabilities.
4. **Given** a valid rendering device, **When** a consumer requests queues, command buffers, synchronization primitives, or swapchains, **Then** those objects are created or rejected through device-owned lifecycle rules.
5. **Given** device lifecycle matrix tests, **When** each supported and invalid state transition is exercised, **Then** every transition reports the expected explicit status.

---

### User Story 2 - Record and Submit Work Through Core RHI Flow (Priority: P1)

An engine developer can express the lifecycle of command recording, queue submission, and idle waiting through RHI-level contracts so renderer code can be written before a real backend exists.

**Why this priority**: Command recording and queue submission define the central rendering workflow. A mockable contract enables renderer and backend development to proceed independently.

**Independent Test**: Can be tested by recording a deterministic sequence of symbolic draw, dispatch, and barrier requests into a mock command buffer and submitting it to a mock queue that verifies ordering and lifecycle rules without requiring concrete resource or pipeline objects.

**Acceptance Scenarios**:

1. **Given** an idle command buffer, **When** recording begins, symbolic work commands are added, and recording ends, **Then** the recorded command sequence is accepted as complete and ready for submission.
2. **Given** a completed command buffer, **When** it is submitted to a compatible queue, **Then** the queue records the submission and exposes completion or idle status through the RHI contract.
3. **Given** an invalid command lifecycle such as submitting before recording ends, **When** the action is attempted, **Then** the contract reports failure or invalid state without crashing.

---

### User Story 3 - Coordinate Work With Synchronization Primitives (Priority: P2)

An engine developer can represent CPU/GPU and GPU/GPU synchronization concepts through RHI-level fences and semaphores, enabling safe ordering without exposing backend-specific details.

**Why this priority**: Synchronization is required for reliable frame execution, but the first MVP can still validate device and command flow before all synchronization behavior is exercised.

**Independent Test**: Can be tested with mock fences and semaphores that transition through unsignaled, signaled, waited, and reset states while queues observe those states during submission.

**Acceptance Scenarios**:

1. **Given** an unsignaled fence, **When** submitted work completes, **Then** the fence can be observed as signaled.
2. **Given** a signaled fence, **When** a consumer waits on it, **Then** the wait completes successfully within the expected validation window.
3. **Given** a semaphore used to order queued work, **When** a queue submission references it, **Then** the submission records the dependency without exposing backend-specific handles.

---

### User Story 4 - Present Frames Through a Swapchain Contract (Priority: P2)

An engine developer can model headless or mock image acquisition, presentation, and resize-required behavior through an RHI swapchain contract so application and renderer integration can be specified before platform-specific presentation exists.

**Why this priority**: Swapchain behavior is needed for the first real display path, but it depends on device and queue contracts being stable first.

**Independent Test**: Can be tested with a headless mock swapchain that provides image indices, accepts presentation requests, and reports resize requirements under controlled conditions without binding to a real window or backend surface.

**Acceptance Scenarios**:

1. **Given** a valid headless mock swapchain, **When** the renderer requests the next image, **Then** it receives an image acquisition result that can be used for the next presentation.
2. **Given** a prepared frame, **When** the renderer presents it, **Then** the swapchain accepts the present request and advances its frame state.
3. **Given** a simulated size change or invalidated presentation state, **When** acquisition or presentation occurs, **Then** the swapchain reports that recreation or resize is required.

### Edge Cases

- Device capability queries must be safe before initialization, after shutdown, and for devices with partial feature support.
- Command buffers must reject or clearly report invalid lifecycle transitions such as double begin, end without begin, record after end, submit while recording, or reuse without reset.
- Symbolic command recording must not require concrete resource handles, pipeline state objects, descriptor sets, or shader modules in this phase.
- Queue submissions must clearly report incompatible queue types or missing required synchronization.
- Fence and semaphore state transitions must remain deterministic across repeated wait, signal, and reset operations.
- Swapchain acquisition and presentation must distinguish successful frame flow from resize-required, unavailable, and invalid-state outcomes.
- Swapchain behavior must remain headless and mockable in this phase, with no requirement to bind a real native window, platform surface, or graphics backend surface.
- Recoverable failures and invalid lifecycle attempts must produce explicit result/status values that tests can assert without relying on exceptions or debug-only assertions.
- Each RHI core contract must have lifecycle-state matrix coverage and negative-path validation for invalid or unsupported actions.
- Mock implementations must be able to verify contracts without creating a real rendering device or display surface.

## Architecture & Design Constraints *(mandatory)*

- **RHI Abstraction**: This feature defines the RHI layer contract. It MUST NOT require Renderer, Application, Backend, Vulkan, Metal, DX12, OpenGL, or platform-window implementation details in public RHI-facing behavior.
- **Design Patterns**: Device, command recording, queues, synchronization, and swapchain responsibilities MUST remain separate. The feature MUST avoid a single catch-all object that owns unrelated rendering responsibilities.
- **Advanced Graphics**: The core contracts MUST leave room for future ray tracing, meshlet, compute, and global illumination workflows without requiring those advanced pipelines in this phase.
- **Naming Conventions**: Public concepts MUST follow the project's UE5-style naming conventions, including interface, enum, and value-object naming consistent with the roadmap.
- **Cross-Platform Compatibility**: The feature MUST describe behavior that can be satisfied consistently on Windows, macOS, and Linux through backend implementations.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST define a rendering device contract that exposes lifecycle state, supported queue types, device capabilities, and feature limits.
- **FR-002**: System MUST allow consumers to query whether graphics, compute, transfer, and present queue work is supported by a device.
- **FR-002a**: System MUST make the rendering device the authoritative creation and lifetime owner for queues, command buffers, fences, semaphores, and swapchains.
- **FR-003**: System MUST define a command buffer lifecycle covering idle, recording, completed, submitted, and resettable states.
- **FR-004**: System MUST allow command buffers to represent symbolic draw, dispatch, and barrier-style work requests at the RHI contract level without validating concrete resources or pipeline state objects.
- **FR-005**: System MUST reject or report invalid command buffer lifecycle transitions without crashing.
- **FR-006**: System MUST define a command queue contract that accepts compatible completed command buffers for submission.
- **FR-007**: System MUST define queue idle and completion observation behavior so callers can determine whether submitted work has finished.
- **FR-008**: System MUST define fence behavior for signal, wait, reset, and status observation.
- **FR-009**: System MUST define semaphore behavior for ordering queue submissions without exposing backend-specific handles.
- **FR-010**: System MUST define a headless/mockable swapchain contract for acquiring the next presentable frame, presenting a completed frame, and reporting resize or recreation requirements.
- **FR-011**: System MUST define a portable format catalog for color, depth, stencil, and common data formats needed by core device and swapchain capabilities.
- **FR-012**: System MUST provide deterministic mock-test behavior for every public RHI core contract in this feature.
- **FR-013**: System MUST keep resource and pipeline creation and validation outside this feature; command recording in this phase MUST remain symbolic.
- **FR-014**: System MUST keep actual graphics backend calls outside this feature.
- **FR-015**: System MUST expose explicit result/status values for recoverable operation outcomes, including success, invalid state, unsupported capability, timeout, and resize-required style states where applicable.
- **FR-016**: System MUST NOT require real native window binding, platform surface binding, or backend surface ownership for swapchain tests in this phase.
- **FR-017**: System MUST include lifecycle-state matrix tests and negative-path validation for each RHI core contract introduced by this feature.

### Key Entities *(include if feature involves data)*

- **Rendering Device**: Represents an initialized or mock rendering endpoint, including lifecycle state, queue support, capability information, and ownership of RHI core object creation.
- **Device Capabilities**: Represents limits and feature support that consumers use to choose rendering paths.
- **Command Buffer**: Represents a unit of symbolic rendering or compute work with strict lifecycle state.
- **Command Queue**: Represents an execution lane for submitted command buffers, associated with a queue type.
- **Fence**: Represents observable completion for submitted work.
- **Semaphore**: Represents ordering between queued work items.
- **Swapchain**: Represents a headless/mockable presentation contract for acquiring and presenting frames and reporting resize or recreation requirements.
- **Format**: Represents portable data and image format identities used by capabilities and presentation flow.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer can validate device lifecycle and capability discovery through a mock device in under 5 minutes using the project test executable.
- **SC-002**: Mock-based tests cover 100% of public RHI core contracts introduced by this feature, including lifecycle-state matrices and negative-path validation.
- **SC-003**: Invalid lifecycle actions for command buffers, queues, fences, semaphores, and swapchains are detected without process crashes in all test scenarios.
- **SC-004**: A renderer-facing smoke test can record, complete, submit, and observe a mock frame workflow with zero backend-specific dependencies.
- **SC-005**: Public RHI core behavior can be reviewed without reference to any concrete graphics API or platform-specific presentation implementation.
- **SC-006**: The feature supports at least graphics, compute, transfer, and present queue classifications in capability queries and validation scenarios.

## Assumptions

- Phase 006 in the roadmap is the next feature after the completed Core platform abstraction work, but the Speckit directory and branch number for this new spec are `007` because `006-core-platform-abstraction` already exists in the repository.
- Core foundation, math, logging, and platform abstraction features are available as dependencies.
- Resource objects, pipeline state objects, descriptors, shader modules, and render passes are deferred to the next RHI feature.
- Real Vulkan, Metal, DX12, OpenGL, native window, or platform surface integration is deferred to backend and application phases.
- Mock implementations are sufficient for this feature's acceptance testing and must not require a real GPU or display surface.
