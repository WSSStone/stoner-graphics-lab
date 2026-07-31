# Quickstart: Static Mesh & Model Pipeline

## Prerequisites

- Conda environment `godot`
- Python 3.12
- SCons 4.10.1
- C++20 compiler
- Vulkan SDK/MoltenVK where native validation is requested
- Offline shader compiler and SPIR-V validator where repository shaders change

Third-party glTF parser, tangent generator, test fixtures, and validator
evidence are checked in and pinned. CI and normal tests must not download them.

## 1. Confirm Feature Context

```bash
git branch --show-current
cat .specify/feature.json
```

Expected branch: `024-static-mesh-model`.

## 2. Build Debug

```bash
conda run -n godot scons platform=macos target=debug
```

Use `platform=windows` or `platform=linux` on the corresponding machine.

## 3. Run The Coordinate Migration Gate

```bash
Build/Mac/Debug/Tests/StonerTest \
  --suite coordinate-convention \
  --suite core-math \
  --suite application-scene \
  --suite renderer-forward \
  --suite deferred-renderer

conda run -n godot python Tests/verify_coordinate_convention.py \
  --root .
```

This gate must pass before importer payload publication work proceeds. Native
validation must include a non-symmetric CPU-to-shader matrix, clockwise
front-face culling, and negative determinant coverage; identity-only evidence
does not satisfy the gate. The scanner must report no stale active
right-handed convention assertion; dated historical amendments are allowlisted
explicitly rather than hidden or rewritten.

## 4. Run Asset Import Tests

```bash
Build/Mac/Debug/Tests/StonerTest \
  --suite asset \
  --suite asset-material-shader \
  --suite asset-static-model
```

The focused static-model suite covers:

- GLB preflight and bounded parser allocation;
- resolver-scoped external buffers and images;
- normalized, interleaved, and sparse accessors;
- 16/32-bit and non-indexed primitives;
- flat-normal splitting and MikkTSpace tangents;
- stable explicit/fallback subresource IDs;
- scene hierarchy and unreferenced meshes;
- Material/MaterialInstance v1/v2 compatibility;
- package atomic rollback and deterministic diagnostics.

## 5. Run Renderer Realization Tests

```bash
Build/Mac/Debug/Tests/StonerTest \
  --suite rhi \
  --suite vulkan \
  --suite renderer-static-mesh
```

The Renderer suite injects failure into validate, plan, allocation, upload, and
finalize stages and verifies that no partial snapshot or live request-owned RHI
resource survives.

## 6. Validate The Fixture Manifest

```bash
conda run -n godot python Tests/verify_static_model_fixtures.py \
  --manifest Validation/024/fixture-manifest.json \
  --fixtures Tests/Fixtures/StaticModel
```

The verifier checks:

- at least 20 valid fixtures and 40 malformed mutations;
- SHA-256, source revision, license/attribution, and expected result;
- pinned Khronos validator evidence;
- no online fixture reference without checked-in bytes;
- importer-scope classification for valid-but-unsupported glTF.

## 7. Run Determinism And Performance Evidence

```bash
Build/Mac/Release/Tests/StonerTest \
  --suite asset-static-model \
  --static-model-determinism-runs 20 \
  --static-model-performance-runs 5 \
  --static-model-performance-max-seconds 5 \
  --static-model-performance-fixture \
    Tests/Fixtures/StaticModel/Performance/Representative.glb
```

The representative fixture must contain at least 100,000 vertices, 300,000
indices, 16 primitives, and 16 material/texture dependencies. The reference
profile is this project's Apple M4 Pro macOS Release development machine; its
exact CPU configuration, memory, OS, compiler, and source hash are frozen in
the report. Run one unmeasured warm-up followed by five independent measured
imports. Every measured import must complete within 5 seconds, and tracked
request-owned peak bytes must not exceed the active import profile's aggregate
allocation limit. Record every elapsed time, the tracked peak, configured
limit, and canonical output digest under `Validation/024/reports/`.

## 8. Build Strict Release

```bash
conda run -n godot scons \
  platform=macos \
  target=release \
  warnings_as_errors=yes
```

Third-party warning suppression must remain isolated to the private C
translation units. Project sources stay under strict warnings.

## 9. Sanitizers

Linux CI runs:

```bash
scons platform=linux target=debug sanitizer=address,undefined
Build/Linux/Debug/Tests/StonerTest \
  --suite coordinate-convention \
  --suite asset-static-model \
  --suite renderer-static-mesh
```

It also runs the existing TSan profile for Asset registry/import concurrency.
Every malformed corpus case must terminate with a deterministic result and no
sanitizer finding.

## 10. Full Local Regression

```bash
Build/Mac/Debug/Tests/StonerTest
git diff --check
```

Feature closeout additionally requires the Windows/macOS/Linux CI matrix,
macOS and Windows visible/native evidence where applicable, Linux Lavapipe
native evidence, and refreshed Feature 018/019 validation affected by the
coordinate migration.

## Expected Public Flow

```cpp
FAssetImportRequest Request;
Request.Descriptor = SourceDescriptor;
Request.Source = SourceLease;
Request.Parameters = MakeShared<FStaticModelImportProfile>(Profile);

TArray<FAssetImportOutput> Outputs;
FAssetDiagnosticList Diagnostics;
const EAssetResult Imported =
    Importer.Import(Request, Outputs, &Diagnostics);

// On success Outputs is the complete, identity-sorted package.
// On failure Outputs is empty.
```

Renderer realization remains separate:

```cpp
FStaticMeshRealizationRequest Request;
Request.Device = Device;
Request.Asset = StaticMesh;
Request.Profile = RealizationProfile;

const FStaticMeshRealizationResult Result =
    FStaticMeshRealizer::Realize(Request);

// Result.Snapshot exists only after every allocation and upload succeeds.
```
