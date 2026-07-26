# B02-S07: Scalar, Vector, And Color Math Inspection

## Inspection Budget

The inspection covered one responsibility domain and five production headers,
totaling 533 lines:

1. `Source/Core/Public/Core/FMath.h`
2. `Source/Core/Public/Core/FVector2.h`
3. `Source/Core/Public/Core/FVector3.h`
4. `Source/Core/Public/Core/FVector4.h`
5. `Source/Core/Public/Core/FColor.h`

Supporting evidence included Feature 004's spec, research, data model, public
API contract, tasks, quickstart, checklist, `Tests/CoreMathTests.cpp`, Git
history, and repository-wide call-site searches. No production implementation
was changed.

## Requirement Mapping

- `004-FR-001`: vector construction, arithmetic, exact and near comparison,
  dot/cross products, length, and safe normalization are exposed, but
  normalization is not correct across the finite float domain.
- `004-FR-005`: the required scalar helpers exist, but interpolation does not
  preserve finite endpoints at representability boundaries.
- `004-FR-006`: finite float/byte color conversion is RGBA, clamps to `[0, 1]`,
  and rounds to nearest byte as documented.
- `004-FR-008`: zero/default vector and color values are explicit.
- `004-FR-009`: computed comparisons use an explicit tolerance, but the valid
  tolerance domain is not defined.
- `004-FR-010` and `004-SC-005`: inspected headers depend only on Core and the
  C++ standard library.
- `004-FR-011`: normal cases are covered; several required numeric boundary
  and invalid-input cases are absent.
- `004-FR-012`: coordinate, layout, and finite color conversion conventions
  are documented, but invalid numeric expectations are not.

## Reproduction

A standalone C++20 probe was compiled with both Debug-like and optimized
Release flags. It exercised extreme finite interpolation endpoints, finite
large vector normalization, infinity normalization, negative tolerance, and
NaN/infinity color conversion.

Both builds produced:

```text
lerp0=nan lerp1=-inf
finite_norms=0,0,0 lengths=0,0,0
infinite_norm=nan,0,0
negative_tolerance=0
invalid_color=255,255,0,255
```

The finite vector cases used one `FLT_MAX` axis component. Their mathematically
valid normalized result is the corresponding unit axis.

## Findings

### CR001-B02-F004 - Accepted S2

`FMath::Lerp` evaluates `B - A` before applying `Alpha`. Opposite finite
extremes overflow that intermediate, so Alpha 0 and 1 fail the data model's
explicit endpoint guarantees.

### CR001-B02-F005 - Accepted S2

All vector dimensions calculate magnitude through component squares before
safe normalization. Large finite components overflow `LengthSquared`, causing
valid directions to collapse to zero; a non-finite component can emit NaN.

### CR001-B02-F006 - Accepted S2

Invalid numeric policy and coverage do not satisfy the feature artifacts. T013
claims NaN/infinity vector behavior coverage but only calls `IsFinite` on one
component. T040 claims invalid-input comments exist, but public comments do not
define invalid tolerance, non-finite normalization, or non-finite color
conversion behavior.

## Confirmed Strengths

- Right-handed `FVector3::Cross` orientation is correct.
- Normal-size vector arithmetic, dot products, lengths, and normalization match
  the documented baseline behavior.
- Near comparison is consistently component-wise and uses the shared default
  tolerance.
- Exact equality remains intentionally available for directly constructed
  values, as required by the public contract.
- Finite out-of-range colors clamp and round correctly, including half-byte
  rounding.
- RGBA channel order and transparent/opaque defaults are explicit.
- All five headers are allocation-free and have no upward engine dependency.

## Non-Findings And Limits

- Absolute rather than relative near-equality is not a finding; Feature 004
  requires an explicit shared tolerance but does not require a relative-error
  model.
- Raw vector division by zero follows floating-point arithmetic and is not
  advertised as safe division. This inspection only evaluates the explicitly
  safe normalization path.
- SIMD layout and alignment changes are outside this baseline implementation
  and are explicitly deferred.
- Matrix, quaternion, transform, and geometry implementations are reserved for
  subsequent B02 responsibility domains.

## B02-S08 Fix Packet

The next step may repair the three related findings:

1. Use an overflow-resistant C++20 interpolation implementation and add exact
   endpoint tests with opposite finite extremes.
2. Make vector length and safe normalization scale-resistant across
   `FVector2`, `FVector3`, and `FVector4`; cover large finite axes and
   diagonals.
3. Define deterministic policies for invalid/non-finite tolerances, vector
   normalization, and color conversion in public comments, then replace the
   nominal infinity assertion with behavioral tests.

