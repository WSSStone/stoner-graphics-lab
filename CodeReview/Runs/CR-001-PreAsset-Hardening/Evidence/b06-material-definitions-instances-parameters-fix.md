# B06-S11 Evidence: Material Definitions, Instances, And Parameters Fix

Step: `B06-S11`.

Finding fixed:

- `CR001-B06-F003`: Material instance binding and resource paths can use
  invalidated parents.

Resolution:

- `FMaterialInstance::Validate` and
  `FMaterialInstance::ResolveEffectiveParameters` now both call the shared
  `ValidateUsableParentChain` helper.
- The helper rejects invalidated current instances, invalidated parent
  instances, missing root materials, and invalidated root materials before
  effective parameters can be copied or consumed by binding/resource paths.

Regression evidence:

- `[PASS] Material instance resolution rejects invalidated parent instances after validation`
- `[PASS] Material shader binding rejects instances whose parent material is invalidated after validation`
- `[PASS] Material instance resource requirements reject invalidated parents after validation`

Gate evidence:

- `scons config=debug`: passed.
- `Build/Mac/Debug/Tests/StonerTest`: passed.
- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/gate-fallback-strict.json`
  records `fallback-strict` passed at `2026-07-27T06:44:30+00:00`.

Code commit:

- `7f525af`: `fix(renderer): reject invalidated material instance parents`
