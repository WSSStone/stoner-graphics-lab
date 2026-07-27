# B06-S11: Material Definitions, Instances, And Parameters Fix

## Scope

Fixed `CR001-B06-F003`, an invalidated-parent bypass in material instance
binding and resource requirement paths.

Code changed:

- `Source/Renderer/Public/Renderer/FMaterialInstance.h`
- `Source/Renderer/Private/FMaterialInstance.cpp`
- `Tests/RendererMaterialShaderTests.cpp`

## Fix

- Added `FMaterialInstance::ValidateUsableParentChain` so validation and
  effective-parameter resolution share the same parent-chain usability gate.
- The shared gate rejects:
  - the current instance after `Invalidate()`;
  - invalidated parent instances in the inheritance chain;
  - missing root materials;
  - invalidated root materials.
- `FMaterialInstance::Validate` now uses the shared gate before checking
  overrides.
- `FMaterialInstance::ResolveEffectiveParameters` now uses the shared gate
  before copying root parameters, so callers that resolve through shader binding
  or resource requirement extraction cannot bypass invalidated-parent rejection.

## Regression Coverage

Added tests for:

- effective-parameter resolution after a parent instance is invalidated
  post-validation;
- material shader binding after a root material is invalidated post-validation;
- material instance resource requirement extraction after a root material is
  invalidated post-validation.

## Verification

- `scons config=debug`: passed.
- `Build/Mac/Debug/Tests/StonerTest`: passed.
- `crctl gate fallback-strict --id CR-001`: passed at
  `2026-07-27T06:44:30+00:00`.

## Commit

- `7f525af`: `fix(renderer): reject invalidated material instance parents`
