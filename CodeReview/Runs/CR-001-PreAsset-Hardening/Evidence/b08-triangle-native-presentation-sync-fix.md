# B08-S05 Evidence: Triangle Native Presentation And Synchronization Fix

Step: `B08-S05`.

Finding fixed:

- `CR001-B08-F003`: Visible startup-zero recovery selects recreate before first
  presentation prepare.

Code evidence:

- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:307` through
  `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:313` now transitions
  from `PresentationPaused` to `RecreatingPresentation`, records recovery
  start time, and increments generation without setting
  `PresentationState.bInitialized`.
- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:386` through
  `Demo/StonerDemo/Private/FStonerDemoApplication.cpp:393` still uses
  `PresentationState.bInitialized` to choose first
  `PrepareVisibleTriangle` versus `RecreateVisiblePresentation`; the flag is
  set only after successful first prepare.
- `Demo/StonerDemo/Private/FStonerDemoApplication.h:57` through
  `Demo/StonerDemo/Private/FStonerDemoApplication.h:60` exposes
  `IsPresentationInitialized()` for deterministic regression assertions.

Test evidence:

- `Tests/TriangleDemoIntegrationTests.cpp:134` through
  `Tests/TriangleDemoIntegrationTests.cpp:155` asserts deterministic recovery
  begins with no presentation resources and remains uninitialized while
  recovery notification/timing is active.

Gate evidence:

- Command: `conda run -n stoner-cr python CodeReview/Tools/crctl.py gate fallback-strict --id CR-001`
- Result: passed.
- Recorded at: `2026-07-27T09:07:32+00:00`.
- Build profile: `scons config=debug strict=1 graphics=disabled`.
- Test executable: `Build/Mac/Debug/Tests/StonerTest`.
- Relevant PASS lines:
  - `Triangle demo deterministic recovery fixture has no presentation resources`
  - `Triangle demo presentation recovery does not mark resources initialized before prepare succeeds`

Commit:

- `71b5fed fix(demo): preserve visible presentation initialization state`

Next step:

- Verify `CR001-B08-F003` in B08-S06 using the recorded fallback-strict gate
  and, if needed, an additional focused local test run.
