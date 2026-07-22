# Feature Specification: Triangle Demo Integration Milestone

**Feature Branch**: `018-triangle-demo-integration`
**Created**: 2026-07-20
**Status**: Approved
**Input**: User description: "Create the next major feature specification from the roadmap. Roadmap phase: Application Triangle Demo integration milestone."

## Clarifications

### Session 2026-07-20

- Q: Which platforms require real visible presentation before Feature 018 is complete? → A: Windows and macOS require real-window triangle validation; Linux requires successful CI build and headless integration execution only. Android is not counted in this feature's supported platform set.
- Q: How should the demo terminate during automated and smoke validation? → A: Run interactively by default, and provide a validation mode with a configurable frame budget that exits automatically after successful completion. Planning research will select CI and smoke defaults that may exceed 300 frames to improve memory-leak sensitivity.
- Q: How should endurance validation determine that memory and resources are not leaking? → A: Require demo-owned live resource counts to return to zero after shutdown and require steady-state process memory growth to remain within a configured platform-aware limit during the bounded run.
- Q: What runtime depth must Linux CI cover without physical graphics hardware? → A: Run both deterministic headless tests and a no-window integration path that executes the real Vulkan backend through a CPU software Vulkan device; visible Linux presentation is not required.
- Q: What evidence proves real pixel presentation on Windows and macOS? → A: A human validates the triangle shape and distinguishable vertex colors on each platform, retaining a screenshot and the matching run log; automated golden-image comparison is not required.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Launch the First Visible Engine Demo (Priority: P1)

An engine developer can build and launch a standalone demo that opens one window and displays a stable, correctly colored triangle through the complete engine rendering path.

**Why this priority**: This is the project's first end-to-end visual milestone. It proves that the independently developed Application, Renderer, RHI, and initial graphics backend layers can cooperate to produce visible output.

**Independent Test**: The visible behavior can be tested independently on either Windows or macOS by launching the demo, confirming that a non-degenerate triangle with distinguishable vertex colors appears inside the window, retaining a screenshot and matching run log, and closing the application normally. Full User Story 1 completion requires successful evidence from both platforms through User Story 4.

**Acceptance Scenarios**:

1. **Given** a supported graphics runtime and display, **When** the developer launches the demo, **Then** one visible window opens and a colored triangle is presented within five seconds.
2. **Given** the demo has initialized successfully, **When** frames are rendered, **Then** the triangle remains visible and stable while its vertex colors are distinguishable.
3. **Given** the visible demo path is requested but real presentation cannot be initialized, **When** startup runs, **Then** the demo reports a clear failure and does not claim success through a simulated or headless fallback.

---

### User Story 2 - Keep Rendering Through Window Changes (Priority: P2)

An engine developer can interact with the demo window while the application maintains a valid frame lifecycle across resizing, minimization, restoration, and temporary presentation unavailability.

**Why this priority**: A triangle that appears only under ideal startup conditions does not validate a usable application loop or presentation lifecycle.

**Independent Test**: Can be tested by repeatedly resizing, minimizing, and restoring the window, then verifying that event processing remains responsive, presentation pauses when no drawable area exists, and rendering resumes when a valid drawable area returns.

**Acceptance Scenarios**:

1. **Given** the triangle is rendering, **When** the window is resized to a new non-zero drawable size, **Then** presentation resources are refreshed as needed and the triangle resumes at the new size without restarting the application.
2. **Given** the window is minimized or has zero drawable size, **When** the application loop continues, **Then** input and window events are still processed while draw submission and presentation remain paused.
3. **Given** presentation is paused because no drawable area is available, **When** a valid drawable size returns, **Then** rendering resumes without duplicating, leaking, or using invalid frame resources.
4. **Given** a frame cannot be acquired or presented because presentation resources became stale, **When** the condition is recoverable, **Then** the demo rebuilds the affected presentation state and continues rendering.

---

### User Story 3 - Exit Cleanly and Diagnose Failures (Priority: P2)

An engine developer can close the demo from normal operation or encounter an initialization/runtime failure and receive a deterministic outcome without a hang, crash, or ambiguous partial success.

**Why this priority**: The integration milestone must prove resource ownership and failure handling across layer boundaries, not only the successful draw path.

**Independent Test**: Can be tested by closing the running window, requesting exit through the supported input action, and injecting representative startup and frame failures while verifying stable results, diagnostics, and cleanup.

**Acceptance Scenarios**:

1. **Given** the demo is running, **When** the user closes the window or requests exit through the supported input action, **Then** the frame loop stops and all owned resources are released in a valid dependency order.
2. **Given** required runtime, presentation, shader, buffer, pipeline, or frame resources cannot be created, **When** initialization fails, **Then** the demo identifies the failed stage, releases previously created resources, and exits with a failure result.
3. **Given** a non-recoverable error occurs during frame processing, **When** the error is detected, **Then** no later stages of that frame execute and shutdown remains clean.
4. **Given** the same deterministic failure is exercised repeatedly, **When** diagnostics are inspected, **Then** the result category and stage ordering remain stable across runs.
5. **Given** a positive frame budget is supplied in validation mode, **When** that many drawable frames complete without failure, **Then** the demo exits automatically with a success result; an earlier failure exits with a failure result.
6. **Given** validation mode has completed its warm-up interval, **When** the remaining frame budget runs, **Then** demo-owned live resource counts remain bounded, steady-state process memory growth stays within the configured platform-aware limit, and all demo-owned live resource counts reach zero after shutdown.

---

### User Story 4 - Validate the Integration Across Supported Platforms (Priority: P3)

An engine maintainer can validate the demo integration on Windows, macOS, and Linux even when an automated runner has no display or graphics device available.

**Why this priority**: The milestone crosses platform-sensitive window, runtime, shader, and presentation boundaries; portability regressions must be caught before advanced rendering work begins.

**Independent Test**: Can be tested by running deterministic headless integration coverage on all supported platforms plus required real-window triangle smoke runs on Windows and macOS.

**Acceptance Scenarios**:

1. **Given** a headless automated runner, **When** deterministic integration validation runs, **Then** initialization, frame ordering, resize/minimize policy, failure handling, and cleanup are verified without requiring visible presentation.
2. **Given** a Windows or macOS display-capable validation environment, **When** the required real-window smoke path runs, **Then** a human confirms the triangle shape and distinguishable vertex colors and retains a screenshot plus the matching run log to distinguish actual presentation success from simulated execution.
3. **Given** the Linux automated environment has no physical GPU or visible display, **When** validation runs, **Then** both deterministic headless coverage and a no-window integration path execute successfully, with the latter using a CPU software Vulkan device to exercise the real backend.
4. **Given** a supported desktop platform build, **When** demo assets and runtime prerequisites are checked, **Then** missing or incompatible inputs fail with actionable diagnostics rather than platform-specific compile or startup crashes.

### Edge Cases

- The selected machine has no compatible graphics adapter, loader, display, or presentation queue.
- The window is closed while initialization is only partially complete.
- The drawable size is zero at startup and becomes valid later.
- The window is resized repeatedly while a frame is being prepared.
- Frame acquisition or presentation reports that its current presentation state is stale or temporarily unavailable.
- The requested number of frames in flight exceeds runtime capabilities.
- Required shader payloads are missing, empty, malformed, or incompatible with the declared pipeline inputs.
- Vertex data upload fails or completes later than the first attempted draw.
- A command, synchronization object, render target, or presentation image is used after invalidation.
- The application receives an exit request while presentation is paused.
- A headless test path succeeds while the real presentation path is unavailable; the two outcomes remain explicitly distinguishable.
- Diagnostics are emitted from several layers for one root failure; the primary failing stage remains identifiable without duplicate success claims.

## Architecture & Design Constraints *(mandatory)*

- **RHI Abstraction**: Application and Renderer behavior MUST NOT bypass the RHI layer to call graphics APIs directly; backend-specific presentation work remains behind the backend boundary.
- **Adjacent Layer Integration**: The demo MUST exercise the intended adjacent-layer path from Application to Renderer to RHI to the selected backend, with Core services shared through their existing boundaries.
- **Design Patterns**: The feature MUST avoid God-classes and keep application-loop policy, frame preparation, resource ownership, and backend execution as separable responsibilities. Runtime selection and window drivers MUST use explicit Strategy boundaries. The Demo Application MUST act as a lifecycle Composite whose Presentation State, Triangle Resource Set, and Frame Context collection are independently owned children released through one deterministic reverse-order shutdown contract.
- **Advanced Graphics**: The integration path MUST preserve extension points for later deferred rendering, meshlets, ray tracing, and global illumination without implementing those features here.
- **Naming Conventions**: The feature's code design MUST adhere to PascalCase, UnrealEngine5-style naming conventions.
- **Cross-Platform Compatibility**: The feature MUST compile and provide equivalent lifecycle behavior on Windows, macOS, and Linux. Platform-specific code and runtime requirements MUST remain isolated behind existing abstraction boundaries.
- **Automated Cross-Platform Validation**: The feature MUST include or update automated Windows, macOS, and Linux build and headless integration validation. Real-window triangle presentation MUST additionally be validated on Windows and macOS. Linux CI MUST run both deterministic headless coverage and no-window real-backend integration through a CPU software Vulkan device, but does not require graphics-capable presentation for this feature.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The project MUST provide a standalone demo application that can be built and launched independently from the test executable.
- **FR-002**: The demo MUST create and own one primary visible window through the existing Application window boundary.
- **FR-003**: The visible demo path MUST initialize a real graphics device and presentation path through the established Renderer, RHI, and backend boundaries; it MUST NOT report visible success when only simulated execution occurred.
- **FR-004**: The demo MUST define exactly one non-degenerate triangle with three vertices and distinguishable per-vertex colors.
- **FR-005**: The demo MUST provide the required vertex and fragment processing inputs as repository-owned, reproducible application assets and MUST reject missing, empty, malformed, or incompatible inputs before drawing.
- **FR-006**: The demo MUST create and populate the graphics resource required to supply triangle vertex data before that resource is used by a draw.
- **FR-007**: The demo MUST prepare a forward-rendering frame containing one triangle draw and one final presentation output through the existing Renderer and render-graph boundaries.
- **FR-008**: The backend execution path MUST translate the prepared frame into an actual render-target transition, render scope, pipeline and vertex-data binding, triangle draw, and presentation-ready transition without exposing backend API objects to Application or Renderer callers.
- **FR-009**: Each drawable frame MUST process window/input events and then perform frame acquisition, command preparation, submission, completion coordination, and presentation in a valid deterministic order.
- **FR-010**: The demo MUST prevent frame resources and presentation images from being reused while prior work that owns them remains incomplete.
- **FR-011**: The demo MUST react to non-zero window size changes and recoverable stale-presentation conditions by refreshing only the affected presentation-dependent state before rendering resumes.
- **FR-012**: While the window is minimized or has zero drawable size, the demo MUST continue processing events and input while skipping draw submission and presentation.
- **FR-013**: The demo MUST resume rendering after a valid drawable size returns without restarting the process or retaining invalid presentation-dependent resources.
- **FR-014**: The demo MUST run interactively by default and allow the user to terminate it through the window close action or the project's supported physical keyboard exit action; a separate validation mode MUST accept a configurable positive frame budget and exit automatically with success only after completing that budget without failure.
- **FR-015**: Normal exit, partial initialization failure, and non-recoverable frame failure MUST release owned resources in dependency-safe order and produce an unambiguous success or failure result.
- **FR-016**: Diagnostics MUST identify the lifecycle stage responsible for initialization, frame, recovery, or shutdown failures without exposing unstable native object addresses as identifiers.
- **FR-017**: The feature MUST provide deterministic headless integration coverage for initialization ordering, frame ordering, resize/minimize handling, recoverable and fatal failures, and cleanup when real presentation is unavailable.
- **FR-018**: Automated validation MUST build the demo and run applicable deterministic integration coverage on Windows, macOS, and Linux; Feature 018 completion MUST include human-verified real-window triangle presentation on Windows and macOS with a retained screenshot and matching run log for each platform, while Linux CI MUST additionally execute the real Vulkan backend without a window through a CPU software Vulkan device.
- **FR-019**: Validation mode MUST report demo-owned live resource counts and sample process memory after a configurable warm-up interval; successful endurance validation MUST keep steady-state memory growth within a configured platform-aware limit and normal shutdown MUST leave zero demo-owned live resources.
- **FR-020**: Existing Core, RHI, backend, Renderer, Application, and scene/ECS regression outcomes MUST remain passing after the integration feature is added.
- **FR-021**: The feature MUST keep scope to one hardcoded colored triangle, one window, the forward path, and the initial desktop backend; Android and other mobile targets, scene/ECS-driven geometry, asset catalogs, runtime shader compilation, textures, materials beyond vertex color, depth testing, lighting, complex geometry, editor UI, deferred rendering, post-processing, meshlets, ray tracing, and global illumination MUST remain out of scope.

### Key Entities

- **Demo Application**: Owns startup, the primary window, frame-loop coordination, exit intent, diagnostics, and orderly shutdown for the integration milestone.
- **Triangle Geometry**: Three positions and three distinguishable vertex colors representing the complete visible scene for this feature.
- **Demo Shader Asset**: Reproducible vertex and fragment processing inputs plus enough declared metadata to validate compatibility before use.
- **Presentation State**: The current drawable extent, presentation images, output compatibility, and readiness state that may be refreshed after resize or invalidation.
- **Frame Context**: Per-frame acquisition identity, command work, completion coordination, and reuse state required to keep multiple frames from conflicting.
- **Frame Plan**: The Renderer-facing declaration of the triangle draw, output target, render work, and ordering dependencies consumed through the render graph path.
- **Demo Diagnostic**: A stable record of stage, severity, result category, and reason for startup, frame, recovery, or shutdown outcomes.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On both Windows and macOS validation machines, a developer can launch the demo, record no more than 5,000 milliseconds from process startup to the first successful presentation, see one correctly colored non-degenerate triangle, and retain one screenshot plus the matching successful run log as validation evidence.
- **SC-002**: Every successful bounded validation run completes 100% of its configured drawable-frame budget without error-level diagnostics, invalid resource reuse, or presentation-order violations, including when the selected budget exceeds 300 frames.
- **SC-003**: Across at least 20 resize, minimize, and restore cycles, event processing remains responsive and every observed cycle records no more than 2,000 milliseconds from the first observed valid non-zero drawable extent after restore to the first successful presentation of the replacement generation.
- **SC-004**: All covered normal-exit, partial-startup-failure, recoverable-presentation, and fatal-frame scenarios complete without hangs or crashes and leave zero live demo-owned resources after shutdown.
- **SC-005**: For every injected startup or frame failure, a developer can identify the primary failing lifecycle stage from the first stable error diagnostic without inspecting native backend object addresses.
- **SC-006**: Deterministic headless integration runs produce identical frame-stage ordering, result categories, and normalized diagnostic output across 20 repeated executions on the same platform.
- **SC-007**: Windows, macOS, and Linux automated jobs all build the standalone demo and pass their applicable deterministic integration coverage; Windows and macOS additionally have recorded real-window triangle success, while Linux additionally passes a no-window integration run through a CPU software Vulkan device.
- **SC-008**: 100% of the existing regression suite remains passing after the demo integration is introduced.
- **SC-009**: 100% of successful endurance runs remain within their configured steady-state memory-growth limit and report zero demo-owned live resources after shutdown.

## Assumptions

- The primary user is an engine developer validating the first visible end-to-end result before starting advanced renderer phases.
- The initial visible path uses the project's existing first graphics backend; additional native backends remain separate roadmap phases.
- A compatible graphics runtime, adapter, presentation queue, and display are prerequisites for visible success. Their absence is a supported diagnostic outcome, not a successful visible run.
- Automated runners may be headless. Deterministic headless integration coverage validates orchestration and failure behavior on all three desktop platforms; separate display-capable Windows and macOS environments validate actual pixels and presentation.
- Linux CI can provide a CPU software Vulkan implementation, allowing real backend device, resource, and command execution without requiring physical graphics hardware or visible presentation.
- Android is not part of Feature 018's supported platform set; mobile windowing, lifecycle, presentation, and packaging constraints require a later dedicated feature.
- Repository-owned reproducible shader assets are available at build or package time; runtime shader source compilation and shader hot reload are deferred.
- One primary window and a bounded number of frames in flight are sufficient for this milestone.
- The validation frame budget is configurable. Planning research will select separate CI and real-window smoke defaults by balancing execution time against sensitivity to memory and resource-lifecycle leaks; 300 frames is not treated as a fixed upper limit.
- Planning research will define the warm-up interval, sampling method, and platform-aware memory-growth limits so allocator initialization and measurement noise do not create false leak failures.
- The existing window/input policy continues polling while minimized, clears held input on focus loss, and exposes a physical keyboard action suitable for exit.
- Scene/ECS data is intentionally not required for the triangle; the milestone uses fixed demo geometry so integration failures are isolated from scene authoring concerns.
- Manual screenshots and matching run logs are required evidence for Windows and macOS visible smoke validation; automated screenshot capture and golden-image comparison are optional for this phase.
