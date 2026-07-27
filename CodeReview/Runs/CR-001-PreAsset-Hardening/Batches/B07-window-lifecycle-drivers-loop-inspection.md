# B07-S01: Window Lifecycle, Drivers, And Loop Inspection

## Scope

Inspected Feature 016 window lifecycle, driver event flow, and application loop
handoff.

Production files read:

- `Source/Application/Public/Application/FWindow.h`
- `Source/Application/Private/FWindow.cpp`
- `Source/Application/Public/Application/FApplicationLoop.h`
- `Source/Application/Private/FApplicationLoop.cpp`
- `Source/Application/Private/FWindowDriver.h`
- `Source/Application/Private/FHeadlessWindowDriver.cpp`
- `Source/Application/Private/FGlfwWindowDriver.cpp`
- `Source/Application/Private/FWindowEvent.cpp`

Supporting files read:

- `Tests/ApplicationWindowInputTests.cpp`
- `specs/016-window-input-system/spec.md`
- `specs/016-window-input-system/contracts/window-input-contract.md`

## Requirements Checked

- Feature 016 FR-014: the minimal loop polls events, updates input state,
  observes window state, and decides whether to continue.
- Feature 016 FR-016: deterministic headless path and real-window smoke path
  remain controlled when display access is unavailable.
- Feature 016 FR-017: minimized or zero drawable state pauses presentation while
  update processing continues.
- Window Lifecycle Contract: validation/runtime failures do not leave a
  partially active window.
- Application Loop Contract: event polling and input derivation happen in order.

## Findings

### `CR001-B07-F001`

`FApplicationLoop::Run` polls window events and immediately derives input state,
but never queues `Window.PollInputEvents()` into `FInputManager`. GLFW stores
keyboard, mouse, pointer, scroll, and focus-loss records in driver
`InputEvents`, which are only exposed through `ConsumeInputEvents()` and
`FWindow::PollInputEvents()`.

Impact: normal real-window loop execution can lose native driver input even
though driver callbacks fired.

Status: Accepted, S2.

### `CR001-B07-F002`

`FWindow::CreateRealWindow` does not clear diagnostics or pending events at
entry, and failure paths return without resetting lifecycle/window identity.
Validation failure and driver-create failure can therefore leave a reused
`FWindow` reporting stale active state from an earlier successful create.

Impact: callers can treat a failed real-window creation as a usable active
window, which can corrupt presentation/input lifecycle decisions.

Status: Accepted, S2.

## Non-Findings

- Window event sequence sorting is deterministic through
  `SortWindowEventsStable`.
- Minimized/restored state updates maintain presentation-paused semantics for
  existing covered tests.
- `FApplicationLoop::Run` preserves the intended high-level order once input
  events are available to `FInputManager`.
