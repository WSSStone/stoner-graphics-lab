# B06-S10: Material Definitions, Instances, And Parameters Inspection

## Scope

This step inspected Feature 014 material definition, material instance, and
material parameter behavior.

Production files inspected:

- `Source/Renderer/Public/Renderer/FMaterial.h`
- `Source/Renderer/Public/Renderer/FMaterialInstance.h`
- `Source/Renderer/Public/Renderer/FMaterialParameterSet.h`
- `Source/Renderer/Public/Renderer/FMaterialDiagnostics.h`
- `Source/Renderer/Private/FMaterial.cpp`
- `Source/Renderer/Private/FMaterialInstance.cpp`
- `Source/Renderer/Private/FMaterialParameterSet.cpp`
- `Source/Renderer/Private/FMaterialDiagnostics.cpp`

Supporting evidence:

- `Source/Renderer/Private/FMaterialShaderBinding.cpp`
- `Source/Renderer/Private/FMaterialResourceRequirement.cpp`
- `Tests/RendererMaterialShaderTests.cpp`
- `specs/014-material-shader-system/spec.md`
- `specs/014-material-shader-system/contracts/material-shader-contract.md`
- `specs/014-material-shader-system/data-model.md`
- `specs/014-material-shader-system/tasks.md`

## Requirement Focus

- `FR-007`: material instances reference a base material or parent instance and
  override selected parameters.
- `FR-008`: effective parameters are resolved by walking the parent chain with
  nearest-override precedence.
- `FR-009`: overrides for parameters not defined by the root material are
  rejected.
- `FR-010`: override value types must match the parent parameter type.
- `FR-015`: material and material-instance resource requirements are exposed in
  stable render-graph-compatible form.
- `FR-017`: inheritance cycles are rejected with diagnostics.
- Contract/data model: invalidated parents are rejected, and bindings exist only
  for valid materials or valid material instances.

## Observations

- `FMaterial::Validate` rejects empty names, empty shader references,
  unsupported domain/blend pairs, invalid resource references, and use after
  material invalidation.
- `FMaterialParameterSet::AddParameter` rejects empty and duplicate names, keeps
  deterministic ordering, and `SetParameter` updates existing values before
  falling back to insertion.
- `FMaterialInstance::Validate` detects cycles, requires a root material,
  rejects an invalidated root material, rejects unknown overrides, rejects type
  mismatches, resolves effective parameters, and then marks the instance valid.
- `FMaterialInstance::ResolveEffectiveParameters` rechecks cycles, discovers
  the root, copies root parameters, applies parent-to-child overrides, and
  rejects unknown or type-mismatched overrides during resolution.
- Tests cover no-override inheritance, parent/child nearest override
  precedence, unknown overrides, type mismatches, cycles, `Validate()` against
  an already-invalidated parent material, and resource requirement override
  replacement.

## Accepted Finding

`CR001-B06-F003` records an invalidation bypass in material-instance runtime
paths:

- `FMaterialInstance::ResolveEffectiveParameters` copies
  `Root->GetParameters()` without checking whether the root material is now
  invalidated.
- `ResolveMaterialShaderBinding(const FMaterialInstance&)` and
  `ExtractMaterialResourceRequirements(const FMaterialInstance&)` call
  `FindRootMaterial`/`ResolveEffectiveParameters` directly, so they can produce
  render-ready shader bindings or resource requirements after an instance was
  validated and its root material was later invalidated.
- Existing tests only cover `Validate()` when the parent is invalidated before
  validation; they do not cover invalidation after a previously valid instance
  enters binding or resource extraction.

This conflicts with the Feature 014 contract that invalidated parents are
rejected and that bindings exist only for valid materials or valid material
instances.

## Step Decision

- `CR001-B06-F003`: Accepted S2.
- No production or test source changed in this inspection step.
