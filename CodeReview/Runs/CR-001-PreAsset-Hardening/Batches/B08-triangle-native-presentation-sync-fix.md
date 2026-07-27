# B08-S05: Triangle Native Presentation And Synchronization Fix

## Scope

Fixed `CR001-B08-F003` in the Feature 018 visible presentation recovery state
machine and added deterministic regression coverage. No native Vulkan,
swapchain, or renderer API behavior was expanded in this step.

Production files changed:

- `Demo/StonerDemo/Private/FStonerDemoApplication.h`
- `Demo/StonerDemo/Private/FStonerDemoApplication.cpp`

Test files changed:

- `Tests/TriangleDemoIntegrationTests.cpp`

## Fix

`NotifyDrawableExtent` no longer marks `PresentationState.bInitialized` when a
paused demo observes a non-zero drawable extent. It still moves the lifecycle
to `RecreatingPresentation`, starts recovery timing, and increments the
presentation generation. Resource readiness now remains owned by the actual
native prepare/recreate result:

- startup-zero visible path: `PresentationState.bInitialized` remains false,
  so `RunVisible` chooses first `PrepareVisibleTriangle`; success then marks
  the presentation initialized.
- already-prepared visible resize path: `PresentationState.bInitialized`
  remains true across pause/recovery notification, so `RunVisible` continues to
  choose `RecreateVisiblePresentation`.

Added `IsPresentationInitialized()` as a read-only diagnostic/test accessor next
to the existing presentation generation and recovery duration accessors.

## Regression Coverage

`TestPresentationRecovery` now asserts:

- a deterministic recovery fixture has no presentation resources after
  initialization;
- pause-to-non-zero recovery enters `RecreatingPresentation` without claiming
  presentation resources are initialized;
- the existing twenty-generation recovery timing and slow-recovery rejection
  checks still pass.

This exercises the state transition that previously let startup-zero visible
recovery select recreation before first native presentation preparation.

## Verification

Passed:

- `conda run -n stoner-cr python CodeReview/Tools/crctl.py gate fallback-strict --id CR-001`

Recorded at: `2026-07-27T09:07:32+00:00`.

The gate rebuilt the affected demo runtime and `TriangleDemoIntegrationTests`
with `strict=1 graphics=disabled`, then ran `Build/Mac/Debug/Tests/StonerTest`.
The output includes:

- `[PASS] Triangle demo deterministic recovery fixture has no presentation resources`
- `[PASS] Triangle demo presentation recovery does not mark resources initialized before prepare succeeds`

## Finding Status

- `CR001-B08-F003`: Fixed by commit `71b5fed`.
