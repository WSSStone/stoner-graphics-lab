# Feature 027 Pre-Implementation Baseline

## Identity

- Base revision: `5244d5086860f4da5548e3b581945adf417146b5`
- Host: macOS arm64
- Compiler: Apple clang 21.0.0 (`clang-2100.1.1.101`)
- Python: 3.12.13 from Conda environment `godot`
- SCons: 4.10.1
- Frozen public RHI overloads: 131

The working tree at capture time contains only Feature 027 planning and Phase 1
test/evidence scaffolding; no Core, Asset, RHI, Renderer, Application, Demo, or
Backend production implementation has changed.

## Commands And Results

| Command | Result |
|---|---|
| `conda run -n godot scons -j8 config=debug strict=1` | PASS |
| `conda run -n godot scons -j8 config=release strict=1` | PASS |
| `conda run -n godot python -m unittest Tests/test_verify_metal_backend.py` | PASS, 5 tests |
| `conda run -n godot python Tests/verify_metal_backend.py --root . --mode all --write-rhi-matrix Validation/027/reports/rhi-operation-matrix.md` | PASS |
| `Build/Mac/Debug/Tests/StonerTest --suite metal --metal-determinism-runs 20 --metal-lifecycle-iterations 10000` | PASS, scaffold only; no native claim |
| `Build/Mac/Release/Tests/StonerTest --suite metal --metal-determinism-runs 20 --metal-lifecycle-iterations 10000` | PASS, scaffold only; no native claim |
| Release focused RHI/Vulkan/native/deferred/triangle/Renderer/Asset/Cooker/Manager suite command | PASS |

The focused command selected `rhi`, `vulkan`, `vulkan-native`, `triangle-demo`,
`deferred-native`, `renderer-material-asset`, `asset-material-shader`,
`asset-cooker-profile`, `asset-cooker-codec`, and `asset-manager-contract`.

## Native Availability

The current process environment reports MoltenVK/Vulkan native execution as
unavailable because the runtime cannot expose Metal. Existing tests classify
that result explicitly and pass their optional-local contract. It is not Metal
or Vulkan native evidence and cannot satisfy a Feature 027 native gate.

The compiler emitted repeated `confstr()` warnings while resolving
`DARWIN_USER_TEMP_DIR` and used `/tmp`; these are host-environment diagnostics,
not project compiler warnings. Strict project compilation completed with exit
code 0.

## Frozen RHI Matrix

`rhi-operation-matrix.md` is generated from current `IRHI*.h` declarations and
contains one row per overload with a normalized signature SHA-256. Every row
starts as `required-native`. Later implementation may update status/evidence but
must not silently alter this baseline inventory.
