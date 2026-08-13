# Feature Specification: Static Mesh & Model Asset Pipeline

**Feature Branch**: `024-static-mesh-model`
**Created**: 2026-07-30
**Status**: In Progress
**Input**: User description: "为 roadmap 下一项制定 spec"

## Clarifications

### Session 2026-07-30

- Q: Feature 024 导入完成后，Canonical Asset 应采用哪套坐标系，并且是否同步修改 Core 的全局坐标约定？ → A: 全工程统一迁移为 Unreal 风格坐标系：左手、Z-up、+X-forward、+Y-right、米制；Core、Asset、Scene 与 Renderer 使用同一约定，迁移是 Feature 024 实现的阻塞前置工作。
- Q: 当 glTF 数组元素插入或重排时，model、mesh、material、image、texture subresource 应如何保持稳定身份？ → A: 优先使用 glTF `extras.stonerAssetId` 中显式、项目定义的稳定键；缺失时回退到 typed structural index，display name 不参与身份。

### Session 2026-07-31

- Q: 当 glTF Material 使用非默认 UV set、sampler wrap/filter、normal scale 或 occlusion strength，而现有 Feature 023 `TextureReference` 无法完整表达 binding 时应如何处理？ → A: 扩展共享的 Material Asset schema，增加包含 Texture 引用、UV set 与 CPU-side sampler intent 的结构化 texture binding；normal scale、occlusion strength 等继续作为明确材质参数。
- Q: 一个 glTF 声明多个 scene 时，静态模型应生成多少个 `FStaticModelAsset`，未被 scene 引用的 mesh 如何处理？ → A: 每个 scene 生成独立 StaticModel subresource并记录 default scene；未引用 mesh 仍独立输出。Skeletal model 不沿用该策略，留待专门 research。
- Q: 静态 mesh 缺少 normal 或 tangent 时应固定补全还是允许 import profile 选择？ → A: 使用 versioned import profile；默认生成 flat normal并在 normal mapping 需要且 UV 有效时生成 tangent，同时提供 require-source strict 模式，并为未来按角度判定 smooth/sharp edge 等策略保留扩展空间。

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Import Canonical Static Geometry (Priority: P1)

An engine developer can import a valid glTF 2.0 or GLB package and receive
immutable static-mesh assets with canonical triangle geometry, vertex streams,
index data, primitive ranges, material slots, and bounds that no longer depend
on the source container's byte layout.

**Why this priority**: Canonical static geometry is the minimum independently
useful output and is the direct prerequisite for model rendering, meshlet
derivation, streaming, and ray-tracing acceleration structures.

**Independent Test**: Import a focused corpus covering JSON and binary
containers, indexed and non-indexed triangles, tightly packed and interleaved
accessors, sparse accessors, supported component encodings, missing optional
attributes, and multiple primitives; then compare normalized mesh summaries
and payload digests over repeated runs.

**Acceptance Scenarios**:

1. **Given** a valid glTF or GLB containing one indexed triangle primitive,
   **When** it is imported, **Then** one static-mesh asset contains the expected
   positions, canonical indices, optional streams, primitive range, material
   slot, and finite local bounds.
2. **Given** a valid non-indexed triangle primitive, **When** it is imported,
   **Then** the output has equivalent canonical indexed topology and uses the
   smallest supported index width that represents every generated index.
3. **Given** equivalent accessor values stored with different valid packing,
   stride, sparse, or normalized-component representations, **When** they are
   imported, **Then** they produce equivalent canonical geometry.
4. **Given** a primitive without normals, **When** it is imported, **Then**
   deterministic flat normals are produced and source tangents are not treated
   as valid without their source normals.
5. **Given** a strict import profile and a primitive missing a required normal
   or tangent stream, **When** it is imported, **Then** import fails before any
   geometry output is published.
6. **Given** identical source bytes, dependencies, import settings, and importer
   version, **When** import is repeated, **Then** output identities, ordering,
   geometry, bounds, dependency records, and version evidence are identical.
7. **Given** a glTF basis probe whose forward, right, and up directions and
   triangle front face are known, **When** it is imported, **Then** positions,
   directions, transforms, tangent handedness, and winding match the canonical
   Unreal coordinate convention.

---

### User Story 2 - Preserve Model Hierarchy and Typed Subresources (Priority: P1)

An engine developer can import a package containing multiple meshes, nodes, and
scenes while preserving local node hierarchy and transforms as model assets
whose model, mesh, material, image, and texture outputs have stable typed
subresource identities.

**Why this priority**: A static mesh alone cannot preserve a model package's
authored assembly. Stable hierarchy and subresources allow later scene,
prefab, cooker, and runtime-manager phases to reference individual outputs
without making display names or source array addresses authoritative.

**Independent Test**: Import packages with multiple scenes, duplicate or empty
display names, shared meshes, repeated node references, nested transforms,
negative scales, and JSON member-order variations; then verify model roots,
topological node order, local transforms, mesh references, typed identities,
and dependency graphs.

**Acceptance Scenarios**:

1. **Given** a valid package with nested nodes and shared mesh references,
   **When** it is imported, **Then** each model preserves its local hierarchy,
   each node references a mesh by typed asset identity, and shared meshes are
   emitted only once.
2. **Given** duplicate, empty, or changed display names with unchanged
   structural indices, **When** the same package is imported, **Then** no
   subresource identity collides or depends on display-name uniqueness.
3. **Given** valid explicit stable keys and source arrays that have been
   inserted into or reordered, **When** equivalent logical outputs are
   reimported, **Then** those outputs retain their typed Asset identities.
4. **Given** no explicit stable keys, **When** a valid package is imported,
   **Then** typed structural indices provide deterministic fallback identities
   and normalized inspection identifies that fallback policy.
5. **Given** multiple declared glTF scenes, **When** the package is imported,
   **Then** each scene produces a distinct model subresource and the declared
   default scene is identified without creating Application entities.
6. **Given** matrix and transform forms that express the same valid local
   transform, **When** they are imported, **Then** the model exposes one
   equivalent Core-space local matrix representation.
7. **Given** a cycle, multiple parents within one model hierarchy, invalid node
   reference, or non-finite transform, **When** import is attempted, **Then**
   the complete package fails before any output is published.

---

### User Story 3 - Import Material and Texture Dependencies (Priority: P2)

An engine developer can import core glTF metallic-roughness materials and their
PNG or JPEG images into the existing material, image, and texture asset
contracts, with texture usage, color-space intent, alpha behavior, and material
slots preserved as typed dependencies.

**Why this priority**: Geometry that loses material and texture meaning is not
a usable model asset. Reusing Features 021 and 023 also proves that the Asset
layer composes instead of creating a second model-specific material system.

**Independent Test**: Import packages covering the core metallic-roughness
factors and textures, normal, occlusion and emissive textures, alpha modes,
double-sided state, missing materials, embedded and external images, shared
images with different usage semantics, and invalid dependencies; then inspect
the emitted assets and dependency roles.

**Acceptance Scenarios**:

1. **Given** a core metallic-roughness material, **When** it is imported with a
   compatible material mapping profile, **Then** the output material asset
   carries equivalent factors, blend behavior, two-sided state, shader
   identity, structured texture bindings, scalar parameters, and dependency
   records.
2. **Given** an image used for base color or emissive data, **When** it is
   imported, **Then** the texture records color data with sRGB intent while
   alpha coverage remains linear.
3. **Given** an image used for normal, metallic-roughness, or occlusion data,
   **When** it is imported, **Then** the texture records linear data semantics
   and is never silently treated as color data.
4. **Given** one source image used under incompatible texture semantics,
   **When** the package is imported, **Then** distinct typed texture outputs
   share the image dependency but retain independent semantic identities.
5. **Given** a primitive with no material, **When** it is imported, **Then** it
   references one deterministic package-local default material that follows
   core glTF default-material behavior.
6. **Given** a missing external image, unsupported image encoding, incompatible
   material mapping, or texture coordinate set absent from the primitive,
   **When** import is attempted, **Then** the package fails atomically with a
   diagnostic identifying the dependency and material use.
7. **Given** a material binding that selects `TEXCOORD_1` and non-default
   wrap/filter behavior, **When** it is imported and serialized, **Then** the
   shared Material Asset preserves the typed Texture reference, UV set, and
   sampler intent without changing the Texture Asset identity.

---

### User Story 4 - Realize Mesh Assets Through Renderer and RHI (Priority: P2)

An engine developer can ask Renderer to convert a validated static-mesh asset
into an immutable draw-ready snapshot and RHI vertex/index resources while
preserving primitive ranges, layout, material slots, source versions, and
all-or-nothing failure behavior.

**Why this priority**: The asset is not proven useful until existing rendering
paths can consume it, but GPU ownership must remain outside Asset and behind
the established Renderer-to-RHI boundary.

**Independent Test**: Convert representative mesh assets through mock RHI
devices covering each supported stream and index width, inspect uploaded bytes
and draw sections, inject failure at each allocation/upload stage, and compare
source manifests and cleanup behavior.

**Acceptance Scenarios**:

1. **Given** a valid static-mesh asset and active RHI device, **When** Renderer
   realizes it, **Then** the result owns draw-ready vertex and index resources,
   one section per primitive, the required vertex layout, material slot
   identities, bounds, and complete source-version evidence.
2. **Given** a mesh with 16-bit or 32-bit canonical indices, **When** it is
   realized, **Then** the resulting section uses the matching RHI index type
   and exact topology.
3. **Given** an allocation or upload failure after earlier resources were
   created, **When** realization returns, **Then** no partial snapshot remains
   usable and all request-owned resources are released.
4. **Given** a source mesh or dependency replaced after realization, **When**
   the existing snapshot is inspected, **Then** it remains a self-contained
   view of its recorded versions until explicit reconversion.
5. **Given** only the Asset module, **When** its dependency boundary is
   inspected, **Then** no RHI, Renderer, Backend, Application, graphics API, or
   native GPU type is reachable from mesh/model asset contracts.

---

### User Story 5 - Diagnose Unsupported and Malformed Packages (Priority: P3)

An engine maintainer can distinguish malformed data, unsupported glTF content,
missing dependencies, limit violations, and Renderer realization failures
through deterministic diagnostics without observing partial registry state or
third-party-library error leakage.

**Why this priority**: Model files are untrusted, nested, and size-rich inputs.
Predictable failure evidence is required before the importer can become the
foundation for the cooker and runtime asset manager.

**Independent Test**: Mutate every source section, offset, count, URI,
extension declaration, hierarchy edge, accessor, primitive, material, and
embedded resource boundary; verify normalized failure category, subject,
location, output preservation, and cross-platform consistency. Inject Renderer
realization failure at validation, allocation, upload, and finalize stages and
verify normalized diagnostics plus complete request-owned resource cleanup.

**Acceptance Scenarios**:

1. **Given** a malformed JSON document, GLB header/chunk table, accessor range,
   sparse patch, or index, **When** import is attempted, **Then** it fails
   before out-of-range access or partial output publication.
2. **Given** a supported-version package requiring an unsupported extension,
   primitive topology, skin, morph target, or compressed geometry feature,
   **When** import is attempted, **Then** it fails closed with the unsupported
   feature named.
3. **Given** an unknown optional extension or excluded camera, light, or
   animation that does not alter imported static geometry, **When** import
   succeeds, **Then** that content produces no runtime asset and the normalized
   inspection report records that it was skipped.
4. **Given** a failed import with pre-existing caller output and registry
   state, **When** the operation returns, **Then** both remain unchanged.
5. **Given** the malformed corpus on Windows, macOS, and Linux, **When** the
   focused validation runs, **Then** result categories and normalized
   diagnostics match apart from explicitly normalized platform evidence.
6. **Given** a validated mesh asset and a Renderer realization failure during
   validation, allocation, upload, or finalize, **When** realization returns,
   **Then** the normalized diagnostic identifies the stable Asset subject,
   failing stage, result category, and actionable reason, the destination
   snapshot remains unchanged, and every request-owned RHI resource is
   released.

### Edge Cases

- The source is empty, truncated, invalid UTF-8, valid JSON with duplicate
  keys, excessive nesting, trailing non-whitespace data, or a valid document
  whose declared asset version is not supported.
- A GLB has the wrong magic/version/length, missing or reordered chunks,
  duplicate JSON or BIN chunks, misaligned chunk lengths, invalid padding, or
  bytes outside the declared container length.
- A relative resource URI is empty, percent-encoded ambiguously, escapes the
  source scope, uses an unsupported scheme, resolves to itself recursively, or
  aliases another dependency after canonicalization.
- A data URI is malformed, declares a mismatched media type, exceeds limits, or
  decodes to bytes inconsistent with its declared buffer or image length.
- Buffer, buffer-view, accessor, sparse-index, or sparse-value arithmetic
  overflows; alignment, stride, offset, count, or component type is invalid; or
  two ranges overlap in a way the format forbids.
- Attribute accessors for one primitive disagree on vertex count, contain
  non-finite values, have invalid normalized encodings, or use unsupported
  custom, color, joint, or weight semantics.
- Position bounds are missing, non-finite, reversed, inconsistent with decoded
  positions, or collapse to a point, line, or plane.
- An index is out of range, the count is not divisible by three, the primitive
  is empty or degenerate, or promotion to a supported index width would exceed
  configured limits.
- Normals or tangents are zero-length, non-finite, not normalizable, or contain
  invalid tangent handedness; normal mapping requires a UV set that is absent.
- A missing-attribute policy is unknown, unsupported by the importer revision,
  or changes without changing version evidence.
- A negative-determinant node transform reverses front-face parity; a singular
  transform prevents a valid normal transform; or matrix and transform fields
  are both present.
- Existing Core, Scene, Renderer, shader, fixture, or validation code still
  assumes the former right-handed convention after the coordinate migration.
- Nodes form a cycle, have multiple parents in one model, reference themselves,
  are unreachable from one scene, share a mesh, or appear in more than one
  scene.
- Multiple meshes, nodes, materials, textures, images, or scenes have identical
  or empty display names.
- An explicit `extras.stonerAssetId` is empty, non-canonical, over-limit,
  duplicated for the same output type, or changes between source revisions.
- A primitive omits a material, several primitives share one material, or one
  image is reused as both color and non-color data.
- A material references an unavailable texture coordinate set, unsupported
  sampler behavior, unsupported required extension, or values outside the
  accepted core glTF range.
- An import emits duplicate typed identities, dependencies with conflicting
  versions, a payload whose runtime type disagrees with its identity, or a
  model reference to an output absent from the same transaction.
- Source resolution, image decoding, geometry normalization, material mapping,
  validation, dependency extraction, registry publication, RHI allocation, or
  upload fails after earlier stages succeeded.
- Two imports run concurrently with the same source and settings, or with
  different settings that must produce distinct version evidence without
  changing logical identity.

## Architecture & Design Constraints *(mandatory)*

- **Asset Boundary**: Static mesh, model, importer, source-format validation,
  hierarchy, material mapping, and dependency code owned by Asset MUST depend
  only on Core and existing Asset contracts. Asset MUST NOT include or link
  RHI, Renderer, Application, Backend, Tools, a graphics API, or native window
  or GPU types.
- **Renderer Ownership**: Translation from immutable mesh assets into vertex
  layouts, RHI buffers, upload operations, draw sections, and GPU lifetimes
  MUST be owned by Renderer. Asset MUST NOT create or retain live GPU objects.
- **Backend Boundary**: Backend MUST continue to depend only on Core and RHI.
  It MUST NOT parse model files, query the Asset Registry, or understand glTF
  identities.
- **RHI Abstraction**: Renderer realization MUST use public RHI resource and
  upload contracts without calling a graphics API directly or embedding native
  handles in Asset-facing records.
- **Stable Identity**: Model, mesh, material, image, and texture outputs MUST
  use Feature 020 typed identities. Display names, array addresses, source
  offsets, filesystem paths, object pointers, and content hashes MUST NOT
  replace logical identity.
- **Multi-Output Atomicity**: A package import MUST be validated as one
  dependency-consistent output set. Callers and registry readers MUST never
  observe only a subset of outputs from a failed package import.
- **Existing Asset Reuse**: Images and textures MUST use Feature 021 payloads;
  materials and shaders MUST use Feature 023 payloads and typed references.
  Feature 024 MUST NOT introduce model-private substitutes for those schemas.
- **Shared Material Evolution**: The structured texture-binding addition MUST
  evolve the shared Feature 023 Material Asset schema and its versioned source
  representation. It MUST remain usable by non-glTF callers and MUST NOT expose
  glTF, RHI, Renderer, or native sampler types.
- **Coordinate Contract**: Imported geometry, directions, tangent handedness,
  transforms, units, UV origin, bounds, and front-face parity MUST be
  normalized to the project-wide Unreal convention before publication.
  Source-format interpretation MUST NOT leak into Renderer call sites.
- **Global Coordinate Migration**: Core, Asset, Scene, Renderer, repository
  shaders, fixtures, and validation MUST share one left-handed, Z-up,
  positive-X-forward, positive-Y-right, meter-based convention. The former
  right-handed Core convention MUST be amended explicitly and all affected
  behavior migrated before Feature 024 geometry import is considered usable;
  mixed-convention operation is not supported.
- **Immutable Payloads**: Published mesh and model payloads and Renderer
  conversion snapshots MUST be immutable, self-contained for their documented
  lifetime, and stamped with the source versions used to produce them.
- **Bounded Input**: JSON, GLB chunks, decoded dependencies, hierarchy depth,
  counts, aggregate geometry, text, and diagnostics MUST have finite,
  configurable limits checked before allocation or traversal.
- **Deterministic Ordering**: Outputs, streams, primitives, slots, nodes,
  roots, dependencies, diagnostics, and inspection summaries MUST use defined
  semantic ordering independent of hash-table, registration, filesystem, or
  thread scheduling order.
- **Versioned Geometry Policy**: Missing-normal and missing-tangent behavior
  MUST be selected by an explicit versioned import profile. The initial policy
  contract MUST permit later deterministic normal strategies, including
  angle-threshold smooth/sharp edge classification, without changing the
  canonical mesh payload schema or introducing Renderer-side source repair.
- **Responsibility Separation**: Container parsing, URI resolution, accessor
  decoding, geometry normalization, hierarchy validation, material mapping,
  dependency extraction, Renderer realization, and inspection MUST remain
  independently testable responsibilities rather than one importer God-class.
- **Source/Cooked Separation**: This feature owns source import and canonical
  CPU payloads only. It MUST NOT introduce manifests, packages, derived-data
  keys, persistent caches, incremental cooking, or runtime request management;
  those belong to Features 025 and 026.
- **Future Importers**: Static mesh and model payload contracts MUST not expose
  glTF-specific parser types. Later OBJ, FBX, USD, or procedural importers MUST
  be able to emit the same canonical assets through Feature 020 importer
  registration without replacing the glTF importer.
- **Advanced Graphics Readiness**: Mesh payloads MUST retain topology, vertex
  semantics, primitive/material boundaries, transforms, and bounds needed by
  later meshlet, streaming, BLAS, and GI derived-data stages without generating
  those products here.
- **No Scene Ownership**: Model hierarchy is Asset data only. This feature MUST
  NOT create ECS entities, mutate a world, define prefab serialization, or own
  scene-component lifetimes.
- **No Skeletal Ownership**: Skins, joints, weights, morph targets, and
  animations MUST NOT be silently flattened into static output. Support for
  those domains requires later dedicated asset phases, and skeletal
  model/scene packaging MUST be selected through dedicated research rather than
  inheriting the static one-model-per-scene policy automatically.
- **Naming Conventions**: Public code design MUST follow PascalCase and Unreal
  Engine-style `F`, `I`, `E`, and `T` naming conventions.
- **Cross-Platform Compatibility**: Import, normalized payloads, diagnostics,
  identity, and mock realization MUST behave consistently on Windows, macOS,
  and Linux. Host endianness, path syntax, locale, and native structure layout
  MUST NOT change the result.
- **Automated Cross-Platform Validation**: Windows, macOS, and Linux automation
  MUST build and run focused mesh/model import tests, malformed corpus tests,
  architecture checks, deterministic comparisons, Renderer mock-RHI
  realization, and the existing regression suite.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST provide immutable `FStaticMeshAsset` and
  `FStaticModelAsset` concepts whose typed identity, version evidence,
  metadata, dependencies, and CPU payload remain separate.
- **FR-002**: Static mesh and model identities MUST use distinct Feature 020
  Asset types and canonical logical paths with optional subresources; typed
  references MUST reject a model identity used as a mesh or vice versa.
- **FR-003**: One source import MUST be able to emit zero or more model, mesh,
  material, image, and texture outputs through the existing multi-output import
  contract and MUST validate the complete set before publication.
- **FR-004**: Subresource identities MUST be derived from the canonical source
  identity, output type, and either an explicit stable source key or a
  deterministic structural-index fallback. A non-empty
  `extras.stonerAssetId` MUST be used as the stable key when present; display
  names MUST remain metadata and MUST NOT participate in identity. Explicit
  keys MUST follow Feature 020 canonical subresource grammar and size limits
  and be unique within one output type; invalid or duplicate keys MUST fail the
  package. When no key is present, identity MUST use a typed source structural
  index and inspection MUST report that fallback; array insertion or reordering
  MAY change fallback identities.
- **FR-005**: For identical source identity and structure, changing source
  content, resolved dependency bytes, import settings, or importer revision
  MUST update version evidence without changing logical subresource identity.
- **FR-006**: Repeated imports of identical source and settings MUST emit
  outputs, dependencies, diagnostics, and inspection summaries in the same
  semantic order on all supported platforms.
- **FR-007**: Every emitted output MUST declare its required source and runtime
  dependencies with stable roles, target types, and resolution state.
- **FR-008**: A failed import or registration MUST leave caller outputs and
  registry state unchanged; no mesh, model, material, image, or texture from
  the failed package may remain observable.
- **FR-009**: Import settings and all source/dependency evidence that affect
  normalized payloads MUST participate in version derivation and normalized
  inspection.
- **FR-010**: The importer MUST recognize core glTF 2.0 JSON documents and GLB
  2.0 binary containers through deterministic extension hints plus bounded
  content probing, and MUST reject ambiguous or mismatched declarations.
- **FR-011**: The importer MUST validate glTF JSON structure, required fields,
  numeric domains, references, buffer layouts, accessors, scenes, nodes,
  meshes, materials, textures, images, and container rules before publishing
  any output.
- **FR-012**: The importer MUST support GLB-embedded buffers and images, glTF
  data URIs, and source-relative external buffers plus PNG/JPEG images through
  the existing resolver/reader boundary. It MUST NOT perform implicit network
  access or bypass the resolver with host filesystem calls.
- **FR-013**: Dependency locators MUST be canonicalized relative to the source
  scope and MUST reject unsupported schemes, scope escape, recursive
  self-resolution, ambiguous percent encoding, and alias collisions before
  bytes are consumed.
- **FR-014**: GLB validation MUST cover header identity, version, declared
  length, chunk order, chunk uniqueness, alignment, padding, and trailing data.
- **FR-015**: Packages whose required glTF version or required extension is not
  supported MUST fail closed. Unknown optional extensions MAY be skipped only
  when they do not change an imported static result and MUST be reported in
  normalized inspection.
- **FR-016**: Source bytes, dependency bytes, decoded text, nesting, object and
  array counts, buffer ranges, accessors, vertices, indices, primitives,
  materials, textures, images, nodes, scenes, hierarchy depth, and diagnostics
  MUST be checked against finite configurable limits before unbounded work or
  allocation.
- **FR-017**: Cameras, punctual lights, and animations MAY be ignored with
  inspection evidence because they do not become outputs in this feature.
  Skins, morph targets, geometry compression, or other unsupported features
  that change selected static geometry MUST cause the affected package import
  to fail rather than silently changing its rendered meaning.
- **FR-018**: A static mesh asset MUST contain one or more canonical triangle
  primitives, shared or per-primitive vertex streams, canonical index data,
  material slots, local bounds, source evidence, and deterministic inspection
  data.
- **FR-019**: Initial geometry support MUST accept only glTF `TRIANGLES`
  primitives. Points, lines, line strips/loops, triangle strips, and triangle
  fans MUST be rejected rather than converted implicitly.
- **FR-020**: Every imported primitive MUST contain a finite `POSITION` stream
  with one value per vertex. Missing, malformed, or non-finite positions MUST
  fail the package.
- **FR-021**: The canonical mesh MUST support position, normal, tangent, and at
  least two texture-coordinate sets. Unsupported or application-specific
  vertex semantics MUST not enter the canonical layout silently.
- **FR-022**: Unsigned 8-bit, 16-bit, and 32-bit glTF indices MUST be range
  checked and normalized to supported 16-bit or 32-bit canonical index data;
  non-indexed triangles MUST receive deterministic sequential indices using
  the smallest supported width.
- **FR-023**: Accessor decoding MUST support valid tightly packed, interleaved,
  normalized-integer, and sparse representations used by in-scope attributes,
  while enforcing component type, element type, count, alignment, stride,
  offset, range, and sparse-index ordering rules.
- **FR-024**: If normals are absent, the importer MUST generate deterministic
  flat normals and disregard source tangents under the default import policy.
  Under the strict policy, missing required normals MUST fail import. If normals
  are present, they MUST be finite and normalizable before canonical
  publication. A primitive whose missing normals cannot be derived because any
  referenced vertex has no non-degenerate incident triangle MUST fail rather
  than receive an arbitrary normal.
- **FR-025**: If tangents required by a normal-mapped material are absent, the
  default policy MUST generate a deterministic glTF-compatible tangent basis
  only when the referenced normal and texture-coordinate data are valid. The
  strict policy MUST reject missing required tangents. Either policy MUST fail
  instead of inventing an unrelated basis when required inputs are invalid.
- **FR-026**: Canonical tangents MUST contain a finite normalized direction and
  a handedness value of positive or negative one; their meaning MUST remain
  consistent with the canonical normal and UV conventions.
- **FR-027**: Canonical Core, mesh, model, Scene, and Renderer space MUST use the
  Unreal convention: left-handed, Z-up, positive X forward, positive Y right,
  meters, and clockwise front faces when viewed from the front for
  positive-determinant transforms.
- **FR-028**: glTF positions, directions, tangent frames, winding, and
  column-major node transforms MUST be converted from glTF right-handed,
  Y-up, positive-Z-forward space into equivalent canonical Unreal-space Core
  row-major matrix value semantics. The conversion MUST map glTF positive Z to
  canonical positive X, glTF negative X to canonical positive Y, and glTF
  positive Y to canonical positive Z; reflection and negative-determinant
  parity MUST update winding and tangent handedness exactly once.
- **FR-029**: Texture coordinates MUST retain the top-left-origin convention
  established by Feature 021. The mesh importer and Renderer adapter MUST NOT
  apply an additional format-specific vertical flip.
- **FR-030**: Primitive, mesh, and model bounds MUST be finite, enclose decoded
  canonical positions after conversion, and be derived or validated
  deterministically rather than trusting inconsistent declared bounds.
- **FR-031**: A model asset MUST contain deterministic root and node records,
  each with a stable node key, optional display name, canonical local matrix,
  ordered children, and an optional typed mesh reference.
- **FR-032**: Model hierarchy validation MUST reject cycles, self-parenting,
  invalid children, duplicate child edges, multiple parents within one model,
  non-finite transforms, and depth or node counts beyond configured limits.
- **FR-033**: Every declared glTF scene MUST produce a distinct model
  subresource; the package result MUST identify the declared default scene.
  Meshes not reachable from a scene MAY still be emitted as mesh assets but
  MUST NOT be attached to a fabricated model. This rule applies only to static
  models and MUST NOT establish skeletal model packaging policy.
- **FR-034**: Shared mesh, material, image, and texture definitions MUST be
  emitted once per typed source subresource and referenced from every consuming
  model node or primitive without payload duplication.
- **FR-035**: Primitive order and material-slot order MUST be deterministic and
  preserve source semantic order. Each primitive MUST reference exactly one
  typed material asset, including a package-local default when omitted.
- **FR-036**: Core glTF metallic-roughness materials MUST map to Feature 023
  material assets, including base-color, metallic, roughness, normal,
  occlusion, emissive, alpha-mode/cutoff, and double-sided behavior that the
  selected material schema can represent.
- **FR-037**: Material conversion MUST use an explicit versioned mapping profile
  that supplies the target surface shader identity and parameter mapping. It
  MUST NOT rely on a physical shader path, registration order, or hidden
  process-global default.
- **FR-038**: glTF images and textures MUST use Feature 021 image/texture
  payloads. Base-color and emissive uses MUST record color semantics; normal,
  metallic-roughness, and occlusion uses MUST record linear data semantics.
- **FR-039**: A source image used under incompatible semantics MUST yield
  separate texture subresources with one shared image dependency. Texture
  identity and version evidence MUST distinguish the semantic use.
- **FR-040**: The importer MUST support core glTF PNG and JPEG image payloads
  through Feature 021. KTX2/Basis glTF extensions, WebP, and other image
  extensions are outside this feature and MUST fail when required.
- **FR-041**: The shared Feature 023 Material Asset schema MUST provide a
  structured texture-binding value containing a typed Texture Asset reference,
  texture-coordinate set, and backend-neutral CPU sampler intent covering core
  glTF minification, magnification, mip filtering, and U/V wrap behavior.
  Normal scale, occlusion strength, and comparable binding factors MUST remain
  explicit typed material parameters. UV/sampler state MUST NOT alter or
  duplicate Texture Asset identity.
- **FR-042**: A primitive without a declared material MUST reference one
  deterministic package-local material asset representing core glTF default
  material values under the active material mapping profile.
- **FR-043**: Missing, type-incompatible, or invalid external buffers, images,
  textures, materials, shaders, or mapping-profile dependencies MUST fail the
  entire package import before registry publication.
- **FR-044**: Renderer MUST validate the complete mesh asset and every required
  material dependency before allocating RHI resources.
- **FR-045**: Successful mesh realization MUST produce an immutable,
  self-contained snapshot containing owned RHI vertex/index resources,
  canonical vertex layout, primitive draw sections, material identities,
  bounds, and a normalized manifest of all source Asset IDs and versions.
- **FR-046**: Renderer MUST preserve canonical 16-bit or 32-bit index width,
  triangle topology, stream values, primitive ranges, base offsets, and
  material-slot associations when producing RHI resources and draw sections.
- **FR-047**: Renderer realization MUST be transactional. Any validation,
  allocation, upload, device, or dependency failure MUST release all
  request-owned resources and leave the destination snapshot unchanged.
- **FR-048**: Realized snapshots MUST deep-own data required after conversion,
  MUST NOT retain borrowed Asset payload pointers, and MUST remain bound to
  recorded source versions until explicit reconversion.
- **FR-049**: Asset replacement MUST affect only subsequent imports or explicit
  reconversions; existing snapshots MUST NOT subscribe to registry changes or
  update in place.
- **FR-050**: Mesh/model Asset public headers and link dependencies MUST remain
  free of RHI, Renderer, Backend, Application, graphics API, parser-library,
  and native handle types.
- **FR-051**: Import and realization diagnostics MUST identify stable Asset
  subject, stage, normalized source section or structural index, result
  category, and actionable reason without exposing machine-specific absolute
  paths or raw third-party error text as the public contract.
- **FR-052**: The feature MUST provide deterministic inspection for package
  outputs, mesh streams/primitives/bounds, model roots/nodes, material slots,
  dependencies, skipped optional content, source versions, and realization
  summaries.
- **FR-053**: Validation MUST include Khronos-valid in-scope fixtures plus
  repository-owned malformed variants covering every input class, boundary,
  failure stage, and atomic rollback path described by this specification.
- **FR-054**: Fixture provenance, license, expected validity, included feature
  coverage, and any deliberate mutation MUST be recorded so validation inputs
  remain auditable and redistributable.
- **FR-055**: Windows, macOS, and Linux automation MUST compile the feature and
  run focused Asset, malformed-input, deterministic, architecture, Renderer
  mock-RHI, and regression validation.
- **FR-056**: The canonical mesh/model contracts and import registration MUST
  allow later OBJ, FBX, USD, and procedural importers to emit equivalent
  payloads without exposing glTF parser state or changing existing Asset
  identities.
- **FR-057**: The feature MUST NOT create ECS entities, prefabs, skeletal or
  animation assets, meshlets, BLAS data, cooked packages, manifests, persistent
  caches, asynchronous request management, streaming chunks, or GPU residency
  policy.
- **FR-058**: Before canonical model import is accepted, the project MUST
  migrate the former Feature 004 right-handed convention and every
  coordinate-sensitive Core, Scene, Renderer, shader, demo, fixture, and
  validation consumer to the canonical Unreal convention, with no compatibility
  mode that allows both conventions in one process.
- **FR-059**: The versioned model import profile MUST expose at least a default
  compatible missing-attribute policy and a require-source strict policy.
  Policy identity and settings MUST participate in Asset version evidence.
  The policy contract MUST allow later angle-threshold smooth/sharp edge or
  other deterministic generation strategies without treating them as
  implemented by Feature 024.

### Key Entities

- **Static Mesh Asset**: Immutable canonical CPU geometry with typed identity,
  version evidence, streams, triangle primitives, indices, material slots,
  bounds, dependencies, and inspection data.
- **Static Mesh Primitive**: One triangle-list draw section with an index range,
  vertex-stream contract, material slot, local bounds, and source structural
  evidence.
- **Vertex Stream**: Canonical values for one supported semantic such as
  position, normal, tangent, or a texture-coordinate set.
- **Material Slot**: Stable primitive-to-material association represented by a
  typed soft reference rather than a live Renderer material.
- **Material Texture Binding**: Shared Material Asset value containing a typed
  Texture reference, UV set, and backend-neutral sampler intent; related
  numeric factors remain explicit material parameters.
- **Static Model Asset**: Immutable package assembly containing model roots,
  local node hierarchy, transforms, typed mesh references, bounds, and
  dependencies without owning ECS entities.
- **Model Node**: Stable-keyed or structural-fallback node record with display
  metadata, canonical local matrix, ordered children, and optional mesh
  reference.
- **Stable Source Key**: Optional project-defined
  `extras.stonerAssetId` value that preserves a typed subresource identity
  across source-array insertion and reordering without making a display name or
  content digest authoritative.
- **Model Import Profile**: Versioned import policy defining limits, geometry
  normalization, missing-attribute policy, material mapping, default
  shader/material behavior, and all settings that affect output version
  evidence.
- **Package Output Set**: Complete ordered collection of model, mesh, material,
  image, and texture outputs that validates and publishes atomically.
- **Mesh Realization Snapshot**: Renderer-owned immutable result containing RHI
  resources, draw sections, source-version manifest, and no borrowed Asset
  payload state.
- **Import Diagnostic**: Stable stage, subject, source location/index, category,
  and reason for an import, dependency, normalization, validation, publication,
  or realization outcome.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: One in-scope validation corpus of at least 20 valid glTF/GLB
  packages covers both containers, indexed/non-indexed geometry, every
  supported accessor representation, hierarchy, multiple primitives/scenes,
  core materials, and embedded/external resources; 100% import successfully on
  Windows, macOS, and Linux.
- **SC-002**: Twenty repeated imports of every valid fixture produce identical
  ordered Asset IDs, version evidence, payload digests, bounds within documented
  tolerance, dependency records, diagnostics, and inspection summaries.
- **SC-003**: A malformed and unsupported corpus of at least 40 targeted
  mutations achieves 100% expected rejection with no crash, out-of-bounds
  access, partial output, or registry mutation.
- **SC-004**: Golden geometry probes across at least 12 representative
  primitives confirm every decoded position, normal, tangent, UV, index,
  transform, front-face parity, primitive bound, and model bound within the
  documented cross-platform tolerance. CPU floating-point comparisons MUST use
  `abs(actual - expected) <= 1e-5 + 1e-5 * abs(expected)` per component;
  indices and front-face parity MUST match exactly, and tangent handedness MUST
  be exactly positive or negative one.
- **SC-005**: Every core metallic-roughness field and texture role supported by
  this feature has at least one acceptance fixture, and 100% of emitted
  materials/textures match the expected typed parameters, semantic/color-space
  intent, alpha behavior, structured UV/sampler binding, dependencies, and slot
  assignment.
- **SC-006**: Renderer mock-RHI validation covers every supported canonical
  stream layout and index width plus injected failure at every allocation and
  upload stage; all success bytes and draw sections match the Asset payload and
  all failures leave zero partial usable resources.
- **SC-007**: Eight concurrent independent imports of the same representative
  package produce the same normalized result as serial import with no shared
  mutable payload, nondeterministic identity, or diagnostic-order difference.
- **SC-008**: A representative package with at least 100,000 vertices, 300,000
  indices, 16 primitives, and 16 material/texture dependencies imports and
  produces an inspectable canonical output on the documented Apple M4 Pro
  macOS Release reference profile. After one unmeasured warm-up, each of five
  independent measured imports MUST complete within 5 seconds, and tracked
  request-owned peak bytes MUST remain at or below the active import profile's
  configured aggregate allocation limit.
- **SC-009**: Architecture validation reports zero Asset dependency violations,
  zero graphics/native types in Asset public contracts, and zero direct
  source-format or filesystem access from Renderer/Backend.
- **SC-010**: All existing focused and regression suites plus new Feature 024
  suites pass in Debug and strict Release configurations on Windows, macOS, and
  Linux, with the project's required sanitizer gates passing where supported.
- **SC-011**: Every accepted static-mesh fixture can be realized into a
  draw-ready Renderer snapshot without re-reading its original glTF, GLB,
  external buffer, or image source.
- **SC-012**: Coordinate migration probes covering basis axes, cross products,
  quaternion rotations, transform composition, camera/view behavior, normal
  transforms, culling, winding, negative scales, and existing demo geometry
  pass under one Unreal convention with zero remaining tests or public
  documentation asserting the former right-handed convention.
- **SC-013**: Reordering or inserting source-array entries in at least one
  multi-output fixture preserves 100% of identities carrying valid explicit
  stable keys, while the corresponding no-key fixture produces deterministic
  documented structural-index fallback identities.
- **SC-014**: Default and strict missing-attribute fixtures produce their
  specified success or rejection outcomes on all supported platforms, changing
  the selected policy changes version evidence, and unsupported future policy
  identities fail without silently using the default.

## Assumptions

- Feature 024 targets core glTF 2.0 and GLB 2.0. No glTF extension is required
  for the initial supported path; unsupported required extensions fail closed.
- Canonical engine and asset space uses Unreal's left-handed, Z-up,
  positive-X-forward, positive-Y-right, meter-based convention. Feature 024
  includes the blocking migration from the former Core right-handed convention.
- Texture coordinates already match Feature 021's top-left origin and are not
  vertically flipped during mesh import or realization.
- Initial primitives are triangle lists only. Triangle strips and fans are not
  triangulated automatically in this feature.
- The default missing-attribute policy uses deterministic flat-normal
  generation and generates tangents only when a normal-mapped material needs
  them and the required normal/UV data is valid. The strict policy requires
  needed source streams. Angle-threshold smooth/sharp edge generation is a
  future policy, not a Feature 024 deliverable.
- The canonical mesh supports `TEXCOORD_0` and `TEXCOORD_1`; additional UV sets,
  vertex colors, custom attributes, joints, and weights are outside the initial
  layout.
- Every declared glTF scene becomes a model subresource; the importer does not
  synthesize Application scene objects or a model for meshes unreachable from
  all declared scenes.
- Skeletal model scene, skeleton-root, shared-skin, and subresource policy is
  intentionally deferred to a later dedicated research phase and does not
  inherit Feature 024's static model policy by default.
- Exporters may preserve project-defined `extras.stonerAssetId` values. Sources
  without them remain supported through deterministic typed structural-index
  fallback, with the documented limitation that source-array edits may change
  fallback identities.
- Core metallic-roughness material mapping requires an explicit versioned
  profile and a compatible Feature 023 surface shader identity. It does not
  compile shaders or infer a shader from a host path.
- Core glTF PNG/JPEG images are imported through Feature 021. KTX2/Basis glTF
  extensions are not included despite Feature 022 existing, because Feature
  024's roadmap dependency and source-format scope are deliberately core-only.
- External dependencies are local resolver-scoped relative resources or data
  URIs. Network retrieval and unrestricted absolute filesystem paths are out of
  scope.
- Feature 020 provides canonical identity, resolver/importer dispatch,
  multi-output validation, registry batches, typed soft references,
  diagnostics, and version evidence.
- Feature 004 provides the Core vectors, matrices, transforms, and bounds used
  by canonical mesh and model payloads; its former right-handed behavioral
  contract requires an explicit migration amendment under Feature 024.
- Features 021 and 023 remain the authority for image/texture and
  material/shader payload validation. Feature 024 extends the shared Material
  Asset schema with backend-neutral structured texture bindings but does not
  duplicate those asset systems or move sampler state into Texture identity.
- Feature 008 provides the RHI buffer and upload contracts used by the
  Renderer-owned realization path.
- Fixture licenses permit repository use and redistribution; provenance is
  recorded before fixtures become required validation inputs.
- Asset cooking, manifests, derived-data cache, and offline package production
  begin in Feature 025; asynchronous runtime loading begins in Feature 026.
