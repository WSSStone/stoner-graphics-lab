# B06-S10 Evidence: Material Definitions, Instances, And Parameters Inspection

Step: `B06-S10`.

Inspected material definitions, instances, parameter sets, binding, and resource
requirement paths against Feature 014 contracts.

Key evidence:

- `FMaterialInstance::Validate` rejects invalidated root materials.
- `FMaterialInstance::ResolveEffectiveParameters` does not reject an invalidated
  root material before copying root parameters.
- `ResolveMaterialShaderBinding(const FMaterialInstance&)` and
  `ExtractMaterialResourceRequirements(const FMaterialInstance&)` call the
  resolver directly and can bypass the explicit `Validate()` invalidated-parent
  check after parent invalidation.
- Tests cover invalidated parent rejection only through `Validate()` on an
  already-invalidated parent; they do not cover binding/resource extraction
  after a previously valid instance's parent is invalidated.

Finding:

- `CR001-B06-F003`: Material instance binding and resource paths can use
  invalidated parents. Severity S2, Accepted.

No production or test source changed in this inspection step.
