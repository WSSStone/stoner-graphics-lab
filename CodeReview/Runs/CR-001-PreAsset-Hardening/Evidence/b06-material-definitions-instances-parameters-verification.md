# B06-S12 Evidence: Material Definitions, Instances, And Parameters Verification

Step: `B06-S12`.

Finding verified:

- `CR001-B06-F003`: Material instance binding and resource paths can use
  invalidated parents.

Verification evidence:

- `git show 7f525af^:Source/Renderer/Private/FMaterialInstance.cpp` showed
  `ResolveEffectiveParameters` copied root parameters after `FindRootMaterial`
  with no invalidated-root check.
- Current `Source/Renderer/Private/FMaterialInstance.cpp` calls
  `ValidateUsableParentChain` before root parameters are copied.
- Current `Tests/RendererMaterialShaderTests.cpp` includes the three required
  invalidation-after-validation regressions.
- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/gate-fallback-strict.json`
  records a passing `fallback-strict` gate at
  `2026-07-27T06:44:30+00:00`.

Result:

- `CR001-B06-F003` is Verified.
