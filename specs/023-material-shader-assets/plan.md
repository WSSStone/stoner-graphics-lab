# Implementation Plan: Material & Shader Assets

**Branch**: `023-material-shader-assets` | **Date**: 2026-07-30 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `specs/023-material-shader-assets/spec.md`

## Summary

Feature 023 replaces process-local material/shader identity and repository-path
shader loading with versioned Asset records while preserving Feature 014
Renderer behavior. One `FShaderAsset` represents a complete logical program:
ordered stage source references, canonical permutation variants, interfaces,
required material parameters, and backend/profile-tagged precompiled payload
references. `FMaterialAsset` and `FMaterialInstanceAsset` replace string/pointer
relationships with typed soft references to Shader, Texture, Material, and
MaterialInstance assets.

Authoring authority is a strict, human-editable UTF-8 JSON source definition.
GLSL and SPIR-V remain separate typed dependencies; no binary payload is
embedded in JSON and Feature 025 retains cooked binary, manifest, DDC, and
offline compilation ownership. Asset privately vendors `yyjson 0.12.0` for
bounded RFC 8259 parsing/writing behind a project-owned strict schema adapter.
Renderer converts validated assets to self-contained immutable Feature 014/RHI
snapshots stamped with all source versions. Application/Renderer composition
loads those snapshots and supplies opaque bytecode through RHI/native helper
contracts; Backend remains `Backend -> RHI + Core` and never queries Asset.

## Technical Context

**Language/Version**: C++20 with traditional public/private headers and sources; C for the private yyjson translation unit; no C++20 Modules
**Primary Dependencies**: Existing Core and Asset contracts; Renderer/RHI/Application/Demo/Vulkan integration boundaries; pinned private yyjson 0.12.0; existing checked-in GLSL/SPIR-V; SCons 4.10.1
**Storage**: Versioned canonical UTF-8 JSON authoring definitions plus separate repository-owned GLSL/SPIR-V dependency files under `Content/`; immutable in-memory Asset payloads and Renderer snapshots; no cooked binary, manifest, DDC, package, database, or runtime cache
**Testing**: Existing `StonerTest` suite registry; focused Asset material/shader and Renderer material suites; schema fixture/mutation corpus; repository shader inventory/digest verifier; architecture verifier; strict Debug/Release; Linux ASan/UBSan and focused TSan; Windows/macOS/Linux GitHub Actions; existing triangle/deferred native gates
**Target Platform**: Windows, macOS, Linux; source parse/normalize, dependency load, Asset validation, selection, conversion, and deterministic tests mandatory on all three
**Project Type**: Cross-platform C++ graphics-engine libraries, demo composition root, and test executable
**Performance Goals**: Twenty parse-validate-normalize repetitions over at least 40 definitions remain byte-identical; Release runs record elapsed time as non-gating telemetry together with host CPU, OS, compiler, build configuration, and corpus counts; eight concurrent immutable readers produce identical output; snapshot lookup adds no filesystem work to per-frame rendering
**Constraints**: Asset depends only on Core; Backend depends only on RHI/Core; strict JSON with duplicate-key rejection; bounded source/dependency reads and instance traversal; no runtime compilation, live binding, hot reload, cooker, manifest, DDC, new backend, or GPU residency
**Scale/Scope**: At least 12 shader programs, 12 materials, 16 instances, 40 malformed/boundary fixtures, 11 GLSL plus 11 SPIR-V repository dependencies; defaults of 1 MiB per definition, 4 MiB per GLSL source, 16 MiB per precompiled payload, 64 MiB resolved program bytes, 1,024 locator bytes, 32 JSON depth, 32,768 JSON values, 16 stages, 64 permutation flags, 1,024 variants, 2,048 payload records, 256 material parameters/interface bindings, 1,024 dependencies, and instance depth 64

## Constitution Check

### Pre-Research Gate

- [x] **Spec-Driven Development**: `spec.md` defines five independently
  testable stories, 41 FRs, 12 measurable SCs, and five accepted
  clarifications.
- [x] **Decoupled Architecture**: Asset remains `Asset -> Core`; Renderer owns
  Asset conversion; Application/Demo compose assets; Backend receives only RHI/
  Core bytecode vocabulary and never includes Asset.
- [x] **Design Pattern Discipline**: schema parsing, canonical writing,
  validation, dependency loading, program selection, snapshot conversion, and
  repository migration are separate collaborators rather than one loader.
- [x] **Multi-API Support**: payload records use backend/profile/format tags with
  explicit MSL, DXIL, GLSL, and SPIR-V vocabulary; only current SPIR-V is
  runtime-ready.
- [x] **Advanced Graphics Readiness**: program-level stages, permutations,
  interfaces, and typed dependencies can add mesh/ray stages through later
  schema versions without changing logical identity.
- [x] **Naming Conventions**: public C++ design uses UE-style `F`, `E`, `I`, and
  `T` prefixes with PascalCase.
- [x] **Cross-Platform Compatibility**: source definitions are UTF-8/LF,
  identity is Feature 020 canonical, parser/writer behavior is host-independent,
  and filesystem paths never enter identity or normalized diagnostics.
- [x] **Automated Cross-Platform Validation**: Windows/macOS/Linux run focused
  schema/conversion/migration tests and strict builds; Linux keeps sanitizers;
  existing native triangle/deferred gates verify migrated payloads.

### Post-Design Gate

All gates remain satisfied. `yyjson` is a private Asset implementation
dependency with pinned source, MIT license, version, upstream commit, and
SHA-256 provenance; no public header exposes it. The strict wrapper rejects
duplicate decoded keys even though upstream preserves them, applies project
UTF-8/NFC and limit policy, and writes canonical JSON from validated model
objects rather than round-tripping the third-party DOM. Renderer snapshots
deep-copy required data and source versions. Native helper signature changes
move path reads upward without adding an Asset include to RHI or Backend. No
constitutional exception is required.

## Project Structure

### Documentation

```text
specs/023-material-shader-assets/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── source-definition-schema.md
│   ├── material-shader-asset-api.md
│   └── renderer-shader-integration.md
└── tasks.md                         # Created later by speckit-tasks
```

### Planned Source Changes

```text
Content/
├── Shaders/
│   ├── Triangle/
│   │   ├── Triangle.shader.json
│   │   ├── Triangle.vert
│   │   ├── Triangle.vert.spv
│   │   ├── Triangle.frag
│   │   └── Triangle.frag.spv
│   └── Deferred/
│       ├── Surface.shader.json
│       ├── Composition.shader.json
│       ├── DirectionalLight.shader.json
│       ├── PointLight.shader.json
│       ├── SpotLight.shader.json
│       └── <11 existing GLSL/SPIR-V stage files>
└── Materials/
    └── <representative .material.json and .material-instance.json definitions>

Source/
├── Asset/
│   ├── Public/Asset/
│   │   ├── EAssetResult.h             # Extend existing result/stage vocabulary
│   │   ├── FShaderAsset.h
│   │   ├── FShaderSourceAsset.h
│   │   ├── FShaderPayloadAsset.h
│   │   ├── FMaterialAsset.h
│   │   ├── FMaterialInstanceAsset.h
│   │   ├── FMaterialShaderAssetLimits.h
│   │   ├── FMaterialShaderInspection.h
│   │   └── FMaterialShaderSourceLoader.h
│   └── Private/
│       ├── FMaterialShaderJsonCodec.{h,cpp}
│       ├── FMaterialShaderSchemaValidator.{h,cpp}
│       ├── FShaderProgramValidator.{h,cpp}
│       ├── FMaterialAssetValidator.{h,cpp}
│       ├── FMaterialDependencyExtractor.{h,cpp}
│       ├── FShaderDependencyLoader.{h,cpp}
│       ├── FShaderPayloadSelector.cpp
│       └── FMaterialShaderSourceLoader.cpp
├── Renderer/
│   ├── Public/Renderer/
│   │   ├── FMaterialAssetConversion.h
│   │   └── FShaderAssetConversion.h
│   └── Private/
│       ├── FMaterialAssetConversion.cpp
│       └── FShaderAssetConversion.cpp
├── RHI/Public/RHI/
│   └── FRHIShaderModuleDesc.h        # Reused; no Asset dependency
└── Backend/Vulkan/
    ├── Public/VulkanRHI/FVulkanNativeContext.h
    └── Private/
        ├── FVulkanNativeContext.cpp
        └── FVulkanNativeOffscreenSession.cpp

Demo/StonerDemo/
├── Private/FStonerDemoApplication.{h,cpp}
└── SConscript

ThirdParty/yyjson/
├── LICENSE
├── UPSTREAM.md
├── VERSION
├── yyjson.c
└── yyjson.h

Tests/
├── AssetMaterialShaderTests.{h,cpp}
├── RendererMaterialShaderAssetTests.{h,cpp}
├── RendererMaterialShaderTests.cpp
├── TriangleDemoIntegrationTests.cpp
├── DeferredNativeIntegrationTests.cpp
├── VulkanNativeIntegrationTests.cpp
├── Fixtures/MaterialShader/
│   ├── Valid/
│   ├── Invalid/
│   └── Golden/
├── verify_asset_layer.py
├── verify_material_shader_provenance.py
├── verify_repository_shader_assets.py
└── test_verify_repository_shader_assets.py
```

The existing shader files move from `Demo/StonerDemo/Shaders` and
`Source/Renderer/Shaders/Deferred` to `Content/Shaders` without changing source
or SPIR-V bytes. SCons copies Content dependencies needed beside build outputs
but does not compile GLSL in Feature 023. Existing optional local GLSL
compilation may remain a validation aid only if its output is not authoritative
and does not satisfy repository digest gates.

**Structure Decision**: Asset owns source definitions, typed CPU payloads,
schema validation, dependency extraction/loading, and deterministic selection.
Renderer owns immutable conversion snapshots and RHI descriptions. Application/
Demo owns synchronous composition before Feature 026. Backend path-based test
helpers become bytecode-input helpers but retain all Vulkan behavior. Source
files live under `Content/` because they are engine content, not Renderer or Demo
implementation files.

## Design Decisions

### 1. Strict Canonical JSON with a Private Parser

Use strict RFC 8259 JSON as the authoring syntax and vendor `yyjson 0.12.0`
privately. The Asset wrapper parses from a bounded memory source using raw
number tokens and a preflighted fixed allocator budget, then rejects
BOM, NUL, comments, trailing commas, invalid UTF-8, lone surrogates, non-finite
or out-of-range numbers, root values other than an object, decoded duplicate
keys, unknown ordinary fields, and values/counts/nesting beyond configured
limits. A non-recursive post-parse walk enforces aggregate values and duplicate
decoded keys, including escaped-equivalent keys.

Canonical output is UTF-8 without BOM, two-space indentation, LF newlines, one
final newline, fixed schema key order, lexicographically sorted map-like keys,
canonical Asset identity order for dependency sets, and preserved order only
for semantically ordered lists such as acceptable target profiles. Float input
is parsed exactly to finite `float`; output uses one shortest round-tripping
decimal policy, normalizes negative zero to `0`, and does not preserve original
lexemes or comments.

Optional extensions live only under a namespaced `extensions` object.
`requiredExtensions` lists extension names whose absence or unknown semantics
must fail. Input extension-name collections may arrive in any order, must be
unique, and are sorted in the typed model and canonical output. Unknown
optional extensions may be skipped and omitted by canonical rewrite. Unknown
root/schema fields fail so misspellings cannot masquerade as forward
compatibility.

### 2. Program-Level Identity and Typed Dependencies

Public Asset types use fixed Feature 020 type names:

- `ShaderProgram`: one complete logical program.
- `ShaderSource`: one GLSL source dependency/subresource.
- `ShaderPayload`: one precompiled target/stage/permutation dependency.
- `Material`: one base material.
- `MaterialInstance`: one instance definition.
- existing `Texture`: texture-valued material parameters.

A Shader Program definition never embeds GLSL or SPIR-V bytes. It references
typed source/payload assets and records the expected digest, producer, stage,
entry point, permutation, backend, format, and profile. A dependency loader
uses existing resolver/source/loader leases, validates exact bytes and digest,
and builds immutable `FShaderSourceAsset`/`FShaderPayloadAsset` values. One
program may reuse a source dependency across multiple complete programs.

### 3. Transactional Source Pipeline

The synchronous Feature 023 development path has explicit phases:

1. bounded source read;
2. strict JSON parse;
3. schema version/extension validation;
4. canonical identity and typed-field construction;
5. semantic validation;
6. dependency extraction;
7. optional dependency load and digest/SPIR-V validation;
8. canonical source generation;
9. all-or-nothing import-output emission;
10. optional `FMaterialShaderImportService::ImportAndRegister` construction of
    one `FAssetMutationBatch` followed by exactly one Registry `Apply`.

Each phase returns stable Asset result/stage/field diagnostics and publishes
nothing on failure. The loader/importer never mutates `FAssetRegistry`; the
explicit import service is the only Feature 023 registry mutation boundary and
leaves the Registry revision unchanged when loading, validation, batch
construction, or `Apply` fails. Parsing and normalization can run without
dependency bytes; runtime-ready validation requires all required runtime
dependencies. Immutable requests share no process-global parser DOM or mutable
selection cache.

### 4. Shader Program, Permutation, and Profile Selection

`FShaderAsset` owns ordered stage records, allowed flags, canonical variants,
required material parameters, interface records, and payload records. Current
supported stages are Vertex, Fragment, and Compute. A graphics program requires
Vertex plus Fragment; a compute program requires exactly Compute. Mixed graphics/
compute programs, duplicate stages/entry points, undeclared flags, duplicate
canonical variants, duplicate target keys, and incompatible interfaces fail.

Payload selection receives one backend plus an explicit ordered acceptable-
profile list. For the requested permutation and required stage set it selects
the first profile having exactly one complete payload set. Ambiguity at that
profile fails immediately; lower profiles are not consulted. There is no
capability scoring, cross-backend fallback, source compilation, or runtime
conversion.

### 5. Material and Instance Asset Semantics

Asset-owned material enums mirror Feature 014 meanings without including
Renderer headers. Material source definitions use a ShaderProgram soft
reference, canonical permutation flags, render state, and typed defaults.
Texture parameters use `TSoftAssetRef<FTextureAsset>`. Instance definitions use
exactly one Material or MaterialInstance parent reference and typed overrides.

Resolution loads the parent chain through a caller-supplied immutable lookup,
limits depth to 64, detects repeated identity before visiting the next parent,
and applies nearest overrides. Missing/unresolved parents remain valid registry
metadata but fail render-ready conversion. Feature 014 domain/blend and
parameter rules remain the behavior authority and are mirrored by compatibility
tests rather than reinterpreted.

### 6. Immutable Renderer Conversion Snapshots

Renderer adapters consume validated Asset payloads plus resolved dependencies
and produce:

- an all-or-nothing `FShaderLibrary` batch plus RHI shader module descriptions
  for the selected target;
- one flattened `FMaterial` value for either a base material or resolved
  instance;
- canonical resource requirements;
- a source manifest containing every input `FAssetId` and `FAssetVersion`.

All strings, parameters, payload words, interfaces, and versions are copied.
Snapshots hold no Asset pointers, leases, Registry callbacks, or live-update
subscription. Replacing a source Asset leaves an existing snapshot usable as
its recorded version; explicit reconversion produces a new snapshot. Existing
Feature 014 APIs remain available and equivalence tests compare both paths.

### 7. Repository Shader Migration without Backend Coupling

Create six program definitions:

1. Triangle: Triangle vertex + fragment.
2. Deferred Surface: Surface vertex + fragment.
3. Deferred Composition: Fullscreen vertex + Composition fragment.
4. Deferred DirectionalLight: Fullscreen vertex + DirectionalLight fragment.
5. Deferred PointLight: PointLight vertex + fragment.
6. Deferred SpotLight: SpotLight vertex + fragment.

The shared Fullscreen source/payload remains one typed dependency referenced by
two programs. All 11 GLSL and 11 SPIR-V files keep exact bytes and recorded
SHA-256 values.

Before moving any repository shader, record the current commit, normalized
triangle/material/forward/deferred summaries, deterministic outputs, native
failure behavior, and all 22 file digests under `Validation/023/Baseline/`.
Post-migration compatibility compares against this frozen evidence as well as
the existing executable assertions.

Demo/Application and native-test composition load the program assets, select
Vulkan desktop SPIR-V snapshots, then pass word arrays/entry points to existing
RHI or Vulkan native helpers. `FVulkanNativeContext` and the offscreen session
accept backend-neutral bytecode input instead of file paths. Backend performs
SPIR-V/runtime validation and Vulkan creation but never opens Content files or
includes Asset. `FShaderLibrary` gains an additive transactional batch
registration API while retaining its existing single-record entry point.
SCons gains one reusable content-staging helper shared by Demo and native tests.

The migration performs a controlled first split of
`FVulkanNativeOffscreenSession::Execute`: shader acquisition is removed from
the function, immutable shader-module descriptions enter as input, and shader
module creation/rollback becomes a separate transactional stage. This closes
the Feature 023 portion of CR001-B09-F005 without turning the feature into a
general native-session rewrite; remaining decomposition stays required before
Feature 027.

### 8. Verification and CI

Add focused `asset-material-shader` and `renderer-material-asset` suites while
retaining `asset` and `renderer-material` aggregate behavior. Tests cover the
canonical corpus, at least 40 mutations, duplicate decoded keys, optional/
required extensions, all limits, dependency/digest failures, stage/interface/
permutation validation, profile ambiguity, instance cycles/depth, rollback,
snapshot versioning, and eight concurrent immutable requests.

A Python verifier inventories six program definitions and all 22 dependency
files, checks expected logical IDs/stages/entry points/digests, rejects direct
production path references outside approved source-loading composition, and has
`unittest` coverage. Existing triangle/deferred deterministic and native gates
consume migrated snapshots. CI runs strict Debug/Release on all three hosts,
Linux ASan/UBSan full/focused tests, and focused TSan concurrency. No new visible
screenshot requirement is introduced because observable shader results are
already covered by Features 018/019 gates.

## Complexity Tracking

No constitution violations require justification.
