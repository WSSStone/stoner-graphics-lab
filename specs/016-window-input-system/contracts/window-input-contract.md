# Contract: Window & Input System

## Scope

This contract describes the Application-layer public behavior for creating and controlling one primary window, polling deterministic physical keyboard/mouse input, observing resize/close/focus events, running a minimal application loop, reporting no-display conditions, and producing diagnostics/debug dumps. It is a C++ library contract, not a network, file format, command-line, renderer presentation, swapchain, backend execution, or native window handle contract.

## Public Types

### `FWindowDesc`

Describes requested initial window state.

Required behavior:

- Store title, requested client size, display mode, initial visibility, and optional debug name.
- Validate positive client dimensions no larger than the v1 default safe maximum of 16384x16384 pixels before creation.
- Provide deterministic defaults for omitted non-critical fields.
- Avoid native platform handles, third-party window objects, or graphics API objects.

### `FWindow`

Represents the single primary application window.

Required behavior:

- Create from a valid `FWindowDesc`.
- Expose lifecycle states: Uncreated, Active, CloseRequested, and Destroyed.
- Expose latest client size, focus state, minimized state, drawable availability, and presentation-paused state.
- Allow application-requested close and user-requested close to be observed without immediate forced teardown.
- Destroy safely and idempotently.
- Report unavailable runtime conditions without leaving a partially active window.

### `FWindowEvent`

Represents window lifecycle and size notifications.

Required behavior:

- Represent created, resized, minimized, restored, focus gained, focus lost, close requested, destroyed, and unavailable runtime events.
- Preserve deterministic event order for a given event sequence.
- Coalesce latest client size into window state when multiple resize events arrive before a frame poll.
- Treat minimized or zero drawable size as presentation-paused, not as a fatal resize error.

### `EKey`

Represents stable physical keyboard identifiers.

Required behavior:

- Include printable-key positions, navigation keys, function keys, modifier keys, and Unknown.
- Represent physical key state only.
- Exclude text character input, keyboard layout translation, and input method composition from v1.

### `EMouseButton`

Represents stable physical mouse-button identifiers.

Required behavior:

- Include common mouse buttons and Unknown.
- Represent physical button state only.
- Avoid platform-specific button codes in public behavior.

### `FInputEvent`

Represents ordered physical input changes.

Required behavior:

- Represent key down/up, mouse button down/up, pointer movement, scroll, and unknown input events.
- Preserve deterministic ordering.
- Allow Unknown input without corrupting known input state.

### `FInputState`

Represents the current frame's derived input snapshot.

Required behavior:

- Expose held, pressed, and released states for supported keys and mouse buttons.
- Expose pointer position and per-frame pointer delta.
- Reset pressed and released transient states at frame boundaries.
- Preserve held states until release or focus loss.
- Clear all held keyboard and mouse-button state immediately on focus loss.

### `FInputManager`

Coordinates input event ingestion and frame-state derivation.

Required behavior:

- Poll or accept ordered input events once per frame.
- Produce deterministic `FInputState` snapshots.
- Return safe empty state with diagnostics when polled before window creation or after window destruction.
- Keep physical key/button state independent from text input or keyboard layout output.

### `FApplicationLoop`

Represents the minimal loop skeleton for polling events, updating input state, observing window state, and deciding whether to continue.

Required behavior:

- Poll window/input events before frame decision output.
- Expose whether the loop should continue, exit, or pause presentation.
- Continue event polling and application update work while presentation is paused due to minimized or zero drawable size.
- Exit cleanly when close is requested at a loop decision point.
- Run without renderer presentation, swapchain recreation, or real GPU work.

### `FApplicationDiagnostics`

Collects deterministic diagnostics for window/input/loop behavior.

Required behavior:

- Include stable diagnostic codes, severity, category, subject name, and human-readable message.
- Identify invalid window descriptions, unavailable display/runtime, unsupported display-mode transitions, unknown input identifiers, unsafe lifecycle calls, focus-loss state clearing, and presentation-paused behavior.
- Preserve deterministic ordering.

## Window Lifecycle Contract

Input:

- A window description.
- Window driver availability.
- Ordered window events.

Output:

- Success with an Active window and deterministic lifecycle diagnostics.
- Failure with diagnostics and no partially active window when validation or runtime availability fails.

Required validation:

- Reject zero, negative, or client dimensions above the v1 default safe maximum of 16384x16384 pixels.
- Reject invalid lifecycle transitions with diagnostics.
- Preserve previous valid window state when fullscreen-style mode changes fail.
- Report display-unavailable or dependency-unavailable runtime conditions distinctly.

## Input Processing Contract

Input:

- Ordered physical keyboard, mouse-button, pointer, scroll, focus, and unknown input events.

Output:

- Deterministic frame input snapshots.
- Diagnostics for unknown or invalid inputs.

Required behavior:

- Key/button down creates pressed and held state for that frame.
- Held key/button remains held on following frames until release or focus loss.
- Key/button up creates released state and clears held state.
- Focus loss clears all held keyboard and mouse-button state immediately.
- Pointer movement updates current position and per-frame delta.
- Unknown inputs do not corrupt known input state.

## Event Polling and Resize Contract

Required behavior:

- Polling returns deterministic event order for a given driver event sequence.
- Multiple resize events before one frame leave the final window state at the latest client size.
- Resize notification remains observable no later than the next poll cycle.
- Minimized or zero drawable size sets presentation-paused while allowing event polling and update processing to continue.

## Application Loop Contract

Preconditions:

- A valid Application window exists or a deterministic headless scenario is configured.
- Input manager is available.

Required behavior:

- Poll events, derive input state, observe window state, and report loop decision in that order.
- Continue running until close is requested or the caller stops the loop.
- Support a representative 300-frame validation run without lifecycle or input inconsistencies.
- Never require renderer presentation, swapchain recreation, or graphics API execution.

## Headless Validation Contract

Required behavior:

- Provide deterministic lifecycle, resize, focus, close, keyboard, mouse, and pointer scenarios without display access.
- Produce the same diagnostics, state transitions, and dumps across repeated equivalent runs.
- Allow tests to verify real-window-unavailable behavior without crashing.

## Real-Window Smoke Contract

Required behavior:

- Run only when the real-window dependency and display access are available.
- Create a primary window, poll at least one frame, observe current size/focus/lifecycle state, request close, and destroy cleanly.
- Report skipped or unavailable status distinctly when display access or dependency support is absent.

## Boundary Rules

- Public Application window/input contracts must not expose GLFW, Win32, Cocoa, X11, Wayland, Vulkan, Metal, DX12, DirectX, OpenGL, GLES, WebGL, swapchain, renderer presentation, or native handle types.
- v1 supports one primary window only.
- Text input, keyboard layout translation, IME composition, gamepad/controller input, multi-window support, debug UI, renderer presentation, swapchain recreation, and triangle rendering are out of scope.
