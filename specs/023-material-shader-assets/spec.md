# Feature Specification: Material & Shader Assets

**Feature Branch**: `023-material-shader-assets`
**Created**: 2026-07-30
**Status**: Draft
**Input**: User description: "依照 roadmap 为下一阶段制定 spec 文件"

## Clarifications

### Session 2026-07-30

- Q: 一个 `FShaderAsset` 应代表完整的多阶段 Shader Program，还是只代表单个 Shader Stage？ → A: 一个 Shader Asset 代表完整逻辑 Program，并在内部包含相关 stages、permutations 和 backend/profile payloads；单个源码文件保留为可追踪的 source record 或 subresource。
- Q: 当请求的 backend/profile 没有完全相同的 Shader payload 时，系统应如何选择兼容 payload？ → A: 请求方提供同一 backend 内按优先级排列的可接受 profile；系统选择第一个唯一匹配项，不允许隐式 capability 评分、跨 backend fallback 或运行时转换。
- Q: Material/Shader Asset 在 Feature 023 中应以哪种持久化表示作为权威来源？ → A: 版本化、可人工编辑的 source definition 是权威；Shader source 与 precompiled payload 是 typed dependencies，发布用 cooked binary、manifest 和 derived-data cache 留给 Feature 025。
- Q: 当 Material、Material Instance 或 Shader Asset 被替换为新版本后，已经转换出的 Renderer 对象应如何处理？ → A: 转换对象是记录源版本的 immutable snapshot，深拷贝所需数据且不监听 Registry；Asset 替换只影响后续显式转换。
- Q: 当较新版本的 source definition 包含当前 reader 不认识的字段时，应如何处理？ → A: 未知 optional 字段可跳过且规范化输出可丢弃；未知 required 字段或行为必须 fail closed，不要求 lossless preservation。

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Preserve Shaders as Stable Assets (Priority: P1)

An engine developer can represent a shader program by a stable logical asset
identity, inspect its source and available runtime payloads, and select the one
payload that matches a requested graphics backend and target profile without
depending on repository-relative file paths.

**Why this priority**: Every imported model and every future native backend
depends on stable shader identity and an explicit payload contract. Shader
assets are therefore the minimum independently useful slice of this feature.

**Independent Test**: Create shader assets containing representative source
units, stages, entry points, permutations, interfaces, and target-tagged
payloads; serialize and reload them; then request matching and non-matching
targets and compare the selected payload and diagnostics over repeated runs.

**Acceptance Scenarios**:

1. **Given** a valid shader asset with source records and a compatible
   backend/profile payload, **When** that target is requested, **Then** the
   matching payload, stage entry points, permutation identity, and interface
   description are returned without consulting a physical source path.
2. **Given** an ordered list of acceptable profiles for one backend, **When**
   payload selection is repeated against assets whose declarations were
   supplied in different orders, **Then** the first profile with one unique
   match is selected and the same normalized summary is produced.
3. **Given** a shader asset with no payload compatible with the requested
   backend, profile, stage set, or permutation, **When** it is selected for
   rendering, **Then** selection fails with an actionable diagnostic and no
   fallback payload is silently substituted.
4. **Given** a source-only shader asset, **When** it is inspected or
   serialized, **Then** it remains a valid authoring record; when it is
   requested for rendering without a compatible precompiled payload, it is
   reported as not runtime-ready.

---

### User Story 2 - Preserve Materials and Instances as Assets (Priority: P1)

An engine developer can store a material or material instance under a stable
asset identity, retain its typed parameters and rendering behavior, and express
shader, texture, and parent-material relationships as typed soft dependencies
rather than process-local strings or pointers.

**Why this priority**: Feature 024 model import needs a durable material target,
while existing forward and deferred paths need the behavior established by
Feature 014 to remain intact.

**Independent Test**: Create representative material and instance assets,
round-trip them, resolve inheritance and dependencies, and compare their
effective parameters and rendering descriptions with equivalent Feature 014
runtime materials.

**Acceptance Scenarios**:

1. **Given** a valid material asset, **When** it is reloaded, **Then** its
   domain, blend behavior, render state, shader reference, permutation request,
   typed default parameters, and declared dependencies match the original.
2. **Given** an instance asset whose parent is a material or another instance,
   **When** effective values are resolved, **Then** the nearest compatible
   override wins and all non-overridden values come from the parent chain.
3. **Given** a texture-valued parameter, **When** metadata dependencies are
   extracted, **Then** the referenced texture appears as a typed runtime
   dependency and no live GPU object or repository path is stored.
4. **Given** an unresolved soft dependency, **When** the asset is registered,
   **Then** metadata remains inspectable with the dependency marked unresolved;
   validation for rendering fails until every required runtime dependency is
   resolved.
5. **Given** a parent cycle, incompatible override, duplicate parameter,
   missing shader, or unsupported domain/blend combination, **When** the asset
   is validated, **Then** it is rejected with a diagnostic that identifies the
   asset and failing field or dependency.

---

### User Story 3 - Load Versioned Source Definitions Safely (Priority: P2)

An engine developer can edit, normalize, serialize, and reload human-readable
material and shader source definitions that reject corruption, unsupported
schema versions, excessive data, and ambiguous declarations before publishing
an asset.

**Why this priority**: Feature 025 will package these payloads and derive cache
keys from them. The schema must be safe, reproducible, and evolvable before a
manifest or derived-data cache makes it durable at scale.

**Independent Test**: Round-trip a canonical source-definition corpus, mutate
every structural section and boundary count, load supported and unsupported
versions, and verify atomic failure and byte-identical normalized source output
across supported platforms.

**Acceptance Scenarios**:

1. **Given** a valid supported source definition, **When** it is loaded and
   normalized again without semantic changes, **Then** the canonical source
   bytes, asset identity, dependency set, and version evidence are unchanged.
2. **Given** fields supplied in a different order but with equivalent meaning,
   **When** they are serialized, **Then** they produce the same canonical
   representation.
3. **Given** a source definition with an unsupported schema version, unknown
   required feature, malformed length, invalid text, duplicate key, overflow,
   truncation, trailing data, or excessive count, **When** loading is
   attempted, **Then** it fails before publication and identifies the first
   invalid section.
4. **Given** a failed load or validation, **When** the operation returns,
   **Then** no partial Asset Registry mutation, Renderer object, or retained
   payload is observable.
5. **Given** a supported schema containing an unknown optional field, **When**
   an older reader normalizes it, **Then** known semantics remain unchanged and
   the unknown field may be omitted; an unknown required field or feature
   causes loading to fail.

---

### User Story 4 - Preserve Renderer Behavior Through Adapters (Priority: P2)

An engine developer can convert validated material, instance, and shader assets
into the established Renderer vocabulary so existing material validation,
permutation resolution, resource requirement extraction, and draw planning
continue to behave the same while callers migrate to asset identities.

**Why this priority**: Asset adoption must not force a simultaneous rewrite of
Feature 014, forward rendering, deferred rendering, or demo execution.

**Independent Test**: Build equivalent legacy runtime records and Asset-backed
records, adapt the latter, and compare validation results, effective
parameters, selected variants, resource requirements, diagnostics, and
deterministic rendering plans.

**Acceptance Scenarios**:

1. **Given** equivalent valid legacy and Asset-backed material definitions,
   **When** both are resolved for rendering, **Then** they produce equivalent
   shader selection, effective parameters, resource requirements, and render
   state.
2. **Given** equivalent invalid definitions, **When** both paths are validated,
   **Then** they fail in the same responsibility domain without the Asset path
   weakening existing validation.
3. **Given** an Asset-backed instance chain, **When** the Renderer adapter
   resolves it, **Then** it contains no borrowed pointer to an Asset payload,
   records the source Asset versions, and remains an immutable snapshot for its
   documented converted-object lifetime.
4. **Given** an adapter failure at any stage, **When** the operation returns,
   **Then** no partially registered shader record or partially usable material
   object is published.
5. **Given** a source Asset that is replaced after successful conversion,
   **When** the existing Renderer snapshot is used, **Then** it continues to
   represent its recorded source version; only an explicit new conversion
   observes the replacement.

---

### User Story 5 - Migrate Repository-Owned Shaders (Priority: P3)

An engine maintainer can address every repository-owned triangle and deferred
shader through stable Asset identities while preserving the checked-in source,
precompiled payload bytes, native execution behavior, and source-to-payload
traceability.

**Why this priority**: A schema that works only for synthetic fixtures has not
proven that the engine can leave path-based shader ownership behind.

**Independent Test**: Inventory the repository shader corpus, load every
source/payload pair as an Asset-backed record, compare payload digests with the
checked-in files, and run existing triangle, forward/deferred, deterministic,
and available native validation.

**Acceptance Scenarios**:

1. **Given** the repository shader corpus, **When** migration validation runs,
   **Then** all 11 source files and all 11 checked-in SPIR-V payloads have
   stable logical identities, declared stage entry points, and source-to-payload
   traceability.
2. **Given** an unchanged checked-in SPIR-V payload, **When** it is loaded
   through the shader asset path, **Then** its complete bytes and digest match
   the repository artifact.
3. **Given** triangle and deferred execution using migrated records, **When**
   existing deterministic and native gates run, **Then** their observable
   rendering results and failure behavior remain compatible with the pre-
   migration baseline.
4. **Given** a missing, mismatched, or stale source/payload pair, **When**
   repository validation runs, **Then** it fails with the logical shader
   identity, stage, and mismatched evidence.

### Edge Cases

- A material, shader, instance, parameter, permutation flag, entry point,
  backend tag, or profile tag contains empty, non-canonical, invalid, duplicate,
  or over-limit text.
- Two payload records claim the same shader, permutation, backend, profile,
  stage, and entry point.
- A shader program contains duplicate stages, a missing required stage, an
  incompatible stage combination, or inconsistent interfaces between stages.
- A shader payload is empty, truncated, misaligned for its declared format,
  associated with the wrong stage, or has revision evidence that does not match
  its bytes.
- A material parameter contains non-finite numeric data, a value of the wrong
  type, or a resource reference whose Asset type is not accepted by that
  parameter.
- A material instance chain is cyclic, exceeds its supported depth, references
  a missing parent, or changes the type of an inherited parameter.
- A shader or material has valid syntax but depends on unresolved or
  type-incompatible assets.
- A valid schema version contains an unknown optional field versus an unknown
  required feature.
- Canonically equivalent declarations arrive in different insertion orders.
- A serialized length, count, offset, aggregate size, or dependency traversal
  would overflow or exceed configured limits.
- Source parsing, dependency extraction, adaptation, or registry publication
  fails after earlier validation stages have succeeded.
- An Asset-backed object is invalidated or replaced after a converted Renderer
  snapshot was produced, while both the old and new source versions remain
  observable.
- The first acceptable target profile has multiple matching payloads, while a
  lower-priority profile has a unique match.
- A future backend target slot is declared without a payload and must remain
  inspectable without being treated as runtime-ready.

## Architecture & Design Constraints *(mandatory)*

- **Asset Boundary**: Production material and shader Asset code MUST depend only
  on Core and existing Asset contracts. It MUST NOT include or link RHI,
  Renderer, Application, Backend, Tools, a graphics API, or a platform shader
  compiler.
- **Renderer Ownership**: Conversion from Asset payloads to Renderer material,
  shader-library, resource-requirement, or RHI-facing vocabulary MUST be owned
  by Renderer. Asset MUST NOT create or retain live Renderer or GPU objects.
- **Backend Boundary**: Backend MUST continue to depend only on Core and RHI.
  Application or Renderer composition resolves shader assets and supplies
  opaque payloads through backend-neutral contracts; Backend MUST NOT query the
  Asset Registry or include Asset contracts.
- **RHI Abstraction**: Runtime shader payload records MUST use portable
  backend/profile/stage tags and opaque bytes. Graphics API enums and native
  handles MUST remain outside Asset.
- **Stable Identity**: Shader, material, instance, texture, and parent
  references MUST use Feature 020 identities and typed soft references.
  Filesystem paths, object addresses, and content hashes MUST NOT replace
  logical identity.
- **Source/Cooked Separation**: Logical shader identity MUST remain stable when
  source content, compilation settings, backend payloads, or cook revisions
  change. Revision evidence and payload target tags MUST be represented
  separately.
- **Authoring Authority**: Versioned human-readable source definitions MUST be
  the authoritative persistent Material/Shader representation in Feature 023.
  Shader source files and precompiled payload files MUST be typed dependencies,
  not embedded binary fields in the authoring definition.
- **Schema Evolution**: Serialized representations MUST be explicitly
  versioned, bounded, deterministic, and fail closed for unsupported required
  behavior. Schema migration policy MUST not depend on host object layout.
- **Behavior Compatibility**: Asset-backed conversion MUST preserve the public
  behavior established by Feature 014 while allowing callers to migrate
  incrementally. Existing Renderer APIs MUST not be removed in this feature.
- **Snapshot Conversion**: Converted Renderer objects MUST be immutable,
  self-contained snapshots stamped with every source Asset version used during
  conversion. They MUST NOT subscribe to Registry changes or update in place.
- **Dependency Integrity**: Shader, texture, material, and parent-instance
  relationships MUST be extractable before registry publication and usable by
  later cooker and runtime-manager phases.
- **Responsibility Separation**: Schema validation, serialization,
  deserialization, dependency extraction, target selection, Renderer
  conversion, repository migration, and runtime realization MUST remain
  independently testable responsibilities.
- **No Runtime Compilation**: This feature MAY preserve shader source and load
  precompiled payloads, but MUST NOT compile arbitrary shader source during
  engine runtime. Offline target production belongs to Feature 025.
- **No Cooker Ownership**: This feature MUST NOT introduce manifests,
  packages, derived-data keys, persistent caches, incremental cooking, or
  standalone cooked binary Asset publication; those belong to Feature 025.
- **Future Backend Compatibility**: The schema MUST represent current GLSL
  source and SPIR-V payloads and reserve explicit target vocabulary for MSL,
  DXIL, and GLSL payloads without claiming those backends are implemented.
- **Future Rendering Compatibility**: Shader stages, interfaces, material
  dependencies, and permutation identity MUST remain extensible for later
  meshlet, ray-tracing, GI, and native-backend phases without implementing
  those pipelines here.
- **Naming Conventions**: Public code design MUST follow PascalCase and Unreal
  Engine-style `F`, `I`, `E`, and `T` naming conventions.
- **Cross-Platform Compatibility**: Asset schemas, validation, round trips,
  selection, and mock Renderer conversion MUST behave consistently on Windows,
  macOS, and Linux.
- **Automated Cross-Platform Validation**: Windows, macOS, and Linux automation
  MUST build and run focused material/shader Asset tests, schema corpus tests,
  architecture checks, repository migration checks, and existing regression
  suites. Native visual evidence is required only where an existing native gate
  already exercises the migrated shaders.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST provide immutable `FShaderAsset`,
  `FMaterialAsset`, and `FMaterialInstanceAsset` concepts whose stable identity,
  version evidence, metadata, dependencies, and CPU payload remain separate.
- **FR-002**: Every material and shader Asset identity MUST use Feature 020
  canonical logical paths and a distinct typed Asset category; references MUST
  reject type-incompatible identities before payload use.
- **FR-003**: A shader asset MUST describe one logical shader program through
  source records, stage entry points, permutation declarations, required
  material parameters, resource-interface declarations, and zero or more
  target-tagged precompiled payload records. Stages and permutations MUST NOT
  become independent top-level shader Asset identities; individual source files
  MAY retain traceable source-record or subresource identities.
- **FR-004**: Shader source records MUST identify language, stage, entry point,
  source revision evidence, and a typed source dependency without using a host
  filesystem path as identity or embedding source text in the authoritative
  program definition.
- **FR-005**: Shader stage vocabulary MUST support current vertex, fragment,
  and compute programs and MUST reject duplicate or incompatible stage
  declarations. It MUST allow later stage categories to be represented by a
  future schema version without treating them as supported now.
- **FR-006**: A shader payload record MUST declare payload format, graphics
  backend family, target profile, stage, entry point, permutation key, payload
  typed payload dependency, producer identity/version, and revision evidence.
  Runtime loading MAY resolve that dependency into immutable payload bytes, but
  the authoritative source definition MUST NOT embed those bytes.
- **FR-007**: Current runtime-ready shader assets MUST support GLSL source and
  SPIR-V payload records. The target vocabulary MUST reserve MSL, DXIL, and GLSL
  payload categories, but absence of those payloads MUST NOT make a Vulkan-
  ready shader invalid.
- **FR-008**: A source-only shader asset MAY be registered, inspected, and
  serialized, but MUST report `NotRuntimeReady` when no compatible precompiled
  payload exists. The system MUST NOT invoke arbitrary runtime compilation as a
  fallback.
- **FR-009**: Equivalent permutation flag sets MUST have one canonical identity
  independent of declaration order. Duplicate flags and flags not declared by
  the shader asset MUST be rejected.
- **FR-010**: Payload selection MUST match shader identity, permutation,
  backend, required stage set, and entry points against a caller-provided
  ordered list of acceptable profiles for that backend. It MUST select the
  first profile with exactly one match. Multiple matches at that first
  matching profile MUST return an ambiguous failure rather than continuing to
  a lower-priority profile. No match MUST fail without implicit capability
  scoring, cross-backend fallback, runtime conversion, or dependence on
  registration/container order.
- **FR-011**: Shader interfaces MUST describe required material parameters and
  abstract resource bindings sufficiently for existing material validation and
  later backend realization without storing native binding handles or graphics
  API enums.
- **FR-012**: Cross-stage interface validation MUST reject incompatible stage
  combinations, duplicate binding declarations, incompatible resource types,
  missing required interfaces, and payload records whose declared stage or
  entry point disagrees with the program.
- **FR-013**: A material asset MUST preserve Feature 014 material domain, blend
  behavior, render state, shader reference, permutation request, and typed
  default parameters.
- **FR-014**: Material parameter values MUST support the existing scalar,
  vector, color, and abstract resource-reference categories. Numeric values
  MUST be finite, names MUST be unique, and resource references MUST use
  accepted typed Asset identities.
- **FR-015**: Texture-valued material parameters MUST refer to Feature 021
  texture assets through typed soft references and MUST be emitted as required
  runtime dependencies without introducing a live texture or RHI dependency.
- **FR-016**: A material instance asset MUST reference exactly one parent
  material or parent instance Asset and contain only typed parameter overrides;
  process-local parent pointers MUST NOT be serialized.
- **FR-017**: Instance resolution MUST preserve Feature 014 nearest-override
  semantics, reject unknown or type-incompatible overrides, detect cycles, and
  apply a documented finite traversal limit before consuming unbounded parent
  chains.
- **FR-018**: Material and instance dependency extraction MUST report shader,
  texture, parent-material, and parent-instance edges with stable roles and
  strength before registry publication.
- **FR-019**: Registration MAY retain assets with explicitly unresolved soft
  dependencies according to Feature 020, but conversion to a render-ready
  material MUST fail until all required runtime dependencies are resolved and
  type-compatible.
- **FR-020**: Asset validation MUST preserve all Feature 014 domain/blend,
  duplicate-parameter, permutation, required-parameter, inheritance,
  invalidation, and resource-requirement rules; Asset-backed records MUST NOT
  weaken an existing rejection.
- **FR-021**: Each material and shader source definition MUST carry an explicit
  asset kind, schema version, required/optional extension declarations,
  canonical identity, and typed dependency references independent of host
  object layout. Successful canonicalization MUST derive `FAssetVersion`
  evidence from the canonical source bytes; the definition MUST NOT embed its
  own self-referential digest.
- **FR-022**: Source serialization MUST produce one canonical human-readable
  UTF-8 representation for semantically equivalent assets, including stable
  ordering of parameters, permutations, stages, payload targets, interfaces,
  attributes, and dependencies.
- **FR-023**: A successful parse-validate-normalize round trip without semantic
  changes MUST reproduce the canonical source bytes exactly on Windows, macOS,
  and Linux.
- **FR-024**: Source parsing MUST reject unsupported schema versions, unknown
  required feature flags, malformed or duplicate fields, invalid text,
  inconsistent counts, arithmetic overflow, truncation, trailing undeclared
  content, dependency-integrity mismatch, and values above configured limits.
- **FR-025**: Unknown optional extension fields under the namespaced
  `extensions` object in an otherwise supported schema MUST be skippable
  without changing the interpreted known fields; unknown ordinary fields,
  required extension fields, or required behavior MUST fail closed.
  Re-serialization MAY omit unknown optional extension fields, is not required
  to preserve them losslessly, and MUST remain canonical for the interpreted
  asset.
- **FR-026**: Default validation limits MUST bound source-definition bytes,
  resolved source/payload dependency bytes, stages, permutations, parameters,
  dependencies, interfaces, text lengths, and instance depth. Callers MAY lower
  or explicitly raise finite limits but MUST NOT disable checked arithmetic or
  bounded traversal.
- **FR-027**: Failed parsing, normalization, serialization, validation,
  dependency extraction, dependency loading, or publication MUST expose no
  partial registry mutation or partially usable asset and MUST release
  request-owned temporary data.
- **FR-028**: Material and shader Asset operations over immutable inputs MUST
  support concurrent independent requests without process-global mutable
  selection state or order-dependent output.
- **FR-029**: Renderer MUST provide conversion adapters from validated shader,
  material, and instance assets to the established Feature 014 runtime
  vocabulary without adding a Renderer dependency to Asset.
- **FR-030**: Shader conversion MUST register an all-or-nothing set of
  compatible runtime shader records and variants; any invalid stage,
  permutation, interface, or payload MUST leave the destination library
  unchanged.
- **FR-031**: Material conversion MUST preserve domain, blend behavior, render
  state, permutation request, effective parameters, and resource requirements;
  converted objects MUST deep-copy all required data, record the source Asset
  identities and versions, and MUST NOT retain borrowed pointers into temporary
  Asset payloads.
- **FR-032**: Asset-backed and equivalent legacy runtime records MUST produce
  matching validation outcomes, selected variant identity, effective
  parameters, resource requirements, deterministic diagnostics, and forward/
  deferred frame-planning inputs.
- **FR-033**: Existing Feature 014 public APIs MUST remain available during
  this feature. Any additive Asset-aware API MUST permit incremental caller
  migration and document ownership, source-version evidence, and explicit
  reconversion behavior. Replacing or invalidating a source Asset MUST NOT
  mutate or automatically invalidate an existing Renderer snapshot; callers
  MUST explicitly convert again to observe a new source version.
- **FR-034**: The repository's 11 GLSL source files and 11 checked-in SPIR-V
  payloads for triangle and deferred rendering MUST be represented by stable
  typed dependencies from stable shader Program Asset identities, with stage,
  entry-point, source, payload, and digest traceability.
- **FR-035**: Repository migration MUST preserve every checked-in SPIR-V byte
  exactly and MUST reject a missing, stale, stage-mismatched, or identity-
  mismatched source/payload pair before native execution.
- **FR-036**: Production Application/Renderer composition paths migrated by
  this feature MUST obtain repository shader payloads through stable shader
  Asset records or their converted runtime records rather than constructing
  direct repository-relative shader paths. Backend MUST receive opaque payloads
  through existing backend-neutral contracts and MUST NOT depend on Asset.
- **FR-037**: Asset inspection MUST provide deterministic human-readable
  summaries for material, instance, shader, stage, permutation, payload target,
  interface, dependency, schema, selection, conversion, and diagnostic state
  without dumping arbitrary shader/payload bytes or native addresses.
- **FR-038**: Diagnostics MUST identify stable asset identity, operation stage,
  result category, schema field or dependency, and actionable reason without
  platform-dependent paths, native addresses, or unstable third-party text in
  normalized output.
- **FR-039**: Focused tests MUST cover valid and invalid schemas, canonical
  round trips, every supported parameter and stage category, inheritance,
  cycles, dependency extraction, payload selection, ambiguity, conversion
  equivalence, rollback, repository migration, limits, deterministic
  repetition, and concurrent immutable requests.
- **FR-040**: Windows, macOS, and Linux automation MUST run focused Asset and
  Renderer material/shader suites, architecture-boundary checks, schema corpus
  validation, repository source/payload verification, strict builds, and the
  existing regression suite.
- **FR-041**: Visual editors, shader graphs, arbitrary runtime shader
  compilation, shader hot reload, source-directory watching, model import,
  manifests, packages, derived-data cache, incremental cooking, runtime
  asynchronous loading, GPU residency, and new graphics backends MUST remain
  outside Feature 023.

### Key Entities

- **Shader Asset**: Immutable CPU-side description of one logical shader
  program, including source records, stages, entry points, permutations,
  interfaces, required parameters, target payloads, identity, version, and
  dependencies. It is the authoritative top-level identity for the complete
  program rather than for one stage or permutation.
- **Shader Source Record**: Source language, stage, entry point, source content
  or typed source reference, and revision evidence used for traceability.
- **Shader Payload Record**: Precompiled opaque bytes tagged by payload format,
  backend, profile, stage, entry point, permutation, and producer evidence.
- **Shader Interface**: Backend-neutral description of material parameters and
  abstract resources required by a shader program or variant.
- **Material Asset**: Stable material definition containing render behavior,
  shader soft reference, permutation request, typed defaults, version, and
  dependencies.
- **Material Instance Asset**: Stable child definition containing one parent
  soft reference and typed overrides, with no process-local parent pointer.
- **Material/Shader Source Definition**: Versioned, deterministic,
  human-readable authoritative representation and rules for parsing,
  normalizing, validating, and loading typed dependencies.
- **Target Profile**: Portable backend and capability vocabulary used to select
  a compatible precompiled payload without exposing native API enums. A
  request supplies an explicit priority-ordered profile list for one backend.
- **Renderer Conversion Adapter**: Renderer-owned boundary that creates the
  established runtime material/shader vocabulary from validated Asset records.
- **Repository Shader Corpus**: Checked-in triangle and deferred source/payload
  pairs migrated to stable shader Asset identities.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A corpus containing at least 12 representative shader source
  definitions, 12 material source definitions, and 16 material-instance source
  definitions completes 20 parse-validate-normalize repetitions with byte-
  identical canonical source output on Windows, macOS, and Linux.
- **SC-002**: Every supported shader stage, material domain, blend behavior,
  parameter category, parent category, and current payload category has at
  least one successful round-trip case and one relevant rejection case.
- **SC-003**: At least 40 malformed or boundary representations covering every
  structural section are rejected atomically, and 100% report the failing asset
  identity when available plus a stable stage and reason category.
- **SC-004**: Equivalent declarations supplied in at least 10 different
  insertion orders produce one canonical byte representation, dependency
  ordering, payload selection, and inspection summary.
- **SC-005**: Eight concurrent readers repeatedly serialize, inspect, select,
  and convert the same immutable corpus without divergent output, data races,
  stale references, or cross-request state leakage.
- **SC-006**: Asset-backed conversions and equivalent Feature 014 runtime
  records agree on 100% of validation results, selected variants, effective
  parameters, resource requirements, and normalized frame-planning summaries
  in the compatibility corpus.
- **SC-007**: All 11 repository GLSL sources and all 11 checked-in SPIR-V
  payloads are mapped to stable identities; 100% of payload bytes and digests
  match their checked-in artifacts after Asset loading.
- **SC-008**: Missing, stale, mismatched, unsupported, and ambiguous shader
  payload cases are detected before execution in 100% of repository migration
  tests, with no silent target fallback.
- **SC-009**: Existing triangle, material/shader, forward, deferred,
  deterministic, and available native validation retain their pre-feature
  observable outcomes after repository shader migration.
- **SC-010**: Architecture validation finds zero Asset production dependencies
  on RHI, Renderer, Application, Backend, Tools, graphics APIs, or platform
  shader compilers; zero Backend dependencies on Asset; and zero direct
  repository shader path construction in migrated production composition
  paths.
- **SC-011**: Focused Feature 023 tests, strict builds, architecture checks,
  repository migration checks, and full regression pass on Windows, macOS, and
  Linux automation.
- **SC-012**: A Feature 024 model importer can refer to material and texture
  assets solely through stable typed identities and metadata dependencies
  without requiring a schema change to Feature 023.

## Assumptions

- Feature 022 is complete and Feature 023 is the first unimplemented roadmap
  phase whose dependencies are satisfied.
- Feature 014 behavior remains the compatibility authority for material
  domains, blend modes, parameter resolution, permutations, shader selection,
  diagnostics, invalidation, and resource requirements.
- Feature 020 identity, metadata, dependency, registry, dispatch, execution-
  lease, and inspection contracts are reused rather than duplicated.
- Feature 021 texture assets are the only concrete texture dependency type in
  this phase; KTX2 remains a derived texture representation and does not change
  a material's logical texture reference.
- The current repository corpus contains 11 GLSL stage sources and 11 matching
  SPIR-V payloads across triangle and deferred rendering.
- `main` is the default entry point for current repository shader stages; the
  schema still records entry points explicitly.
- Source-only shader assets are valid authoring records but are not
  runtime-ready. Runtime execution requires a compatible precompiled payload.
- Unknown optional extension fields under the namespaced `extensions` object
  may be skipped for forward readability; unknown ordinary fields, unsupported
  versions, and unknown required behavior fail closed.
- Material and shader source definitions are the Feature 023 authoring
  authority. Shader source and SPIR-V files remain separate typed dependencies;
  Feature 025 owns later cooked binary publication.
- Concrete finite schema limits and instance-depth defaults will be finalized
  during clarification and planning, while all processing remains bounded.
- Metal, DX12, OpenGL, and GLES payload slots are schema vocabulary only in this
  feature. Producing and executing those payloads belongs to Feature 025 and
  the corresponding backend phases.
- Tests may use in-memory assets, human-readable checked-in definitions, and
  checked-in dependency fixtures. Feature 023 does not require a persistent
  catalog, cooked binary, manifest, package, or runtime Asset Manager.
