# B06-S12: Material Definitions, Instances, And Parameters Verification

## Scope

Verified `CR001-B06-F003` after code commit `7f525af`.

## Source Verification

- Parent commit `7f525af^` showed
  `FMaterialInstance::ResolveEffectiveParameters` discovering the root through
  `FindRootMaterial` and immediately copying `Root->GetParameters()` without a
  root invalidation check.
- Current `FMaterialInstance::ResolveEffectiveParameters` calls
  `ValidateUsableParentChain` before copying root parameters.
- `ValidateUsableParentChain` rejects invalidated current instances,
  invalidated parent instances, missing root materials, and invalidated root
  materials.
- `FMaterialInstance::Validate` also uses the shared gate, so validation and
  effective-parameter resolution now share the same parent-chain invalidation
  semantics.

## Regression Verification

`Tests/RendererMaterialShaderTests.cpp` contains regressions for:

- material instance resolution after a parent instance is invalidated
  post-validation;
- material shader binding after a root material is invalidated
  post-validation;
- material instance resource requirement extraction after a root material is
  invalidated post-validation.

The saved test output recorded all three as passing.

## Gate Verification

- `scons config=debug`: passed.
- `Build/Mac/Debug/Tests/StonerTest`: passed.
- `crctl gate fallback-strict --id CR-001`: passed at
  `2026-07-27T06:44:30+00:00`.

## Decision

- `CR001-B06-F003`: Verified.
