# Feature 028 US5 Evidence Gates

## Scope

This report records bounded local evidence for acceptance-report construction,
the 30-case failure catalog, artifact integrity, privacy, window-only capture
metadata, size limits, and 20-run deterministic correctness separation. Native
device observations and accepted reference-image calibration remain tracked by
the dedicated US3 and hardware closeout tasks.

## Results

| Gate | Command | Result |
|---|---|---|
| Image, baseline, native-proof and failure-catalog contracts | `Build/Mac/Debug/Tests/StonerTest --suite production-content --suite production-image-acceptance` | PASS, 17 checks |
| Report schema, result/failure invariants, digest and boundedness | `conda run -n godot python .github/scripts/test_production_acceptance_report.py` | PASS, 14 tests |
| Absolute-path, credential, identifier and capture privacy | `conda run -n godot python .github/scripts/test_production_evidence_privacy.py` | PASS, 6 tests |
| Canonical PPM comparison helper | `conda run -n godot python .github/scripts/test_compare_production_images.py` | PASS, 1 test |
| Profile runner, 30-case catalog, Unsupported aggregation, artifact substitution and 20-run correctness determinism | `conda run -n godot python .github/scripts/test_run_production_content_validation.py` | PASS, 25 tests |

The runner tests intentionally invoke invalid CLI boundaries and therefore
print argparse usage for rejected determinism counts; the enclosing tests pass
only when those invalid inputs are rejected. An initial local invocation using
`python -m unittest` with a filesystem path was invalid because the module
loader interpreted the leading dot as an empty module name. It was replaced by
the direct script commands above and is not counted as gate evidence.

## Outcome

All T107 deterministic and structural gates pass. `Unsupported` remains a
structured non-success, artifact substitution is rejected, reports enforce the
declared file/count/size limits, and observational timing/RSS/image fields do
not alter the 20-run canonical correctness digest.
