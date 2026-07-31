# Research: Static Mesh & Model Pipeline

## Decision 1: Migrate the whole active engine to an Unreal-style convention

**Decision**: Feature 024 begins with a blocking migration to
`UnrealLH_ZUp_XForward_YRight_Meters_CW`: left-handed world interpretation,
+X forward, +Y right, +Z up, meters, clockwise front faces, and positive
rotations that follow the existing quaternion component algebra. This is one
active convention, not a runtime switch.

The existing component cross-product implementation remains unchanged:
`UnitX.Cross(UnitY) == UnitZ`. Hamilton quaternion multiplication,
`q * v * q^-1`, row-major matrix storage, column-vector multiplication, and
scale-rotate-translate composition also remain unchanged. In the chosen
physical axis interpretation these component formulas produce the expected
Unreal-style basis and positive yaw. Reversing `Cross()` or replacing
quaternion algebra would create needless API and numerical churn.

World camera forward changes to +X. A centralized view/projection builder maps
that world convention to the Renderer view-depth and `[0,1]` clip-depth
contracts. RHI's canonical front face becomes clockwise. Backends remain
responsible only for API-specific viewport and raster-state adaptation.

**Rationale**: Feature 024 must convert glTF geometry once into the engine's
canonical space. A full project convention avoids permanent per-layer basis
adapters and matches the requested Unreal-style engine direction. Epic
documents Unreal as left-handed, Z-up, +X-forward, and +Y-right:
[Unreal coordinate system](https://dev.epicgames.com/documentation/en-us/unreal-engine/coordinate-system-and-spaces-in-unreal-engine).

**Alternatives considered**:

- Convert only imported assets while preserving right-handed Core: rejected
  because Scene, Renderer, authoring data, and future importers would carry two
  long-lived conventions.
- Reverse the component cross product and quaternion formulas: rejected
  because handedness here is a coordinate interpretation; the existing
  component formulas already match the desired Unreal-style basis behavior.
- Support selectable coordinate modes: rejected because it doubles fixture,
  shader, culling, and hierarchy states without a roadmap requirement.

## Decision 2: Make matrix layout conversion explicit

**Decision**: Preserve CPU row-major matrix storage and column-vector
multiplication, but add one Renderer-owned CPU-to-shader packing adapter. Native
tests use non-symmetric translation, rotation, and non-uniform scale matrices
to prove the shader receives the intended matrix. Direct struct byte copying
into default GLSL `mat4` storage is not accepted as a contract.

**Rationale**: Existing identity and diagonal native fixtures can hide a
transpose defect. Matrix representation is independent from world handedness,
so it should be corrected at the CPU/shader boundary rather than by changing
Core storage.

**Alternatives considered**:

- Change all Core matrices to column-major: rejected as unrelated global API
  churn.
- Add `layout(row_major)` to every shader and continue copying: rejected
  because support and packing rules differ by target language/backend.

## Decision 3: Use a single glTF-to-engine basis conversion

**Decision**: Convert glTF vectors with:

```text
Engine.X =  Gltf.Z
Engine.Y = -Gltf.X
Engine.Z =  Gltf.Y
```

For affine node transforms, build or read the glTF matrix in an importer-local
right-handed representation, then apply `Mengine = C * Mgltf * inverse(C)`.
Normals use the inverse transpose of the final linear transform.

The basis transform is a reflection. Source triangle index order is preserved:
glTF counter-clockwise source order becomes the engine's canonical clockwise
front-face interpretation. Tangent `w` is negated once for the basis reflection
so `cross(N, T) * w` reconstructs the same physical bitangent after conversion.
Node negative-determinant parity remains separate and is applied exactly once
by the draw/culling contract.

**Rationale**: glTF defines a right-handed, Y-up, +Z-forward coordinate system,
meters, counter-clockwise positive rotation, and tangent handedness:
[glTF 2.0 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html).
The explicit basis and parity rules prevent duplicate winding/tangent flips.

**Alternatives considered**:

- Swap triangle indices during import: rejected because the basis reflection
  already maps the source winding to the canonical clockwise contract.
- Convert translation and Euler angles independently: rejected because matrix
  conjugation handles quaternion, matrix-authored, scale, and hierarchy cases
  consistently.

## Decision 4: Use patched cgltf as a private document parser

**Decision**: Vendor `cgltf v1.15` at upstream commit `360db1a`, plus the
mandatory integer-overflow fix from upstream PR 293 commit `8211a9f`. The
dependency is MIT licensed and compiled in one private C translation unit.
No `cgltf_*` type may appear in a public header.

`cgltf` receives bounded in-memory source bytes and a capped allocator. Project
code performs GLB preflight, checked range arithmetic, URI resolution,
aggregate limits, semantic validation, and package assembly. It never calls
`cgltf_parse_file()` or the default `cgltf_load_buffers()`.

**Rationale**: `cgltf` is a small C99, single-header, no-transitive-dependency
parser with memory callbacks, glTF/GLB object coverage, extras, and accessor
helpers. This fits the existing private C dependency pattern:
[cgltf repository](https://github.com/jkuhlmann/cgltf). The upstream overflow
fix remains explicit evidence rather than an undocumented local patch:
[cgltf PR 293](https://github.com/jkuhlmann/cgltf/pull/293).

**Alternatives considered**:

- fastgltf 0.9.0: capable runner-up, but adds simdjson/PMR complexity and makes
  hard aggregate memory budgeting and parser isolation less direct.
- TinyGLTF 2.x: rejected because its JSON/base64/stb dependency set overlaps
  existing project dependencies and its maintenance direction is less suitable
  for a new long-lived boundary.
- TinyGLTF 3.0: rejected while its C runtime is still described as
  experimental.
- Full custom parser: rejected because the project-specific value is bounded
  validation and canonicalization, not reimplementing the glTF object model.

## Decision 5: Use MikkTSpace only for the default tangent policy

**Decision**: Vendor private MikkTSpace at commit
`3e895b49d05ea07e4c2133156cfa94369e19e409` with its zlib-style source
license preserved in the dependency manifest. When a primitive lacks tangents,
a normal-mapped material requires them, and the selected UV set is valid,
generate tangents deterministically using MikkTSpace. Missing normals are
generated as flat normals by deterministic per-face vertex splitting before
tangent generation.

The import profile exposes versioned policies:

- `GenerateFlatRequiredTangents` (default)
- `RequireSourceAttributes` (strict)

The policy vocabulary leaves room for a later angle-based smooth/sharp-edge
normal strategy without changing payload identity rules.

**Rationale**: glTF requires clients to calculate flat normals when normals are
absent and recommends default MikkTSpace tangent generation when tangents are
absent:
[glTF mesh rules](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html).
MikkTSpace is the established reference implementation:
[MikkTSpace repository](https://github.com/mmikk/MikkTSpace).

**Alternatives considered**:

- Hand-written tangent generation: rejected because tangent basis details are
  standardized and error-prone.
- Always generate tangents: rejected because primitives without normal mapping
  do not need the extra stream or processing.
- Angle-based smoothing in Feature 024: deferred pending a separate policy and
  compatibility study; the public policy extension point is retained.

## Decision 6: Keep canonical mesh data typed and GPU-layout independent

**Decision**: `FStaticMeshAsset` stores typed positions, normals, tangents, up
to two UV sets, 16/32-bit indices, primitives, bounds, and material slots.
Renderer owns deterministic interleaving, RHI format selection, buffer packing,
and resource lifetime.

**Rationale**: Typed semantic arrays preserve import meaning, make validation
and future recooking possible, and avoid binding Asset schema to the current
Vulkan shader layout. Meshlet and ray-tracing phases can consume the same
canonical payload.

**Alternatives considered**:

- Store one opaque interleaved GPU byte stream in Asset: rejected because it
  makes a backend/profile packing choice authoritative.
- Store parser-native accessor views: rejected because source buffers and
  parser lifetime would leak into the public Asset contract.

## Decision 7: Upgrade Material and MaterialInstance schemas to v2

**Decision**: Add Asset-owned `FMaterialTextureBinding` with typed texture
reference, UV set, and `FMaterialSamplerIntent`. Sampler enums are defined in
Asset and mapped by Renderer to RHI; Asset does not include RHI headers.

Readers accept schema v1 and v2. A v1 texture reference upgrades in memory to
UV0, repeat address mode, and automatic linear filtering defaults. Writers
preserve existing v1 canonical output/digests when asked to write v1. New glTF
materials are v2. A binding with non-default UV or sampler state cannot be
losslessly downgraded to v1. MaterialInstance overrides receive the same value
type.

Normal scale, occlusion strength, alpha cutoff, and other scalar factors remain
separate material parameters. The texture binding owns only the texture,
coordinate set, and sampling intent.

**Rationale**: glTF texture info carries `texCoord`; samplers carry min/mag
filter and wrap modes. The current schema stores only an `FAssetId`, so it
cannot preserve those semantics. The glTF defaults are encoded explicitly in
the upgrade path:
[glTF texture contracts](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html).

**Alternatives considered**:

- Put sampler state in `FTextureAsset`: rejected because one texture can be
  sampled differently by different material slots.
- Reference RHI sampler enums directly: rejected by the Asset-to-Core-only
  constitution boundary.
- Break all v1 definitions: rejected because Feature 023 content and golden
  digests are already completed evidence.

## Decision 8: Add backend-neutral RHI upload and full indexed-draw contracts

**Decision**: Add `FRHIBufferUploadDesc`/`IRHIDevice::UploadBuffer` and
`FRHIIndexedDrawArguments`/`IRHICommandBuffer::RecordDrawIndexed`. Indexed draw
arguments include index count, instance count, first index, signed vertex
offset, and first instance. Existing simple overloads may remain as forwarding
compatibility helpers during migration.

**Rationale**: Renderer cannot realize device-local mesh buffers by calling
Vulkan upload staging directly. Multi-primitive packed meshes also require
first-index and vertex-offset fields that the current command contract lacks.

**Alternatives considered**:

- Create host-visible mesh buffers forever: rejected as a backend-specific
  performance policy and poor foundation for later meshlets.
- Give Renderer a Vulkan upload service: rejected by dependency direction.
- Allocate one vertex/index buffer per primitive: rejected as unnecessary
  object overhead and insufficient reason to avoid complete draw arguments.

## Decision 9: Publish import and realization transactions atomically

**Decision**: The importer builds all mesh/model/material/image/texture outputs
in request-owned scratch state. Stable identities, versions, dependencies, and
payloads are validated as a package before any output is returned for registry
publication. A failure returns no outputs.

Renderer follows:

```text
Validate -> Plan -> Allocate -> Upload -> Finalize -> Publish
```

Any failure invalidates all resources created by that realization and returns
an empty snapshot with deterministic diagnostics.

**Rationale**: A glTF source is a multi-output package. Partial publication
would leave dangling material/texture/model dependencies. The existing texture
realizer already establishes the appropriate all-or-nothing style.

**Alternatives considered**:

- Publish each subresource as soon as decoded: rejected because later failures
  cannot roll back a registry assembled by the caller.
- Keep partially realized buffers for retry: rejected until an Asset Manager
  and residency policy exist.

## Decision 10: Use explicit stable subresource key namespaces

**Decision**: Prefer `extras.stonerAssetId` and normalize it into an explicit
`key.<value>` namespace. When absent, use a deterministic typed structural key
such as `idx.mesh.3`, `idx.scene.0`, or `idx.material.2`. Display names never
participate in identity. Explicit keys must be unique within output type.

One `FStaticModelAsset` is produced per glTF scene. The default-scene identity
is recorded, and meshes not referenced by any scene still produce mesh assets.

**Rationale**: glTF names are optional and not unique, while `extras` is the
standard application-specific extension point. Namespaced fallback keys avoid
collisions between explicit and structural identities.

**Alternatives considered**:

- Name-derived IDs: rejected because authoring name edits would churn identity
  and duplicate names are legal.
- Raw array index only: rejected because reordering would churn identity even
  when the author supplies a stable key.
- One model for the whole file: rejected because scene selection and hierarchy
  identity would become ambiguous.

## Decision 11: Enforce finite configurable import limits

**Decision**: `FStaticModelImportLimits` carries all limits and contributes to
the import profile/version evidence. Recommended defaults:

| Limit | Default |
|---|---:|
| Main glTF/GLB bytes | 256 MiB |
| Single external dependency bytes | 512 MiB |
| Aggregate dependency bytes | 1 GiB |
| Parser allocation budget | 512 MiB |
| Scenes | 1,024 |
| Nodes | 1,000,000 |
| Hierarchy depth | 1,024 |
| Meshes | 262,144 |
| Primitives | 1,000,000 |
| Materials/textures/images each | 262,144 |
| Vertices per primitive | 16,777,216 |
| Indices per primitive | 50,331,648 |
| Total decoded geometry bytes | 2 GiB |
| Diagnostics | 4,096 |

Every product and sum uses checked 64-bit arithmetic before conversion to
container sizes. Exceeding a limit produces a stable `CapacityExceeded`
diagnostic and no outputs.

**Rationale**: The importer consumes untrusted structured sizes and nested
references. Limits must be configurable for tools while finite by default for
tests and runtime use.

**Alternatives considered**:

- Trust parser/platform allocation failure: rejected because behavior and
  diagnostics would be nondeterministic.
- Tiny test-sized defaults: rejected because legitimate production static
  models must remain importable.

## Decision 12: Pin and classify the fixture corpus

**Decision**: The checked-in corpus has three sources:

1. Khronos glTF Asset Generator v0.6.1 cases.
2. Selected Khronos Sample Assets whose individual licenses permit repository
   redistribution, preferring CC0 and recording attribution where required.
3. Repository-owned valid models and malformed byte/JSON mutations.

Each fixture records source URL, upstream revision, SHA-256, license, validator
result, expected importer result, and feature-scope classification. CI does not
download fixtures. glTF Validator `2.0.0-dev.3.10` at `bcd52cc` is pinned as an
offline conformance oracle:
[glTF Validator](https://github.com/KhronosGroup/glTF-Validator).

**Rationale**: The validator and Asset Generator provide broad spec evidence,
while project-owned mutations cover URI scope, overflow, atomic rollback,
identity, and Unreal conversion. Sample Asset licenses vary by model and cannot
be treated as one repository-wide license:
[Khronos Sample Assets](https://github.com/KhronosGroup/glTF-Sample-Assets).

**Alternatives considered**:

- Download latest corpus/validator in CI: rejected because it breaks
  reproducibility and can consume network unexpectedly.
- Use only hand-written fixtures: rejected because they under-cover legal
  accessor and material combinations.
- Treat validator success as importer success for every file: rejected because
  valid glTF may intentionally use excluded skins, morphs, modes, or required
  extensions.
