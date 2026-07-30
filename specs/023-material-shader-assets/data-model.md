# Data Model: Material & Shader Assets

## Fixed Asset Types

| Type text | Runtime payload | Meaning |
|---|---|---|
| `ShaderProgram` | `FShaderAsset` | Complete logical graphics or compute program |
| `ShaderSource` | `FShaderSourceAsset` | One immutable source dependency |
| `ShaderPayload` | `FShaderPayloadAsset` | One immutable precompiled target/stage/permutation dependency |
| `Material` | `FMaterialAsset` | Base material definition |
| `MaterialInstance` | `FMaterialInstanceAsset` | One-parent override definition |
| `Texture` | Existing `FTextureAsset` | Texture-valued material dependency |

All values use Feature 020 `FAssetId`, `FAssetVersion`, `FAssetMetadata`,
`FAssetDependency`, and `TSoftAssetRef<T>`. Shader stage/permutation records are
not top-level ShaderProgram identities. Dependency subresources commonly share
the program logical path but have distinct type and subresource components.

## Source Definition Envelope

Every `.shader.json`, `.material.json`, and `.material-instance.json` root has:

- `schema`: exact `stoner.shader-program`, `stoner.material`, or
  `stoner.material-instance`.
- `version`: positive integer; Feature 023 supports exactly `1`.
- `id`: canonical typed Asset identity.
- `requiredExtensions`: unique extension names; input order is ignored and the
  typed model/canonical output sorts them.
- kind-specific fields.
- `extensions`: object of namespaced extension values.

Ordinary root/object schemas are closed. Unknown ordinary fields fail. Unknown
entries under `extensions` are optional unless named by `requiredExtensions`.
Unknown optional entries may be discarded when the model is normalized.

`FAssetVersion` is not an envelope field. After canonical writing, the loader
derives it from the complete canonical source bytes. Embedding that digest in
the same definition is forbidden because it would be self-referential.

### Canonical JSON Rules

- valid UTF-8, no BOM, no NUL or invalid/lone-surrogate text;
- strict RFC 8259 grammar; no comments, trailing commas, NaN, or Infinity;
- decoded object names are unique;
- two-space indent, LF, one final newline;
- schema-defined object key order;
- map/set values sorted by canonical token or Asset identity;
- arrays preserve order only where semantically ordered;
- finite floats use shortest round-tripping decimal; negative zero writes `0`;
- no insignificant input formatting survives normalization.

## Limits

### FMaterialShaderAssetLimits

| Field | Default | Applies before |
|---|---:|---|
| MaxDefinitionBytes | 1 MiB | JSON parse |
| MaxShaderSourceBytes | 4 MiB | source dependency allocation |
| MaxShaderPayloadBytes | 16 MiB | payload dependency allocation |
| MaxProgramDependencyBytes | 64 MiB | aggregate program dependency allocation |
| MaxJsonDepth | 32 | nested DOM traversal |
| MaxJsonValues | 32,768 | aggregate DOM traversal |
| MaxObjectMembers | 256 | one-object duplicate/member allocation |
| MaxArrayElements | 4,096 | one-array traversal/allocation |
| MaxTextBytes | 4 KiB | general schema strings; smaller Feature 020 rules win |
| MaxTokenBytes | 128 | name, tag, entry point, and profile tokens |
| MaxLocatorBytes | 1,024 | relative dependency locator |
| MaxNumberTokenBytes | 64 | raw numeric token conversion |
| MaxExtensions | 64 | required plus optional extension declarations |
| MaxStages | 16 | stage array allocation |
| MaxPermutationFlags | 64 | flag set allocation |
| MaxSourceRecords | 64 | source-reference allocation |
| MaxVariants | 1,024 | variant allocation |
| MaxPayloadRecords | 2,048 | aggregate payload-reference allocation |
| MaxParameters | 256 | material defaults or instance overrides |
| MaxInterfaceBindingsPerStage | 256 | binding allocation |
| MaxDependencies | 1,024 | metadata/dependency publication |
| MaxInstanceDepth | 64 | parent traversal |

All fields are positive finite values. Callers may lower or explicitly raise
them; no value disables checked arithmetic or cycle/depth validation.

## Shader Program Model

### EShaderProgramKind

- `Graphics`: requires exactly one Vertex and one Fragment stage for schema v1.
- `Compute`: requires exactly one Compute stage for schema v1.

Graphics and Compute stages cannot coexist in one v1 program.

### EShaderStage

Supported values: `Vertex`, `Fragment`, `Compute`.

Reserved future values include geometry, tessellation, task, mesh, ray
generation, hit, and miss stages. A v1 reader reports them as an unknown
required capability unless an understood extension defines them; it does not
silently map them to a current stage.

### EShaderSourceLanguage

Feature 023 supports `GLSL`. The enum/token vocabulary leaves explicit future
values for HLSL, MSL, WGSL, and Slang but does not accept them as current
runtime-ready source records.

### FShaderSourceReference

- Stage.
- EntryPoint: ASCII identifier, 1..MaxTokenBytes; current repository value
  `main`.
- SourceLanguage.
- Source: `TSoftAssetRef<FShaderSourceAsset>`.
- Locator: canonical relative resolver locator used only to build metadata; not
  identity.
- ExpectedDigest: SHA-256 of exact source bytes.

Validation:

1. typed ID is valid and distinct from the ShaderProgram ID;
2. locator is relative, NFC, NUL/control-free, and contains no parent traversal;
3. stage is supported by ProgramKind;
4. stage/entry-point pair is unique;
5. source digest is available and agrees with loaded bytes.

### FShaderSourceAsset

An immutable `FAssetPayload`:

- Id: type `ShaderSource`.
- Version: source/content digest over exact UTF-8 bytes.
- Language.
- Bytes: exact source bytes; valid UTF-8, no normalization or rewriting.

Source text is preserved byte-for-byte because compilation provenance depends
on exact input. It is inspectable by digest/size/language, never dumped in full.

### EShaderBackendFamily

Current accepted runtime target: `Vulkan`.

Schema vocabulary also contains `Metal`, `DirectX12`, `OpenGL`, and `GLES`.
Those records may be represented only when their payload format is understood;
Feature 023 does not execute them.

### EShaderPayloadFormat

- `SPIRV`: current runtime-ready opaque 32-bit word stream.
- `MSL`, `DXIL`, `GLSL`: reserved target slots for later producer/backend
  features.

### FShaderPayloadReference

- Backend.
- Profile: bounded case-sensitive ASCII token.
- Format.
- Stage.
- EntryPoint.
- PermutationKey.
- Payload: `TSoftAssetRef<FShaderPayloadAsset>`.
- Locator: canonical relative resolver locator.
- ExpectedDigest.
- Producer and ProducerVersion.

The uniqueness key is:

`(Backend, Profile, Format, Stage, EntryPoint, PermutationKey)`.

Duplicate keys fail regardless of locator, digest, or declaration order.

### FShaderPayloadAsset

An immutable `FAssetPayload`:

- Id: type `ShaderPayload`.
- Version.
- Backend, Profile, Format, Stage, EntryPoint, PermutationKey.
- Bytes: exact immutable dependency bytes.

For SPIR-V:

- size is 20..MaxShaderPayloadBytes and divisible by four;
- magic, version, bound, and instruction word counts are structurally valid;
- one `OpEntryPoint` matches Stage and EntryPoint;
- content digest equals ExpectedDigest.

No native module/handle, RHI object, filesystem path, compiler object, or
reflection cache is retained.

### FShaderPermutationDomain

- AllowedFlags: sorted unique ASCII tokens, maximum 64.
- Variants: sorted by canonical permutation key.

### FShaderVariantDefinition

- VariantName: stable diagnostic token.
- Permutation: sorted unique subset of AllowedFlags.
- Payloads: target records for every required program stage.

The canonical permutation key is the delimiter-safe ordered encoding already
established by Feature 014, not caller insertion order.

### FShaderRequiredParameter

- Name.
- Type: Scalar, Vector, Color, or TextureReference.

Names are unique. Types must agree with material parameters during conversion.

### EShaderResourceKind

Backend-neutral schema v1 values:

- UniformBuffer.
- SampledTexture.
- Sampler.
- StorageBuffer.
- StorageTexture.

### FShaderInterfaceBinding

- SetIndex and BindingIndex: checked unsigned values.
- Kind.
- ArrayCount: 1..configured limit.
- Visibility: non-empty subset of program stages.
- Name: optional stable diagnostic token.

`(SetIndex, BindingIndex)` is unique. Sampler/resource relationships remain
explicit records rather than native descriptor handles.

### FShaderConstantRange

- OffsetBytes.
- SizeBytes: positive.
- Visibility.

Ranges use checked arithmetic and must not overlap incompatibly. Renderer maps
understood records into `FRHIShaderInterfaceMetadata`.

### FShaderAsset

An immutable `FAssetPayload`:

- Id: type `ShaderProgram`.
- Version and schema version.
- ProgramKind.
- Stages.
- PermutationDomain.
- RequiredParameters.
- InterfaceBindings and ConstantRanges.
- PayloadReferences.
- Metadata dependencies.
- CanonicalDefinition: normalized source bytes for inspection/digest evidence.

States:

1. `Parsed`: schema was constructed without dependency bytes.
2. `Validated`: identity, fields, stages, variants, interfaces, and dependency
   declarations are valid.
3. `RuntimeReady(TargetRequest)`: every selected dependency loaded and passed
   target/stage/entry/digest validation.
4. `Rejected`: no payload is published.

The immutable payload stores no mutable state enum; these are operation outcomes.

## Target Selection Model

### FShaderTargetRequest

- Backend: exactly one family.
- AcceptableProfiles: non-empty ordered unique profile tokens.
- Permutation: canonical flag set.
- RequiredStages: derived from ProgramKind; caller cannot weaken it.

### FSelectedShaderProgram

- ShaderId and ShaderVersion.
- Backend and SelectedProfile.
- PermutationKey.
- one loaded FShaderPayloadAsset per required stage;
- interfaces and required parameter declarations;
- complete source manifest.

Selection:

1. validate request/backend/profile uniqueness;
2. validate permutation against AllowedFlags and find one exact variant;
3. for each acceptable profile in request order, collect complete matching stage
   sets;
4. zero complete sets continues to next profile;
5. one complete set succeeds;
6. more than one complete set returns AmbiguousTarget immediately;
7. exhaustion returns TargetUnavailable.

Records from different profiles or backends never combine.

## Material Model

### EMaterialAssetDomain

`Surface`, `PostProcess`, `UI`, `Decal`; meanings mirror Feature 014.

### EMaterialAssetBlendMode

`Opaque`, `Translucent`, `Additive`, `Masked`; compatibility rules mirror
Feature 014.

### FMaterialAssetRenderState

- bDepthTest.
- bDepthWrite.
- bTwoSided.

### EMaterialAssetParameterType

- Scalar.
- Vector.
- Color.
- TextureReference.

### FMaterialAssetParameterValue

Tagged union:

- Scalar: finite float.
- Vector: four finite floats.
- Color: four finite floats using current Feature 014 meaning.
- TextureReference: `TSoftAssetRef<FTextureAsset>`.

The inactive union alternatives are default/empty and do not enter canonical
output.

### FMaterialAssetParameter

- Name: unique bounded token.
- Value.

Parameters are canonicalized by name. Texture references become required runtime
dependencies.

### FMaterialAsset

An immutable `FAssetPayload`:

- Id: type `Material`.
- Version and schema version.
- Domain, BlendMode, RenderState.
- Shader: `TSoftAssetRef<FShaderAsset>`.
- PermutationRequest.
- Parameters.
- Dependencies and canonical definition.

The Shader dependency is required for runtime. Texture dependencies are required
for runtime but may remain unresolved at registry publication.

## Material Instance Model

### FMaterialParentReference

Exactly one tagged alternative:

- `TSoftAssetRef<FMaterialAsset>`, or
- `TSoftAssetRef<FMaterialInstanceAsset>`.

Empty, both-present, or type-mismatched states are invalid.

### FMaterialInstanceAsset

An immutable `FAssetPayload`:

- Id: type `MaterialInstance`.
- Version and schema version.
- Parent.
- Overrides: sorted unique `FMaterialAssetParameter` values.
- Dependencies and canonical definition.

### FResolvedMaterialAsset

Output of instance resolution:

- LeafId and LeafVersion.
- RootMaterialId and RootMaterialVersion.
- SourceManifest: ordered every Material/MaterialInstance ID+version in the
  chain plus shader/texture dependencies.
- Domain, BlendMode, RenderState.
- Shader and PermutationRequest from root.
- EffectiveParameters after root-to-leaf overrides.

Resolution states:

1. Validate leaf identity and overrides.
2. Follow immutable lookup, checking visited IDs before each visit.
3. Reject cycle, missing parent, type mismatch, invalid parent, or depth >64.
4. Validate every override against root parameter presence/type.
5. Apply overrides root to leaf.
6. Publish one resolved value.

No `FMaterialInstance` parent pointer crosses into Renderer.

## Renderer Snapshot Model

### FAssetSourceVersionRecord

- Id.
- Version.
- Role: Program, Source, Payload, Material, Parent, Texture.

Records are sorted by canonical Asset ID and duplicate IDs must carry one
identical version.

### FShaderAssetSnapshot

- SourceManifest.
- SelectedTarget.
- Feature 014 `FShaderRecord` values.
- one `FRHIShaderModuleDesc` per required stage.
- normalized conversion diagnostics.

All bytecode words, entry points, interfaces, strings, and versions are owned by
the snapshot. Registration into `FShaderLibrary` uses one transactional batch.

### FMaterialAssetSnapshot

- SourceManifest.
- flattened Feature 014 `FMaterial`.
- selected ShaderId/VariantId/PermutationKey.
- effective parameters.
- resource requirements.

It is immutable after success and contains no Asset pointer, Registry pointer,
execution lease, callback, or `FMaterialInstance` parent pointer.

### Snapshot Replacement Semantics

- replacing/removing a source Asset does not mutate or invalidate a snapshot;
- the snapshot continues to identify exact source versions;
- explicit reconversion produces a new snapshot;
- Feature 026 may own handle swaps later without changing snapshot values.

## Import and Publication Transactions

### FMaterialShaderImportParameters

- Expected top-level ID.
- Limits.
- bLoadDependencies: development source import default true; parse-only tests
  may set false.

### FMaterialShaderDefinitionImporter

Probe recognizes the closed root schema after extension hint filtering.

Shader definition success emits:

- one ShaderProgram output;
- one ShaderSource output per unique source dependency when loading is enabled;
- one ShaderPayload output per unique payload dependency when loading is
  enabled;
- metadata/dependency records for all outputs.

Material/instance definition success emits one top-level payload plus metadata.
The loader/importer emits the complete output set or none and never mutates the
Registry. `FMaterialShaderImportService::ImportAndRegister` converts successful
metadata to one Feature 020 `FAssetMutationBatch` and invokes Registry `Apply`
exactly once. Any earlier failure leaves the Registry untouched; an `Apply`
failure exposes no payload result and preserves the prior Registry revision.

## Diagnostics

New stable Asset result categories:

| Result | Meaning |
|---|---|
| InvalidDefinition | JSON/schema field invalid |
| UnsupportedSchema | schema/version unsupported |
| UnknownRequiredExtension | required extension not understood |
| DefinitionLimitExceeded | source, depth, count, or text limit exceeded |
| DependencyMismatch | typed ID, locator, digest, producer, stage, or entry disagrees |
| InvalidShaderProgram | stage/permutation/interface invariant failed |
| TargetUnavailable | no requested profile has a complete set |
| AmbiguousTarget | first matching profile has multiple complete sets |
| InvalidMaterialAsset | domain/blend/parameter/dependency invariant failed |
| InvalidInstanceChain | parent missing/type/cycle/depth/override failure |

Asset stages include Parse, Normalize, Validate, Dependency, Select, Resolve,
and Registry. Renderer snapshot conversion continues to use existing
`EMaterialResult` and `FMaterialDiagnosticLog`; native validation continues to
use RHI/Backend result and diagnostic vocabulary. Diagnostics identify stable
subject, field/index/limit, and reason; no full source text, payload bytes,
native address, thread ID, absolute path, timing, or uncontrolled third-party
text is normalized.
