# Data Model: Core Foundation Math Library

## Vector Value

**Represents**: A 2D, 3D, or 4D floating-point quantity used for positions, directions, dimensions, colors, and homogeneous coordinates.

**Fields**:

- `X`, `Y`, `Z`, `W` as applicable to the dimensionality

**Validation Rules**:

- Components may contain any finite floating-point value.
- Public helpers that accept raw components must document NaN and infinity behavior.
- Normalization of a zero or near-zero vector must not divide by zero.

**Relationships**:

- Used by matrices, quaternions, transforms, colors, boxes, spheres, and planes.

## Matrix Value

**Represents**: A 4x4 transformation value for composing spatial operations and transforming points or vectors.

**Fields**:

- Sixteen floating-point components in documented row-major storage order

**Validation Rules**:

- Identity matrix must preserve transformed inputs.
- Singular matrix inverse requests must report failure deterministically.
- Multiplication and transpose must preserve documented layout and semantics.

**Relationships**:

- Can be produced from quaternion and transform values.
- Can transform vector values.

## Quaternion Value

**Represents**: A rotation value for composing orientations.

**Fields**:

- `X`, `Y`, `Z`, `W` floating-point components

**Validation Rules**:

- Identity quaternion must represent no rotation.
- Normalization of zero or near-zero quaternions must be deterministic.
- Equivalent rotations must compare through tolerance-aware behavior.

**Relationships**:

- Used by transform values.
- Can convert to matrix-compatible rotation behavior.

## Transform Value

**Represents**: A combined translation, rotation, and scale for moving between spaces.

**Fields**:

- `Translation` vector
- `Rotation` quaternion
- `Scale` vector

**Validation Rules**:

- Identity transform must preserve points and directions.
- Zero or near-zero scale inverse behavior must be deterministic.
- Direction transforms must not apply translation.

**State Transitions**:

- Identity -> composed transform through multiplication/composition.
- Valid transform -> non-invertible transform when scale or matrix conversion becomes singular.

## Math Utility Set

**Represents**: Shared scalar operations and tolerance rules.

**Fields**:

- Public tolerance constants
- Public mathematical constants such as pi

**Validation Rules**:

- Clamp must preserve values already inside bounds.
- Interpolation endpoints must return the corresponding endpoint values.
- Degree/radian conversion must be reversible within documented tolerance.

## Color Value

**Represents**: A color with explicit channel ordering and range behavior.

**Fields**:

- Floating-point `R`, `G`, `B`, `A` channels for normalized color workflows
- Byte-channel representation for packed or resource-facing color workflows

**Validation Rules**:

- Channel ordering must remain RGBA.
- Float-to-byte conversion must document clamping and rounding behavior.
- Transparent and opaque defaults must be explicit.

## Box Primitive

**Represents**: An axis-aligned bounding box.

**Fields**:

- Minimum vector
- Maximum vector
- Validity state

**Validation Rules**:

- Empty box must be distinguishable from a valid zero-size box.
- Adding points or boxes must update bounds predictably.
- Containment checks must handle boundary points consistently.

## Sphere Primitive

**Represents**: A bounding sphere.

**Fields**:

- Center vector
- Radius

**Validation Rules**:

- Radius must be non-negative for valid spheres.
- Point containment must account for tolerance at the boundary.
- Construction from invalid inputs must be deterministic.

## Plane Primitive

**Represents**: A plane used for classification and distance queries.

**Fields**:

- Normal vector
- Distance from origin using documented sign convention

**Validation Rules**:

- Normal must be normalized or normalization behavior must be documented.
- Point classification must distinguish front, back, and on-plane within tolerance.
- Construction from degenerate points must report invalid behavior deterministically.

## Tolerance Rule

**Represents**: A named threshold for comparing floating-point results.

**Fields**:

- Default scalar tolerance
- Optional tighter or looser tolerances for specific operations if needed

**Validation Rules**:

- Tests must use documented tolerance rules for computed floating-point results.
- Tolerance values must not hide obviously incorrect results.
