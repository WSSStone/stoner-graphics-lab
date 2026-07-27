# B08-S15 Verification: Validation, Failure Injection, And Artifacts

## Finding Verified

- `CR001-B08-F006`: Deferred validation artifacts and wrapper under-check local light readback coverage

## Verified Commit

- Fix commit: `3fe5a6d fix(ci): tighten deferred readback artifact validation`
- Verification started from current CR HEAD after `43a0e6f docs(review): complete b08 validation artifact fix`

## Independent Checks

```text
conda run -n stoner-cr python .github/scripts/test_run_deferred_validation.py
```

Result: 8 tests passed. The wrapper test suite now rejects stale 12-probe reports and reports missing a required local-light probe.

```text
conda run -n stoner-cr python -c <validate retained Linux readback report is rejected>
```

Result: exit code 0 with `ERROR: fewer than 18 StandardZ probes`, proving the original retained Linux report is no longer accepted as current stricter readback evidence.

```text
rg -n "at least 12|12 named semantic|12 native probes|fewer than twelve|range\(12\)" .github/scripts specs/019-deferred-rendering-pipeline Validation/019
```

Result: no matches. Active wrapper code, Feature 019 docs, and Validation/019 README no longer present the old 12-probe threshold as current validation policy.

```text
rg -n "REQUIRED_PROBES_PER_CONVENTION|REQUIRED_LOCAL_LIGHT_PROBES|point-visible|spot-near-plane|At least 18|at least 18" .github/scripts/run_deferred_validation.py .github/scripts/test_run_deferred_validation.py specs/019-deferred-rendering-pipeline/spec.md specs/019-deferred-rendering-pipeline/plan.md specs/019-deferred-rendering-pipeline/quickstart.md specs/019-deferred-rendering-pipeline/tasks.md specs/019-deferred-rendering-pipeline/contracts/deferred-validation-contract.md Validation/019/README.md Validation/019/completion.md
```

Result: current validator constants and active documentation all reference the 18-probe and local-light requirements.

## Conclusion

`CR001-B08-F006` is Verified. A fresh post-CR Linux CI artifact is still a future evidence-refresh action, not a prerequisite for verifying this local wrapper/documentation correction.
