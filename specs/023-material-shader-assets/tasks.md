# Tasks: Material & Shader Assets

**Input**: Design documents from `/specs/023-material-shader-assets/`
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`,
`contracts/source-definition-schema.md`,
`contracts/material-shader-asset-api.md`,
`contracts/renderer-shader-integration.md`, and `quickstart.md`

**Tests**: Required. The specification requires canonical round trips,
malformed/boundary coverage, deterministic and concurrent execution,
Feature 014 compatibility, repository shader migration, native regression,
strict cross-platform builds, and architecture verification.

**Organization**: Tasks are grouped by user story. Setup and Foundation create
the private parser, immutable shared vocabulary, diagnostics, test harness, and
architecture gates used by every story. Every story ends at an independently
runnable checkpoint.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Safe to execute in parallel because the task edits different files
  and has no dependency on another incomplete task.
- **[Story]**: Maps the task to the corresponding user story in `spec.md`.
- Every task includes an exact target path.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish reproducible parser provenance, fixture ownership, and
build/test wiring without exposing third-party JSON types through public
headers.

- [X] T001 [P] Vendor private yyjson 0.12.0 source plus exact release, upstream commit, MIT license, version, and SHA-256 evidence in `ThirdParty/yyjson/yyjson.c`, `ThirdParty/yyjson/yyjson.h`, `ThirdParty/yyjson/LICENSE`, `ThirdParty/yyjson/UPSTREAM.md`, and `ThirdParty/yyjson/VERSION`
- [X] T002 [P] Define the valid corpus matrix for at least 12 shader programs, 12 materials, 16 instances, every supported enum/value category, ten equivalent insertion orders, and expected canonical evidence in `Tests/Fixtures/MaterialShader/README.md`; before implementation also freeze the current commit, normalized triangle/material/forward/deferred summaries, deterministic/native failure outcomes, and all 22 shader digests in `Validation/023/Baseline/README.md` and `Validation/023/Baseline/`
- [X] T003 Add representative valid source definitions and exact dependency facts from T002 under `Tests/Fixtures/MaterialShader/Valid/` and canonical byte/digest expectations under `Tests/Fixtures/MaterialShader/Golden/`
- [X] T004 Add at least 40 bounded malformed and boundary source definitions covering every schema section, duplicate decoded keys, unsupported versions/extensions, numeric/text/count limits, dependency mismatch, and truncation under `Tests/Fixtures/MaterialShader/Invalid/`
- [X] T005 Configure the private yyjson C translation unit with scoped third-party warnings and no public include leakage in `Source/Asset/SConscript` and `site_scons/LayerBuilder.py`
- [X] T006 [P] Create Feature 023 Asset and Renderer test entry-point headers and empty runners in `Tests/AssetMaterialShaderTests.h`, `Tests/AssetMaterialShaderTests.cpp`, `Tests/RendererMaterialShaderAssetTests.h`, and `Tests/RendererMaterialShaderAssetTests.cpp`
- [X] T007 Register `asset-material-shader` and `renderer-material-asset` focused suites while preserving existing `asset` and `renderer-material` aggregate behavior in `Tests/Main.cpp`, `Tests/AssetTests.h`, and `Tests/AssetTests.cpp`
- [X] T008 Add Feature 023 private test include paths and translation-unit wiring in `Tests/SConscript`
- [X] T009 Add pinned yyjson version/checksum/license verification and self-tests for missing, altered, malformed, and valid provenance in `Tests/verify_material_shader_provenance.py` and `Tests/test_verify_material_shader_provenance.py`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Establish stable result/diagnostic vocabulary, finite limits,
immutable shared models, strict JSON infrastructure, importer boundaries, and
architecture enforcement before story-specific behavior.

**Critical**: No user-story implementation begins until strict duplicate-key
rejection, finite parser allocation, immutable payload ownership, and the
Asset-to-Core boundary are proven.

- [X] T010 Extend stable Asset result and Parse/Normalize/Dependency/Select/Resolve/Registry stages without changing existing enum meanings or adding Renderer conversion/native-validation vocabulary in `Source/Asset/Public/Asset/EAssetResult.h`
- [X] T011 Implement normalized material/shader diagnostic formatting and redact absolute paths, addresses, payload text, timings, and raw yyjson errors in `Source/Asset/Public/Asset/FAssetDiagnostics.h` and `Source/Asset/Private/FAssetDiagnostics.cpp`
- [X] T012 [P] Define validated finite defaults, checked aggregate arithmetic, and no-unbounded-sentinel behavior in `Source/Asset/Public/Asset/FMaterialShaderAssetLimits.h` and `Source/Asset/Private/FMaterialShaderAssetLimits.cpp`
- [X] T013 [P] Define fixed material/shader backend, payload, stage, language, program-kind, parameter, interface, and inspection vocabulary in `Source/Asset/Public/Asset/FMaterialShaderTypes.h` and `Source/Asset/Public/Asset/FMaterialShaderInspection.h`
- [X] T014 Define immutable source-version records, source manifests, stable roles, duplicate-version rejection, and deterministic ordering in `Source/Asset/Public/Asset/FAssetSourceVersionRecord.h` and `Source/Asset/Private/FAssetSourceVersionRecord.cpp`
- [X] T015 Define immutable `FShaderSourceAsset`, `FShaderPayloadAsset`, and their fixed Feature 020 type traits in `Source/Asset/Public/Asset/FShaderSourceAsset.h`, `Source/Asset/Public/Asset/FShaderPayloadAsset.h`, and `Source/Asset/Private/FShaderDependencyAssets.cpp`
- [X] T016 Define immutable `FShaderAsset` program, stage, source, payload, permutation, required-parameter, interface, and target-request records in `Source/Asset/Public/Asset/FShaderAsset.h`
- [X] T017 Define immutable `FMaterialAsset`, `FMaterialInstanceAsset`, typed parent/parameter values, and resolved-material output in `Source/Asset/Public/Asset/FMaterialAsset.h` and `Source/Asset/Public/Asset/FMaterialInstanceAsset.h`
- [X] T018 Define synchronous load/import requests, transactional outputs, parse-only mode, and immutable lookup contracts in `Source/Asset/Public/Asset/FMaterialShaderSourceLoader.h`
- [X] T019 Export only Core-dependent Feature 023 public vocabulary and verify no yyjson, RHI, Renderer, Application, Backend, Tools, or graphics API headers leak through `Source/Asset/Public/Asset/AssetMinimal.h`
- [X] T020 Add failing foundation tests for parser allocation bounds, escaped-equivalent duplicate keys, canonical number/text behavior, extension rules, diagnostics redaction, immutable ownership, import-output rollback, and unchanged Registry revision on load or batch-apply failure in `Tests/AssetMaterialShaderTests.cpp`
- [X] T021 Implement bounded yyjson parsing with no permissive flags, raw-number tokens, preflighted fixed allocator, non-recursive depth/value/member/array traversal, decoded duplicate-key rejection, and project-owned model copies in `Source/Asset/Private/FMaterialShaderJsonCodec.h` and `Source/Asset/Private/FMaterialShaderJsonCodec.cpp`
- [X] T022 Implement closed-field schema dispatch, version checks, namespaced optional/required extensions, UTF-8/NFC/token rules, and stable JSON-pointer diagnostics in `Source/Asset/Private/FMaterialShaderSchemaValidator.h` and `Source/Asset/Private/FMaterialShaderSchemaValidator.cpp`
- [X] T023 Implement canonical model writing with fixed schema order, sorted unordered collections, two-space UTF-8/LF output, one final newline, finite binary32 shortest-round-trip formatting, negative-zero normalization, and top-level `FAssetVersion` derivation from canonical bytes in `Source/Asset/Private/FMaterialShaderJsonCodec.cpp`
- [X] T024 Implement the `IAssetImporter` adapter, schema-based probing, recognized-malformed fail-closed dispatch, resolver execution leases, all-or-nothing output emission, and explicit `FMaterialShaderImportService::ImportAndRegister` using one `FAssetMutationBatch` plus one Registry `Apply` in `Source/Asset/Public/Asset/FMaterialShaderSourceLoader.h`, `Source/Asset/Private/FMaterialShaderDefinitionImporter.cpp`, `Source/Asset/Private/FMaterialShaderSourceLoader.cpp`, and `Source/Asset/Private/AssetModule.cpp`
- [X] T025 Extend Asset architecture verification for private yyjson-only access, Asset-to-Core production dependencies, Backend-to-no-Asset dependencies, forbidden runtime compiler/Tools includes, and absence of Renderer conversion/native-validation result vocabulary in `Tests/verify_asset_layer.py`
- [X] T026 Complete and run the foundation tests, yyjson provenance unit tests, strict Asset build, and architecture verifier before starting US1 in `Tests/AssetMaterialShaderTests.cpp`, `Tests/test_verify_material_shader_provenance.py`, and `Tests/verify_asset_layer.py`

**Checkpoint**: Strict JSON can be parsed and rewritten into immutable
project-owned values with bounded work, deterministic diagnostics, atomic
failure, and no Asset dependency beyond Core.

---

## Phase 3: User Story 1 - Preserve Shaders as Stable Assets (Priority: P1) MVP

**Goal**: Represent one complete logical shader program under a stable typed
identity, inspect its sources/payloads, and deterministically select one
runtime-ready backend/profile payload set without filesystem identity or hidden
fallback.

**Independent Test**: Run `StonerTest --suite asset-material-shader` over
representative graphics, compute, source-only, permutation, interface, target,
ambiguity, and dependency-integrity cases; repeated selection and normalized
inspection must be byte-identical.

### Tests for User Story 1

- [X] T027 [US1] Add failing shader identity, graphics/compute stage-set, duplicate stage/entry, unsupported future stage, source-only, and fixed type/subresource cases in `Tests/AssetMaterialShaderTests.cpp`
- [X] T028 [US1] Add failing permutation canonicalization, undeclared/duplicate flag, duplicate variant, required-parameter, binding, constant-range, and cross-stage interface cases in `Tests/AssetMaterialShaderTests.cpp`
- [X] T029 [US1] Add failing SPIR-V size/alignment/header/version/entry-point/stage, source UTF-8, exact digest, producer, locator, and aggregate dependency-limit cases in `Tests/AssetMaterialShaderTests.cpp`
- [X] T030 [US1] Add failing ordered-profile selection cases for first unique match, incomplete profile, ambiguity-at-first-profile, lower unique profile after ambiguity, duplicate request profiles, cross-profile mixing, cross-backend fallback, and registration-order independence in `Tests/AssetMaterialShaderTests.cpp`
- [X] T031 [US1] Add shader definition canonical round-trip, source-only inspection, runtime-ready inspection, stable diagnostic, and repeated immutable-reader cases in `Tests/AssetMaterialShaderTests.cpp`

### Implementation for User Story 1

- [X] T032 [US1] Implement immutable shader source/payload construction, exact byte ownership, fixed identities, versions, and bounded inspection summaries in `Source/Asset/Private/FShaderDependencyAssets.cpp`
- [X] T033 [US1] Implement shader program kind, supported stage-set, stage/entry uniqueness, source-reference, future-stage rejection, and source-only validation in `Source/Asset/Private/FShaderProgramValidator.h` and `Source/Asset/Private/FShaderProgramValidator.cpp`
- [X] T034 [US1] Implement canonical permutation domains/keys, variant uniqueness, required-parameter validation, and declaration-order-independent ordering in `Source/Asset/Private/FShaderProgramValidator.cpp`
- [X] T035 [US1] Implement backend-neutral interface binding, visibility, constant-range, overlap, required-stage, and payload/interface agreement validation in `Source/Asset/Private/FShaderProgramValidator.cpp`
- [X] T036 [US1] Implement bounded source/payload resolution, request-local deduplication, exact SHA-256 checking, typed identity/locator agreement, UTF-8 source checks, and aggregate byte accounting in `Source/Asset/Private/FShaderDependencyLoader.h` and `Source/Asset/Private/FShaderDependencyLoader.cpp`
- [X] T037 [US1] Implement SPIR-V word/header/instruction traversal and exact `OpEntryPoint` execution-model/stage/name validation without graphics API or SPIRV-Tools dependencies in `Source/Asset/Private/FShaderPayloadValidation.h` and `Source/Asset/Private/FShaderPayloadValidation.cpp`
- [X] T038 [US1] Implement caller-ordered one-backend profile selection, exact permutation/stage matching, first-profile ambiguity failure, no mixing/fallback, and complete source manifests in `Source/Asset/Private/FShaderPayloadSelector.cpp`
- [X] T039 [US1] Implement deterministic shader, source, payload, stage, variant, target, interface, dependency, selection, and diagnostic inspection in `Source/Asset/Private/FMaterialShaderInspection.cpp`
- [X] T040 [US1] Decode and encode schema-v1 `.shader.json` definitions with closed fields, typed dependency references, canonical collection ordering, and source-only support in `Source/Asset/Private/FMaterialShaderSchemaValidator.cpp` and `Source/Asset/Private/FMaterialShaderJsonCodec.cpp`
- [X] T041 [US1] Implement shader load phases through parse, normalize, semantic validation, optional dependency loading, runtime-ready validation, canonical output, and transactional import outputs in `Source/Asset/Private/FMaterialShaderSourceLoader.cpp`
- [X] T042 [US1] Register ShaderProgram, ShaderSource, and ShaderPayload type behavior through existing scoped importer/loader participants without adding a process-wide payload cache in `Source/Asset/Private/AssetModule.cpp`
- [X] T043 [US1] Complete all shader validation, selection, dependency, inspection, rollback, round-trip, ordering, source-only, and concurrent-reader tests in `Tests/AssetMaterialShaderTests.cpp`

**Checkpoint**: US1 provides stable complete-program Shader Assets, exact typed
source/payload dependencies, deterministic explicit target selection, and
source-only authoring without runtime compilation.

---

## Phase 4: User Story 2 - Preserve Materials and Instances as Assets (Priority: P1)

**Goal**: Preserve Feature 014 material behavior and typed shader/texture/parent
relationships as immutable Asset records, then resolve instance chains to one
flattened value without process-local parent pointers.

**Independent Test**: Run `StonerTest --suite asset-material-shader` over every
material domain, blend, render state, parameter type, dependency role, parent
category, nearest-override case, unresolved dependency, cycle, and depth
boundary; compare effective values with equivalent Feature 014 semantics.

### Tests for User Story 2

- [X] T044 [US2] Add failing material domain/blend/render-state, duplicate parameter, non-finite scalar/vector/color, invalid typed texture/shader identity, permutation, and required-parameter cases in `Tests/AssetMaterialShaderTests.cpp`
- [X] T045 [US2] Add failing parent tagged-union, missing/type-mismatched parent, cycle, exact-depth/first-over-depth, unknown/type-changing override, and nearest-override cases in `Tests/AssetMaterialShaderTests.cpp`
- [X] T046 [US2] Add metadata extraction cases for shader, texture, parent Material, parent MaterialInstance, stable role/strength/order, unresolved soft dependency retention, and render-ready rejection in `Tests/AssetMaterialShaderTests.cpp`
- [X] T047 [US2] Add material/instance schema-v1 canonical round trips, ten insertion orders, inspection summaries, registry rollback, source-version manifest, and eight-reader resolution cases in `Tests/AssetMaterialShaderTests.cpp`

### Implementation for User Story 2

- [X] T048 [US2] Implement Asset-owned material domain/blend/render-state and scalar/vector/color/Texture parameter validation mirroring Feature 014 without including Renderer in `Source/Asset/Private/FMaterialAssetValidator.h` and `Source/Asset/Private/FMaterialAssetValidator.cpp`
- [X] T049 [US2] Implement stable Material dependencies for ShaderProgram and Texture identities, unresolved metadata state, permutation request validation, and runtime-ready dependency checks in `Source/Asset/Private/FMaterialDependencyExtractor.h` and `Source/Asset/Private/FMaterialDependencyExtractor.cpp`
- [X] T050 [US2] Implement MaterialInstance one-parent tagged references, sorted unique typed overrides, and definition-level validation in `Source/Asset/Private/FMaterialAssetValidator.cpp`
- [X] T051 [US2] Implement immutable parent lookup, pre-visit cycle detection, configurable default-64 depth, root discovery, root-to-leaf nearest overrides, and flattened `FResolvedMaterialAsset` output in `Source/Asset/Private/FMaterialInstanceResolver.h` and `Source/Asset/Private/FMaterialInstanceResolver.cpp`
- [X] T052 [US2] Include every Material/MaterialInstance chain version plus shader/texture dependency version in one deduplicated deterministic source manifest in `Source/Asset/Private/FMaterialInstanceResolver.cpp`
- [X] T053 [US2] Decode and encode schema-v1 `.material.json` definitions with closed fields, typed ShaderProgram/Texture references, canonical parameters, and Feature 014 compatibility rules in `Source/Asset/Private/FMaterialShaderSchemaValidator.cpp` and `Source/Asset/Private/FMaterialShaderJsonCodec.cpp`
- [X] T054 [US2] Decode and encode schema-v1 `.material-instance.json` definitions with exact parent union, typed overrides, canonical ordering, and deferred parent-chain validation in `Source/Asset/Private/FMaterialShaderSchemaValidator.cpp` and `Source/Asset/Private/FMaterialShaderJsonCodec.cpp`
- [X] T055 [US2] Extend transactional source loading and all-or-nothing import-output emission for Material and MaterialInstance definitions without mutating the Registry in `Source/Asset/Private/FMaterialShaderSourceLoader.cpp` and `Source/Asset/Private/FMaterialShaderDefinitionImporter.cpp`
- [X] T056 [US2] Implement deterministic material, instance, parent-chain, effective-parameter, dependency, unresolved, and validation inspection summaries in `Source/Asset/Private/FMaterialShaderInspection.cpp`
- [X] T057 [US2] Register Material and MaterialInstance type behavior through existing scoped participants while preserving execution leases and atomic registry batches in `Source/Asset/Private/AssetModule.cpp`
- [X] T058 [US2] Add compile-time and runtime proof that Feature 024 clients can express material and texture references using only `FAssetId`, metadata dependencies, and typed soft references in `Tests/AssetMaterialShaderTests.cpp`
- [X] T059 [US2] Complete all material/instance validation, inheritance, dependency, unresolved, inspection, rollback, round-trip, ordering, and concurrent-reader tests in `Tests/AssetMaterialShaderTests.cpp`

**Checkpoint**: US2 provides persistent base/instance Material Assets, typed
dependency metadata, bounded inheritance, and one flattened immutable effective
material value without Renderer or GPU ownership.

---

## Phase 5: User Story 3 - Load Versioned Source Definitions Safely (Priority: P2)

**Goal**: Harden all three authoring schemas so canonical source, version
evidence, limits, extensions, failures, and concurrent requests are
cross-platform deterministic and safe before publication.

**Independent Test**: Run the complete valid/golden/malformed corpus for 20
parse-validate-normalize repetitions, ten insertion orders, every exact limit
and first value above, and eight concurrent readers; every failure must be
atomic and identify a stable stage/reason.

### Tests for User Story 3

- [X] T060 [US3] Add exact-limit and first-value-above cases for definition/source/payload/aggregate bytes, relative locator bytes, JSON depth/values/members/array elements, text/token/number length, extensions, stages, flags, sources, variants, payloads, parameters, interfaces, dependencies, and instance depth in `Tests/AssetMaterialShaderTests.cpp`
- [X] T061 [US3] Add malformed JSON cases for BOM, NUL, invalid UTF-8, lone surrogate, comments, trailing comma/data, decoded duplicate keys at every object level, invalid numbers, overflow, underflow-to-zero, and root non-object in `Tests/AssetMaterialShaderTests.cpp`
- [X] T062 [US3] Add schema-evolution cases for unknown ordinary fields, known/unknown optional extensions, missing/unknown required extensions, duplicate extension names, reordered unique extension-name inputs that canonicalize identically, wrong extension body type, and canonical omission of unknown optional bodies in `Tests/AssetMaterialShaderTests.cpp`
- [X] T063 [US3] Add 20-run byte-identity, ten-order equivalence, parse-canonical-parse idempotence, dependency/version stability, normalized diagnostic stability, and eight-concurrent-reader stress cases over the full fixture corpus in `Tests/AssetMaterialShaderTests.cpp`
- [X] T064 [US3] Add failure-after-each-phase cases proving parser/model/dependency/import temporary data release and zero partial Registry, Asset payload, Renderer output, or retained execution lease in `Tests/AssetMaterialShaderTests.cpp`

### Implementation for User Story 3

- [X] T065 [US3] Enforce every `FMaterialShaderAssetLimits` field before allocation/traversal and use checked addition/multiplication for aggregate parser, dependency, permutation, interface, and import-output work in `Source/Asset/Private/FMaterialShaderJsonCodec.cpp`, `Source/Asset/Private/FMaterialShaderSchemaValidator.cpp`, and `Source/Asset/Private/FMaterialShaderSourceLoader.cpp`
- [X] T066 [US3] Harden decoded-key uniqueness, raw number conversion to checked integer/finite binary32, overflow/underflow rejection, negative-zero normalization, and host-locale-independent canonical writing in `Source/Asset/Private/FMaterialShaderJsonCodec.cpp`
- [X] T067 [US3] Harden required/optional extension validation, extension strategy dispatch, unknown optional subtree dropping, unknown required fail-closed behavior, and canonical extension ordering in `Source/Asset/Private/FMaterialShaderSchemaValidator.cpp`
- [X] T068 [US3] Make parse, normalize, validate, dependency, import-output emission, batch construction, and single Registry `Apply` explicit transactions with unchanged outputs/revision and released request-owned data on every failure in `Source/Asset/Private/FMaterialShaderSourceLoader.cpp` and `Source/Asset/Private/FMaterialShaderDefinitionImporter.cpp`
- [X] T069 [US3] Add test-binary options for `--material-shader-determinism-runs` and `--material-shader-report` while preserving existing KTX2 `--report` ownership and behavior in `Tests/Main.cpp`, `Tests/AssetMaterialShaderTests.h`, and `Tests/AssetMaterialShaderTests.cpp`
- [X] T070 [US3] Emit a normalized report containing corpus counts, repetitions, canonical digests, diagnostics, concurrency outcome, non-gating elapsed telemetry, host CPU, OS, compiler, and build configuration while excluding absolute paths and timestamps in `Tests/AssetMaterialShaderTests.cpp`
- [X] T071 [US3] Reconcile the final valid/golden/invalid corpus inventory, expected first failure, canonical bytes, and limit provenance in `Tests/Fixtures/MaterialShader/README.md`
- [X] T072 [US3] Complete all malformed, boundary, extension, canonicalization, transaction, determinism, report, and concurrency tests in `Tests/AssetMaterialShaderTests.cpp`

**Checkpoint**: US3 supplies bounded, evolvable, canonical authoring schemas
whose results and failures remain identical across repetition, insertion order,
threads, and supported hosts.

---

## Phase 6: User Story 4 - Preserve Renderer Behavior Through Adapters (Priority: P2)

**Goal**: Convert validated Shader and flattened Material Assets into immutable
Feature 014/RHI snapshots with complete source versions, transactional library
registration, and no Asset pointer or live update subscription.

**Independent Test**: Run `StonerTest --suite renderer-material-asset` over
equivalent legacy and Asset-backed valid/invalid records and compare validation,
variant, effective parameters, requirements, diagnostics, and frame-plan input;
inject failure at every conversion stage and verify unchanged destination state.

### Tests for User Story 4

- [X] T073 [US4] Add failing shader conversion cases for incomplete stages, invalid interface, payload mismatch, duplicate/conflicting library key, mid-batch failure, and complete source-manifest disagreement in `Tests/RendererMaterialShaderAssetTests.cpp`
- [X] T074 [US4] Add failing material conversion cases for unresolved shader/texture, invalid flattened values, required-parameter mismatch, invalid domain/blend, resource requirement failure, and borrowed-pointer lifetime probes in `Tests/RendererMaterialShaderAssetTests.cpp`
- [X] T075 [US4] Add legacy-versus-Asset compatibility cases for every domain, blend mode, render state, parameter category, permutation, instance override, selected variant, resource requirement, normalized diagnostic, forward plan, and deferred plan input in `Tests/RendererMaterialShaderAssetTests.cpp`
- [X] T076 [US4] Add snapshot replacement tests proving old version usability, explicit reconversion for new versions, deep byte/string/value ownership, no Registry callback, and eight concurrent readers in `Tests/RendererMaterialShaderAssetTests.cpp`

### Implementation for User Story 4

- [X] T077 [US4] Define immutable shader conversion requests/results, selected target evidence, complete source manifests, Feature 014 records, and owned RHI module descriptions in `Source/Renderer/Public/Renderer/FShaderAssetConversion.h`
- [X] T078 [US4] Implement all-or-nothing shader conversion, Asset-to-Renderer stage/interface/permutation mapping, deep SPIR-V word ownership, and stable diagnostics in `Source/Renderer/Private/FShaderAssetConversion.cpp`
- [X] T079 [US4] Add `EMaterialResult RegisterShaderRecords(std::span<const FShaderRecord>, FMaterialDiagnosticLog*)` with prevalidated transactional batch registration while retaining the existing one-record API as a batch delegate in `Source/Renderer/Public/Renderer/FShaderLibrary.h` and `Source/Renderer/Private/FShaderLibrary.cpp`
- [X] T080 [US4] Define immutable material conversion requests/results, flattened Feature 014 `FMaterial`, resource requirements, and complete source manifest in `Source/Renderer/Public/Renderer/FMaterialAssetConversion.h`
- [X] T081 [US4] Implement resolved-Asset-to-Feature-014 enum, state, permutation, parameter, texture reference, and diagnostic mapping without constructing `FMaterialInstance` or retaining Asset pointers in `Source/Renderer/Private/FMaterialAssetConversion.cpp`
- [X] T082 [US4] Merge material/shader/parent/texture manifests with canonical identity order and reject one identity carrying conflicting versions in `Source/Renderer/Private/FMaterialAssetConversion.cpp`
- [X] T083 [US4] Export additive conversion/snapshot APIs without removing or altering Feature 014 entry points in `Source/Renderer/Public/Renderer/RendererMinimal.h`
- [X] T084 [US4] Compare converted snapshots with legacy `FMaterial`, `FShaderLibrary`, `FMaterialResourceRequirement`, forward frame preparation, and deferred frame planning in `Tests/RendererMaterialShaderAssetTests.cpp`
- [X] T085 [US4] Complete conversion rollback, equivalence, source-version, explicit-reconversion, ownership, diagnostics, planning, and concurrent-reader tests in `Tests/RendererMaterialShaderAssetTests.cpp`

**Checkpoint**: US4 preserves Feature 014 behavior through immutable,
self-contained Asset conversion snapshots and transactional shader-library
updates while keeping Asset independent of Renderer/RHI.

---

## Phase 7: User Story 5 - Migrate Repository-Owned Shaders (Priority: P3)

**Goal**: Move all triangle/deferred shader ownership to six stable program
Assets, preserve all 22 source/payload bytes and digests, and pass bytecode
snapshots to native helpers without Backend path or Asset coupling.

**Independent Test**: Run repository inventory verification, load all six
programs and 22 typed dependencies, compare every exact digest/byte, then run
triangle, renderer material/forward/deferred, deterministic, failure-injection,
and available native suites with no direct shader path below composition.

### Tests for User Story 5

- [X] T086 [US5] Add failing repository verifier unit cases for missing/extra program, missing/stale/mismatched file, wrong typed identity/stage/entry/profile/permutation/digest, duplicate destination, unowned dependency, and direct Backend path read in `Tests/test_verify_repository_shader_assets.py`
- [X] T087 [US5] Add Asset integration cases loading all six repository programs, shared Fullscreen dependencies, distinct Point/Spot identities despite equal digest, all 11 GLSL sources, all 11 SPIR-V payloads, and exact source manifests in `Tests/AssetMaterialShaderTests.cpp`
- [X] T088 [US5] Add byte-input triangle native tests for valid shaders, swapped/missing/corrupt stage, partial module creation failure, cleanup, deterministic output, and visible preparation in `Tests/VulkanNativeIntegrationTests.cpp` and `Tests/TriangleDemoIntegrationTests.cpp`
- [X] T089 [US5] Add byte-input deferred native tests for the complete shader set, missing/mismatched stage, partial module creation rollback, failure injection, attachment readback, and deterministic comparison in `Tests/DeferredNativeIntegrationTests.cpp`
- [X] T090 [US5] Add migrated composition compatibility cases for material/shader, forward, deferred, Demo, deterministic output, native failures, and all 22 shader digests against the frozen `Validation/023/Baseline/` evidence from T002 in `Tests/RendererMaterialShaderAssetTests.cpp` and `Tests/TriangleDemoIntegrationTests.cpp`

### Implementation for User Story 5

- [X] T091 [US5] Move the triangle GLSL/SPIR-V files byte-for-byte into `Content/Shaders/Triangle/` and add `Content/Shaders/Triangle/Triangle.shader.json` with stable source/payload subresources and exact digests
- [X] T092 [US5] Move the deferred GLSL/SPIR-V files byte-for-byte into `Content/Shaders/Deferred/` and add `Content/Shaders/Deferred/Surface.shader.json`, `Content/Shaders/Deferred/Composition.shader.json`, `Content/Shaders/Deferred/DirectionalLight.shader.json`, `Content/Shaders/Deferred/PointLight.shader.json`, and `Content/Shaders/Deferred/SpotLight.shader.json`
- [X] T093 [US5] Add representative base and instance definitions consuming the migrated programs and Feature 021 texture identities under `Content/Materials/`
- [X] T094 [US5] Implement a no-shell repository inventory/digest/identity/stage/entry/profile/dependency/direct-path verifier with normalized output in `Tests/verify_repository_shader_assets.py`
- [X] T095 [US5] Implement one reusable explicit-content staging helper that preserves repository-relative layout, rejects missing/duplicate destinations, and performs no compile/cook/manifest work in `site_scons/ContentStaging.py`
- [X] T096 [US5] Replace Renderer and Demo shader copy/compile lists with the shared Content staging helper and declare all source definitions, GLSL, and SPIR-V inputs in `Source/Renderer/SConscript` and `Demo/StonerDemo/SConscript`
- [X] T097 [US5] Add Asset to the Demo runtime's legal composition dependencies and load six program definitions through an Application/Demo-owned resolver mount while retaining `--shader-dir` only as a compatibility mount root in `Demo/StonerDemo/SConscript`, `Demo/StonerDemo/Private/FDemoConfiguration.h`, and `Demo/StonerDemo/Private/FDemoConfiguration.cpp`
- [X] T098 [US5] Replace Demo path concatenation and local SPIR-V reads with Asset loading, explicit Vulkan profile selection, Renderer snapshot conversion, and owned RHI module descriptions in `Demo/StonerDemo/Private/FStonerDemoApplication.h` and `Demo/StonerDemo/Private/FStonerDemoApplication.cpp`
- [X] T099 [US5] Change triangle offscreen and visible native APIs to accept vertex/fragment `FRHIShaderModuleDesc` values and remove path-input overloads in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanNativeContext.h` and `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp`
- [X] T100 [US5] Define a Backend-local `FVulkanDeferredShaderSet` containing only RHI/Core module descriptions and pass it into deferred native execution in `Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.h` and `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp`
- [X] T101 [US5] Remove shader file acquisition from `FVulkanNativeOffscreenSession::Execute` and isolate shader-module creation plus unified rollback into a focused private transaction in `Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp`
- [X] T102 [US5] Preserve command, attachment, pipeline, submission, readback, failure-injection, and teardown behavior while adapting deferred execution to the immutable shader set in `Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp`
- [X] T103 [US5] Migrate native and Demo tests from physical shader directories to Asset-backed/RHI-description inputs while retaining resolver-mount validation at composition boundaries in `Tests/VulkanNativeIntegrationTests.cpp`, `Tests/DeferredNativeIntegrationTests.cpp`, and `Tests/TriangleDemoIntegrationTests.cpp`
- [X] T104 [US5] Remove obsolete direct shader directories/copy targets and prove every moved GLSL/SPIR-V byte and SHA-256 remains unchanged in `Demo/StonerDemo/Shaders/`, `Source/Renderer/Shaders/Deferred/`, and `Tests/verify_repository_shader_assets.py`
- [X] T105 [US5] Complete repository verifier, six-program Asset load, exact-byte, path-boundary, native rollback, deterministic output, and pre-migration compatibility tests in `Tests/test_verify_repository_shader_assets.py`, `Tests/AssetMaterialShaderTests.cpp`, `Tests/VulkanNativeIntegrationTests.cpp`, `Tests/DeferredNativeIntegrationTests.cpp`, and `Tests/TriangleDemoIntegrationTests.cpp`

**Checkpoint**: US5 addresses all repository shaders through six stable program
Assets, preserves the 22 dependency bytes, and supplies RHI/Core bytecode values
to Backend with no path or Asset dependency.

---

## Phase 8: Polish and Cross-Cutting Validation

**Purpose**: Complete deterministic reports, full architecture checks,
sanitizers, strict three-platform automation, native regression, delivered
documentation, and final Feature 023 reconciliation.

- [X] T106 [P] Audit final yyjson provenance, parser flags, fixed allocator assumptions, third-party warning isolation, and private-header containment in `Tests/verify_material_shader_provenance.py` and `Source/Asset/SConscript`
- [X] T107 [P] Extend final repository verifier unit coverage for deterministic ordering, mixed separators, duplicate declarations, optional tool absence, normalized errors, and Content staging inventory in `Tests/test_verify_repository_shader_assets.py`
- [X] T108 Run strict Debug plus `asset-material-shader`, `renderer-material-asset`, `renderer-material`, triangle, forward, deferred, Vulkan native, architecture, provenance, repository verifier, and full regression locally using `specs/023-material-shader-assets/quickstart.md`
- [X] T109 Run strict Release with 20 deterministic repetitions, eight-reader stress, normalized Feature 023 report, non-gating elapsed/host telemetry, and available native Vulkan evidence using `specs/023-material-shader-assets/quickstart.md`
- [X] T110 Add Feature 023 provenance unit tests, architecture/repository verifiers, focused Asset/Renderer suites, deterministic report, strict Release, and existing native regressions after local gates in `.github/workflows/ci.yml`
- [X] T111 Add Linux ASan/UBSan coverage for full malformed/dependency/conversion/native paths and focused ThreadSanitizer coverage for eight concurrent immutable parse/select/resolve/convert requests in `.github/workflows/ci.yml`
- [X] T112 Upload normalized Feature 023 determinism/repository reports and preserve existing Feature 018/019 native artifacts without introducing a substitute semantic-oracle or screenshot requirement in `.github/workflows/ci.yml`
- [ ] T113 Run Windows, macOS, and Linux focused suites, strict Debug/Release, architecture/repository checks, full regression, sanitizer profiles, and applicable native gates through `.github/workflows/ci.yml`
- [ ] T114 Record local and CI canonical digests, corpus counts, compatibility totals, repository byte agreement, sanitizer status, and native outcomes under `Validation/023/README.md` and `Validation/023/`
- [X] T115 Update delivered architecture, schema, dependency provenance, Renderer/native migration, validation evidence, exclusions, roadmap status, next phase, and the completed Feature 023 portion plus remaining Feature 027 debt for `CR001-B09-F005` in `doc/023-material-shader-assets.html`, `doc/SYSTEM_DESIGN.MD`, `doc/roadmap.md`, `doc/code-reviews/CR-001-pre-asset-hardening.md`, and `AGENTS.md`
- [X] T116 Re-run every applicable command in `specs/023-material-shader-assets/quickstart.md` and reconcile final implementation status against `specs/023-material-shader-assets/spec.md`, `specs/023-material-shader-assets/plan.md`, and `specs/023-material-shader-assets/tasks.md`

---

## Dependencies and Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies. T001, T002, and T006 may begin in
  parallel; fixture population and test registration follow their respective
  definitions.
- **Foundational (Phase 2)**: Depends on Setup. T020-T026 form the blocking
  strict-parser, ownership, provenance, and architecture proof.
- **US1 (Phase 3)**: Depends on Foundation and establishes the complete Shader
  Program and target-selection authority used by US4 and US5.
- **US2 (Phase 4)**: Depends on Foundation. It may begin alongside US1 after the
  shared Shader identity vocabulary exists, but its final render-ready checks
  consume US1 selection semantics.
- **US3 (Phase 5)**: Depends on US1 and US2 because it hardens all three
  completed schema/model families and their shared transaction.
- **US4 (Phase 6)**: Depends on US1 and US2. It can proceed in parallel with US3
  after the public model contracts stabilize; final conversion corpus consumes
  US3 canonical definitions.
- **US5 (Phase 7)**: Depends on US1, US3, and US4. It migrates real content and
  the native call chain only after Asset loading and snapshots are proven.
- **Polish (Phase 8)**: Depends on all five stories. CI changes intentionally
  follow successful local focused and regression gates.

### User Story Dependency Graph

```text
Setup -> Foundational -> US1
                    \--> US2
US1 + US2 -> US3
US1 + US2 -> US4
US3 + US4 -> US5 -> Polish
```

### Within Each User Story

- Add the story's failing tests before its implementation.
- Establish immutable models before validators/loaders.
- Validate and resolve dependencies before publication or conversion.
- Complete transactional rollback before positive integration.
- Finish the independent checkpoint before consuming the story downstream.

### Parallel Opportunities

- T001, T002, and T006 operate on independent provenance, fixture-design, and
  test-entry files.
- T012 and T013 can define limits and shared enum/inspection vocabulary in
  parallel; T017 follows the completed T013 vocabulary.
- After Foundation, US1 shader validation and US2 material behavior may be
  developed by separate owners while sharing only stable public vocabulary.
- After US1/US2 models stabilize, US3 schema hardening and US4 Renderer adapters
  can proceed in parallel on Asset-private versus Renderer files.
- T106 and T107 audit separate provenance and repository-verifier tools.

## Parallel Examples

### Foundation

```text
Task T012: Define finite material/shader limits.
Task T013: Define shared material/shader vocabulary and inspection records.
```

### P1 Stories

```text
US1 owner: T027-T043 in shader validation, dependency, and selection files.
US2 owner: T044-T059 in material validation, dependency, and resolution files.
Coordinate before editing shared JSON codec files in T040 and T053-T054.
```

### P2 Stories

```text
US3 owner: T060-T072 in Asset parser/schema/transaction and fixture files.
US4 owner: T073-T085 in Renderer conversion/library and compatibility tests.
```

## Implementation Strategy

### MVP First

1. Complete Setup and Foundation.
2. Complete US1 Shader Program Assets.
3. Run the independent `asset-material-shader` shader checkpoint.
4. Stop with a useful source-only plus explicit-runtime-payload Shader Asset
   MVP before adding material and repository migration.

### Incremental Delivery

1. Setup + Foundation establish bounded source ingestion.
2. US1 delivers stable Shader Program identity and target selection.
3. US2 delivers Material/Instance assets and typed dependencies.
4. US3 proves canonical, safe, deterministic authoring at scale.
5. US4 preserves Feature 014 through immutable Renderer snapshots.
6. US5 migrates the real repository and native execution.
7. Polish closes three-platform, sanitizer, native, and documentation gates.

### Commit Boundaries

- Commit Setup/Foundation separately from story behavior.
- Prefer one commit for tests plus implementation of one cohesive responsibility
  such as shader selection, instance resolution, or snapshot conversion.
- Keep repository content movement, SCons staging, Demo migration, Backend API
  migration, and native-session split as separate reviewable commits.
- Do not mix CI/documentation closeout with unresolved implementation changes.

## Notes

- `[P]` appears only where tasks edit disjoint files and require no incomplete
  dependency.
- Every user-story task carries its `[USn]` traceability label.
- Tests precede implementation and must fail for the intended reason.
- Feature 023 must not add cooker, DDC, manifest, async manager, hot reload,
  runtime compilation, GPU residency, model import, or a new graphics backend.
- Backend receives RHI/Core bytecode descriptions only; Asset remains
  Core-only.
