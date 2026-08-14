# Tasks: Static Mesh & Model Pipeline

**Input**: Design documents from `/specs/024-static-mesh-model/`
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`,
`contracts/`, `quickstart.md`

**Tests**: Feature 024 explicitly requires contract, malformed-input,
determinism, concurrency, performance, native, sanitizer, and cross-platform
validation. Test tasks are therefore mandatory and precede their corresponding
implementation tasks.

**Organization**: Shared coordinate, Material v2, and RHI contract migrations
are blocking foundations. Remaining tasks are grouped by the five user stories
and preserve their independent acceptance gates.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: May run in parallel because it writes different files and has no
  dependency on another incomplete task in the same wave.
- **[Story]**: Maps the task to a user story from `spec.md`.
- Every task names the exact file or directory it changes.

---

## Phase 1: Setup And Pinned Inputs

**Purpose**: Establish reproducible third-party source and fixture scaffolding
before changing public engine contracts.

- [X] T001 Vendor cgltf v1.15 commit `360db1a` plus overflow backport `8211a9f`, preserve the MIT license, and record source hashes in `ThirdParty/cgltf/cgltf.h`, `ThirdParty/cgltf/LICENSE`, `ThirdParty/cgltf/UPSTREAM.md`, and `ThirdParty/cgltf/SHA256SUMS`
- [X] T002 [P] Vendor MikkTSpace commit `3e895b49d05ea07e4c2133156cfa94369e19e409`, preserve its zlib-style source notice, and record hashes in `ThirdParty/mikktspace/mikktspace.c`, `ThirdParty/mikktspace/mikktspace.h`, `ThirdParty/mikktspace/UPSTREAM.md`, and `ThirdParty/mikktspace/SHA256SUMS`
- [X] T003 Compile cgltf and MikkTSpace as private C sources with third-party-only warning suppression and no public include leakage in `Source/Asset/SConscript`
- [X] T004 [P] Create the licensed static-model corpus layout and manifest schema in `Tests/Fixtures/StaticModel/README.md`, `Validation/024/fixture-manifest.json`, and `Validation/024/licenses/README.md`
- [X] T005 Implement pinned dependency and fixture-manifest verification with unit coverage in `Tests/verify_static_model_provenance.py` and `Tests/test_verify_static_model_provenance.py`
- [X] T006 [P] Establish Feature 024 evidence structure and command conventions in `Validation/024/README.md` and `Validation/024/reports/README.md`

**Checkpoint**: Third-party inputs and future validation data have pinned,
auditable ownership before engine implementation begins.

---

## Phase 2: Foundational Migrations

**Purpose**: Complete the blocking coordinate convention, Material schema v2,
and backend-neutral mesh transfer contracts required by every story.

**Critical**: M0 coordinate migration T007-T021 completes first. Material v2
T022-T029 and RHI transfer T030-T036 may then proceed in parallel; T037 joins
all three. No canonical mesh/model payload may be accepted before T037.

### Coordinate Convention Migration

- [X] T007 Add convention identity, basis, yaw, camera-depth, culling, negative-scale, and non-symmetric shader-matrix tests in `Tests/CoordinateConventionTests.cpp` and `Tests/CoordinateConventionTests.h`
- [X] T008 [P] Replace historical right-handed assertions with failing Unreal-convention algebra and TRS probes in `Tests/CoreMathTests.cpp`
- [X] T009 [P] Add failing +X-forward light, hierarchy composition, and preserve-world reparent probes in `Tests/ApplicationSceneEcsTests.cpp`
- [X] T010 [P] Add +X view-depth, transparency ordering, deferred reconstruction, and normal-transform probes in `Tests/RendererForwardPipelineTests.cpp` and `Tests/DeferredRenderingTests.cpp`
- [X] T011 [P] Add clockwise-default, negative-determinant, and backend front-face mapping probes in `Tests/RHICoreTests.cpp` and `Tests/VulkanBackendTests.cpp`
- [X] T012 [P] Add a native non-symmetric CPU-to-GLSL matrix readback and clockwise-culling probe in `Tests/DeferredNativeIntegrationTests.cpp`
- [X] T013 Define `UnrealLH_ZUp_XForward_YRight_Meters_CW`, preserve component cross/Hamilton/S-R-T algebra, and update public convention comments in `Source/Core/Public/Core/FCoordinateConvention.h`, `Source/Core/Public/Core/FMath.h`, `Source/Core/Public/Core/FQuat.h`, and `Source/Core/Public/Core/FVector3.h`
- [X] T014 Migrate default world directions and Scene render summaries to +X forward, +Y right, +Z up in `Source/Application/Private/FRenderSystem.cpp`, `Source/Application/Public/Application/FSceneRenderSummary.h`, and `Source/Application/Private/FSceneRenderSummary.cpp`
- [X] T015 Centralize world-to-view and named view-depth construction for forward/deferred consumers in `Source/Renderer/Public/Renderer/FForwardViewData.h`, `Source/Renderer/Private/FForwardViewData.cpp`, `Source/Renderer/Private/FMeshDrawCommand.cpp`, and `Source/Renderer/Private/FDeferredLightVolume.cpp`
- [X] T016 Add explicit row-major CPU to target shader matrix packing and remove implicit matrix byte copying in `Source/Renderer/Public/Renderer/FShaderMatrixPacking.h`, `Source/Renderer/Private/FShaderMatrixPacking.cpp`, and `Source/Renderer/Private/FDeferredFrameExecutor.cpp`
- [X] T017 Change the backend-neutral canonical raster default to clockwise while retaining explicit overrides in `Source/RHI/Public/RHI/FRHIGraphicsPipelineDesc.h`
- [X] T018 Apply clockwise/front-face parity exactly once in Vulkan pipeline creation and native execution in `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp` and `Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp`
- [X] T019 Migrate repository shader matrix conventions and rebuild checked-in SPIR-V in `Content/Shaders/Deferred/Surface.vert`, `Content/Shaders/Deferred/PointLight.frag`, `Content/Shaders/Deferred/SpotLight.frag`, and their `.spv` siblings
- [X] T020 Update deterministic/native validation orchestration for non-identity matrices and canonical front faces in `.github/scripts/run_triangle_demo_validation.py`, `.github/scripts/run_deferred_validation.py`, and `.github/scripts/test_run_deferred_validation.py`
- [X] T021 Append dated coordinate migration amendments without rewriting historical decisions in `specs/004-core-math-library/plan.md`, `specs/004-core-math-library/research.md`, `specs/004-core-math-library/tasks.md`, `specs/004-core-math-library/contracts/core-math-api.md`, `specs/017-scene-graph-ecs/spec.md`, `specs/018-triangle-demo-integration/spec.md`, and `specs/019-deferred-rendering-pipeline/spec.md`; add a stale active-convention scanner with historical-amendment allowlisting and unit coverage in `Tests/verify_coordinate_convention.py` and `Tests/test_verify_coordinate_convention.py`, then record its clean result in `Validation/024/reports/coordinate-migration.txt`

### Shared Material Schema v2

- [X] T022 Add failing Material/MaterialInstance v1 compatibility, v2 texture-binding, canonical JSON, dependency, and lossy-downgrade tests in `Tests/AssetMaterialShaderTests.cpp`
- [X] T023 [P] Add failing Asset sampler-intent to RHI sampler conversion tests in `Tests/RendererMaterialShaderAssetTests.cpp`
- [X] T024 Add Asset-owned sampler enums, `FMaterialSamplerIntent`, `FMaterialTextureBinding`, and the `TextureBinding` parameter variant in `Source/Asset/Public/Asset/FMaterialShaderTypes.h` and `Source/Asset/Private/FMaterialShaderTypes.cpp`
- [X] T025 Implement strict schema-v2 read/write plus byte-identical schema-v1 round trips in `Source/Asset/Private/FMaterialShaderJsonCodec.h` and `Source/Asset/Private/FMaterialShaderJsonCodec.cpp`
- [X] T026 Validate structured bindings, upgrade v1 defaults, extract typed texture dependencies, and support instance overrides in `Source/Asset/Private/FMaterialShaderSchemaValidator.cpp`, `Source/Asset/Private/FMaterialAssetValidator.cpp`, `Source/Asset/Private/FMaterialDependencyExtractor.cpp`, and `Source/Asset/Private/FMaterialInstanceResolver.cpp`
- [X] T027 Map backend-neutral sampler intent and complete texture bindings into Renderer snapshots without Asset-to-RHI leakage in `Source/Renderer/Private/FMaterialAssetConversion.cpp` and `Source/Renderer/Public/Renderer/FMaterialAssetConversion.h`
- [X] T028 Add schema-v2 valid/invalid definitions and preserve schema-v1 golden digests in `Tests/Fixtures/MaterialShader/Valid/`, `Tests/Fixtures/MaterialShader/Invalid/`, and `Tests/Fixtures/MaterialShader/Golden/canonical-digests.txt`
- [X] T029 Run and record the Material v1/v2 compatibility gate in `Validation/024/reports/material-schema-v2.txt`

### RHI Buffer Upload And Indexed Draw

- [X] T030 Add failing range, lifecycle, unsupported-device, and full indexed-draw argument tests in `Tests/RHICoreTests.cpp`
- [X] T031 [P] Add failing device-local upload, first-index, signed vertex-offset, and invalidation tests in `Tests/VulkanBackendTests.cpp`
- [X] T032 Define `FRHIBufferUploadDesc`, `FRHIIndexedDrawArguments`, non-retaining upload semantics, and compatibility overloads in `Source/RHI/Public/RHI/FRHIBufferUploadDesc.h`, `Source/RHI/Public/RHI/FRHIIndexedDrawArguments.h`, `Source/RHI/Public/RHI/IRHIDevice.h`, and `Source/RHI/Public/RHI/IRHICommandBuffer.h`
- [X] T033 Implement validated host-visible and staged device-local buffer upload behind `IRHIDevice::UploadBuffer` in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDevice.h` and `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T034 Implement full indexed-draw argument recording and Vulkan command mapping in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandBuffer.h`, `Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp`, and `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp`
- [X] T035 Migrate existing Renderer draw call sites to compatibility-safe indexed arguments in `Source/Renderer/Private/FDeferredFrameExecutor.cpp`
- [X] T036 Run and record Debug plus strict Release Core/RHI/Vulkan migration gates in `Validation/024/reports/foundation-rhi.txt`
- [X] T037 Register the `coordinate-convention` suite, include all foundation test sources, and run the complete blocking gate in `Tests/Main.cpp`, `Tests/SConscript`, and `Validation/024/reports/foundation-complete.txt`

**Checkpoint**: One engine-wide coordinate convention, shared Material schema
v2, and backend-neutral mesh upload/draw contracts are stable.

---

## Phase 3: User Story 1 - Import Canonical Static Geometry (Priority: P1)

**Goal**: Import glTF/GLB triangle geometry into deterministic immutable
`FStaticMeshAsset` payloads independent of source packing.

**Independent Test**: Run `StonerTest --suite asset-static-mesh` against JSON,
GLB, indexed/non-indexed, dense/interleaved/sparse/normalized, missing
attribute, strict-policy, and basis fixtures for 20 identical runs.

The currently registered suite is a foundation gate only. It does not satisfy
this independent test until T040-T057 add the policy corpus, real importer,
deterministic repetition, and recorded acceptance evidence.

### Tests For User Story 1

- [X] T038 [P] [US1] Add failing payload validation, stream, index-width, primitive, and bounds tests in `Tests/AssetStaticMeshGeometryTests.cpp` and `Tests/AssetStaticMeshGeometryTests.h`
- [X] T039 [P] [US1] Add failing JSON/GLB preflight plus dense/interleaved/sparse/normalized accessor tests in `Tests/AssetGLTFContainerTests.cpp` and `Tests/AssetGLTFContainerTests.h`
- [X] T040 [P] [US1] Add failing default/strict missing-normal, tangent, degenerate, UV, and policy-version tests in `Tests/AssetGLTFPolicyTests.cpp` and `Tests/AssetGLTFPolicyTests.h`
- [X] T041 [P] [US1] Add at least 12 repository-owned golden primitives covering basis, packing, index width, missing attributes, transforms, front-face parity, primitive bounds, and model bounds with SC-004 expected values in `Tests/Fixtures/StaticModel/Valid/Geometry/`

### Implementation For User Story 1

- [X] T042 [US1] Define Asset-only mesh bounds, semantic streams, 16/32-bit index data, primitives, and material slots in `Source/Asset/Public/Asset/FStaticMeshTypes.h` and `Source/Asset/Private/FStaticMeshTypes.cpp`
- [X] T043 [US1] Implement immutable `FStaticMeshAsset::CreateValidated`, aggregate bounds, dependency/source-manifest checks, and type traits in `Source/Asset/Public/Asset/FStaticMeshAsset.h`, `Source/Asset/Private/FStaticMeshAsset.cpp`, and `Source/Asset/Private/FStaticMeshAssetValidator.cpp`
- [X] T044 [US1] Implement versioned geometry policies, finite import limits, coordinate identity, and profile digesting in `Source/Asset/Public/Asset/FStaticModelImport.h` and `Source/Asset/Private/FStaticModelImport.cpp`
- [X] T045 [US1] Wrap patched cgltf with capped allocation, bounded in-memory parse, RAII cleanup, and no filesystem callbacks in `Source/Asset/Private/FCgltfDocument.h` and `Source/Asset/Private/FCgltfDocument.cpp`
- [X] T046 [US1] Implement checked JSON/GLB source preflight for header, chunks, lengths, alignment, padding, and trailing bytes in `Source/Asset/Private/FGLTFContainerPreflight.h` and `Source/Asset/Private/FGLTFContainerPreflight.cpp`
- [X] T047 [US1] Implement checked dense, interleaved, normalized, and sparse accessor decoding with strict index ordering in `Source/Asset/Private/FGLTFAccessorDecoder.h` and `Source/Asset/Private/FGLTFAccessorDecoder.cpp`
- [X] T048 [US1] Implement glTF-to-Unreal basis conjugation, index-order preservation, tangent reflection parity, and finite stream normalization in `Source/Asset/Private/FGLTFGeometryNormalizer.h` and `Source/Asset/Private/FGLTFGeometryNormalizer.cpp`
- [X] T049 [US1] Implement deterministic flat-normal vertex splitting and reject non-derivable degenerate references in `Source/Asset/Private/FStaticMeshNormalGenerator.h` and `Source/Asset/Private/FStaticMeshNormalGenerator.cpp`
- [X] T050 [US1] Adapt pinned MikkTSpace for required-tangent generation, selected UV validation, and canonical handedness in `Source/Asset/Private/FStaticMeshTangentGenerator.h` and `Source/Asset/Private/FStaticMeshTangentGenerator.cpp`
- [X] T051 [US1] Derive and validate deterministic box/sphere primitive and mesh bounds from canonical positions in `Source/Asset/Private/FStaticMeshBounds.cpp`
- [X] T052 [US1] Implement deterministic glTF/GLB probing and canonical mesh-only import flow in `Source/Asset/Private/FGLTFStaticModelImporter.h` and `Source/Asset/Private/FGLTFStaticModelImporter.cpp`
- [X] T053 [US1] Expose scoped glTF importer registration without parser-native public types in `Source/Asset/Public/Asset/FStaticModelImport.h` and `Source/Asset/Private/AssetModule.cpp`
- [X] T054 [US1] Implement normalized mesh stream, primitive, bounds, policy, and source-version inspection in `Source/Asset/Public/Asset/FStaticMeshInspection.h` and `Source/Asset/Private/FStaticMeshInspection.cpp`
- [X] T055 [US1] Register `asset-static-mesh`, aggregate it into `asset`, and compile US1 test sources in `Tests/Main.cpp`, `Tests/AssetTests.cpp`, `Tests/AssetTests.h`, and `Tests/SConscript`
- [X] T056 [US1] Add 20-run canonical geometry digest/report support in `Tests/AssetStaticMeshGeometryTests.cpp` and `Validation/024/reports/geometry-determinism.txt`
- [X] T057 [US1] Run the complete US1 independent test and record accepted fixture IDs, digests, SC-004 comparison results, and failures in `Validation/024/reports/us1-canonical-geometry.txt`

**Checkpoint**: Canonical static mesh geometry is independently importable,
deterministic, bounded, and inspectable.

---

## Phase 4: User Story 2 - Preserve Model Hierarchy And Typed Subresources (Priority: P1)

**Goal**: Preserve scenes, local hierarchy, shared meshes, stable typed
subresource identities, and atomic package output.

**Independent Test**: Run `StonerTest --suite asset-static-model` over
multi-scene, shared-mesh, duplicate-name, explicit-key, fallback-key, matrix/TRS,
negative-scale, cycle, multiple-parent, and unreferenced-mesh fixtures.

### Tests For User Story 2

- [X] T058 [P] [US2] Add failing scene/root/node order, matrix/TRS, shared-mesh, cycle, parent, depth, and unreferenced-mesh tests in `Tests/AssetStaticModelHierarchyTests.cpp` and `Tests/AssetStaticModelHierarchyTests.h`
- [X] T059 [P] [US2] Add failing explicit-key reorder, duplicate-key, invalid-key, name-independence, and fallback identity tests in `Tests/AssetStaticModelIdentityTests.cpp` and `Tests/AssetStaticModelIdentityTests.h`
- [X] T060 [P] [US2] Add multi-scene, hierarchy, identity-reorder, no-key fallback, and invalid graph fixtures in `Tests/Fixtures/StaticModel/Valid/Hierarchy/` and `Tests/Fixtures/StaticModel/Invalid/Hierarchy/`

### Implementation For User Story 2

- [X] T061 [US2] Define immutable model nodes, roots, scene evidence, mesh references, hierarchy bounds, and model type traits in `Source/Asset/Public/Asset/FStaticModelAsset.h` and `Source/Asset/Private/FStaticModelAsset.cpp`
- [X] T062 [US2] Validate one-parent acyclic hierarchy, root completeness, finite transforms, depth, references, and aggregate bounds in `Source/Asset/Private/FStaticModelAssetValidator.h` and `Source/Asset/Private/FStaticModelAssetValidator.cpp`
- [X] T063 [US2] Convert glTF matrix/TRS nodes by basis conjugation and build deterministic root/child order in `Source/Asset/Private/FGLTFHierarchyBuilder.h` and `Source/Asset/Private/FGLTFHierarchyBuilder.cpp`
- [X] T064 [US2] Extract and normalize `extras.stonerAssetId`, enforce per-type uniqueness, and generate typed structural fallback keys in `Source/Asset/Private/FGLTFStableKey.h` and `Source/Asset/Private/FGLTFStableKey.cpp`
- [X] T065 [US2] Plan model, mesh, material, image, and texture IDs before decoding so references are independent of allocation and publication order in `Source/Asset/Private/FGLTFPackageIdentityPlanner.h` and `Source/Asset/Private/FGLTFPackageIdentityPlanner.cpp`
- [X] T066 [US2] Assemble one model per scene, record the default scene, deduplicate shared meshes, and retain unreferenced mesh outputs in `Source/Asset/Private/FGLTFPackageAssembler.h` and `Source/Asset/Private/FGLTFPackageAssembler.cpp`
- [X] T067 [US2] Validate complete output identities, payload types, versions, dependencies, and references before returning any output in `Source/Asset/Private/FGLTFPackageValidator.h` and `Source/Asset/Private/FGLTFPackageValidator.cpp`
- [X] T068 [US2] Integrate hierarchy and package assembly into the importer with empty-output rollback on failure in `Source/Asset/Private/FGLTFStaticModelImporter.cpp`
- [X] T069 [US2] Add deterministic model roots, nodes, scene/default-scene, stable-key policy, and package dependency inspection in `Source/Asset/Public/Asset/FStaticModelInspection.h` and `Source/Asset/Private/FStaticModelInspection.cpp`
- [X] T070 [US2] Register `asset-static-model`, aggregate hierarchy/identity tests, and compile US2 sources in `Tests/Main.cpp`, `Tests/AssetTests.cpp`, and `Tests/SConscript`
- [X] T071 [US2] Run the complete US2 independent test including explicit-key reorder and atomic registry publication evidence in `Validation/024/reports/us2-model-hierarchy.txt`

**Checkpoint**: Models preserve authored static assembly and stable typed
subresources without creating Scene/ECS objects.

---

## Phase 5: User Story 3 - Import Material And Texture Dependencies (Priority: P2)

**Goal**: Map core glTF metallic-roughness materials and PNG/JPEG resources into
the shared Material v2, Image, and Texture contracts.

**Independent Test**: Run `StonerTest --suite asset-gltf-material` over every
supported factor, texture role, UV set, sampler mode, alpha mode, default
material, embedded/external image, shared-image semantic split, and dependency
failure.

### Tests For User Story 3

- [X] T072 [P] [US3] Add failing PBR factors, alpha, two-sided, texture-binding, UV/sampler, and default-material mapping tests in `Tests/AssetGLTFMaterialTests.cpp` and `Tests/AssetGLTFMaterialTests.h`
- [X] T073 [P] [US3] Add failing color/data semantic split, shared-image, embedded/external PNG/JPEG, and dependency failure tests in `Tests/AssetGLTFImageDependencyTests.cpp` and `Tests/AssetGLTFImageDependencyTests.h`
- [X] T074 [P] [US3] Add licensed core PBR, sampler, UV1, alpha, shared-image, and invalid dependency fixtures in `Tests/Fixtures/StaticModel/Valid/Materials/` and `Tests/Fixtures/StaticModel/Invalid/Materials/`

### Implementation For User Story 3

- [X] T075 [US3] Define and validate a versioned glTF material mapping profile with explicit surface shader and parameter identities in `Source/Asset/Public/Asset/FGLTFMaterialMappingProfile.h` and `Source/Asset/Private/FGLTFMaterialMappingProfile.cpp`
- [X] T076 [US3] Map core metallic-roughness factors, texture roles, alpha state, double-sided state, scalar factors, and structured bindings in `Source/Asset/Private/FGLTFMaterialMapper.h` and `Source/Asset/Private/FGLTFMaterialMapper.cpp`
- [X] T077 [US3] Route embedded/data-URI/external PNG and JPEG bytes through Feature 021 Image/Texture import contracts with explicit color/data semantics in `Source/Asset/Private/FGLTFImageTextureBridge.h` and `Source/Asset/Private/FGLTFImageTextureBridge.cpp`
- [X] T078 [US3] Resolve source-relative image dependencies only through the scoped resolver and canonical source manifest in `Source/Asset/Private/FGLTFDependencyResolver.h` and `Source/Asset/Private/FGLTFDependencyResolver.cpp`
- [X] T079 [US3] Generate one deterministic package-local core-default Material v2 asset under the active mapping profile in `Source/Asset/Private/FGLTFDefaultMaterial.cpp`
- [X] T080 [US3] Deduplicate source images while splitting incompatible Texture semantic identities and dependency roles in `Source/Asset/Private/FGLTFPackageAssembler.cpp`
- [X] T081 [US3] Upgrade the repository deferred surface material and interface to schema v2 texture bindings without changing shader logical identity in `Content/Materials/DeferredSurface.material.json` and `Content/Shaders/Deferred/Surface.shader.json`
- [X] T082 [US3] Integrate material/image/texture outputs and all-or-nothing dependency validation into `Source/Asset/Private/FGLTFStaticModelImporter.cpp` and `Source/Asset/Private/FGLTFPackageValidator.cpp`
- [X] T083 [US3] Register `asset-gltf-material`, aggregate it into `asset`, and compile US3 sources in `Tests/Main.cpp`, `Tests/AssetTests.cpp`, and `Tests/SConscript`
- [X] T084 [US3] Add 20-run material/texture mapping and dependency digest evidence in `Tests/AssetGLTFMaterialTests.cpp` and `Validation/024/reports/material-determinism.txt`
- [X] T085 [US3] Run the complete US3 independent test and record all supported PBR fields, semantic splits, and atomic failures in `Validation/024/reports/us3-material-texture.txt`

**Checkpoint**: Imported geometry retains equivalent material and texture
meaning through shared Asset contracts.

---

## Phase 6: Bounded Import Hardening

**Purpose**: Complete Plan M4 safety, limits, normalized diagnostics, and atomic
publication before Renderer consumes any imported payload. This is a blocking
foundation, not the completed US5 acceptance story.

- [X] T086 [P] Add failing malformed JSON, GLB, accessor, sparse, index, topology, extension, skin, morph, and compression tests in `Tests/AssetGLTFMalformedTests.cpp` and `Tests/AssetGLTFMalformedTests.h`
- [X] T087 [P] Add failing URI traversal, percent ambiguity, unsupported scheme, alias, self-resolution, and missing dependency tests in `Tests/AssetGLTFResolverTests.cpp` and `Tests/AssetGLTFResolverTests.h`
- [X] T088 [P] Add failing source/dependency/allocation/count/depth/geometry/diagnostic limit tests in `Tests/AssetGLTFLimitTests.cpp` and `Tests/AssetGLTFLimitTests.h`
- [X] T089 [P] Add failing normalized diagnostic, caller-output preservation, registry rollback, and skipped-optional inspection tests in `Tests/AssetGLTFDiagnosticTests.cpp` and `Tests/AssetGLTFDiagnosticTests.h`
- [X] T090 [P] Implement deterministic malformed fixture mutation tooling with unit tests in `Tests/Fixtures/StaticModel/generate_invalid_fixtures.py` and `Tests/test_generate_static_model_invalid_fixtures.py`
- [X] T091 Enforce capped parser allocations and aggregate checked counters before parser traversal in `Source/Asset/Private/FCgltfDocument.cpp` and `Source/Asset/Private/FStaticModelImport.cpp`
- [X] T092 Harden source-relative dependency canonicalization, data URI validation, recursion detection, and scope containment in `Source/Asset/Private/FGLTFDependencyResolver.cpp`
- [X] T093 Fail closed for unsupported required versions/extensions, topology, skins, morphs, compression, and semantics while allowing evidenced optional skips in `Source/Asset/Private/FGLTFPackageValidator.cpp`
- [X] T094 Normalize stage, subject, structural location, result category, and actionable reason without raw parser text or absolute paths in `Source/Asset/Private/FGLTFDiagnostics.h` and `Source/Asset/Private/FGLTFDiagnostics.cpp`
- [X] T095 Record unknown optional extensions and excluded cameras/lights/animations deterministically in `Source/Asset/Private/FStaticModelInspection.cpp`
- [X] T096 Preserve pre-existing caller outputs, apply registry mutations only after complete package validation, register `asset-gltf-hardening`, and record the passing foundation gate in `Source/Asset/Private/FGLTFStaticModelImporter.cpp`, `Source/Asset/Private/FGLTFPackageAssembler.cpp`, `Tests/Main.cpp`, `Tests/AssetTests.cpp`, `Tests/AssetTests.h`, `Tests/SConscript`, and `Validation/024/reports/import-hardening.txt`

**Checkpoint**: Bounded import rejects every implemented invalid class safely
and atomically before Renderer integration begins.

---

## Phase 7: User Story 4 - Realize Mesh Assets Through Renderer And RHI (Priority: P2)

**Goal**: Transactionally convert fully hardened and validated mesh assets into
immutable draw-ready Renderer snapshots and RHI resources.

**Independent Test**: Run `StonerTest --suite renderer-static-mesh` using mock
RHI for every stream/index width and every injected failure, then run the native
Vulkan transformed-mesh readback probe.

### Tests For User Story 4

- [X] T097 [P] [US4] Add failing packing, uint16/uint32, section offset, material, bounds, and source-manifest tests in `Tests/RendererStaticMeshTests.cpp` and `Tests/RendererStaticMeshTests.h`
- [X] T098 [P] [US4] Add failing validate/plan/allocate/upload/finalize rollback, normalized realization-diagnostic, and source-replacement tests in `Tests/RendererStaticMeshFailureTests.cpp` and `Tests/RendererStaticMeshFailureTests.h`
- [X] T099 [P] [US4] Add failing full indexed section recording and resource lifecycle tests in `Tests/RHICoreTests.cpp`

### Implementation For User Story 4

- [X] T100 [US4] Define realization profile, stages, diagnostics, sections, immutable snapshot, request, and result contracts in `Source/Renderer/Public/Renderer/FStaticMeshAssetConversion.h` and `Source/Renderer/Public/Renderer/FStaticMeshRealization.h`
- [X] T101 [US4] Build deterministic semantic-to-RHI vertex formats, interleaving, alignments, index packing, buffer sizes, and section offsets in `Source/Renderer/Private/FStaticMeshAssetConversion.cpp`
- [X] T102 [US4] Implement validate-plan-allocate-upload-finalize-publish realization with complete rollback in `Source/Renderer/Private/FStaticMeshRealization.cpp`
- [X] T103 [US4] Validate material dependencies, normalized source manifests, realization profile digests, and explicit reconversion semantics in `Source/Renderer/Private/FStaticMeshRealization.cpp`
- [X] T104 [US4] Upload packed vertex/index bytes only through `IRHIDevice::UploadBuffer` and invalidate every request-owned resource on failure in `Source/Renderer/Private/FStaticMeshRealization.cpp`
- [X] T105 [US4] Map Material v2 sampler intent to RHI samplers without mutating Asset definitions in `Source/Renderer/Private/FMaterialAssetConversion.cpp` and `Source/Renderer/Private/FStaticMeshRealization.cpp`
- [X] T106 [US4] Emit complete `FRHIIndexedDrawArguments` from realized primitive sections in `Source/Renderer/Private/FMeshDrawCommand.cpp` and `Source/Renderer/Public/Renderer/FMeshDrawCommand.h`
- [X] T107 [US4] Add native Vulkan transformed-mesh upload, clockwise culling, non-symmetric matrix, and readback coverage in `Tests/VulkanNativeIntegrationTests.cpp`
- [X] T108 [US4] Add deterministic and native realization orchestration plus timeout/report handling in `.github/scripts/run_static_model_validation.py` and `.github/scripts/test_run_static_model_validation.py`
- [X] T109 [US4] Register `renderer-static-mesh`, compile US4 sources, and expose focused CLI selection in `Tests/Main.cpp` and `Tests/SConscript`
- [ ] T110 [US4] Run mock and native US4 gates and record uploaded byte hashes, sections, cleanup, normalized realization diagnostics, and readback in `Validation/024/reports/us4-renderer-realization.txt`

**Checkpoint**: Every accepted mesh can become a self-contained draw-ready
snapshot without re-reading source files or leaking partial GPU state.

---

## Phase 8: User Story 5 - Diagnose Unsupported And Malformed Packages (Priority: P3)

**Goal**: Prove malformed import and Renderer realization failures produce
deterministic diagnostics without partial registry or GPU state.

**Independent Test**: Run `StonerTest --suite asset-gltf-malformed` over at
least 40 manifest-listed mutations on all platforms, then compare normalized
Renderer realization-failure diagnostics from `renderer-static-mesh`.

- [ ] T111 [US5] Generate and classify at least 40 malformed/unsupported fixtures with expected results and mutation provenance in `Tests/Fixtures/StaticModel/Invalid/` and `Validation/024/fixture-manifest.json`
- [ ] T112 [US5] Register `asset-gltf-malformed`, aggregate it into `asset`, and compile the completed US5 acceptance sources in `Tests/Main.cpp`, `Tests/AssetTests.cpp`, and `Tests/SConscript`
- [ ] T113 [US5] Run the complete US5 malformed and Renderer realization-diagnostic acceptance under normal and sanitizer profiles, compare normalized cross-platform expectations, and record results in `Validation/024/reports/us5-malformed-diagnostics.txt`

**Checkpoint**: All defined import and realization failure classes are
diagnosable, deterministic, and leave no partial observable state.

---

## Phase 9: Polish, Evidence, And Cross-Cutting Gates

**Purpose**: Complete measurable corpus, architecture, performance,
cross-platform, documentation, and closeout evidence.

- [ ] T114 Add fixture count, hash, provenance, license, validator, scope, expected-result, and minimum-12 SC-004 golden-primitive verification plus unit tests in `Tests/verify_static_model_fixtures.py` and `Tests/test_verify_static_model_fixtures.py`
- [ ] T115 Extend the Asset architecture verifier for cgltf/MikkTSpace privacy, public-header dependencies, Renderer filesystem/source-format exclusion, and native type leakage in `Tests/verify_asset_layer.py`
- [ ] T116 [P] Add the Apple M4 Pro macOS Release reference benchmark runner and report schema for one warm-up plus five independent 100k-vertex, 300k-index, 16-primitive, 16-dependency imports, failing if any measured run exceeds 5 seconds or tracked request-owned peak bytes exceed the active aggregate allocation limit, in `Tests/AssetStaticModelBenchmark.cpp`, `Tests/AssetStaticModelBenchmark.h`, and `Validation/024/reports/performance.json`
- [ ] T117 [P] Check in or deterministically generate the licensed representative performance fixture and manifest evidence in `Tests/Fixtures/StaticModel/Performance/` and `Validation/024/fixture-manifest.json`
- [ ] T118 Add a 20-run every-valid-fixture deterministic comparator and wire `--static-model-determinism-runs`, `--static-model-performance-runs`, `--static-model-performance-max-seconds`, and `--static-model-performance-fixture` into the `asset-static-model` suite in `Tests/AssetStaticModelDeterminism.cpp`, `Tests/AssetStaticModelDeterminism.h`, `Tests/Main.cpp`, `Tests/AssetTests.cpp`, and `Tests/AssetTests.h`
- [ ] T119 Add the eight-way concurrent import gate with serial equivalence and no shared mutable payload, and invoke it from the `asset-static-model` aggregate in `Tests/AssetStaticModelConcurrency.cpp`, `Tests/AssetStaticModelConcurrency.h`, `Tests/AssetTests.cpp`, and `Tests/AssetTests.h`
- [ ] T120 Extend the Windows/macOS/Linux Debug matrix with provenance, fixture, focused import, malformed, Renderer, coordinate, and full regression gates in `.github/workflows/ci.yml`
- [ ] T121 Extend the three-platform strict Release matrix with Feature 024 determinism and non-reference performance evidence; keep the 5-second pass/fail threshold exclusive to the documented Apple M4 Pro reference gate in `.github/workflows/ci.yml`
- [ ] T122 Extend Linux ASan/UBSan and TSan jobs with malformed import, realization rollback, and eight-way concurrency suites in `.github/workflows/ci.yml`
- [ ] T123 Upload Feature 024 deterministic, native, malformed, performance, and architecture reports with required-file behavior in `.github/workflows/ci.yml`
- [ ] T124 Run the complete local quickstart, refresh affected Feature 018/019 native evidence, and record command/result hashes in `Validation/018/`, `Validation/019/`, and `Validation/024/reports/quickstart.txt`
- [ ] T125 Generate the implementation summary according to `doc/SYSTEM_DESIGN.MD` in `doc/024-static-mesh-model.html`
- [ ] T126 Execute the Windows/macOS/Linux CI matrix and archive run URLs, toolchains, tolerated numeric deltas, and artifact hashes in `Validation/024/reports/cross-platform-ci.md`
- [ ] T127 Run `git diff --check`, all focused suites, full Debug/strict Release regressions, sanitizer gates, fixture verifiers, architecture and stale-coordinate checks, and the commands in `specs/024-static-mesh-model/quickstart.md`; require the Apple M4 Pro reference performance gate to pass, then record the final closeout in `Validation/024/README.md`
- [ ] T128 After T126 and T127 pass, update completed Feature 024 status, actual dependency evidence, and next-stage guidance in `doc/roadmap.md` and `AGENTS.md`

---

## Dependencies And Execution Order

### Phase Dependencies

- **Phase 1 - Setup**: Starts immediately. T001 and T002 may run in parallel;
  T003 depends on both, T005 depends on the vendored and fixture manifests, and
  the phase completes only after T001-T006.
- **Phase 2 - Foundations**: Depends on all of T001-T006. Coordinate work
  T007-T021 runs first and blocks every later track. After T021, Material v2
  T022-T029 and RHI T030-T036 may proceed as two ownership-separated tracks;
  T037 joins them and blocks every story.
- **Phase 3 - US1**: Depends on T037 and produces canonical mesh payloads.
- **Phase 4 - US2**: Depends on US1 because model nodes reference canonical
  mesh assets and the package assembler reuses the importer.
- **Phase 5 - US3**: Depends on completed US2 package identity, assembler,
  importer, and validator integration. Its failing tests and leaf material/image
  helpers may be prepared earlier, but T080 and T082-T085 MUST wait for T071.
- **Phase 6 - Bounded Import Hardening**: Depends on the completed US1-US3
  importer and closes the Plan M4 limits, resolver, diagnostic, and
  atomic-publication envelope before Renderer may consume imported payloads.
- **Phase 7 - US4**: Depends on T096, foundational RHI contracts, and US3
  material dependency semantics. Native proof also depends on coordinate M0.
- **Phase 8 - US5**: Depends on completed importer hardening and US4 realization
  diagnostics; it performs the full malformed corpus and cross-platform
  acceptance required by Plan M6.
- **Phase 9 - Polish**: Depends on all selected user stories.

### User Story Dependency Graph

```text
Setup
  -> Foundations
      -> US1 Canonical Geometry
          -> US2 Hierarchy & Identity
              -> US3 Material & Texture
                  -> Bounded Import Hardening
                      -> US4 Renderer Realization
                          -> US5 Malformed Diagnostics
      -> Polish / Cross-Platform Closeout
```

### Requirement Ownership

| Story/Foundation | Primary Requirements |
|---|---|
| Foundations | FR-027, FR-041, FR-044/045/046/047 contract prerequisites, FR-058 |
| US1 | FR-001 mesh payload, FR-005/006, FR-009/010, FR-018 to FR-030, FR-052 mesh inspection, FR-059 |
| US2 | FR-001 model payload, FR-002 to FR-008, FR-031 to FR-035, FR-052 model inspection |
| US3 | FR-003, FR-007, FR-035 to FR-043, FR-052 material/dependency inspection |
| Bounded Import Hardening | FR-008, FR-010 to FR-017, FR-019 to FR-026, FR-032, FR-043, FR-051, FR-053 |
| US4 | FR-044 to FR-052 |
| US5 | FR-008, FR-015 to FR-017, FR-032, FR-043, FR-051 to FR-055 |
| Shared cross-cutting | FR-005 to FR-009, FR-050, FR-053 to FR-058, SC-001 to SC-014 |

### Within Each User Story

1. Write the listed tests and confirm they fail for the intended missing
   behavior.
2. Implement public/value contracts before services and private orchestration.
3. Implement leaf validators/converters before package integration.
4. Register the focused suite only after all referenced test functions compile.
5. Run the independent gate and record evidence before starting a dependent
   story.

---

## Parallel Execution Examples

### Foundation Tracks

```text
Blocking track: T007-T021 coordinate migration
Then in parallel:
  Track B: T022-T029 Material schema v2
  Track C: T030-T036 RHI upload/indexed draw
Join: T037
```

### User Story 1

```text
Parallel failing-test wave: T038, T039, T040, T041
Then serial implementation: T042 -> T043 -> T044 -> T045 -> T046 -> T047
                            -> T048 -> T049 -> T050 -> T051 -> T052 -> T053
                            -> T054 -> T055 -> T056 -> T057
```

### User Story 2

```text
Parallel failing-test/fixture wave: T058, T059, T060
Then: T061 -> T062 -> T063 -> T064 -> T065 -> T066 -> T067
      -> T068 -> T069 -> T070 -> T071
```

### User Story 3

```text
Parallel failing-test/fixture wave: T072, T073, T074
Then: T075 -> T076 -> T077 -> T078 -> T079 -> T080 -> T081
      -> T082 -> T083 -> T084 -> T085
```

### Bounded Import Hardening

```text
Parallel failing-test/tool wave: T086, T087, T088, T089, T090
Then: T091 -> T092 -> T093 -> T094 -> T095 -> T096
```

### User Story 4

```text
Parallel failing-test wave: T097, T098, T099
Then: T100 -> T101 -> T102 -> T103 -> T104 -> T105 -> T106
      -> T107 -> T108 -> T109 -> T110
```

### User Story 5

```text
T111 fixture corpus -> T112 suite registration -> T113 complete acceptance
```

---

## Implementation Strategy

### Technical MVP

1. Complete T001-T006 for reproducible inputs.
2. Complete T007-T037; do not permit mixed coordinate conventions.
3. Complete T038-T057 for US1 canonical geometry.
4. Stop and validate `asset-static-mesh` over 20 repeated imports.

This MVP is useful as a canonical geometry library, even before model assembly,
materials, and GPU realization are added.

### Incremental Delivery

1. **Foundation ready**: coordinate, Material v2, and RHI contracts pass.
2. **US1**: canonical static mesh import passes independently.
3. **US2**: hierarchy, identity, and package integration become authoritative.
4. **US3**: material/texture integration builds on the completed package
   assembler; only its tests and leaf helpers may overlap late US2 work.
5. **Bounded Import Hardening**: limits, resolver safety, diagnostics, and
   atomic publication pass before Renderer integration.
6. **US4**: Renderer proves fully validated canonical assets are draw-ready.
7. **US5**: malformed import and realization diagnostics pass as one complete
   cross-platform acceptance story.
8. **Closeout**: performance, sanitizers, CI, evidence, docs, and roadmap.

### Git Strategy

- Commit each foundation track separately with `refactor`, `feat`, or `test`
  prefixes before the T037 join.
- Commit each completed user-story checkpoint as one or more cohesive
  conventional commits; do not mix fixture imports with unrelated engine
  refactors.
- Push and run CI after T037, T057, T071/T085, T096, T110, T113, and final
  closeout.
- Preserve third-party provenance and generated fixture evidence in the same
  commit as the dependency or corpus addition.

## Notes

- `[P]` never permits concurrent edits to the same file.
- Asset public headers remain Core-only even when a task also changes Renderer
  or RHI in a separate owning file.
- Khronos Validator is an offline conformance oracle, never the importer safety
  gate.
- No task may satisfy native realization through a semantic oracle or by
  re-reading source data after the canonical Asset payload is built.
- Feature 024 excludes ECS creation, skeletal assets, animation, morph targets,
  meshlets, cooking, manifests, DDC, runtime loading, and streaming.
