# B07-S06: Input Mapping, Events, And Snapshots Verification

## Scope

Verified `CR001-B07-F003` and `CR001-B07-F004`.

Files checked:

- `Source/Application/Private/FGlfwWindowDriver.cpp`
- `Source/Application/Private/FWindowDriver.h`
- `Source/Application/Private/FInputState.cpp`
- `Source/Application/Private/FInputManager.cpp`
- `Source/Application/Public/Application/FInputState.h`
- `Tests/ApplicationWindowInputTests.cpp`

## Verification

### `CR001-B07-F003`

- GLFW mapping helpers cover representative navigation, modifier, function, and
  extra mouse-button controls.
- Application tests verify the mapping when GLFW is available and verify
  Unknown behavior when the mapping backend is unavailable.

### `CR001-B07-F004`

- `FInputManager::PollFrame` restores focused snapshot state on focused frames.
- Full reset paths clear key/button state, pointer position, pointer deltas,
  has-pointer state, and focus state.
- Focus-loss handling still clears key/button state immediately without making
  focus loss permanent.

## Gate Evidence

- `crctl gate fallback-strict --id CR-001`: passed at
  `2026-07-27T07:54:38+00:00`.

## Result

`CR001-B07-F003` and `CR001-B07-F004` are verified.
