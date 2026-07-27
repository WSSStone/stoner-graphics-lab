# B07-S03: Window Lifecycle, Drivers, And Loop Verification

## Scope

Verified `CR001-B07-F001` and `CR001-B07-F002`.

Files checked:

- `Source/Application/Private/FApplicationLoop.cpp`
- `Source/Application/Private/FWindow.cpp`
- `Source/Application/Public/Application/FWindow.h`
- `Tests/ApplicationWindowInputTests.cpp`

## Verification

### `CR001-B07-F001`

- `FApplicationLoop::Run` queues `Window.PollInputEvents()` before
  `InputManager.PollFrame`.
- Regression coverage confirms scripted native key input reaches the loop's
  derived frame state.

### `CR001-B07-F002`

- `FWindow::CreateRealWindow` calls `ResetRuntimeState` for validation failure,
  unavailable driver runtime, and driver-create failure paths.
- Regression coverage confirms a validation failure after an active real-window
  create clears stale lifecycle, platform handle, and drawable state.

## Gate Evidence

- `crctl gate fallback-strict --id CR-001`: passed at
  `2026-07-27T07:38:08+00:00`.

## Result

`CR001-B07-F001` and `CR001-B07-F002` are verified.
