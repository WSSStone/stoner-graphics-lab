# B07-S06 Evidence: Input Mapping, Events, And Snapshots Verification

Step: `B07-S06`.

Findings verified:

- `CR001-B07-F003`: GLFW input mapping omits declared key and mouse vocabulary.
- `CR001-B07-F004`: Input snapshot focus and reset paths can preserve stale
  state.

Source evidence:

- `Source/Application/Private/FGlfwWindowDriver.cpp:196` defines the key mapping
  helper used by the GLFW callback path.
- `Source/Application/Private/FGlfwWindowDriver.cpp:245` defines the mouse
  mapping helper used by the GLFW callback path.
- `Source/Application/Private/FInputManager.cpp:37` restores snapshot focus from
  the owning window before focus-loss handling.
- `Source/Application/Private/FInputState.cpp:44` through
  `Source/Application/Private/FInputState.cpp:52` perform full snapshot reset.
- `Source/Application/Private/FInputState.cpp:155` through
  `Source/Application/Private/FInputState.cpp:162` clear key/button state for
  focus loss without using the full snapshot reset path.

Regression evidence:

- `Tests/ApplicationWindowInputTests.cpp:244` checks invalid lifecycle polling
  clears stale pointer and focus snapshot state.
- `Tests/ApplicationWindowInputTests.cpp:269` through
  `Tests/ApplicationWindowInputTests.cpp:280` check GLFW mapping coverage when
  available and Unknown fallback when unavailable.
- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/output/b07-input-snapshots-fix-stonertest.txt`
  records the new Application input regressions as passing.

Gate evidence:

- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/gate-fallback-strict.json`
  records `fallback-strict` passed at `2026-07-27T07:54:38+00:00`.

Result:

- `CR001-B07-F003` is verified.
- `CR001-B07-F004` is verified.
