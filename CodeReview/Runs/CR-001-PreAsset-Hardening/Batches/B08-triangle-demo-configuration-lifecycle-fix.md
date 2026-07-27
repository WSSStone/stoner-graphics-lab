# B08-S02: Triangle Demo Configuration And Lifecycle Fix

## Scope

Fixed `CR001-B08-F002`.

Code changed:

- `Demo/StonerDemo/Private/FStonerDemoApplication.h`
- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp`
- `Tests/TriangleDemoIntegrationTests.cpp`

## Fixes

### `CR001-B08-F002`

- Added a shared initialization-failure cleanup helper for late startup
  failures that already may own window/native resources.
- Shader validation failure now records the primary failing stage, enters the
  failed state, and immediately runs shutdown cleanup before returning.
- Visible presentation resource creation failure now follows the same cleanup
  path.
- Injected upload and pipeline initialization failures now call `Shutdown`
  before returning.
- The cleanup path preserves first-failure ownership while making the public
  `Initialize` API leave the demo in a stopped, idempotently shut-down state
  after late startup failures.

## Regression Coverage

Updated the swapped-shader direct `Initialize` regression to assert:

- `InitializationFailed` is returned;
- the primary diagnostic exit code remains `InitializationFailed`;
- lifecycle state is `Stopped` after the failed initialization cleanup;
- a subsequent `Shutdown` call succeeds idempotently.

## Verification

- `git diff --check -- Demo/StonerDemo/Private/FStonerDemoApplication.h Demo/StonerDemo/Private/FStonerDemoApplication.cpp Tests/TriangleDemoIntegrationTests.cpp`: passed.
- `scons config=debug`: passed.
- `Build/Mac/Debug/Tests/StonerTest`: passed.
- `crctl gate fallback-strict --id CR-001`: passed at
  `2026-07-27T08:49:42+00:00`.

## Commit

- `3cb5420`: `fix(demo): clean partial initialization failures`
