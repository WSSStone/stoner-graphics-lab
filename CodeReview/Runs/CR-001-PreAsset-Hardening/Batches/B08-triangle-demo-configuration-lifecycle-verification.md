# B08-S03: Triangle Demo Configuration And Lifecycle Verification

## Scope

Verified `CR001-B08-F002`.

Files checked:

- `Demo/StonerDemo/Private/FStonerDemoApplication.h`
- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp`
- `Tests/TriangleDemoIntegrationTests.cpp`

## Verification

### `CR001-B08-F002`

- Late initialization failures after native/window allocation now call cleanup
  before returning.
- Shader validation failure and visible presentation resource failure use the
  shared initialization failure cleanup path.
- Injected upload and pipeline startup failures call `Shutdown` before
  returning.
- Public direct `Initialize` failure now leaves lifecycle `Stopped`.
- `Shutdown` remains idempotent after a failed initialization cleanup.
- First-failure ownership remains stable after cleanup.

## Gate Evidence

- `crctl gate fallback-strict --id CR-001`: passed at
  `2026-07-27T08:53:13+00:00`.

## Result

`CR001-B08-F002` is verified.
