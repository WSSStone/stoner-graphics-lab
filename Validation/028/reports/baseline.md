# Feature 028 Pre-Change Baseline

Captured on 2026-08-22 from branch `028-production-content-acceptance` before
production-content implementation beyond the validation scaffolding.

## Build Gates

| Gate | Result |
|---|---|
| `conda run -n godot scons config=debug -j8` | PASS |
| `conda run -n godot scons config=release strict=1 -j8` | PASS |
| Debug focused engine suites listed below | PASS |
| Release `production-content` scaffolding suite | PASS, 3 tests |

The strict Release executable was produced at
`Build/Mac/Release/Tests/StonerTest`; no build process remained after the gate.

## Focused Engine Regression

The following Debug command passed without failures:

```text
Build/Mac/Debug/Tests/StonerTest \
  --suite asset-cooker-target-profile \
  --suite asset-manager \
  --suite asset-static-model \
  --suite renderer-static-mesh \
  --suite deferred-renderer \
  --suite renderer-forward \
  --suite vulkan \
  --suite metal-device
```

This covers the existing AssetCooker profile contract, Runtime Asset Manager,
static-model import and realization, forward/deferred planning, and the Vulkan
and Metal device foundations that Feature 028 will compose.

## Architecture And Validation Scripts

`conda run -n godot python Tests/verify_architecture.py` passed the Asset,
Tools, Metal/Objective-C++, SPIRV-Cross, and native/private ownership checks.

The existing Python unit tests for the Feature 025-027 validation runners all
passed:

- `test_run_asset_cooker_validation.py`: 4 tests
- `test_run_runtime_asset_manager_validation.py`: 4 tests
- `test_run_static_model_validation.py`: 4 tests
- `test_run_deferred_validation.py`: 9 tests
- `test_run_metal_validation.py`: 17 tests

## Feature 028 Test-First Boundary

The four newly introduced Feature 028 Python skeletons intentionally fail at
this baseline because their production modules do not exist yet:

- `verify_production_corpus.py`
- `run_production_content_validation.py`
- `compare_production_images.py`
- `production_acceptance_report.py`

These failures are expected red tests, not regressions in the inherited
Features 025-027 baseline. They must become green before Feature 028 closeout.
