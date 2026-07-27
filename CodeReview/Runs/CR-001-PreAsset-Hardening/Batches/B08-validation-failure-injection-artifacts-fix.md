# B08-S14 Fix: Validation, Failure Injection, And Artifacts

## Finding Addressed

- `CR001-B08-F006`: Deferred validation artifacts and wrapper under-check local light readback coverage

## Fix Commit

- `3fe5a6d fix(ci): tighten deferred readback artifact validation`

## Changes

- Tightened `.github/scripts/run_deferred_validation.py` readback validation to require at least 18 probes per depth convention.
- Required all six point/spot `LocalLightCase` probes for both `StandardZ` and `ReversedZ` in wrapper-level artifact validation.
- Expanded `.github/scripts/test_run_deferred_validation.py` to reject stale 12-probe reports and reports missing a required local-light probe.
- Updated active Feature 019 spec, plan, quickstart, task, contract, and Validation README wording from the old 12-probe threshold to the current 18-probe local-light-aware threshold.
- Preserved `Validation/019/completion.md` as historical Feature 019 closeout evidence, while adding an explicit CR-001 refresh note that a fresh post-CR Linux CI artifact is required before it can prove current stricter coverage.

## Verification

Commands run:

```text
conda run -n stoner-cr python .github/scripts/test_run_deferred_validation.py
```

Result: 8 tests passed.

```text
conda run -n stoner-cr python -c <validate retained Linux readback report is rejected>
```

Result: exit code 0; validator rejected `Validation/019/Linux/deferred-readback-report.txt` with `ERROR: fewer than 18 StandardZ probes`, proving the stale retained artifact is no longer accepted as current coverage evidence.

```text
rg -n "at least 12|12 named semantic|12 native probes|fewer than twelve|range\(12\)" .github/scripts specs/019-deferred-rendering-pipeline Validation/019 CodeReview/Runs/CR-001-PreAsset-Hardening/Batches/B08-validation-failure-injection-artifacts-inspection.md CodeReview/Runs/CR-001-PreAsset-Hardening/findings.md
```

Result: no matches. Active validation docs and wrapper code no longer retain the old current threshold.

## Follow-Up

`CR001-B08-F006` is Fixed, not Verified. B08-S15 should independently rerun the focused wrapper tests and inspect the validator/doc alignment from current HEAD. A fresh Linux CI artifact may be collected later when remote validation budget is intentionally spent.
