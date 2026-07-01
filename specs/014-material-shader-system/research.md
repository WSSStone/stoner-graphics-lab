# Research: Material & Shader System

## Decision: Renderer-layer material contracts remain backend-agnostic

**Rationale**: Materials describe surface/rendering intent and shader selection policy, not backend command recording. Keeping public material contracts in Renderer terminology preserves the architecture boundary and lets current Vulkan plus future Metal/DX/OpenGL backends share the same material decisions.

**Alternatives considered**:

- Backend-specific material objects: rejected because it would duplicate material policy per backend and leak graphics API details upward.
- RHI-layer material objects: rejected because RHI should expose low-level shader, descriptor, pipeline, and resource contracts, while Renderer owns material semantics.

## Decision: Shader library uses explicit in-memory registration of precompiled shader records

**Rationale**: The clarified scope excludes runtime shader compilation and local file scanning/loading. Explicit registration keeps the feature deterministic, testable, and independent of an asset catalog or shader build pipeline that does not exist yet.

**Alternatives considered**:

- Load shader records from local files: rejected for this phase because it would introduce file format, discovery, reload, and error-handling scope outside the material foundation.
- Runtime source compilation: rejected because the spec explicitly reserves shader compilation pipeline work for later tooling or renderer phases.

## Decision: Material resource parameters use abstract Renderer-level references

**Rationale**: Materials should express what texture/resource inputs they need without owning live RHI resources or graph-local handles. Abstract references allow render graph and future pipeline code to resolve bindings at the correct point in frame construction.

**Alternatives considered**:

- Store live RHI resources in material parameters: rejected because it couples materials to resource lifetime and backend-facing objects.
- Store render graph handles directly: rejected because graph handles are local to a graph declaration and should not persist inside reusable materials.

## Decision: Material instances may inherit from base materials or other instances

**Rationale**: Instance chains are more expressive for layered overrides and match common material workflows. Deterministic parent-chain resolution plus cycle detection makes this behavior testable and prevents invalid graphs of instance dependencies from reaching rendering.

**Alternatives considered**:

- Only direct base-material inheritance: rejected because the user selected instance-to-instance inheritance during clarification.
- Full copied material definitions: rejected because it loses override intent and increases duplication.

## Decision: Shader permutation flags are declared per shader record

**Rationale**: Per-shader allowed flags catch typos and invalid feature requests before variant selection. This keeps missing-flag diagnostics distinct from missing-variant diagnostics and reduces downstream ambiguity in tests.

**Alternatives considered**:

- Accept any flag and fail only on missing final variants: rejected because it weakens validation and hides incorrect material requests.
- Global flags for the entire shader library: rejected because valid feature flags can vary by shader.

## Decision: Deterministic text dumps are the primary inspection artifact

**Rationale**: Text dumps are easy to compare in tests, easy to read in command-line workflows, and consistent with the completed render graph debug experience. Byte-stable output gives strong regression coverage without requiring a visual editor or serialized asset format.

**Alternatives considered**:

- Structured inspection data only: rejected because the spec asks for human-readable diagnostics and deterministic inspection.
- Machine-readable serialized files: rejected because persistence and interchange formats are not part of this phase.

## Decision: Validate before considering a material render-ready

**Rationale**: Invalid domains, blend behavior, missing shader records, unknown permutation flags, wrong override types, inheritance cycles, and missing required parameters should fail before future render passes try to consume material data. This produces deterministic failures and protects later forward rendering integration.

**Alternatives considered**:

- Lazy validation during binding: rejected because it would push errors into later rendering phases.
- Best-effort fallback materials: rejected because fallback policy belongs to a higher-level renderer or editor workflow.

## Decision: Integration with render graph is limited to resource requirement summaries

**Rationale**: The render graph foundation already handles resource declaration and scheduling. This feature should expose stable material resource needs that graph-building code can consume, without implementing concrete forward/deferred passes.

**Alternatives considered**:

- Build concrete material render passes now: rejected because forward rendering is the next roadmap phase.
- Skip render graph integration entirely: rejected because the roadmap explicitly requires materials to declare resource requirements.
