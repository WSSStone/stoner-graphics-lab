# Quickstart: Material & Shader Assets

## Prerequisites

- C++20 compiler and SCons 4.10.1.
- Existing project Vulkan prerequisites for native gates.
- Private vendored `yyjson 0.12.0`; no system yyjson package is required.
- Optional pinned `spirv-val` for the independent repository oracle.

The vendored parser must include `LICENSE`, `UPSTREAM.md`, `VERSION`, upstream
commit, and checked SHA-256 evidence. Run the provenance verifier before any
build that changes `ThirdParty/yyjson`.

## Build

```bash
conda run -n godot scons platform=macos config=debug strict=1 -j8
conda run -n godot scons platform=macos config=release strict=1 -j8
```

Use `platform=windows` or `platform=linux` on those hosts. Strict builds cover
project code; third-party warnings remain isolated from project warning policy.

## Focused Tests

```bash
Build/Mac/Debug/Tests/StonerTest --suite asset-material-shader
Build/Mac/Debug/Tests/StonerTest --suite renderer-material-asset
Build/Mac/Debug/Tests/StonerTest --suite renderer-material
Build/Mac/Debug/Tests/StonerTest --suite asset
```

The first suite covers strict parsing, canonical writing, limits, typed
dependencies, target selection, instance resolution, rollback, and concurrent
readers. The second covers flattened material conversion, complete source
manifests, transactional ShaderLibrary registration, and legacy Feature 014
equivalence.

## Repository Verification

```bash
conda run -n godot python Tests/verify_material_shader_provenance.py
conda run -n godot python Tests/verify_repository_shader_assets.py
conda run -n godot python -m unittest Tests/test_verify_repository_shader_assets.py
conda run -n godot python Tests/verify_asset_layer.py
```

The inventory gate expects six program definitions, 11 GLSL dependencies, and
11 SPIR-V dependencies under `Content/Shaders`. It verifies typed IDs,
stage/entry metadata, exact SHA-256 digests, shared Fullscreen dependencies,
and the absence of production shader file reads below Application/Demo
composition.

## Determinism

```bash
Build/Mac/Release/Tests/StonerTest \
  --suite asset-material-shader \
  --material-shader-determinism-runs 20 \
  --material-shader-report Validation/023/material-shader-determinism.json
```

The corpus contains at least 12 shader programs, 12 materials, 16 instances,
and 40 malformed/boundary definitions. All successful canonical definitions,
source manifests, selections, and normalized diagnostics must remain
byte-identical across repetitions. Eight-reader concurrency coverage must
produce the same results without a mutable global parser/cache. The report
records elapsed time with host CPU, OS, compiler, build configuration, and
corpus counts as comparison telemetry; elapsed time is not a cross-host merge
gate in Feature 023.

## Regression and Native Gates

```bash
Build/Mac/Debug/Tests/StonerTest --suite triangle-demo
Build/Mac/Debug/Tests/StonerTest --suite deferred-native
Build/Mac/Debug/Tests/StonerTest --suite vulkan-native
Build/Mac/Debug/Tests/StonerTest
```

Triangle and deferred native sessions must consume Renderer snapshots and
bytecode descriptions. Their existing deterministic image/readback,
failure-injection, and native cleanup evidence remains authoritative; Feature
023 introduces no semantic-oracle substitute and no new screenshot gate.

On Linux CI, run the full ASan/UBSan profile plus the focused concurrency
profile selected by the build system. Windows, macOS, and Linux must pass strict
Debug/Release, schema/conversion tests, repository verification, and existing
native gates supported by each platform policy.

## Expected Failure Checks

Before closeout, confirm each operation fails without partial publication:

- duplicate decoded JSON keys, including escaped-equivalent names;
- unknown ordinary field or unknown required extension;
- any configured size/depth/count limit;
- source/payload digest mismatch or wrong SPIR-V stage/entry point;
- duplicate or incomplete target sets and target ambiguity;
- material parent cycle, missing parent, depth overflow, or type-changing
  override;
- conflicting ShaderLibrary batch;
- native shader-module creation failure after an earlier stage succeeded.
