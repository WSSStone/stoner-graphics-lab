# Data Model: Window & Input System

## Window Description

**Purpose**: Captures the requested initial state for the primary application window.

**Key fields**:

- `Title`: Human-readable window title.
- `ClientWidth`: Requested initial client-area width.
- `ClientHeight`: Requested initial client-area height.
- `DisplayMode`: Windowed or fullscreen-style mode request.
- `bVisible`: Whether the window should be visible after creation.
- `DebugName`: Optional stable identifier for diagnostics and dumps.

**Validation rules**:

- Title must be present or defaulted deterministically.
- Client width and height must be positive and no larger than the v1 default safe maximum of 16384x16384 client pixels.
- Invalid dimensions reject creation with diagnostics and no partially active window.
- Display-mode failure must preserve the previous valid window state.

## Application Window

**Purpose**: Represents the single primary runtime window owned by the Application layer.

**Key fields**:

- `WindowId`: Stable identifier for the primary window.
- `Description`: Original validated window description.
- `LifecycleState`: Uncreated, Active, CloseRequested, or Destroyed.
- `ClientWidth`: Latest client width.
- `ClientHeight`: Latest client height.
- `bVisible`: Current visibility state.
- `bFocused`: Current focus state.
- `bMinimized`: Whether the window is minimized.
- `bDrawable`: Whether the window has a positive drawable size.
- `bPresentationPaused`: Whether presentation should pause due to no drawable area.
- `Diagnostics`: Window lifecycle and validation diagnostics.

**Validation rules**:

- Only one primary window may be active in v1.
- Creation transitions from Uncreated to Active only after description validation succeeds.
- Close request transitions Active to CloseRequested without forcing immediate destruction.
- Destroy transitions any non-destroyed state to Destroyed and remains idempotent.
- Minimized or zero drawable size sets `bPresentationPaused` while polling and update work may continue.

**State transitions**:

```text
Uncreated -> Active -> CloseRequested -> Destroyed
Uncreated -> Destroyed
Active -> Destroyed
CloseRequested -> Destroyed
Destroyed -> Destroyed
```

## Window Event

**Purpose**: Represents deterministic lifecycle, size, focus, and close notifications emitted by the window driver.

**Key fields**:

- `EventType`: Created, Resized, Minimized, Restored, FocusGained, FocusLost, CloseRequested, Destroyed, or UnavailableRuntime.
- `WindowId`: Target primary window identity.
- `ClientWidth`: Optional latest client width for resize/minimize/restore events.
- `ClientHeight`: Optional latest client height for resize/minimize/restore events.
- `Sequence`: Stable per-driver event order.
- `Message`: Optional diagnostic text.

**Validation rules**:

- Events must preserve deterministic order for a given input sequence.
- Multiple resize events before one poll must leave the window with the latest size.
- Minimized or zero-size events must not be treated as fatal errors.
- Unavailable runtime events must not create a partially active real window.

## Input Identifier

**Purpose**: Provides stable engine-level names for physical keyboard keys and mouse buttons.

**Key fields**:

- `Key`: Physical keyboard identifier such as printable-key positions, navigation keys, function keys, modifiers, or Unknown.
- `MouseButton`: Physical mouse button identifier such as left, right, middle, extra buttons, or Unknown.

**Validation rules**:

- Identifiers represent physical controls, not text characters.
- Keyboard layout translation, text input, and input method composition are out of scope for v1.
- Unknown inputs must not corrupt known input state.

## Input Event

**Purpose**: Represents one ordered physical input change received during event polling or deterministic test playback.

**Key fields**:

- `EventType`: KeyDown, KeyUp, MouseButtonDown, MouseButtonUp, PointerMove, Scroll, or Unknown.
- `InputId`: Stable input identifier when applicable.
- `PointerX`: Current pointer X position when applicable.
- `PointerY`: Current pointer Y position when applicable.
- `DeltaX`: Pointer movement delta when applicable.
- `DeltaY`: Pointer movement delta when applicable.
- `Sequence`: Stable event order.

**Validation rules**:

- Key and button events require a known or Unknown input identifier.
- Pointer movement before a known initial position must initialize position deterministically and report a stable delta policy.
- Unknown input events may produce diagnostics but must not alter known held state.

## Input Frame State

**Purpose**: Represents the derived per-frame input snapshot available to application code.

**Key fields**:

- `HeldKeys`: Physical keys held at the end of the current frame.
- `PressedKeys`: Physical keys pressed during the current frame.
- `ReleasedKeys`: Physical keys released during the current frame.
- `HeldMouseButtons`: Mouse buttons held at the end of the current frame.
- `PressedMouseButtons`: Mouse buttons pressed during the current frame.
- `ReleasedMouseButtons`: Mouse buttons released during the current frame.
- `PointerX`: Current pointer X position.
- `PointerY`: Current pointer Y position.
- `PointerDeltaX`: Per-frame pointer X delta.
- `PointerDeltaY`: Per-frame pointer Y delta.
- `bFocused`: Whether the owning window is focused.

**Validation rules**:

- Pressed and released sets reset at frame boundaries.
- Held sets persist until release or focus loss.
- Focus loss clears all held keyboard and mouse-button state immediately.
- Replaying the same input event sequence must produce identical frame states.

**State transitions**:

```text
NotHeld + KeyDown/ButtonDown -> Pressed + Held
Held + no release -> Held
Held + KeyUp/ButtonUp -> Released + NotHeld
Held + FocusLost -> NotHeld
Pressed/Released + NextFrame -> cleared transient state
```

## Input Manager

**Purpose**: Owns input event ingestion and frame-state derivation.

**Key fields**:

- `PendingEvents`: Ordered input events not yet applied to the frame state.
- `CurrentState`: Current frame input snapshot.
- `PreviousState`: Previous frame input snapshot for transition derivation.
- `Diagnostics`: Input validation and unknown-input diagnostics.

**Validation rules**:

- Polling before window creation or after destruction must return a safe empty state with diagnostics.
- Event order must be deterministic.
- Focus-loss processing must run before later frame snapshots can expose stale held state.

## Application Loop State

**Purpose**: Captures the per-frame decision made by the minimal application loop.

**Key fields**:

- `FrameIndex`: Current frame count.
- `bShouldContinue`: Whether another frame should run.
- `bCloseRequested`: Whether the window or application requested shutdown.
- `bPresentationPaused`: Whether rendering/presentation should pause due to no drawable area.
- `bUpdatedThisFrame`: Whether update work ran for the frame.
- `LastWindowState`: Latest window lifecycle and drawable summary.
- `LastInputState`: Latest input snapshot.
- `Diagnostics`: Loop decision diagnostics.

**Validation rules**:

- Loop can run at least 300 frames or until close request in representative validation.
- Close request allows clean exit at the loop decision point.
- Presentation pause does not prevent event polling or update processing.
- The loop skeleton must not require renderer presentation or swapchain work.

## Window Driver

**Purpose**: Abstracts the source of window and input events.

**Key fields**:

- `DriverName`: Stable driver identifier such as Headless or GLFW.
- `RuntimeAvailability`: Available, DisplayUnavailable, DependencyUnavailable, or Failed.
- `EventQueue`: Ordered window and input events.
- `Diagnostics`: Driver availability and event translation diagnostics.

**Validation rules**:

- Headless driver is required and must be deterministic.
- Real-window driver reports unavailable runtime conditions without crashing.
- Driver-specific details must not appear in public Application contracts.

## Application Diagnostic

**Purpose**: Structured message for validation, lifecycle, input, driver, loop, and smoke-test behavior.

**Key fields**:

- `Severity`: Info, Warning, or Error.
- `Category`: Window, Input, Driver, Loop, Validation, RuntimeAvailability, or Dump.
- `SubjectName`: Window, event, input identifier, driver, or loop involved.
- `StableCode`: Deterministic diagnostic identifier.
- `Message`: Human-readable explanation.

**Validation rules**:

- Invalid window configurations must identify the failing field.
- Display-unavailable and dependency-unavailable conditions must be distinguishable.
- Diagnostic ordering must be deterministic for repeated equivalent scenarios.

## Application Debug Dump

**Purpose**: Deterministic text summary for headless verification and developer inspection.

**Key fields**:

- `WindowSummary`
- `LifecycleSummary`
- `EventSummary`
- `InputStateSummary`
- `LoopDecisionSummary`
- `DiagnosticsSummary`

**Validation rules**:

- Dump text must not include pointer values, native handles, or platform-specific object addresses.
- Repeated equivalent scenarios must produce byte-identical dumps.
- Dumps must include presentation-paused, focus-loss clearing, close-request, and unavailable-runtime outcomes when present.
