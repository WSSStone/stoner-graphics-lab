# B09-S04 Inspection: Diagnostics, Determinism, And Failure Semantics

## Scope

Inspected representative cross-cutting diagnostic and validation-report paths, with emphasis on stable ordering, omission of native addresses, and actionable failure reasons.

Files inspected:

- `Source/Renderer/Private/FDeferredDiagnostics.cpp`
- `Source/Renderer/Private/FForwardDiagnostics.cpp`
- `Source/Application/Private/FApplicationDiagnostics.cpp`
- `Source/Renderer/Private/FRendererComparisonReport.cpp`
- `Tests/DeferredRenderingTests.cpp`
- `Tests/RendererComparisonTests.cpp`
- `Tests/TriangleDemoIntegrationTests.cpp`
- `.github/scripts/run_deferred_validation.py`

## Positive Evidence

- Deferred diagnostics preserve first-error ownership by sequence even after display sorting. `Tests/DeferredRenderingTests.cpp` covers this with `Deferred diagnostics preserve first occurrence ownership after display sorting`.
- Deferred diagnostics dumps are normalized and tested to omit native-address-like output.
- Forward, application, scene, demo, material, and deferred tests include byte-stability or deterministic-diagnostic checks across repeated runs.
- Validation wrappers reject native addresses/non-finite values in retained reports and use stable watchdog failures.
- Renderer comparison tiers are sorted by tier count before report construction, making valid 0/16/64/256 artifacts deterministic independent of input order.

## Finding

Accepted `CR001-B09-F004` as S3. `BuildRendererComparisonReport()` records diagnostics for invalid tier content, timing samples, and non-constant surface work, but the missing/extra tier-count path sets `Invalid` and returns with no stable diagnostic code. The current tests check valid normalization and per-tier helper failures, but do not check missing/extra tier diagnostics.

## Recommended Fix Direction

B09-S05 can add a stable `DEF-COMPARE-TIER-COUNT` diagnostic, preserve the received tier list on invalid count, and add focused `RendererComparisonTests` coverage for missing and extra tiers. This is low-risk and does not change the pass criteria for valid reports.
