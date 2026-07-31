# Research: Core Foundation Math Library

## Decision: Use right-handed Core coordinate convention

**Rationale**: A single Core-level convention avoids each layer inventing its own interpretation of directions, cross products, transforms, and planes. Right-handed math is widely understood in graphics and can be converted at API boundaries where a backend or shader convention differs.

**Alternatives considered**:

- Left-handed Core convention: viable, but would make Vulkan-style examples and many graphics references require earlier conversion.
- Per-backend convention: rejected because Core math must remain backend-independent and deterministic.

## Decision: Store `FMatrix4x4` in row-major memory order with documented multiplication semantics

**Rationale**: Row-major storage is straightforward for C++ value access and debugging. The important compatibility requirement is not the storage order alone, but that point/vector transformation and matrix multiplication semantics are explicit and verified.

**Alternatives considered**:

- Column-major storage: common in shader literature, but not inherently more portable for the engine Core layer.
- Leave layout unspecified: rejected because public math values will be used across many later systems and tests need stable expectations.

## Decision: Use explicit tolerance helpers for floating-point comparisons

**Rationale**: Exact equality is unreliable for most vector, matrix, quaternion, and transform operations. Centralizing tolerances in `FMath` makes tests and caller behavior consistent across platforms.

**Alternatives considered**:

- Exact comparison only: rejected for computed values due to normal floating-point precision differences.
- Per-type hidden tolerances: rejected because inconsistent tolerances make failures difficult to reason about.

## Decision: Implement baseline scalar behavior first while preserving SIMD-ready public usage

**Rationale**: The roadmap asks for a naive implementation first with SIMD optimization planned. A correct scalar baseline is easier to verify, portable across supported compilers, and gives future SIMD paths a reference behavior.

**Alternatives considered**:

- Add platform SIMD intrinsics immediately: rejected because it increases cross-platform risk before the public contract is stable.
- Use a third-party math library: rejected because the project is learning-oriented and prefers custom Core implementations.

## Decision: Keep math values lightweight and allocation-free

**Rationale**: Vectors, matrices, quaternions, colors, and primitives should be cheap value types used pervasively by rendering code. Avoiding heap allocation keeps behavior deterministic and supports later high-performance rendering systems.

**Alternatives considered**:

- Heap-backed math objects: rejected because it adds ownership and allocation behavior to basic numeric values.
- Runtime-polymorphic math hierarchy: rejected as unnecessary complexity and a design-pattern misuse for simple value semantics.

## Decision: Represent invalid geometric states explicitly where meaningful

**Rationale**: Empty boxes, failed inverses, and invalid planes are common edge cases. Deterministic invalid states are easier to test and safer for later visibility, culling, and renderer code.

**Alternatives considered**:

- Undefined behavior for invalid inputs: rejected because the spec requires documented invalid-input behavior.
- Exceptions for all invalid math states: rejected because Core currently uses deterministic return values and verification rather than exception-driven control flow.

## Decision: Provide separate float and byte color workflows

**Rationale**: Rendering systems need both normalized float colors and byte-channel colors for resource and diagnostic scenarios. Explicit construction and conversion behavior prevents ambiguous channel ranges.

**Alternatives considered**:

- Only float colors: insufficient for byte resource and packed color workflows.
- Only byte colors: insufficient for linear math and interpolation workflows.

## Feature 024 Coordinate Convention Amendment (2026-07-31)

This historical research remains evidence for Feature 004. The active engine
world convention is now `UnrealLH_ZUp_XForward_YRight_Meters_CW` (+X forward,
+Y right, +Z up, meters, clockwise default front faces). This changes named
world axes and graphics-boundary defaults, not component vector algebra,
Hamilton quaternion multiplication, or SRT composition. CPU row-major
matrices are packed explicitly into GLSL column-major shader storage.
