# B06-S02: Render Graph Declaration, Compilation, And Lifetimes Fix

## Repair Target

Implementation commit `7292a65` fixes `CR001-B06-F001`: render graph execution
resolved and validated all declared graph resources, including resources used
only by culled passes.

## Repair Summary

- `FRenderGraphExecutor::Execute` now derives a required resource set from
  `Graph.GetCompiledGraph().ScheduledPasses`.
- Imported-resource binding validation and transient-resource resolution now run
  only for resources accessed by scheduled executable passes.
- Culled-branch resources are left unresolved with `BackingAllocationId = 0` and
  `bResolvedDuringExecution = false`.
- Existing required-resource behavior is preserved: scheduled imported resources
  still require valid caller bindings, and scheduled transient resources still
  report resolution failures.

## Tests Added

New regression coverage in `Tests/RendererRenderGraphTests.cpp`:

- `[PASS] Render graph culled-import fixture compiles`
- `[PASS] Render graph ignores missing imports used only by culled passes`
- `[PASS] Render graph culled-transient fixture compiles`
- `[PASS] Render graph ignores transient resolution failures used only by culled passes`

These tests close the coverage gap identified in B06-S01: culled imported and
transient resources no longer affect executable graph results.

## Local Gates

- `scons config=debug`: passed locally after the source/test patch.
- Focused `StonerTest | rg ...`: new render graph regression tests passed;
  current Mac execution still reports MoltenVK/Metal unavailable and the known
  triangle deterministic lifecycle environment failure.
- `fallback-strict`: passed at `2026-07-27T06:04:23+00:00`.
- `strict-release`: passed at `2026-07-27T06:04:34+00:00`.
- `sanitizers`: passed at `2026-07-27T06:05:44+00:00`.

## Finding State

- `CR001-B06-F001`: Fixed at `7292a65`.

B06 verify should independently confirm parent/current behavior and then move
the finding to Verified.
