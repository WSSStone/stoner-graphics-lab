# B05-S15: Native Context Execution Verification

## Verification Target

`CR001-B05-F013` verifies implementation commit `d5b83ac`: visible native
frame failure paths must release acquired swapchain image state and leave
frame-slot fences reusable after suboptimal acquire, command recording failure,
or submit failure after fence reset.

## Parent Reproduction

Parent revision `d5b83ac^` showed the accepted failure shape:

- `AcquireVisibleFrame` accepted `VK_SUBOPTIMAL_KHR`, set acquired-frame state,
  then returned `ResizeRequired` immediately at line 1321, bypassing submit and
  present cleanup.
- `SubmitAndPresentVisibleFrame` cleared `bFrameAcquired` on reset/submit
  failure at lines 1338 and 1348, but did not restore a signaled fence after a
  submit failure following `vkResetFences`.
- `DrawVisibleFrame` cleared only `bFrameAcquired` after record failure at line
  1388, leaving no shared cleanup path for acquired image release.

## Current Verification

Current revision `9cacd5e` contains the intended recovery mechanics:

- `bAcquiredSuboptimal` tracks suboptimal acquire state separately from image
  ownership.
- `AbandonAcquiredVisibleFrame()` centralizes acquired visible image cleanup.
- `RecreateVisibleFenceSignaled()` recreates a failed submit slot fence as
  signaled before the slot can be reused.
- `AcquireVisibleFrame` treats `VK_SUCCESS` and `VK_SUBOPTIMAL_KHR` as acquired
  frames that should continue through normal ownership cleanup.
- `SubmitAndPresentVisibleFrame` recreates the fence and abandons the acquired
  frame on submit failure, clears suboptimal/acquired flags after present, and
  reports `ResizeRequired` only after ownership has been released.
- `DrawVisibleFrame` calls the shared cleanup path after command recording
  failure.

Maintained regression evidence:

- `[PASS] Vulkan visible frame failure lifecycle releases acquired state and reusable fences`

## Local Gates

- `strict-debug`: passed at `2026-07-27T05:49:40+00:00`.
- `fallback-strict`: passed at `2026-07-27T05:50:03+00:00`, including the
  visible frame lifecycle regression with graphics disabled.
- `strict-release`: passed at `2026-07-27T05:50:04+00:00`.
- `sanitizers`: passed at `2026-07-27T05:53:09+00:00` with
  `STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1`.
- `tests`: refreshed at `2026-07-27T05:54:10+00:00`; build succeeded, test
  executable returned 1. Narrow failure scan showed MoltenVK reporting
  `VK_ERROR_INCOMPATIBLE_DRIVER` because Metal is unavailable on this execution
  device, followed by
  `[FAIL] Triangle demo deterministic lifecycle completes exact frame budget and shutdown`.

The refreshed `tests` failure is tracked as an environment/native presentation
boundary, not as a regression in `CR001-B05-F013`: strict, fallback, release,
and sanitizer profiles all pass, and the F013-specific lifecycle regression
passes in both fallback and sanitizer runs.

## Finding Decision

- `CR001-B05-F013`: Verified.

B05 native context execution has no remaining accepted S0-S2 finding after this
verification step. The remaining formal tests boundary belongs to later
integration/native-environment review scope.
