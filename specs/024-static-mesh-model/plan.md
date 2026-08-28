# Implementation Plan: Static Mesh & Model Pipeline

**Branch**: `024-static-mesh-model` | **Date**: 2026-07-31 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/024-static-mesh-model/spec.md`

## Summary

Feature 024 adds a bounded, deterministic glTF 2.0/GLB import pipeline that
publishes immutable static mesh, static model, material, image, and texture
assets through the existing Asset contracts. It also adds transactional
Renderer realization of static meshes through backend-neutral RHI buffer
upload and indexed-draw contracts.

Before any imported payload may be published, the project migrates its active
coordinate convention from the historical right-handed convention to the
Unreal-style convention selected during clarification: left-handed, Z-up,
+X-forward, +Y-right, meters, clockwise front faces. The migration preserves
the existing component cross-product formula, Hamilton quaternion algebra,
row-major matrix storage, column-vector multiplication, and scale-rotate-
translate order. It changes the documented axis interpretation, camera/view
basis, canonical front-face state, shader matrix packing, fixtures, and
cross-layer validation. Historical specifications retain their original
decision and receive explicit migration amendments.

The private document parser is pinned `cgltf v1.15` (`360db1a`) with the
mandatory accessor-overflow fix from upstream PR 293 (`8211a9f`). Project code
owns GLB preflight, checked arithmetic, URI resolution, limits, semantic
validation, coordinate conversion, missing-attribute policy, stable identity,
and atomic multi-output publication. MikkTSpace is pinned at
`3e895b49d05ea07e4c2133156cfa94369e19e409` under its zlib-style source
license and used privately for the default tangent generation path; image
decoding remains owned by Feature 021.

## Technical Context

**Language/Version**: C++20 with traditional public/private header and source separation; C99 translation units for private `cgltf` and MikkTSpace; no C++20 Modules
**Primary Dependencies**: Existing Core, Asset, RHI, Renderer, Application, and Vulkan Backend contracts; pinned private `cgltf v1.15` plus overflow backport; pinned private MikkTSpace at `3e895b49d05ea07e4c2133156cfa94369e19e409`; existing private yyjson 0.12.0 and stb_image 2.30; SCons 4.10.1
**Storage**: Source `.gltf`, `.glb`, external buffer, and image files resolved through `IAssetResolver`; immutable process-local CPU payloads and Renderer RHI snapshots; checked-in fixtures and validation evidence; no database, manifest, DDC, package, or persistent runtime cache
**Testing**: Existing `StonerTest` registry with new `asset-static-mesh`, `asset-static-model`, `asset-gltf-material`, `asset-gltf-hardening`, `asset-gltf-malformed`, `renderer-static-mesh`, and `coordinate-convention` suites; Khronos validator as an offline fixture oracle; repository boundary/fixture/coordinate-convention scripts; Debug, strict Release, Linux ASan/UBSan/TSan, deterministic repeat, Apple M4 Pro reference performance, native Vulkan matrix/culling probes, and Windows/macOS/Linux GitHub Actions
**Target Platform**: Windows x64, macOS Apple Silicon through MoltenVK, and Linux x64 including Lavapipe CI; the Asset import contract is graphics-API independent
**Project Type**: Layered C++ graphics engine and offline-capable asset import library
**Performance Goals**: On the documented Apple M4 Pro macOS Release reference profile, import and validate a representative model containing at least 100,000 vertices and 300,000 indices once for warm-up and five independently measured times, with every measured run completing within 5 seconds and tracked request-owned peak bytes remaining within the active aggregate allocation limit; produce identical canonical identities, versions, streams, hierarchy, dependencies, and diagnostics across 20 repeated imports
**Constraints**: Core-only Asset dependency; bounded source and dependency reads; checked 64-bit size arithmetic; no direct filesystem access from `cgltf`; no graphics types in Asset payloads; no native backend calls from Renderer; atomic package publication and all-or-nothing Renderer realization; glTF core 2.0 triangle primitives only; two UV sets in Feature 024
**Scale/Scope**: glTF/GLB static scenes with multiple scenes, nodes, meshes, primitives, materials, textures, embedded and external data; at least 20 valid fixtures and 40 malformed mutations; no skins, animation, morph targets, camera/light import, ECS creation, mesh optimization, or runtime streaming

## Constitution Check

*GATE: Passed before research and re-checked after Phase 1 design.*

- [x] **Spec-Driven Development**: The clarified specification contains 59
  functional requirements, 14 measurable success criteria, explicit
  exclusions, and stable decisions for all previously ambiguous behavior.
- [x] **Decoupled Architecture**: Asset remains Core-only. Renderer consumes
  Asset payloads and RHI interfaces. RHI exposes backend-neutral buffer upload
  and indexed-draw vocabulary. Vulkan implements those interfaces. `cgltf` and
  MikkTSpace are private to Asset.
- [x] **Design Pattern Discipline**: Parsing is split into preflight, document,
  resolver, accessor, geometry, hierarchy, material mapping, package assembly,
  and validation responsibilities. Import policy is a versioned profile, and
  Renderer realization uses a staged transaction.
- [x] **Multi-API Support**: Static mesh payloads contain typed CPU semantics,
  not Vulkan layouts. Renderer creates backend-neutral RHI descriptions, and
  no public contract exposes `Vk*`, `cgltf_*`, or parser-specific types.
- [x] **Advanced Graphics Readiness**: Stable primitive identity, canonical
  bounds, typed vertex semantics, source manifests, and separate Renderer
  packing profiles can feed later meshlet, ray-tracing, and residency stages
  without making a GPU layout authoritative.
- [x] **Naming Conventions**: Public types use `F`/`E`/`I`/`T` prefixes and
  PascalCase in the established Unreal-style project convention.
- [x] **Cross-Platform Compatibility**: All parsing, canonicalization, and
  realization contracts are portable; third-party C sources use isolated
  warning policy and pinned source evidence.
- [x] **Automated Cross-Platform Validation**: CI covers Windows, macOS, and
  Linux Debug/strict Release; Linux sanitizer and thread-sanitizer jobs remain
  required. Native matrix and culling probes run where Vulkan is available.

### Post-Design Re-check

Phase 1 introduces no constitutional exception. The only cross-layer additions
are backend-neutral RHI contracts required to keep Renderer out of Vulkan and
Asset out of graphics ownership. Historical coordinate documentation is
amended rather than rewritten, preserving the spec authority chain.

## Project Structure

### Documentation (this feature)

```text
specs/024-static-mesh-model/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── coordinate-convention.md
│   ├── gltf-import.md
│   ├── material-texture-binding.md
│   ├── renderer-static-mesh-realization.md
│   └── static-mesh-model-api.md
└── tasks.md
```

### Source Code (repository root)

```text
Source/
├── Core/
│   ├── Public/Core/
│   │   ├── FCoordinateConvention.h
│   │   ├── FMath.h
│   │   ├── FQuat.h
│   │   └── FVector3.h
│   └── Private/
├── Asset/
│   ├── Public/Asset/
│   │   ├── FMaterialShaderTypes.h
│   │   ├── FStaticMeshAsset.h
│   │   ├── FStaticMeshTypes.h
│   │   ├── FStaticModelAsset.h
│   │   └── FStaticModelImport.h
│   ├── Private/
│   │   ├── FCgltfDocument.*
│   │   ├── FGLTFAccessorDecoder.*
│   │   ├── FGLTFContainerPreflight.*
│   │   ├── FGLTFDependencyResolver.*
│   │   ├── FGLTFGeometryNormalizer.*
│   │   ├── FGLTFHierarchyBuilder.*
│   │   ├── FGLTFMaterialMapper.*
│   │   ├── FGLTFPackageAssembler.*
│   │   ├── FGLTFPackageValidator.*
│   │   ├── FGLTFStaticModelImporter.*
│   │   └── FMaterialShaderJsonCodec.cpp
│   └── ThirdParty/
│       ├── cgltf/
│       └── mikktspace/
├── RHI/
│   └── Public/RHI/
│       ├── FRHIBufferUploadDesc.h
│       ├── FRHIIndexedDrawArguments.h
│       ├── IRHICommandBuffer.h
│       └── IRHIDevice.h
├── Renderer/
│   ├── Public/Renderer/
│   │   ├── FShaderMatrixPacking.h
│   │   ├── FStaticMeshAssetConversion.h
│   │   └── FStaticMeshRealization.h
│   └── Private/
├── Application/
│   ├── Public/Application/
│   └── Private/
└── Backend/Vulkan/
    ├── Public/VulkanRHI/
    └── Private/

Content/
├── Materials/
├── Shaders/
└── Textures/

Tests/
├── AssetStaticMeshModelTests.*
├── AssetStaticModelBenchmark.*
├── AssetStaticModelConcurrency.*
├── AssetStaticModelDeterminism.*
├── CoordinateConventionTests.*
├── RendererStaticMeshTests.*
├── Fixtures/StaticModel/
│   └── fixture-manifest.json
├── verify_coordinate_convention.py
└── verify_static_model_fixtures.py

Validation/024/
├── README.md
├── licenses/
└── reports/

specs/
├── 004-core-math-library/        # explicit migration amendment
├── 017-scene-graph-ecs/          # migration impact note
├── 018-triangle-demo-integration/# migration impact note
└── 019-deferred-rendering-pipeline/# migration impact note
```

**Structure Decision**: Extend the existing layered engine in place. Canonical
CPU mesh/model ownership belongs to Asset; graphics realization belongs to
Renderer; upload and draw vocabulary belongs to RHI; Vulkan only implements the
backend side. Parser and tangent-generator source remain private Asset
dependencies. Coordinate convention changes are made in their owning layers
and validated before the importer can publish data.

## Design And Delivery Order

### M0 - Coordinate Convention Migration

1. Freeze the convention identity
   `UnrealLH_ZUp_XForward_YRight_Meters_CW`.
2. Add failing basis, positive-yaw, TRS, camera-depth, culling, negative-scale,
   normal-matrix, and non-symmetric CPU-to-GLSL matrix tests.
3. Preserve component `X.Cross(Y) == Z`, Hamilton quaternion multiplication,
   row-major storage, column-vector multiplication, and S-R-T composition.
4. Change world semantics to +X forward, +Y right, +Z up; centralize
   world-to-view/projection construction; make clockwise the canonical RHI
   front face.
5. Replace implicit matrix byte copying with explicit shader layout packing.
6. Amend Features 004, 017, 018, and 019 and rebuild their affected validation
   evidence. This milestone blocks M1-M6.

### M1 - Material Schema v2

1. Add Asset-owned sampler intent and structured texture binding.
2. Read both v1 and v2; upgrade v1 in memory with UV0/repeat/automatic-linear
   defaults; preserve existing v1 canonical digests.
3. Write glTF-derived materials as v2 and forbid lossy v2-to-v1 downgrade.
4. Extend MaterialInstance overrides and Renderer conversion transactionally.

### M2 - RHI Mesh Transfer Contracts

1. Add bounded buffer upload and full indexed-draw arguments including
   `FirstIndex` and signed `VertexOffset`.
2. Implement deterministic mock behavior and Vulkan behavior without exposing
   Vulkan staging APIs to Renderer.
3. Validate partial failure, lifecycle invalidation, range checks, and existing
   call-site compatibility.

### M3 - Immutable Mesh And Model Payloads

1. Add typed semantic streams, 16/32-bit index storage, primitives, material
   slots, bounds, model scenes/nodes, and versioned import profile.
2. Enforce finite values, matching stream counts, triangle-only topology,
   index range, unique stable keys, one-parent acyclic hierarchy, and complete
   dependency evidence.
3. Keep Asset data independent of GPU interleaving and vertex layout.

### M4 - Bounded glTF Import

1. Vendor pinned `cgltf` plus the required overflow patch and pinned
   MikkTSpace with source/license/hash manifests.
2. Run container preflight and capped allocation before parser traversal.
3. Resolve external buffers/images only through `IAssetResolver`; reject
   traversal, unsupported schemes, aliases that escape scope, and recursion.
4. Decode dense, normalized, interleaved, and sparse accessors with project
   checked arithmetic.
5. Convert positions as `(X, Y, Z)_UE = (Z, -X, Y)_glTF`; convert transforms by
   basis conjugation; preserve index order for the canonical clockwise
   contract; flip tangent handedness exactly once for the basis reflection and
   combine it with node determinant parity where consumed.
6. Generate flat normals by deterministic vertex splitting. Generate tangents
   with MikkTSpace only when normal mapping requires them and the selected UV
   set is valid. Keep extension points for future angle-based smoothing.
7. Build all outputs in scratch storage, validate the whole package, then
   publish atomically.

### M5 - Renderer Static Mesh Realization

1. Validate payloads and build a deterministic packing plan.
2. Allocate and upload all vertex/index buffers through RHI.
3. Publish an immutable snapshot containing buffers, vertex input, index type,
   primitive sections, material identities, bounds, and source manifest only
   after every stage succeeds.
4. Invalidate all newly created resources and return no snapshot on any
   failure.

### M6 - Integration And Evidence

1. Check in a licensed, hashed corpus from Khronos Asset Generator, selected
   Sample Assets, and repository-owned malformed mutations.
2. Run offline glTF Validator as a conformance oracle, not as the importer
   safety gate.
3. Prove deterministic repeat, registry atomicity, stable IDs across reorder,
   Release performance, sanitizers, native matrix/culling behavior, and all
   three CI platforms.
4. Produce `doc/024-static-mesh-model.html` and Validation 024 evidence.

## Complexity Tracking

No constitution violations require justification.
