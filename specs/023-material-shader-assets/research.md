# Research: Material & Shader Assets

## Authoring Format

### Decision: Strict RFC 8259 JSON with one project canonical profile

Use UTF-8 JSON source definitions. Canonical files use no BOM, two-space
indentation, LF, one terminal newline, fixed schema key order, sorted map-like
collections, and explicit arrays only where order has semantic meaning.
Comments, trailing commas, NaN/Infinity, duplicate decoded keys, and unknown
ordinary schema fields are rejected.

**Rationale**: JSON is widely understood and manually editable without adding a
custom grammar. RFC 8259 requires UTF-8 for interoperable exchange but notes
that duplicate-member behavior differs between implementations; the project
therefore rejects duplicates explicitly and never depends on input object
order. Canonical rewriting makes source/content digests host-independent.

**Alternatives considered**:

- TOML: pleasant for shallow configuration but cumbersome for nested shader
  stages, variants, interfaces, and typed parameter unions.
- YAML: comments and concise syntax are attractive, but implicit typing,
  aliases, duplicate-key diversity, and larger parser behavior surface make
  deterministic fail-closed ingestion harder.
- Custom DSL: could be domain-friendly later, but creates a parser/tooling
  project before the asset contracts stabilize.
- Binary-only source: rejected by clarification and overlaps Feature 025.

**Sources**:
[RFC 8259](https://www.rfc-editor.org/info/rfc8259/).

## JSON Parser

### Decision: Vendor yyjson 0.12.0 privately and wrap it with strict validation

Pin release 0.12.0, upstream commit, MIT license, and SHA-256 for `yyjson.c` and
`yyjson.h`. Compile one Asset-private C translation unit. Parse bounded memory
with `YYJSON_READ_NUMBER_AS_RAW`, no permissive flags, preflighted
`yyjson_read_max_memory_usage`, and a fixed-pool allocator. A non-recursive
post-parse walk enforces depth/value/member/array/token limits and detects
duplicate decoded keys, including `"a"` versus `"\u0061"`. Copy only validated
values into project-owned immutable models. Canonical output is built from model
values, not by mutating or re-emitting the input DOM.

**Rationale**: Upstream is portable ANSI C, RFC 8259-oriented, validates UTF-8,
supports precise integer/double handling, and integrates through one C/H pair.
Its documented duplicate-key preservation is useful because the wrapper can
detect and reject duplicates rather than inherit first/last-wins behavior.

**Alternatives considered**:

- nlohmann/json: convenient C++ API but a much larger header compile-time and
  public-include temptation for a narrow private parser.
- RapidJSON: mature and fast, but its older API/maintenance profile and larger
  integration surface offer no advantage here.
- Handwritten parser: rejected because strict Unicode, number, nesting, and
  malformed-input handling are not engine-learning goals.
- OS JSON APIs: rejected because behavior and availability differ by platform.

**Sources**:
[yyjson repository and integration model](https://github.com/ibireme/yyjson),
[yyjson 0.12.0 release](https://github.com/ibireme/yyjson/releases/tag/0.12.0).

## Optional and Required Evolution

### Decision: Namespaced extensions plus an explicit required list

Root fields are closed and unknown names fail. Extensible data appears under:

```json
"requiredExtensions": ["stoner.example"],
"extensions": {
  "stoner.example": {}
}
```

An unknown extension absent from `requiredExtensions` is optional and may be
ignored/dropped during canonical rewrite. An unknown required extension, a
listed extension with no body, duplicate names, or a body in the wrong type
fails.

Input order of unique `requiredExtensions` names and `extensions` object members
is not semantic. The typed model and canonical writer sort both collections;
only duplicate decoded names or invalid names fail.

**Rationale**: Treating every unknown root key as optional hides typos. A
namespaced extension island makes the clarification decision testable and
preserves fail-closed semantics for behavior-changing additions.

**Alternatives considered**:

- Ignore all unknown keys: rejected because misspelled material/shader fields
  become silent semantic loss.
- Preserve arbitrary unknown fields losslessly: rejected by clarification and
  conflicts with normalization into a project-owned model.
- Major-version bump for every optional addition: rejected because harmless
  metadata extensions would force unnecessary reader lockstep.

## Numeric and Text Canonicalization

### Decision: Validate to project types, then write canonical model values

All input must be valid UTF-8. Identity-bearing text passes existing Feature 020
NFC/type/path validation. Schema tokens use bounded case-sensitive ASCII.
Human-readable labels accept NFC text but reject NUL/control characters.

Material numeric values parse to finite IEEE-754 `float`. Overflow, underflow to
an unintended zero, NaN, Infinity, and values not representable by the target
type fail. Canonical output uses a shortest round-tripping decimal for the
stored float, normalizes negative zero to `0`, and ignores the input lexeme.
Integer counts/indices use checked unsigned ranges and never route through
floating point.

The top-level `FAssetVersion` is the SHA-256 evidence derived after canonical
writing from the complete canonical source bytes. It is not serialized inside
the same definition because embedding its own digest would be self-referential.
External GLSL/SPIR-V dependency digests remain explicit schema fields.

**Rationale**: Rewriting from typed values prevents platform-specific source
lexemes, object order, locale, or host layout from entering content evidence.

**Alternatives considered**:

- Preserve original number strings: rejected because semantically equivalent
  files would hash differently.
- Fixed decimal precision: rejected because it can lose float round-trip
  identity or create artificial changes.
- Store all material numbers as decimal strings: rejected because it moves
  numeric validation into every consumer.

## Asset Decomposition

### Decision: Program authority with typed source and payload dependencies

Use five fixed asset types: ShaderProgram, ShaderSource, ShaderPayload, Material,
and MaterialInstance, plus existing Texture references. One ShaderProgram is
the authority for complete stages/permutations/interfaces. Individual GLSL and
SPIR-V files are source/payload dependencies, commonly expressed as stable
subresources associated with the program logical path.

**Rationale**: This implements clarification Q1 while allowing shared sources,
exact payload digest checks, and Feature 025 independent recooking. A Material
has one ShaderProgram soft reference instead of assembling stage assets.

**Alternatives considered**:

- Stage-level top-level shader assets: rejected by clarification because
  program compatibility and material references become multi-asset assembly.
- One embedded JSON/base64 file: rejected by clarification because binary
  diffs/manual editing degrade and payloads cannot be independently versioned.
- One Asset per permutation: rejected because identity and registry count scale
  with variant count.

## Shader Payload Validation

### Decision: Project structural preflight plus existing RHI/runtime validation

Asset validates dependency size, digest, 4-byte alignment, SPIR-V magic/header,
declared version, exact stage execution model, entry point, and target metadata
before publishing a runtime-ready ShaderProgram. Renderer maps validated words
and interfaces into `FRHIShaderModuleDesc`. Existing RHI/Vulkan validation
remains authoritative for execution-environment and native-device acceptance.
CI may run pinned `spirv-val` as an independent repository oracle when present,
but no public Asset API depends on SPIRV-Tools.

**Rationale**: The SPIR-V specification defines modules as word streams and
permits multiple entry points; exact requested stage/entry validation is
necessary before selection. Reusing RHI/runtime checks prevents Asset from
becoming a Vulkan validator while still rejecting obvious dependency mismatch.

**Alternatives considered**:

- Link SPIRV-Tools into Asset: rejected for dependency size and because complete
  environment validation belongs nearer RHI/Backend.
- Trust filename extensions: rejected because identity hints cannot establish
  stage, entry point, integrity, or byte validity.
- Validate only at pipeline creation: rejected because malformed repository
  assets would survive too far into runtime composition.

**Sources**:
[Khronos SPIR-V Registry](https://registry.khronos.org/SPIR-V/),
[SPIR-V unified specification](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html),
[Khronos SPIR-V tools resources](https://www.khronos.org/spirv/resources).

## Target Selection

### Decision: Caller-ordered profiles inside one backend

A selection request supplies one backend and a non-empty, duplicate-free list
of acceptable profiles in priority order. For a canonical permutation and
required stage set, selection examines profiles in request order. The first
profile with one complete set wins; multiple complete sets at that profile
return AmbiguousTarget and stop; incomplete sets do not combine across profiles.

**Rationale**: This directly implements clarification Q2. It permits explicit
baseline fallback while avoiding hidden capability scoring, enum ordering,
cross-backend translation, or registration-order behavior.

**Alternatives considered**:

- Exact profile only: safe but duplicates payloads for compatible baseline
  environments.
- Automatic capability scoring: rejected because policy becomes hidden and
  difficult to reproduce across future backends.
- Source fallback/runtime compilation: explicitly excluded.

## Material Instance Resolution

### Decision: Identity-based immutable lookup with depth 64

An instance has exactly one parent reference: Material or MaterialInstance.
Resolution keeps a visited identity set, checks before each parent visit, and
fails on repetition, missing/type-mismatched parent, invalid base material, or
depth greater than 64. It copies root defaults then applies overrides from root
to leaf so nearest wins. Unknown/type-changing overrides fail.

**Rationale**: Sixty-four levels are far beyond useful authored chains while
bounding malformed content. Identity lookup removes process-local pointers and
matches Feature 020 registry semantics.

**Alternatives considered**:

- Unlimited traversal with cycle detection: rejected because an acyclic hostile
  chain can still exhaust work/memory.
- Flatten instances during Feature 023 persistence: rejected because source
  authoring loses inheritance and Feature 025 owns derived/cooked flattening.
- Live parent pointers: rejected because they cannot be serialized safely.

## Renderer Conversion Lifetime

### Decision: Deep immutable snapshots with complete source manifests

Every conversion copies material values, strings, interfaces, selected payload
words, dependency identities, and versions into a Renderer-owned snapshot. The
snapshot records all source versions and never registers a callback with Asset.
Asset replacement does not mutate or invalidate it; explicit reconversion is
the only update path.

**Rationale**: This implements clarification Q4, prevents dangling pointers, and
keeps Feature 023 synchronous. Feature 026 can later manage handle replacement
without changing snapshot value semantics.

**Alternatives considered**:

- Registry-driven live binding: rejected as hidden hot reload/lifecycle work.
- Immediate invalidation: rejected because every consumer would need Registry
  access before use and completed frame plans could become unstable.
- Mixed live textures/snapshot materials: rejected because lifetime semantics
  would depend on parameter type.

## Repository Migration

### Decision: Six program definitions under Content and byte-input native helpers

Move the 11 GLSL and 11 SPIR-V files under `Content/Shaders` without changing
bytes. Define Triangle, Surface, Composition, DirectionalLight, PointLight, and
SpotLight programs; Composition and DirectionalLight share Fullscreen vertex
dependencies. Demo/Application and integration-test composition load Assets and
pass selected bytecode downward. Backend native helpers accept bytecode/stage/
entry-point values, not repository paths, and remain independent of Asset.

**Rationale**: This proves the source schema against real engine content and
removes path ownership from Backend. Six programs match actual pipeline units
rather than one asset per file.

**Alternatives considered**:

- Leave production path loading and test only synthetic assets: rejected by
  repository migration requirements.
- Let Vulkan Backend resolve Asset IDs: constitutional violation.
- Duplicate Fullscreen source into two programs: rejected because it creates
  multiple authorities for identical source/payload bytes.

## Default Limits

### Decision: Conservative editable-source limits, all finite and overridable

| Limit | Default |
|---|---:|
| Source definition | 1 MiB |
| One GLSL dependency | 4 MiB |
| One precompiled payload | 16 MiB |
| Aggregate resolved program dependency bytes | 64 MiB |
| JSON nesting depth | 32 |
| Total JSON values | 32,768 |
| Members per object | 256 |
| Elements per array | 4,096 |
| Text field | 4 KiB unless Feature 020 identity rules are smaller |
| Name/tag/entry point | 128 bytes |
| Relative dependency locator | 1,024 bytes |
| Raw number token | 64 bytes |
| Required/optional extensions | 64 |
| Program stages | 16 |
| Permutation flags | 64 |
| Source records | 64 |
| Variants | 1,024 |
| Payload records | 2,048 |
| Material parameters/overrides | 256 |
| Interface bindings | 256 per stage |
| Dependencies | 1,024 |
| Material instance depth | 64 |

Callers may lower or explicitly raise positive limits. No zero/unbounded
sentinel exists, and checked aggregate arithmetic remains mandatory.

**Rationale**: Legitimate current files are tiny relative to these limits. The
defaults allow future complex shaders while bounding parser DOM, dependency
reads, Cartesian variant mistakes, and hostile chains.

**Alternatives considered**:

- Reuse KTX2 512 MiB/1 GiB defaults: rejected because human-authored definitions
  and shader modules require much smaller budgets.
- No defaults: rejected because every caller would define incompatible safety
  policy.
- Hard non-overridable limits: rejected because offline tools may process larger
  future programs under explicit budgets.
