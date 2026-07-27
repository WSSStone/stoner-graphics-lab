# B07-S04: Input Mapping, Events, And Snapshots Inspection

## Scope

Inspected Feature 016 input vocabulary, event ordering, frame-state derivation,
driver mapping, and Application input tests.

Production files read:

- `Source/Application/Public/Application/EKey.h`
- `Source/Application/Public/Application/EMouseButton.h`
- `Source/Application/Public/Application/FInputEvent.h`
- `Source/Application/Public/Application/FInputState.h`
- `Source/Application/Public/Application/FInputManager.h`
- `Source/Application/Private/FInputEvent.cpp`
- `Source/Application/Private/FInputState.cpp`
- `Source/Application/Private/FInputManager.cpp`
- `Source/Application/Private/FGlfwWindowDriver.cpp`

Supporting files read:

- `Tests/ApplicationWindowInputTests.cpp`
- `specs/016-window-input-system/spec.md`
- `specs/016-window-input-system/contracts/window-input-contract.md`
- `specs/016-window-input-system/data-model.md`

## Requirements Checked

- Feature 016 FR-008: per-frame snapshots for keyboard, mouse buttons, pointer
  position, and pointer delta.
- Feature 016 FR-009: pressed, released, and held state transitions.
- Feature 016 FR-010: transient state clears at frame boundaries while held
  state persists until release or focus-loss handling.
- Feature 016 FR-011: stable physical vocabulary for printable positions,
  navigation keys, function keys, modifiers, common mouse buttons, and Unknown.
- Feature 016 FR-012: focus loss clears held keyboard and mouse-button state.
- Input data model: `bFocused` reflects owning window focus, and invalid
  lifecycle polling returns a safe empty state.

## Findings

### `CR001-B07-F003`

The public key and mouse enums declare navigation keys, function keys,
modifiers, and extra mouse buttons, but the GLFW adapter maps only letters,
digits, Escape, Space, Enter, Tab, Backspace, arrow keys, and three mouse
buttons.

Impact: real-window input reports declared common controls as Unknown, so
`FInputState` ignores them despite the public vocabulary contract.

Status: Accepted, S2.

### `CR001-B07-F004`

`FInputState::ApplyEvent(FocusLost)` sets `bFocused=false`, but
`FInputManager::PollFrame` never restores `bFocused=true` on focused frames.
`FInputState::ClearAll` also clears only key/button sets, leaving pointer
position, has-pointer flag, deltas, and focus state intact for invalid lifecycle
and manager clear paths.

Impact: input snapshots and deterministic debug dumps can expose stale focus or
pointer metadata even after focus restoration, manager clear, or invalid
lifecycle polling.

Status: Accepted, S2.

## Non-Findings

- Input event ordering is deterministic for explicit sequence values through
  stable sequence sorting.
- Repeated key/button down events avoid duplicate pressed transitions while a
  held entry is already present.
- Unknown key, mouse, and event records report diagnostics without mutating held
  key/button state.
