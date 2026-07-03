# Feature Specification: Window & Input System

**Feature Branch**: `016-window-input-system`  
**Created**: 2026-07-03  
**Status**: Implemented  
**Input**: User description: "Create the next major feature specification from the roadmap. Roadmap phase: Application Window & Input System."

## Clarifications

### Session 2026-07-03

- Q: Should v1 keyboard input represent physical key/button state only, include text character input, or normalize by user-visible keyboard layout? → A: Physical key/button state only; text input and keyboard layout translation are out of scope.
- Q: What validation level is required for environments without display access? → A: Deterministic headless validation is required, with real-window smoke tests when display access is available.
- Q: How should the application loop behave when the window is minimized or has zero drawable size? → A: Keep polling events and updating, but pause presentation until a drawable size returns.
- Q: How should held input state behave when the window loses focus? → A: Clear all held keys and mouse buttons immediately on focus loss.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Create and Control the Primary Window (Priority: P1)

An application developer can create a single primary application window with a clear title, requested client size, visibility state, and display mode, then observe its lifecycle until the user requests close or the application shuts it down.

**Why this priority**: A renderable application cannot present visual output or reach the first interactive demo milestone without a reliable window lifecycle.

**Independent Test**: Can be fully tested through deterministic lifecycle validation, plus a real-window smoke test when display access is available, by creating a primary window from a valid configuration, observing that it becomes active, requesting close, and confirming clean teardown without involving rendering.

**Acceptance Scenarios**:

1. **Given** a valid window configuration, **When** the application creates the primary window, **Then** the window becomes active with the requested title, size, mode, and visible state.
2. **Given** an active window, **When** the user requests close, **Then** the window reports a close-requested state that the application can consume before teardown.
3. **Given** an active window, **When** the application destroys it, **Then** the window becomes inactive and repeated destroy requests remain safe and predictable.

---

### User Story 2 - Read Frame-Based Keyboard and Mouse State (Priority: P1)

An application developer can poll input once per frame and read deterministic keyboard and mouse state, including held, pressed, released, pointer position, and pointer delta information.

**Why this priority**: Interactive demos and tools need stable per-frame input semantics before scene control, camera movement, or editor workflows can be built.

**Independent Test**: Can be fully tested by feeding a known sequence of keyboard and mouse events across frames and verifying the resulting input state after each poll.

**Acceptance Scenarios**:

1. **Given** no key is active, **When** a key-down event is processed during a frame, **Then** that key is reported as pressed and held for that frame.
2. **Given** a key was held in the previous frame, **When** no key-up event is processed, **Then** the key remains held and is not reported as newly pressed again.
3. **Given** a held mouse button, **When** a button-up event is processed, **Then** that button is reported as released for that frame and no longer held afterward.
4. **Given** pointer movement occurs between frames, **When** input is polled, **Then** the current pointer position and per-frame delta match the event sequence.
5. **Given** keys or mouse buttons are held, **When** the window loses focus, **Then** all held keyboard and mouse-button state is cleared immediately and no stuck input remains on later frames.

---

### User Story 3 - React to Window Events in the Application Loop (Priority: P2)

An application developer can run a minimal application loop that polls events, updates input state, observes resize and close notifications, and exits cleanly when requested.

**Why this priority**: The feature must bridge window lifecycle and frame execution so later rendering integration can respond to resize and shutdown conditions.

**Independent Test**: Can be fully tested by running a loop with deterministic simulated events, plus real-window smoke coverage when display access is available, and verifying the order of poll, update, event observation, and shutdown decisions.

**Acceptance Scenarios**:

1. **Given** an active window, **When** the user resizes it, **Then** the next event poll exposes the latest client size and a resize notification.
2. **Given** several resize events arrive before the next frame, **When** the application polls events, **Then** the final observed size is deterministic and no stale final size is reported.
3. **Given** the user requests close during a frame, **When** the loop reaches its shutdown decision point, **Then** it can exit without processing additional frames unnecessarily.
4. **Given** the active window is minimized or reports zero drawable size, **When** the loop polls events, **Then** input and update processing may continue while presentation is paused until a drawable size returns.

---

### User Story 4 - Fail Safely on Unsupported Runtime Conditions (Priority: P3)

An application developer receives clear diagnostics when a window cannot be created or an input event cannot be represented, and unsupported v1 scope is bounded instead of silently misbehaving.

**Why this priority**: Cross-platform development and automated validation need predictable behavior when display access, input devices, or requested modes are unavailable.

**Independent Test**: Can be fully tested by requesting invalid configurations, unsupported display modes, unknown input identifiers, and no-display execution, then verifying diagnostics and safe fallback behavior.

**Acceptance Scenarios**:

1. **Given** an invalid window size, **When** creation is requested, **Then** creation is rejected with a diagnostic and no partially active window remains.
2. **Given** display access is unavailable, **When** a real window is requested, **Then** the system reports the unavailable runtime condition without crashing.
3. **Given** an unknown key or mouse input is received, **When** input is polled, **Then** it is represented as an unknown input and does not corrupt known input state.

### Edge Cases

- Window creation is requested with zero, negative, or dimensions above the v1 default safe maximum of 16384x16384 client pixels.
- A resize event reports a minimized or zero-sized client area; event polling and updates continue, while presentation is paused until a drawable size returns.
- Multiple resize events arrive before a frame polls events.
- Close is requested while input events are still pending.
- Fullscreen or display-mode changes are requested but the platform cannot satisfy them.
- The window loses focus while keys or mouse buttons are held; all held keyboard and mouse-button state is cleared immediately.
- Pointer movement occurs before an initial pointer position is known.
- Input events include keys or buttons outside the engine's supported vocabulary.
- Event polling is called before window creation or after window destruction.
- Automated test environments cannot open a real display-backed window.

## Architecture & Design Constraints *(mandatory)*

- **RHI Abstraction**: The feature MUST NOT bypass the RHI layer to call Graphics APIs directly.
- **Design Patterns**: The feature MUST avoid God-classes and utilize Strategy/Composite patterns for orthogonal responsibilities.
- **Advanced Graphics**: The feature MUST consider compatibility with Ray Tracing, Meshlets, and Global Illumination pipelines.
- **Naming Conventions**: The feature's code design MUST adhere to PascalCase, UnrealEngine5-style naming conventions.
- **Cross-Platform Compatibility**: The feature MUST compile and run on all supported platforms (Windows, macOS, Linux). Platform-specific code MUST be isolated behind abstraction layers or conditional compilation guards.
- **Automated Cross-Platform Validation**: Because this feature affects windowing, input, runtime startup, and platform-sensitive behavior, it MUST include or update a GitHub Actions or equivalent CI validation path for Windows, macOS, and Linux deterministic headless build/test coverage, while keeping real-window smoke validation optional or unavailable-safe.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST allow an application developer to create one primary application window from a configuration containing title, client size, display mode, and initial visibility.
- **FR-002**: System MUST validate window creation requests and reject invalid configurations with an actionable diagnostic and no partially active window; v1 MUST define a stable default safe maximum client size of 16384x16384 pixels and treat dimensions above that limit as invalid unless a later feature explicitly revises the bound.
- **FR-003**: System MUST expose window lifecycle states including uncreated, active, close requested, and destroyed.
- **FR-004**: System MUST allow the application to request window close and to observe user-requested close without forcing immediate teardown.
- **FR-005**: System MUST support resize observation, including the latest client width and height and whether the window is minimized or temporarily has no drawable area.
- **FR-006**: System MUST support switching the primary window between windowed and fullscreen-style presentation modes when the runtime environment allows it, including an observable successful transition path in deterministic headless validation.
- **FR-007**: System MUST report display-mode failures without losing the previous valid window state.
- **FR-008**: System MUST provide a per-frame input snapshot for keyboard keys, mouse buttons, pointer position, and pointer delta.
- **FR-009**: System MUST distinguish pressed, released, and held states for supported keys and mouse buttons.
- **FR-010**: System MUST reset transient pressed and released states at frame boundaries while preserving held states until release or focus-loss handling.
- **FR-011**: System MUST define a stable physical input vocabulary covering printable-key positions, navigation keys, function keys, modifier keys, common mouse buttons, and an unknown input value; text character input and keyboard layout translation MUST remain out of scope for v1.
- **FR-012**: System MUST clear all held keyboard and mouse-button states immediately when the window loses focus, preventing stuck held inputs in subsequent frames.
- **FR-013**: System MUST deliver window and input events in deterministic frame order for a given event sequence.
- **FR-014**: System MUST allow a minimal application loop to poll events, update input state, observe window state, and decide whether to continue or shut down.
- **FR-015**: System MUST expose resize and close information in a way that downstream rendering or presentation systems can consume without direct coupling.
- **FR-016**: System MUST provide deterministic headless validation paths for environments where opening a real display-backed window is unavailable, and MUST support real-window smoke validation when display access is available.
- **FR-017**: System MUST expose a presentation-paused state when the window is minimized or has zero drawable size, while allowing event polling and application updates to continue.
- **FR-018**: System MUST keep v1 scope to a single primary window and keyboard/mouse input; multi-window, gamepad/controller input, text input composition, and debug UI integration MUST be excluded from this feature.

### Key Entities

- **Window Configuration**: Desired initial title, client size, display mode, visibility, and validation boundaries for the primary window.
- **Application Window**: The active runtime window, its lifecycle state, current client size, display mode, focus state, and close-request status.
- **Window Event**: A lifecycle or size-related change such as created, resized, minimized, focus changed, close requested, or destroyed.
- **Input Identifier**: A stable physical key or mouse-button identifier, including an unknown value for unrepresented inputs; it does not represent text characters or localized keyboard-layout output.
- **Input Event**: A timestamped or ordered keyboard, mouse-button, or pointer movement change received during event polling.
- **Input Frame State**: The current frame's derived input snapshot, including held, pressed, released, pointer position, and pointer delta.
- **Application Loop State**: The per-frame decision context that records whether the loop should continue, pause presentation due to no drawable area while still polling/updating, or exit.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer can create, observe, close, and destroy a primary window through the documented flow in under 10 minutes.
- **SC-002**: 100% of covered keyboard and mouse transition tests produce the expected pressed, held, released, pointer position, and pointer delta states.
- **SC-003**: Resize and close notifications become observable no later than the next event polling cycle in all covered scenarios.
- **SC-004**: Invalid window configurations, unavailable display access, unsupported display-mode changes, and unknown input identifiers complete without crashes in all covered tests.
- **SC-005**: A representative minimal application loop can run for at least 300 frames or until close request, whichever comes first, without lifecycle or input-state inconsistencies.
- **SC-006**: The same feature behavior is verifiable on Windows, macOS, and Linux through deterministic headless validation, with a CI or equivalent platform matrix covering all three operating systems; real-window smoke validation passes when display access is available and no-display environments report a controlled unavailable-runtime result.

## Assumptions

- The primary user is an application or engine developer building the first interactive demo and later editor/runtime tooling.
- This feature owns one primary window for v1; multi-window behavior is intentionally deferred.
- Keyboard and mouse physical state input is sufficient for the first interactive milestones; gamepad/controller input, text composition, and keyboard layout translation are deferred.
- Rendering presentation integration consumes resize and close information later, but real rendering output is not required for this feature to be valuable.
- Automated validation may run without display access, so deterministic headless validation is required; real-window smoke validation is required whenever the execution environment provides display access.
- No persistent input recording, replay, or user preference storage is required.

## Implementation Notes

- Implemented 2026-07-03.
- Local macOS verification passed with `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`.
- Cross-platform automated validation is defined in `.github/workflows/ci.yml` for Linux, macOS, and Windows deterministic headless build/test coverage.
