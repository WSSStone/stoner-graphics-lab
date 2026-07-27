# B06-S08: Shader Library And Permutations Fix

## Repair Target

Implementation commit `0daa334` fixes `CR001-B06-F002`: shader library
registration accepted invalid or ambiguous variant metadata.

## Repair Summary

`FShaderLibrary::RegisterShaderRecord` now validates shader-record internal
consistency before insertion:

- duplicate allowed permutation flags are rejected with
  `MAT-SHADER-FLAG-DUPLICATE`;
- empty allowed permutation flags are rejected with `MAT-SHADER-FLAG-EMPTY`;
- empty variant ids are rejected with `MAT-SHADER-VARIANT-ID-EMPTY`;
- variant permutations using undeclared flags are rejected with
  `MAT-SHADER-VARIANT-FLAG`;
- duplicate variant ids are rejected with `MAT-SHADER-VARIANT-DUPLICATE`;
- duplicate canonical variant permutation keys are rejected with
  `MAT-SHADER-VARIANT-KEY-DUPLICATE`.

Existing resolve-time behavior is preserved: requested unknown flags still fail
before variant lookup, and missing variants remain deterministic `NotFound`
results.

## Tests Added

New regression coverage in `Tests/RendererMaterialShaderTests.cpp`:

- `[PASS] Shader library rejects duplicate allowed permutation flags at registration`
- `[PASS] Shader library rejects variant permutation flags not declared by the record`
- `[PASS] Shader library rejects duplicate shader variant ids at registration`
- `[PASS] Shader library rejects duplicate shader variant permutation keys at registration`

## Local Gates

- `scons config=debug`: passed after the source/test patch.
- Focused `StonerTest | rg ...`: all four new registration tests passed; the
  graphics-enabled local run still shows the current Mac MoltenVK/Metal
  environment boundary and triangle deterministic lifecycle failure.
- `fallback-strict`: passed at `2026-07-27T06:24:18+00:00`.
- `strict-release`: passed at `2026-07-27T06:24:35+00:00`.
- `sanitizers`: passed at `2026-07-27T06:25:57+00:00`.

## Finding State

- `CR001-B06-F002`: Fixed at `0daa334`.

B06 verify should independently confirm parent/current behavior and then move
the finding to Verified.
