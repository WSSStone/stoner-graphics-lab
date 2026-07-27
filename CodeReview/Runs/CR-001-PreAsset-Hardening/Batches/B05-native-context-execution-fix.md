# B05-S14: Native Context Execution Fix

## Repair Target

Implementation commit `d5b83ac` repairs `CR001-B05-F013`: visible native frame
failure paths could leave an acquired image or unsignaled frame-slot fence
behind after `VK_SUBOPTIMAL_KHR`, command recording failure, or submit failure
after fence reset.

## Repair Summary

- `AcquireVisibleFrame` now records suboptimal acquisition as state and still
  returns `Success` so the acquired image can be submitted/presented through the
  normal ownership path.
- `SubmitAndPresentVisibleFrame` converts a successful present of a suboptimal
  acquire into `ResizeRequired`, while preserving real present failures as
  `Failed`.
- Recording failure in `DrawVisibleFrame` now abandons the acquired visible
  image through a cleanup path instead of only clearing `bFrameAcquired`.
- Submit failure after `vkResetFences` recreates the frame-slot fence in a
  signaled state before the next acquire can touch that slot.
- A runtime-independent visible failure lifecycle report covers
  `AcquireSuboptimal`, `Record`, and `SubmitAfterFenceReset`.

## Tests And Evidence

Maintained regression:

- `Vulkan visible frame failure lifecycle releases acquired state and reusable fences`

Fresh local evidence:

- `strict-debug`: passed at `2026-07-27T05:44:08+00:00`.
- `fallback-strict`: passed at `2026-07-27T05:44:46+00:00`, including full
  tests with graphics disabled.
- `strict-release`: passed at `2026-07-27T05:45:06+00:00`.
- `sanitizers`: passed at `2026-07-27T05:46:09+00:00` with
  `STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1`.

The sanitizer run also covered existing native context integration paths:

- real native offscreen triangle submission and zero live frame-local objects;
- RHI native shader/pipeline ownership and explicit shutdown invalidation.

## Finding State

- `CR001-B05-F013`: Fixed at `d5b83ac`.

B05-S15 must independently verify parent/current behavior, maintain the fixed
visible failure lifecycle result, and decide whether the remaining formal
`tests` gate failure is still only the pre-existing B08 deferred native
readback boundary.

