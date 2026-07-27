# B07-S01 Evidence: Window Lifecycle, Drivers, And Loop Inspection

Step: `B07-S01`.

Evidence commands:

- `wc -l Source/Application/Public/Application/FWindow.h ... Tests/ApplicationWindowInputTests.cpp specs/016-window-input-system/spec.md specs/016-window-input-system/contracts/window-input-contract.md`
- `rg -n "Window|Loop|Driver|Tick|Poll|Close|Minimized|Drawable|Lifecycle|FR-|SC-" specs/016-window-input-system specs/017-scene-graph-ecs Source/Application/Public Source/Application/Private Tests/ApplicationWindowInputTests.cpp`
- `nl -ba` over the production and test files listed in the batch summary.

Findings opened and accepted:

- `CR001-B07-F001`: Application loop ignores driver input events.
- `CR001-B07-F002`: Real-window create failure can preserve stale active window
  state.

Key source evidence:

- `Source/Application/Private/FApplicationLoop.cpp:16` calls
  `Window.PollEvents()`.
- `Source/Application/Private/FApplicationLoop.cpp:17` calls
  `InputManager.PollFrame(...)` without first queuing window-driver input
  events.
- `Source/Application/Private/FGlfwWindowDriver.cpp:116` exposes driver input
  only through `ConsumeInputEvents()`.
- `Source/Application/Private/FWindow.cpp:101` enters `CreateRealWindow`
  without clearing prior transient window state.
- `Source/Application/Private/FWindow.cpp:111` returns validation failure
  without resetting lifecycle or identity.
- `Source/Application/Private/FWindow.cpp:114` through
  `Source/Application/Private/FWindow.cpp:117` return driver creation failure
  after resetting only the driver pointer.

Test coverage gap:

- `Tests/ApplicationWindowInputTests.cpp:157` through
  `Tests/ApplicationWindowInputTests.cpp:162` manually call
  `Window.PollInputEvents()`, but loop tests at
  `Tests/ApplicationWindowInputTests.cpp:251` through
  `Tests/ApplicationWindowInputTests.cpp:266` do not validate driver input
  ingestion through `FApplicationLoop`.

Result:

- B07-S01 completed with two Accepted S2 findings for the next B07 fix step.
