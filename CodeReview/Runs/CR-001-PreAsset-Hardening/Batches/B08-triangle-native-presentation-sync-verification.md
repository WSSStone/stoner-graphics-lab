# B08-S06: Triangle Native Presentation And Synchronization Verification

## Scope

Verified `CR001-B08-F003` after the B08-S05 fix. This step did not change
production or test code.

## Verified Finding

### `CR001-B08-F003`

Status moved from `Fixed` to `Verified`.

The fix keeps presentation resource readiness separate from recovery timing:

- `NotifyDrawableExtent` no longer sets `PresentationState.bInitialized`.
- Startup-zero visible recovery therefore keeps
  `PresentationState.bInitialized=false` until first
  `PrepareVisibleTriangle` succeeds.
- Already-prepared resize/recovery still keeps the initialized flag true across
  pause/recovery notification and continues to use
  `RecreateVisiblePresentation`.

## Verification

Passed:

- `conda run -n stoner-cr python CodeReview/Tools/crctl.py gate fallback-strict --id CR-001`

Recorded at: `2026-07-27T09:12:06+00:00`.

The gate ran:

- `scons config=debug strict=1 graphics=disabled`
- `Build/Mac/Debug/Tests/StonerTest`

Relevant regression output:

- `[PASS] Triangle demo deterministic recovery fixture has no presentation resources`
- `[PASS] Triangle demo presentation recovery does not mark resources initialized before prepare succeeds`

## Residual Risk

The verification is deterministic and does not create a real visible window.
That is appropriate for this state-machine defect because the broken branch was
caused by demo-owned presentation flags before native Vulkan presentation was
called. Full real-window evidence remains governed by the broader Feature 018
native validation track.
