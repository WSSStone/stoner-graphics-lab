# B02-S10: Matrix, Quaternion, Transform, And Geometry Inspection

## Inspection Budget

The inspection covered one spatial-math responsibility domain and six public
Core headers, totaling 690 production lines:

1. `Source/Core/Public/Core/FMatrix4x4.h`
2. `Source/Core/Public/Core/FQuat.h`
3. `Source/Core/Public/Core/FTransform.h`
4. `Source/Core/Public/Core/FBox.h`
5. `Source/Core/Public/Core/FSphere.h`
6. `Source/Core/Public/Core/FPlane.h`

Supporting evidence included Feature 004's specification, data model, API
contract, tests, `FWorld` PreserveWorld reparent implementation, deferred view
validation, and standalone Debug/Release probes. No production implementation
was changed.

## Requirement Mapping

- `004-FR-002` and `FR-004`: matrix and transform APIs exist, but composition
  and inverse success results are not reliable across supported inputs.
- `004-FR-003`: quaternion rotation APIs exist, but large finite and
  non-finite values violate normalization and comparison requirements.
- `004-FR-007`: box, sphere, and plane APIs exist, but extreme finite and
  invalid-input behavior is inconsistent.
- `004-FR-009` and `FR-012`: public tolerance and invalid-number contracts
  are not enforced or documented consistently in this responsibility domain.
- `004-FR-011`: normal baseline tests exist; required boundary and
  invalid-input scenarios are missing.
- `004-FR-010` and `SC-005`: all inspected headers remain Core-only with no
  upward engine dependencies.

## Reproduction

The standalone C++20 probe was compiled and run with Debug-like and optimized
Release flags. Both produced the same output:

```text
quat_huge=0,0 quat_equivalent=0 quat_nonfinite_finite=0
matrix_zero_negative_tol=1 matrix_nan=1 inverse00=nan
transform_composition_matches=0 transform_inverse_succeeded=1 transform_inverse_roundtrip=0 zero_scale_negative_tol=1
plane_scaled_distance=-1 box_center_finite=0 sphere_inf_valid=1 sphere_far_inside=1 plane_negative_tol=1
```

## Findings

### CR001-B02-F007 - Accepted S2

`FQuat` calculates its norm via squared components, so large finite
quaternions overflow and become identity. Non-finite values can reach rotation
output, and `NearlyEquals` rejects `q` versus `-q` although both encode the
same rotation.

### CR001-B02-F008 - Accepted S1

`FMatrix4x4::TryInverse` accepts negative tolerance and non-finite matrix
values, reporting success with NaN output. `FTransform` similarly accepts
negative tolerance for zero scale. More seriously, its TRS composition and
inverse claim exact success with non-uniform scale plus rotation even though
that composition contains shear and cannot be represented by the current TRS
value. `FWorld::Reparent(PreserveWorld)` directly depends on both APIs.

### CR001-B02-F009 - Accepted S2

Geometry primitives lack a shared finite/tolerance policy. `FPlane` normalizes
a supplied normal without rescaling its equation distance; `FBox` center can
overflow for finite bounds; `FSphere` accepts infinite radii and squared
distance arithmetic misclassifies large finite inputs; negative plane
tolerance changes an on-plane answer.

## Confirmed Strengths

- Matrix row-major layout, point/vector translation distinction, multiplication,
  transpose, and normal-scale inverse baseline cases are correct.
- Quaternion identity, axis-angle orientation, Hamilton composition, and
  matrix-compatible behavior are correct for finite normal-sized inputs.
- Transform applies documented scale, rotation, then translation for a single
  transform.
- Default invalid box, normal-size bounds, standard spheres, and plane point
  construction work as expected.
- The inspected types are value-only, allocation-free, and correctly isolated
  from upper engine layers.

## B02-S11 Fix Packet

The following three tightly related findings form the next repair packet:

1. Make quaternion normalization, inverse/rotation, and rotation equivalence
   finite-safe and test the `q/-q` invariant.
2. Make matrix/transform inversion reject invalid inputs and make the TRS API
   truthful about non-uniform-scale rotation composition/inversion rather than
   returning an incorrect successful transform.
3. Define and enforce finite/tolerance behavior for box, sphere, and plane;
   remove finite-overflow false results and verify scaled-normal plane meaning.
