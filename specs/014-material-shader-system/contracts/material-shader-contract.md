# Contract: Material & Shader System

## Scope

This contract describes the Renderer-layer public behavior for defining materials, resolving material instances, registering precompiled shader records, selecting shader variants, reporting resource requirements, validating inputs, invalidating records, and producing deterministic text dumps. It is a C++ library contract, not a network, file format, or CLI contract.

## Public Types

### `FMaterial`

Represents a reusable material definition.

Required behavior:

- Store material name, shader reference, material domain, blend mode, render behavior summary, permutation request, and default parameters.
- Validate domain/blend/shader/parameter compatibility.
- Reject duplicate parameters.
- Report resource requirements derived from effective resource-reference parameters.
- Reject use after invalidation.

### `FMaterialInstance`

Represents an inherited material override.

Required behavior:

- Reference either a base material or another material instance.
- Override selected parent parameters.
- Resolve effective parameters through the parent chain.
- Apply nearest override precedence.
- Reject unknown parameters, type mismatches, invalidated parents, and inheritance cycles.

### `FMaterialParameterSet`

Owns typed named material values.

Required behavior:

- Support scalar, vector, color-like, and abstract Renderer-level resource-reference values.
- Preserve deterministic parameter ordering for inspection.
- Reject duplicate names in root material definitions.
- Keep resource references abstract; do not hold live RHI resources or graph-local handles.

### `FShaderLibrary`

Owns explicitly registered precompiled shader records and variants.

Required behavior:

- Register shader records in memory.
- Reject duplicate shader identities.
- Expose lookup and validation diagnostics.
- Reject local file scanning/loading and runtime source compilation as unsupported in this feature scope.
- Reject use after invalidation.

### `FShaderPermutation`

Represents a deterministic set of requested shader feature flags.

Required behavior:

- Canonicalize equivalent flag sets independent of input order.
- Validate requested flags against the target shader record.
- Reject unknown flags before variant lookup.

### `FMaterialShaderBinding`

Represents a successful material-to-shader-variant resolution.

Required behavior:

- Expose material identity, shader identity, selected variant identity, canonical permutation key, effective parameters, and resource requirements.
- Fail deterministically when shader record, permutation flags, variant, or required parameters are unavailable.

### `FMaterialResourceRequirement`

Describes material resource needs for render graph declaration.

Required behavior:

- Expose stable parameter name, abstract resource reference, access intent, and source material/instance identity.
- Produce an empty requirement list for materials with no resource parameters.
- Reflect instance resource overrides instead of inherited values when present.

### `FMaterialDiagnostics`

Collects validation, selection, resource requirement, invalidation, and inspection diagnostics.

Required behavior:

- Include stable diagnostic codes, severity, category, subject name, and human-readable message.
- Identify the failing material or shader request for invalid cases.
- Preserve deterministic ordering.

## Validation Contract

Input:

- Material definitions.
- Material instance chains and overrides.
- Shader library records and available variants.
- Requested shader permutations.
- Material parameter sets.

Output:

- Success with valid material state, optional shader binding, resource requirements, and diagnostics.
- Failure with deterministic diagnostics and no render-ready binding.

Required validation:

- Detect duplicate material parameter names.
- Detect unsupported domain/blend combinations.
- Detect missing shader records.
- Detect unknown permutation flags before variant lookup.
- Detect missing shader variants.
- Detect missing required shader parameters.
- Detect material instance inheritance cycles.
- Detect overrides for parameters not defined by the root material.
- Detect override type mismatches.
- Detect use after invalidation.

## Shader Selection Contract

Preconditions:

- Shader record is explicitly registered in memory.
- Material or material instance is valid.
- Requested permutation flags are declared by the target shader record.

Required behavior:

- Canonicalize permutation flags deterministically.
- Select the matching precompiled variant when available.
- Reject unknown flags separately from missing variants.
- Produce stable selected variant identity for repeated equivalent requests.

## Resource Requirement Contract

Preconditions:

- Material or material instance parameters have been validated and resolved.

Required behavior:

- Report abstract Renderer-level resource references for resource-like parameters.
- Avoid live RHI resources, backend objects, or graph-local handles in material data.
- Preserve deterministic requirement order.
- Allow render graph declaration code to consume requirements before execution.

## Inspection Dump Contract

Required behavior:

- Produce deterministic human-readable text output.
- Include material list, instance list, shader records, permutations, selected bindings, resource requirements, and diagnostics.
- Produce byte-identical output across repeated dumps of unchanged data.
- Include invalid material, instance, shader, parameter, and resource-reference context when validation fails.

## Boundary Rules

- Public Renderer material contracts must not include Vulkan, Metal, DX12, OpenGL, GLES, WebGL, platform-window, or backend-specific concepts.
- Material parameters must not own live RHI resources or render graph local handles.
- Runtime shader compilation, shader source parsing, local shader file scanning/loading, visual material editing, concrete forward/deferred passes, scene graph integration, visible presentation, and PBR-specific material models are out of scope.
