# B07-S04 Evidence: Input Mapping, Events, And Snapshots Inspection

Step: `B07-S04`.

Evidence commands:

- `wc -l Source/Application/Public/Application/EKey.h ... Tests/ApplicationWindowInputTests.cpp specs/016-window-input-system/*.md`
- `nl -ba` over the production and test files listed in the batch summary.
- `rg -n "Home|Page|Insert|Delete|Shift|Control|Alt|F1|X1|X2|IsFocused|HasPointerPosition|ClearAll|Pointer" Tests/ApplicationWindowInputTests.cpp Source/Application specs/016-window-input-system`

Findings opened and accepted:

- `CR001-B07-F003`: GLFW input mapping omits declared key and mouse vocabulary.
- `CR001-B07-F004`: Input snapshot focus and reset paths can preserve stale
  state.

Key source evidence:

- `Source/Application/Public/Application/EKey.h:54` through
  `Source/Application/Public/Application/EKey.h:77` declare navigation,
  modifier, and function keys.
- `Source/Application/Public/Application/EMouseButton.h:12` through
  `Source/Application/Public/Application/EMouseButton.h:13` declare extra mouse
  buttons.
- `Source/Application/Private/FGlfwWindowDriver.cpp:17` through
  `Source/Application/Private/FGlfwWindowDriver.cpp:35` map only a subset of the
  declared key vocabulary.
- `Source/Application/Private/FGlfwWindowDriver.cpp:38` through
  `Source/Application/Private/FGlfwWindowDriver.cpp:47` map only left, right,
  and middle mouse buttons.
- `Source/Application/Private/FInputState.cpp:44` through
  `Source/Application/Private/FInputState.cpp:52` clear only key/button sets.
- `Source/Application/Private/FInputState.cpp:139` through
  `Source/Application/Private/FInputState.cpp:146` set focus false on focus
  loss and clear held state.
- `Source/Application/Private/FInputManager.cpp:37` through
  `Source/Application/Private/FInputManager.cpp:40` handle focus loss, but no
  focused-frame path restores input `bFocused=true`.

Test coverage gap:

- Existing Application input tests cover printable key transitions, left mouse
  button state, pointer deltas, unknown identifiers, and focus-loss clearing.
- They do not cover real adapter mapping for modifiers/function/navigation
  keys or extra mouse buttons.
- They do not cover focus restoration, manager clear, or invalid lifecycle
  pointer/focus snapshot reset.

Result:

- B07-S04 completed with two Accepted S2 findings for the next B07 fix step.
