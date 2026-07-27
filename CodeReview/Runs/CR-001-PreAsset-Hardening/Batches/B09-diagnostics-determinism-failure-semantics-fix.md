# B09-S05 Fix: Diagnostics, Determinism, And Failure Semantics

## Finding Addressed

- `CR001-B09-F004`: Renderer comparison report omits diagnostics for missing or extra tiers

## Fix Commit

- `715dba2 fix(renderer): report comparison tier count failures`

## Changes

- `BuildRendererComparisonReport()` now emits a stable `DEF-COMPARE-TIER-COUNT` diagnostic when the input does not contain exactly the required four local-light tiers.
- Invalid tier-count reports now preserve the received tier list in `Report.Tiers`, making failed artifacts easier to inspect.
- `RendererComparisonTests` now covers both missing-tier and extra-tier diagnostics.

## Verification

```text
conda run -n stoner-cr scons config=debug strict=1 graphics=disabled --implicit-deps-changed
```

Result: passed; rebuilt `FRendererComparisonReport.cpp`, `RendererComparisonTests.cpp`, and `StonerTest`.

```text
Build/Mac/Debug/Tests/StonerTest
```

Result: exit code 0. Output included:

- `[PASS] Renderer comparison reports missing tier count diagnostics`
- `[PASS] Renderer comparison reports extra tier count diagnostics`

## Follow-Up

`CR001-B09-F004` is Fixed, not Verified. B09-S06 should independently rerun focused checks and inspect the report failure path from current HEAD.
