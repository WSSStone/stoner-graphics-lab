# B07-S02: Window Lifecycle, Drivers, And Loop Fix

## Scope

Fixed `CR001-B07-F001` and `CR001-B07-F002`.

Code changed:

- `Source/Application/Private/FApplicationLoop.cpp`
- `Source/Application/Private/FWindow.cpp`
- `Source/Application/Public/Application/FWindow.h`
- `Tests/ApplicationWindowInputTests.cpp`

## Fixes

### `CR001-B07-F001`

- `FApplicationLoop::Run` now queues `Window.PollInputEvents()` into
  `FInputManager` before calling `PollFrame`.
- Driver input events therefore participate in the normal loop order:
  window poll, input event ingestion, input state derivation, window state
  observation, update callback, and loop decision.

### `CR001-B07-F002`

- `FWindow::CreateRealWindow` now clears diagnostics and pending events before a
  new real-window create attempt.
- Validation and driver-create failure paths now reset runtime state: lifecycle,
  window identity, platform handle, drawable extent, focus, visibility,
  minimized, and presentation-paused flags.
- The reset destroys any prior driver runtime before clearing the driver pointer.

## Regression Coverage

Added tests for:

- failed real-window validation after a prior successful real-window create
  clearing stale active/native/drawable state;
- scripted native key input being consumed through `FApplicationLoop` before
  frame input state derivation.

## Verification

- `scons config=debug`: passed.
- `Build/Mac/Debug/Tests/StonerTest`: both new Application regressions passed;
  this local graphics-enabled run hit the known intermittent Deferred native
  MoltenVK failure recorded separately as `CR001-B08-F001`.
- `crctl gate fallback-strict --id CR-001`: passed at
  `2026-07-27T07:35:15+00:00`.

## Commit

- `89c8e0e`: `fix(application): harden window loop lifecycle`
