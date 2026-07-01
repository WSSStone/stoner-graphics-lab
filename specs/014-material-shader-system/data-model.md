# Data Model: Material & Shader System

## Material

**Purpose**: Represents a reusable rendering description for a surface, post-process, UI, or decal use case.

**Key fields**:

- `Name`: Required deterministic debug name.
- `ShaderReference`: Stable identity of the shader record requested by this material.
- `Domain`: Surface, PostProcess, UI, or Decal.
- `BlendMode`: Opaque, Translucent, Additive, or Masked.
- `RenderStateSummary`: Renderer-level state intent needed by future pipeline selection.
- `PermutationRequest`: Deterministic feature-flag set requested from the shader record.
- `Parameters`: Default `Material Parameter Set`.
- `ValidationState`: Draft, Valid, Invalid, or Invalidated.

**Validation rules**:

- Name and shader reference must be present.
- Domain and blend mode must be a supported combination.
- Parameter names must be unique within the material.
- Requested permutation flags must be declared by the target shader record.
- Required shader parameters must be present with matching value types.
- Invalidated materials cannot be treated as render-ready.

## Material Instance

**Purpose**: Represents a child material description that inherits from a base material or another instance and overrides selected parameters.

**Key fields**:

- `Name`: Required deterministic debug name.
- `Parent`: Reference to a root material or parent material instance.
- `Overrides`: Parameter values that replace inherited values.
- `ResolvedParameters`: Effective parameter set after parent-chain resolution.
- `ValidationState`: Draft, Valid, Invalid, CycleDetected, or Invalidated.

**Validation rules**:

- Parent must exist and must not be invalidated.
- Parent chains must be acyclic.
- Overrides must reference parameters defined by the root material.
- Override value types must match root material parameter types.
- The nearest override wins when multiple ancestors define a value.

## Material Parameter Set

**Purpose**: Stores named typed values used by materials and material instances.

**Key fields**:

- `Parameters`: Ordered collection of named values.
- `ValueType`: Scalar, Vector, Color, or ResourceReference.
- `DefaultValue`: Material default value.
- `OverrideValue`: Optional instance override value.

**Validation rules**:

- Parameter names must be unique and deterministic.
- Type changes are invalid once a root material defines a parameter.
- Resource reference values are abstract Renderer-level identifiers, not live RHI resources or graph-local handles.
- Empty parameter sets are valid when a shader record has no required parameters.

## Material Resource Reference

**Purpose**: Identifies a texture/resource input needed by a material without owning or resolving the live resource.

**Key fields**:

- `Name`: Stable parameter name.
- `ReferenceId`: Abstract Renderer-level resource identifier.
- `AccessIntent`: Read or sampled/read-like access needed by render graph declaration.
- `ExpectedKind`: Texture, buffer, or unspecified Renderer resource class.

**Validation rules**:

- References must be stable across material and instance inspection.
- References are not executable resources by themselves.
- Resolution into live resources or graph-local handles is deferred to graph or pipeline code.

## Shader Library

**Purpose**: Registry of explicitly registered precompiled shader records and their available variants.

**Key fields**:

- `ShaderRecords`: Ordered collection of shader records.
- `Diagnostics`: Registration, lookup, permutation, and validation diagnostics.
- `ValidationState`: Draft, Ready, Invalid, or Invalidated.

**Validation rules**:

- Shader identities must be unique.
- Only explicitly registered precompiled shader records are available.
- Local file scanning/loading and runtime source compilation are out of scope.
- Invalidated records cannot be selected.

## Shader Record

**Purpose**: Describes one shader family available to materials.

**Key fields**:

- `ShaderId`: Stable identity used by materials.
- `AllowedPermutationFlags`: Declared flag names valid for this shader.
- `Variants`: Available precompiled variants.
- `RequiredParameters`: Parameter names and expected value types needed by this shader.
- `DiagnosticsName`: Human-readable name for dumps and errors.

**Validation rules**:

- Allowed flag names must be unique per shader record.
- Variant identities must be unique within a shader record.
- Required parameters must not contain duplicate names.
- Missing variants fail selection with deterministic diagnostics.

## Shader Permutation

**Purpose**: Deterministic feature-flag set used to select a shader variant.

**Key fields**:

- `Flags`: Stable set of requested flag names.
- `CanonicalKey`: Deterministic representation independent of input order.
- `TargetShaderId`: Shader record being requested.

**Validation rules**:

- Equivalent flag sets produce the same canonical key.
- Flags must be declared by the target shader record.
- Unknown flags fail before variant lookup.

## Shader Variant

**Purpose**: Represents a registered precompiled shader option for a specific permutation.

**Key fields**:

- `VariantId`: Stable variant identity.
- `PermutationKey`: Canonical flag key.
- `StageSummary`: Renderer/RHI-compatible shader stage summary.
- `ParameterExpectations`: Parameters this variant expects.

**Validation rules**:

- Variant permutation key must use only flags declared by the shader record.
- Selection succeeds only when a variant matching the requested canonical key exists.
- Parameter expectations must match material parameter types.

## Material Shader Binding

**Purpose**: Result of validating a material against the shader library and selecting a shader variant.

**Key fields**:

- `MaterialName`
- `ShaderId`
- `VariantId`
- `PermutationKey`
- `ResolvedParameters`
- `ResourceRequirements`
- `Diagnostics`

**Validation rules**:

- Binding exists only for valid materials or valid material instances.
- Binding fails if shader record, requested flags, variant, or required parameters are missing.
- Binding output must be deterministic for repeated equivalent inputs.

## Material Resource Requirement

**Purpose**: Stable resource-needs summary that future render graph passes can declare.

**Key fields**:

- `ParameterName`: Material parameter that created the requirement.
- `ReferenceId`: Abstract Renderer-level resource reference.
- `AccessIntent`: Read-like access needed by the pass.
- `SourceMaterial`: Material or instance that produced the effective value.

**Validation rules**:

- A material with no resource parameters produces an empty requirement list without error.
- Instance overrides replace inherited resource references.
- Requirement ordering must be deterministic.

## Material Diagnostic

**Purpose**: Structured validation or lookup message for material, instance, shader, permutation, resource, and invalidation failures.

**Key fields**:

- `Severity`: Info, Warning, or Error.
- `Category`: Material, Instance, Parameter, ShaderLibrary, Permutation, ResourceRequirement, Invalidation, or Dump.
- `SubjectName`: Material, instance, shader, parameter, or resource reference involved.
- `Message`: Human-readable explanation.
- `StableCode`: Deterministic diagnostic identifier.

**Validation rules**:

- At least 95% of invalid test cases must identify the failing material or shader request.
- Diagnostics must remain stable for equivalent failures.

## Inspection Dump

**Purpose**: Deterministic human-readable text summary for tests and developer inspection.

**Key fields**:

- `MaterialList`
- `InstanceList`
- `ShaderRecordList`
- `PermutationSummary`
- `BindingSummary`
- `ResourceRequirementSummary`
- `Diagnostics`

**Validation rules**:

- Repeated dumps of unchanged inputs must be byte-identical.
- Dump ordering must be stable and not depend on unordered container iteration.
- Dumps must include enough subject identity to locate validation failures.

## State Transitions

```text
Draft
  ├── Validate succeeds -> Valid
  ├── Validate fails    -> Invalid
  └── Invalidate        -> Invalidated

Valid
  ├── Resolve binding succeeds -> Valid
  ├── Resolve binding fails    -> Invalid
  └── Invalidate              -> Invalidated

Invalid
  ├── Reset/clear and rebuild -> Draft
  └── Invalidate             -> Invalidated

Invalidated
  └── Reset/clear and rebuild -> Draft
```
