# B02-S12: Spatial Math Verification

## Scope

This step independently verifies commit `70cacb7` and its three related
findings:

- `CR001-B02-F007`: scale-resistant quaternion normalization, inversion, and
  rotation equivalence;
- `CR001-B02-F008`: exact, failure-aware TRS composition/inverse/relative
  conversion and transactional Scene hierarchy migration;
- `CR001-B02-F009`: finite-safe box, sphere, and plane edge behavior.

No production or specification file was changed during verification.

## Independent Review

The seven Core math headers, `FWorld.cpp`, affected tests, Feature 004/017
contracts, and decision D002 were re-read independently. The review confirmed:

- every failed `FTransform` Try operation initializes its output to Identity;
- composition is checked against the exact matrix product before acceptance;
- decomposition rejects non-orthogonal affine shear and preserves exact
  reflections by carrying a negative determinant into one scale axis;
- Scene validates prospective local/world transforms before mutating parent,
  child, root, or local-transform state;
- a parent without a transform remains distinct from a failed parent transform;
- invalid numeric inputs and tolerances cannot escape as successful geometry or
  inverse results;
- no infallible `FTransform::operator*` call remains.

No new defect was found in the repaired responsibility domain.

## Deterministic Property Probe

An independent C++20 probe under `/tmp` performed 1,597 checks in each build:

- Debug: `1597 checks, 0 failures`;
- optimized Release (`-O2 -DNDEBUG`): `1597 checks, 0 failures`.

The probe compared accepted transforms with independently multiplied matrices
and covered arbitrary uniform composition, axis permutations, negative scale
and reflection, generic shear rejection, Identity-on-failure, large finite
quaternions, `q/-q` equivalence, and extreme finite geometry.

## Repository Gates

- `git show --check 70cacb7`: passed.
- Strict Debug build: passed.
- Strict Release build: passed.
- Strict graphics-disabled full suite: passed.
- Debug full suite with the already-tracked optional deferred-native check
  skipped: passed; CoreMath `91/0`.
- Release full suite with the same optional check skipped: passed; CoreMath
  `91/0`.
- ASan/UBSan strict build and full suite: passed.

The ordinary local native suite remains subject to accepted finding
`CR001-B08-F001` (intermittent MoltenVK deferred validation). It is unrelated
to this responsibility domain and is not used to weaken the deterministic or
sanitizer evidence.

## Finding State

`CR001-B02-F007`, `CR001-B02-F008`, and `CR001-B02-F009` remain `Fixed`.
Feature 004 requires Windows, macOS, and Linux evidence, so all three transition
to `Verified` only after the B02 batch-boundary GitHub Actions matrix passes.

No push or GitHub Actions run was triggered by this step.
