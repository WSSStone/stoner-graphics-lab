# B06-S09: Shader Library And Permutations Verification

## Verification Target

`CR001-B06-F002` verifies implementation commit `0daa334`: shader library
registration must reject invalid or ambiguous variant metadata before inserting
shader records.

## Parent Behavior

Parent revision `0daa334^` showed:

- `RegisterShaderRecord` sorted and uniqued `AllowedPermutationFlags`.
- It sorted variants by canonical permutation key and variant id.
- It inserted the record without rejecting duplicate allowed flags, empty
  variant ids, duplicate variant ids, duplicate canonical variant keys, or
  variant flags outside the allowed flag set.

## Current Behavior

Current revision validates shader-record metadata before insertion:

- duplicate allowed flags -> `MAT-SHADER-FLAG-DUPLICATE`;
- empty allowed flags -> `MAT-SHADER-FLAG-EMPTY`;
- empty variant ids -> `MAT-SHADER-VARIANT-ID-EMPTY`;
- undeclared variant flags -> `MAT-SHADER-VARIANT-FLAG`;
- duplicate variant ids -> `MAT-SHADER-VARIANT-DUPLICATE`;
- duplicate canonical variant keys -> `MAT-SHADER-VARIANT-KEY-DUPLICATE`.

Resolve-time behavior remains intact: requested unknown flags still fail before
variant lookup, and missing variants still return deterministic `NotFound`.

## Regression Evidence

New registration-time tests are present:

- `Shader library rejects duplicate allowed permutation flags at registration`
- `Shader library rejects variant permutation flags not declared by the record`
- `Shader library rejects duplicate shader variant ids at registration`
- `Shader library rejects duplicate shader variant permutation keys at registration`

Focused local output observed all four checks passing.

## Local Gates

- `fallback-strict`: passed at `2026-07-27T06:24:18+00:00`.
- `strict-release`: passed at `2026-07-27T06:24:35+00:00`.
- `sanitizers`: passed at `2026-07-27T06:25:57+00:00`.

## Finding Decision

- `CR001-B06-F002`: Verified.

B06 shader library/permutation scope has no remaining accepted finding after
this verification step.
