# B06-S07: Shader Library And Permutations Inspection

## Scope

This step inspected Feature 014 shader library, permutation, material binding,
and resource-requirement contracts.

Production files inspected:

- `Source/Renderer/Public/Renderer/FShaderLibrary.h`
- `Source/Renderer/Public/Renderer/FShaderPermutation.h`
- `Source/Renderer/Public/Renderer/FMaterialShaderBinding.h`
- `Source/Renderer/Public/Renderer/FMaterialResourceRequirement.h`
- `Source/Renderer/Private/FShaderLibrary.cpp`
- `Source/Renderer/Private/FShaderPermutation.cpp`
- `Source/Renderer/Private/FMaterialShaderBinding.cpp`
- `Source/Renderer/Private/FMaterialResourceRequirement.cpp`

Supporting evidence:

- `Tests/RendererMaterialShaderTests.cpp`
- `Tests/ShaderTestFixtures.h`
- `specs/014-material-shader-system/spec.md`
- `specs/014-material-shader-system/contracts/material-shader-contract.md`
- `specs/014-material-shader-system/data-model.md`

## Requirement Focus

- `FR-011`: shader library records explicitly registered precompiled shader
  identities, allowed permutation flags, variants, and parameter expectations.
- `FR-012`: shader permutations are deterministic feature-flag sets.
- `FR-014`: unavailable shader, requested permutation, or permutation flag must
  be rejected with diagnostics identifying shader/permutation.
- `FR-018`: requested permutation flags are validated against the target shader
  record before variant selection.
- Data model: allowed flag names are unique per shader record; variant
  identities are unique within a shader record; variant permutation keys use
  only flags declared by the shader record.

## Observations

- `FShaderPermutation::SetFlags` sorts and uniques request flags, so equivalent
  caller requests produce the same canonical key.
- `FShaderLibrary::ResolveVariant` validates requested flags against the target
  shader record before variant lookup and reports deterministic diagnostics for
  missing records, invalidated records, unknown flags, and missing variants.
- `ResolveMaterialShaderBinding` validates shader variant selection and required
  parameters before extracting abstract resource requirements.
- Resource requirement extraction rejects live or graph-local resource handles
  and sorts requirements by parameter name.
- Existing tests cover resolve-time unknown flags, missing shader records,
  missing variants, missing required parameters, invalidated shader records,
  resource requirement extraction, and deterministic repeated resolution.

## Accepted Finding

`CR001-B06-F002` records a shader-record registration validation gap:

- `FShaderLibrary::RegisterShaderRecord` sorts and uniques
  `AllowedPermutationFlags`, but duplicate declarations are silently accepted
  instead of diagnosed.
- It sorts variants by canonical permutation key and variant id, but does not
  reject empty variant ids, duplicate variant ids, duplicate canonical
  permutation keys, or variant permutations containing flags outside the
  record's allowed flag set.
- Existing tests validate unknown flags only during `ResolveVariant`, not at
  registration time.

This conflicts with Feature 014's shader library registry contract and becomes
more important before Feature 024/Asset-backed shader records because cooked or
imported shader metadata should fail at registration/import boundaries rather
than entering the registry as ambiguous data.

## Step Decision

- `CR001-B06-F002`: Accepted S2.
- No production or test source changed in this inspection step.
