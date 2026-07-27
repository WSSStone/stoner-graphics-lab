# B07-S05: Input Mapping, Events, And Snapshots Fix

## Scope

Fixed `CR001-B07-F003` and `CR001-B07-F004`.

Code changed:

- `Source/Application/Public/Application/FInputState.h`
- `Source/Application/Private/FInputState.cpp`
- `Source/Application/Private/FInputManager.cpp`
- `Source/Application/Private/FWindowDriver.h`
- `Source/Application/Private/FGlfwWindowDriver.cpp`
- `Tests/ApplicationWindowInputTests.cpp`

## Fixes

### `CR001-B07-F003`

- GLFW key mapping now covers the declared navigation, modifier, and F1-F12
  function-key vocabulary.
- GLFW mouse mapping now covers extra mouse buttons X1 and X2.
- The private GLFW mapping helpers are callable from Application tests without
  exposing GLFW types through the public Application API.

### `CR001-B07-F004`

- `FInputManager::PollFrame` now restores input snapshot focus on focused
  frames before processing pending events.
- `FInputState` now separates key/button state clearing from full snapshot
  reset.
- Full reset paths clear pointer position, pointer deltas, has-pointer state,
  and focus state.
- Focus-loss handling still clears key/button state immediately without
  treating focus restoration as a persistent lost state.

## Regression Coverage

Added tests for:

- focus restoration after a focus-loss frame;
- invalid lifecycle polling clearing stale pointer and focus snapshot state;
- GLFW mapping coverage for representative navigation, modifier, function, and
  extra mouse-button controls when GLFW is available, with unavailable-safe
  fallback behavior otherwise.

## Verification

- `scons config=debug`: passed.
- `Build/Mac/Debug/Tests/StonerTest`: the new Application regressions passed;
  this local graphics-enabled run hit the known intermittent Deferred native
  MoltenVK failure recorded separately as `CR001-B08-F001`.
- `crctl gate fallback-strict --id CR-001`: passed at
  `2026-07-27T07:51:11+00:00`.

## Commit

- `21629d7`: `fix(application): complete input mapping snapshots`
