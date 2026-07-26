# B02-S11: Matrix, Quaternion, Transform, And Geometry Fix

## Findings Resolved

- `CR001-B02-F007` (S2): quaternion normalization and rotation equivalence.
- `CR001-B02-F008` (S1): spatial composition/inversion false success.
- `CR001-B02-F009` (S2): geometry finite/tolerance/extreme behavior.

Implementation commit: `70cacb7`.

## Quaternion Repair

- Length and safe normalization use scale-resistant magnitude calculations.
- Inverse avoids squared-magnitude overflow and preserves large finite values.
- Non-finite values, invalid tolerances, and invalid axis-angle input use the
  documented Identity fallback.
- Rotation uses the normalized conjugate path.
- `NearlyEquals` accepts both direct component proximity and the equivalent
  negated quaternion.

## Matrix And Transform Repair

- Matrix inverse rejects non-finite matrices and invalid tolerances, detects
  non-finite elimination intermediates, and leaves Identity on failure.
- Infallible `FTransform::operator*` was replaced by `TryCompose`,
  `TryInverse`, and `TryRelativeTo`.
- Exact matrix results are decomposed only when their basis remains orthogonal;
  representable rotated non-uniform scales succeed, while shear returns false
  with Identity output.
- Scene hierarchy validates prospective world/local transforms before mutation.
  Unrepresentable changes return `InvalidHierarchyOperation`, preserve the old
  parent/local state, and emit
  `SCENE-HIERARCHY-TRANSFORM-UNREPRESENTABLE`.
- Feature 004 and 017 contracts plus decision D002 record the mathematical TRS
  boundary and public migration.

## Geometry Repair

- Boxes reject non-finite points and bounds; midpoint/extent calculations remain
  finite at float extrema.
- Spheres require finite centers/radii/tolerances and use double-precision
  distance calculation to avoid squared-distance overflow.
- Planes normalize normal and distance equation coefficients together, build
  point normals with double intermediates, saturate unrepresentable signed
  distances, and return deterministic `On` for invalid classification inputs.

## Local Verification

- Isolated CoreMath Debug: `91 passed, 0 failed`.
- Isolated CoreMath optimized Release: `91 passed, 0 failed`.
- Strict Debug fallback full build and test suite: passed.
- Strict Release full build: passed.
- Release full test suite: passed before the final additional quaternion
  assertion; the final isolated Release CoreMath suite covers that assertion.
- ASan/UBSan strict full build and test suite: passed.
- `git diff --check`: passed.
- No remaining infallible FTransform composition call sites.

## Finding State

F007-F009 are `Fixed` at `70cacb7`. Their final `Verified` transition remains
reserved for B02-S12 local independent verification and the B02 batch-boundary
Windows/macOS/Linux matrix.
