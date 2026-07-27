# B07-S03 Evidence: Window Lifecycle, Drivers, And Loop Verification

Step: `B07-S03`.

Findings verified:

- `CR001-B07-F001`: Application loop ignores driver input events.
- `CR001-B07-F002`: Real-window create failure can preserve stale active window
  state.

Source evidence:

- `Source/Application/Private/FApplicationLoop.cpp:17` queues
  `Window.PollInputEvents()` into `FInputManager`.
- `Source/Application/Public/Application/FWindow.h:58` declares
  `ResetRuntimeState`.
- `Source/Application/Private/FWindow.cpp:110`,
  `Source/Application/Private/FWindow.cpp:115`, and
  `Source/Application/Private/FWindow.cpp:122` reset runtime state for
  real-window create failure paths.
- `Source/Application/Private/FWindow.cpp:308` defines the lifecycle, identity,
  platform handle, drawable, visibility, focus, minimized, and presentation
  state reset.

Regression evidence:

- `Tests/ApplicationWindowInputTests.cpp:137` checks stale active real-window
  state is cleared after failed validation.
- `Tests/ApplicationWindowInputTests.cpp:294` checks driver input reaches the
  loop-derived frame input state.

Gate evidence:

- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/gate-fallback-strict.json`
  records `fallback-strict` passed at `2026-07-27T07:38:08+00:00`.

Result:

- `CR001-B07-F001` is verified.
- `CR001-B07-F002` is verified.
