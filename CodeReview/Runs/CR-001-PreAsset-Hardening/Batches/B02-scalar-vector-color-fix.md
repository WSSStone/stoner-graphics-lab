# B02-S08: Scalar, Vector, And Color Math Fix

## Scope

Commit `e077419` resolves the three related findings accepted in B02-S07:

- `CR001-B02-F004`: finite Lerp endpoint failure.
- `CR001-B02-F005`: large finite vector normalization failure.
- `CR001-B02-F006`: missing invalid numeric policy and behavior tests.

## Implementation

1. `FMath::Lerp` now uses C++20 `std::lerp`, preserving opposite finite
   extremes at Alpha 0 and 1 and producing a finite midpoint.
2. `FVector2`, `FVector3`, and `FVector4` calculate length with
   overflow-resistant `std::hypot`.
3. Safe normalization validates components and tolerance, scales by the
   largest absolute component, and normalizes the bounded scaled vector.
4. Safe normalization returns Zero for non-finite components, negative or
   non-finite tolerance, and near-zero magnitude.
5. Near comparisons return false for non-finite compared values or invalid
   tolerance.
6. Float-to-byte color conversion maps NaN to zero and clamps infinities by
   sign.
7. Public comments define these invalid-input policies.

## Test Expansion

Core math coverage increased from 67 to 77 passing assertions:

- opposite-`FLT_MAX` Lerp endpoints and midpoint;
- invalid and non-finite near comparisons;
- overflow-resistant `FVector2` length;
- large finite axis normalization for every vector dimension;
- large finite `FVector3` diagonal normalization;
- non-finite normalization for every vector dimension;
- invalid normalization tolerance;
- NaN and signed-infinity color conversion.

## Local Verification

- `git diff --check`: passed.
- Focused C++20 Debug and optimized Release probes: passed.
- Strict Debug fallback build with `-Werror`: passed.
- Full deterministic test executable: passed.
- Core math tests: `77 passed, 0 failed`.
- Strict Release build with `-Werror`: passed.
- ASan/UBSan strict Debug build and full test executable: passed.

Authoritative gate records:

- `Evidence/gate-fallback-strict.json`
- `Evidence/gate-strict-release.json`
- `Evidence/gate-sanitizers.json`

## Finding State And Remote Policy

The three findings are Fixed at `e077419`. Hosted cross-platform verification
is intentionally deferred to the B02 batch boundary to conserve the
repository's GitHub Actions quota. This step is committed locally and is not
pushed.

