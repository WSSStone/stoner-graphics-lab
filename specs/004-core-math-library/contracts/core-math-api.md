# Contract: Core Math Public API

## Scope

The Core math API provides engine-wide math values and helpers under the Core public include path. It must be usable by Core-only code and by later RHI, Backend, Renderer, and Application code without introducing dependencies on those higher layers.

## Public Include Contract

The aggregate Core header must expose the math API:

```cpp
#include "Core/CoreMinimal.h"
```

Focused headers must also be usable independently:

```cpp
#include "Core/FMath.h"
#include "Core/FVector2.h"
#include "Core/FVector3.h"
#include "Core/FVector4.h"
#include "Core/FMatrix4x4.h"
#include "Core/FQuat.h"
#include "Core/FTransform.h"
#include "Core/FColor.h"
#include "Core/FBox.h"
#include "Core/FSphere.h"
#include "Core/FPlane.h"
```

## Namespace Contract

All public math deliverables must live in:

```cpp
namespace Stoner::Core
```

## Coordinate and Numeric Contract

- Core math uses a right-handed coordinate convention.
- `FMatrix4x4` stores components in row-major memory order.
- Matrix multiplication, vector transformation, point transformation, and transform composition semantics must be documented by tests and comments.
- Computed floating-point comparisons must use explicit tolerance helpers rather than exact equality.
- NaN and infinity inputs must not crash public scenarios that accept raw numeric components; behavior must be documented and covered where practical.

## Type Contracts

### `FMath`

Must provide common scalar constants and helpers:

- Pi-related constants
- `Min`, `Max`, `Clamp`, `Abs`
- `Lerp`
- `DegreesToRadians`, `RadiansToDegrees`
- Trigonometric wrappers needed by quaternion and transform scenarios
- `Sqrt`
- `IsNearlyEqual`, `IsNearlyZero`

### `FVector2`, `FVector3`, `FVector4`

Must support:

- Default and component construction
- Zero and unit-axis constants or constructors where applicable
- Component access by public named fields or equivalent direct accessors
- Addition, subtraction, scalar multiply/divide, unary negation
- Length, squared length, normalization, safe normalization
- Dot product for all vector dimensions
- Cross product for `FVector3`
- Exact comparison for direct component equality and near-equality helper behavior for computed results

### `FMatrix4x4`

Must support:

- Identity construction
- Component construction or named factory behavior
- Matrix multiplication
- Transpose
- Deterministic inverse behavior for invertible and non-invertible matrices
- Point and direction/vector transformation with documented translation behavior

### `FQuat`

Must support:

- Identity construction
- Component construction
- Construction from common rotation inputs selected during implementation
- Normalization and safe normalization
- Quaternion multiplication/composition
- Matrix-compatible rotation behavior
- Near-equality behavior for computed rotations

### `FTransform`

Must support:

- Identity construction
- Construction from translation, rotation, and scale
- Point transformation
- Direction/vector transformation without translation
- Try-based composition that returns failure rather than an approximation when
  the exact affine result contains shear
- Deterministic Try-based inverse and relative-transform behavior for valid,
  non-invertible, invalid-numeric, and non-representable transforms
- Identity output on failed composition, inverse, or relative conversion

### `FColor`

Must support:

- Float RGBA construction
- Byte-channel construction or conversion
- Transparent and opaque defaults
- Predictable RGBA channel ordering
- Documented clamping and rounding behavior for float/byte conversion
- Comparison and near-equality behavior where applicable

### `FBox`

Must support:

- Empty/default state
- Construction from min/max values
- Adding points and combining boxes
- Validity query
- Point containment
- Center and extent queries for valid boxes

### `FSphere`

Must support:

- Construction from center and radius
- Validity query
- Point containment
- Boundary behavior using documented tolerance

### `FPlane`

Must support:

- Construction from normal and distance
- Construction from points if non-degenerate input is available
- Validity query
- Signed distance to point
- Point classification as front, back, or on-plane within tolerance

## Dependency Contract

The public math headers may depend on:

- C++ standard library headers needed for scalar math and limits
- Existing Core foundation headers from `Source/Core/Public/Core/`

The public math headers must not include:

- RHI headers
- Backend headers
- Renderer headers
- Application headers
- Platform windowing headers
- Graphics API headers
- Physics system headers

## Verification Contract

The `StonerTest` executable must verify:

- Construction and arithmetic for every vector type
- Matrix identity, multiplication, transpose, inverse, and transform behavior
- Quaternion identity, normalization, composition, and matrix-compatible behavior
- Transform identity, composition, point transform, direction transform, and inverse behavior
- Scalar utility boundary behavior
- Color channel ordering, conversion, and boundary cases
- Box, sphere, and plane validity, containment, combination, and classification
- Cross-platform deterministic behavior within documented tolerances
