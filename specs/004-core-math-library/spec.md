# Feature Specification: Core Foundation Math Library

**Feature Branch**: `004-core-math-library`  
**Created**: 2026-04-27  
**Status**: Draft  
**Input**: User requested the next spec feature based on `doc/roadmap.md` and current development progress; selected the next critical-path roadmap phase: "Core math library for the graphics engine: FVector2/3/4, FMatrix4x4, FQuat, FTransform, FMath utilities, FColor (float and uint8), and basic geometric primitives (FBox, FSphere, FPlane). UE5 naming conventions. Cross-platform. SIMD-ready design (naive implementation first). Comprehensive unit tests including edge cases."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Use Core Spatial Values in Engine Code (Priority: P1)

An engine developer building RHI, Renderer, or Application features needs a common vocabulary for positions, directions, dimensions, colors, and numeric math. They can use Core-provided vector, matrix, quaternion, transform, color, and math utility values without inventing local substitutes.

**Why this priority**: Later RHI and Renderer phases depend on shared math types. Without stable Core spatial values, every layer would define incompatible representations for geometry, transforms, and colors.

**Independent Test**: Can be tested by building a Core-only usage sample that creates vectors, matrices, quaternions, transforms, colors, and scalar math results without including higher engine layers.

**Acceptance Scenarios**:

1. **Given** a developer needs to represent 2D, 3D, or 4D numeric values, **When** they create and combine Core vector values, **Then** the values support expected arithmetic, comparison, length, normalization, dot product, and cross product behavior where applicable.
2. **Given** a developer needs to represent orientation or coordinate conversion, **When** they use Core matrix and quaternion values, **Then** they can compose rotations and transforms with predictable identity and inverse behavior.
3. **Given** a developer needs common numeric helpers, **When** they use Core math utilities, **Then** they can clamp, interpolate, compare near-equal values, and convert between degrees and radians consistently.

---

### User Story 2 - Validate Geometry and Color Building Blocks (Priority: P2)

A rendering developer preparing basic scene and resource code needs simple geometric primitives and color values that can be passed through engine systems consistently. They can represent bounds, planes, spheres, and colors in Core without depending on Renderer-specific code.

**Why this priority**: Bounding volumes and color values appear throughout visibility, resource setup, debug drawing, and later rendering features. They must be available before higher-level systems can share data safely.

**Independent Test**: Can be tested by constructing boxes, spheres, planes, and colors in isolation and verifying containment, intersection, normalization, conversion, and boundary behavior.

**Acceptance Scenarios**:

1. **Given** a developer creates a box or sphere from points or dimensions, **When** they query containment or combine bounds, **Then** the result matches the expected geometric relationship.
2. **Given** a developer creates a plane from a normal and distance or from points, **When** they classify points against the plane, **Then** the result is stable for points in front of, behind, and on the plane within the documented tolerance.
3. **Given** a developer creates float or byte color values, **When** they convert or compare those values, **Then** channel ordering, range handling, and equality behavior are predictable.

---

### User Story 3 - Confirm Cross-Platform Math Consistency (Priority: P3)

A maintainer needs confidence that math behavior remains consistent across Windows, macOS, and Linux before RHI and Renderer features depend on it. They can run the math verification suite on each supported platform and compare the public behavior.

**Why this priority**: Cross-platform consistency is required for engine correctness, but it follows the initial creation of the math vocabulary and geometric primitives.

**Independent Test**: Can be tested by running Core math verification on every supported platform and confirming that the same deterministic scenarios pass within documented floating-point tolerances.

**Acceptance Scenarios**:

1. **Given** the Core math verification suite runs on any supported platform, **When** it checks vector, matrix, quaternion, transform, color, and geometry scenarios, **Then** all required scenarios pass within the documented tolerances.
2. **Given** a floating-point scenario is sensitive to precision, **When** results are compared, **Then** the comparison uses explicit tolerances and reports failures clearly.
3. **Given** later SIMD optimization is introduced, **When** optimized and baseline paths are compared, **Then** they produce equivalent public results within the same tolerances.

---

### Edge Cases

- Zero vectors must behave predictably when length, normalization, and near-equality are queried.
- Matrix and transform identity values must preserve input points, vectors, colors, and orientations where applicable.
- Singular or non-invertible matrices must fail or report invalid inverse operations deterministically without corrupting unrelated values.
- TRS composition, inverse, or relative conversion that would require shear MUST report failure without returning an approximate transform.
- Quaternion operations must handle identity rotations, near-zero length inputs, repeated composition, and equivalent rotations consistently.
- Floating-point comparisons must account for tiny precision differences without hiding meaningful errors.
- Colors with out-of-range channels, transparent values, opaque values, and byte/float conversion boundaries must be handled consistently.
- Empty or invalid bounds must remain distinguishable from valid zero-size bounds.
- Plane classification must remain stable for points close to the plane within the documented tolerance.
- NaN or infinite numeric inputs must not cause crashes; behavior must be documented and verified for the public scenarios that accept raw numeric input.

## Architecture & Design Constraints *(mandatory)*

- **RHI Abstraction**: This feature belongs to the Core foundation and MUST NOT depend on RHI, Backend, Renderer, Application, or any graphics API behavior.
- **Design Patterns**: Responsibilities MUST remain separated by math category; the feature MUST NOT introduce a single global manager that owns all math behavior.
- **Advanced Graphics**: The math vocabulary MUST be suitable for later high-performance rendering work, including camera transforms, bounding volumes, color values, mesh data, ray tracing, meshlets, and global illumination inputs.
- **Naming Conventions**: Public deliverables MUST follow PascalCase, UnrealEngine5-style naming conventions, including the planned names `FVector2`, `FVector3`, `FVector4`, `FMatrix4x4`, `FQuat`, `FTransform`, `FMath`, `FColor`, `FBox`, `FSphere`, and `FPlane`.
- **Cross-Platform Compatibility**: Observable math behavior MUST be consistent across Windows, macOS, and Linux, with platform-specific optimization details hidden from feature users.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST provide Core vector values for 2D, 3D, and 4D floating-point quantities with arithmetic, component access, comparison, length, normalization, dot product, and cross product behavior where mathematically applicable.
- **FR-002**: The system MUST provide a 4x4 matrix value that supports identity construction, multiplication, transpose, inverse handling, and point/vector transformation.
- **FR-003**: The system MUST provide a quaternion value for rotations, including identity rotation, construction from common rotation inputs, normalization, multiplication/composition, and conversion to or from matrix-compatible rotation behavior.
- **FR-004**: The system MUST provide a transform value that combines translation, rotation, and scale and can transform points and directions predictably; composition, inverse, and relative conversion MUST expose deterministic failure when the exact affine result cannot be represented by translation, rotation, and scale alone.
- **FR-005**: The system MUST provide math utilities for common scalar operations, including clamp, min, max, absolute value, interpolation, degree/radian conversion, trigonometric helpers, square root, and near-equality comparison.
- **FR-006**: The system MUST provide color values for floating-point and byte channel use cases with predictable channel ordering, construction, comparison, and conversion behavior.
- **FR-007**: The system MUST provide basic geometric primitives for boxes, spheres, and planes with construction, validity, containment, combination, and classification behavior appropriate to each primitive.
- **FR-008**: Every public math value MUST expose explicit identity, zero, or invalid/default behavior where that concept is meaningful.
- **FR-009**: Floating-point equality and validation MUST use documented tolerance rules for scenarios where exact equality is not reliable.
- **FR-010**: Public Core math capabilities MUST be accessible without including or depending on higher engine layers.
- **FR-011**: The feature MUST include verification coverage for normal, boundary, and invalid-input cases across vectors, matrices, quaternions, transforms, math utilities, colors, boxes, spheres, and planes.
- **FR-012**: Public documentation or discoverable comments MUST describe coordinate, transform, color channel, tolerance, and invalid-input expectations used by the math library.
- **FR-013**: The feature MUST leave room for later SIMD optimization without requiring callers to change public math usage.
- **FR-014**: The feature MUST exclude spatial acceleration structures, physics-specific math, camera systems, animation systems, and renderer-owned scene data.

### Key Entities *(include if feature involves data)*

- **Vector Value**: A 2D, 3D, or 4D numeric value used for positions, directions, dimensions, colors, and homogeneous coordinates.
- **Matrix Value**: A 4x4 transformation value used to compose spatial operations and transform points or vectors.
- **Quaternion Value**: A rotation value used to represent and compose orientations without relying on Euler-only behavior.
- **Transform Value**: A combined translation, rotation, and scale value used to move between local and world spaces.
- **Math Utility Set**: A collection of scalar helpers and tolerance rules used by Core and later engine layers.
- **Color Value**: A floating-point or byte channel representation used to pass color data consistently through engine systems.
- **Geometric Primitive**: A box, sphere, or plane value used for bounds, classification, containment, and later visibility workflows.
- **Tolerance Rule**: A documented numeric threshold used to compare floating-point results safely.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer can create a Core-only sample that uses all required math categories in under 20 minutes using the names and behavior described by this spec.
- **SC-002**: Verification covers 100% of required math categories: vectors, matrices, quaternions, transforms, math utilities, colors, boxes, spheres, and planes.
- **SC-003**: The Core math verification suite completes with zero failures on Windows, macOS, and Linux.
- **SC-004**: At least 95% of defined boundary and invalid-input scenarios pass before this feature is considered complete, and any accepted gaps are documented with rationale.
- **SC-005**: No public math capability in this feature requires a caller to include or initialize RHI, Backend, Renderer, or Application functionality.
- **SC-006**: Equivalent deterministic math scenarios produce results within documented tolerances on all supported platforms.
- **SC-007**: Later roadmap phases that need spatial values can reference this math vocabulary without defining duplicate vector, matrix, quaternion, transform, color, or primitive types.

## Assumptions

- Phase 001, the SCons project skeleton, is complete and provides the existing Core layer and test executable structure.
- The previous Core Types & Memory feature is complete and supplies stable type, string/name, pointer, container, and memory utilities for this feature to build on.
- This feature is the next critical-path Core phase because later RHI and Renderer work requires shared math values.
- Initial implementations may use straightforward baseline math behavior as long as public semantics, tolerance rules, and future SIMD compatibility are preserved.
- The first version focuses on float-oriented graphics math; integer vector variants and double-precision variants are out of scope unless a later roadmap phase requires them.
- Coordinate handedness and storage/layout conventions will be documented during planning before implementation tasks are generated.
