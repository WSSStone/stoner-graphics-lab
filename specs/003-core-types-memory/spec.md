# Feature Specification: Core Foundation Types & Memory

**Feature Branch**: `003-core-types-memory`  
**Created**: 2026-04-27  
**Status**: Draft  
**Input**: User requested the next spec stage from `doc/roadmap.md`; selected Phase 002: "Core foundation types and memory management: fixed-width integer types (FPlatformTypes), engine string type (FString), hashed name type (FName), smart pointer wrappers (TSharedPtr, TUniquePtr), memory utilities (FMemory with aligned allocation), and container aliases (TArray, TMap). All types follow UE5 naming conventions. Must be cross-platform (Win/Mac/Linux) and include unit tests."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Use Stable Core Types Across Engine Layers (Priority: P1)

An engine developer starting the next layer of work needs a common set of fixed-width types, string/name primitives, ownership helpers, memory utilities, and container aliases. They can include the Core foundation in a new feature and use those primitives consistently without inventing local substitutes.

**Why this priority**: Every later engine layer depends on these primitives. Without stable Core types, RHI, Backend, Renderer, and Application work will duplicate conventions and create incompatible APIs.

**Independent Test**: Can be tested by building a small Core-only usage sample that declares values, strings, names, ownership wrappers, aligned allocations, arrays, and maps without depending on higher engine layers.

**Acceptance Scenarios**:

1. **Given** a developer is writing Core or lower-level engine code, **When** they need fixed-width integer and size types, **Then** they can use the project-provided type names with predictable sizes on every supported platform.
2. **Given** a developer needs text or stable identifiers, **When** they create strings and hashed names, **Then** those values can be copied, compared, and passed through Core APIs using consistent project naming.
3. **Given** a developer needs common ownership or collection primitives, **When** they use the provided pointer and container aliases, **Then** their code follows the project-wide naming and ownership vocabulary.

---

### User Story 2 - Verify Memory Behavior Safely (Priority: P2)

A developer implementing low-level systems needs predictable allocation, deallocation, copying, zeroing, and alignment behavior. They can request aligned memory and validate that the returned memory satisfies the requested alignment and can be released cleanly.

**Why this priority**: Memory behavior is foundational for math, rendering resources, and platform integrations. Incorrect allocation or alignment behavior will create subtle downstream defects.

**Independent Test**: Can be tested by exercising memory operations with zero-size, small, large, and aligned allocations and confirming that cleanup leaves no detectable leaks or invalid frees.

**Acceptance Scenarios**:

1. **Given** a caller requests aligned memory with a valid alignment, **When** allocation succeeds, **Then** the returned address satisfies the requested alignment and can be released by the matching memory utility.
2. **Given** a caller copies, moves, or clears a memory range, **When** the operation completes, **Then** the destination contains the expected byte pattern and unrelated memory is unchanged.
3. **Given** a caller passes an invalid or unsupported allocation request, **When** the memory utility handles it, **Then** the behavior is deterministic, documented, and does not corrupt existing memory.

---

### User Story 3 - Confirm Cross-Platform Consistency (Priority: P3)

A maintainer preparing future roadmap phases needs confidence that Core foundation behavior is the same across Windows, macOS, and Linux. They can run the Core verification suite on each supported platform and see the same public behavior.

**Why this priority**: Cross-platform consistency is a constitutional requirement, but it is secondary to first establishing the primitives and memory behavior.

**Independent Test**: Can be tested by running the Core foundation verification suite on each supported platform and comparing the reported type sizes, alignment behavior, string/name behavior, and container behavior.

**Acceptance Scenarios**:

1. **Given** the Core foundation verification suite runs on any supported platform, **When** it checks type sizes and alignments, **Then** all required sizes and alignments match the documented expectations.
2. **Given** equivalent strings, names, and containers are created on different platforms, **When** their observable behavior is compared, **Then** equality, ordering where defined, and count behavior are consistent.
3. **Given** a future phase depends on the Core foundation, **When** that phase includes the public Core primitives, **Then** it does not need platform-specific replacements for the capabilities covered by this feature.

---

### Edge Cases

- Zero-length strings, empty names, empty arrays, and empty maps must behave predictably.
- Very long strings and large container sizes must either work within platform limits or fail in a documented, non-corrupting way.
- Duplicate names created from the same text must compare as equal and produce stable observable identity behavior within a single run.
- Name values created from different text that collide internally must still preserve correct equality semantics.
- Memory allocation requests with uncommon but valid alignments must return correctly aligned memory or report failure deterministically.
- Move and copy operations must leave source and destination values in valid, documented states.

## Architecture & Design Constraints *(mandatory)*

- **RHI Abstraction**: This feature belongs to the Core foundation and MUST NOT depend on RHI, Backend, Renderer, Application, or any graphics API behavior.
- **Design Patterns**: Responsibilities MUST remain separated by primitive category; the feature MUST NOT introduce a single manager that owns all Core type, memory, string, name, and container behavior.
- **Advanced Graphics**: The primitives MUST be suitable for later high-performance rendering work, including predictable sizes, alignment-aware memory use, and stable data ownership conventions.
- **Naming Conventions**: Public deliverables MUST follow PascalCase, UnrealEngine5-style naming conventions, including the planned names `FPlatformTypes`, `FString`, `FName`, `TSharedPtr`, `TUniquePtr`, `FMemory`, `TArray`, and `TMap`.
- **Cross-Platform Compatibility**: Observable behavior MUST be consistent across Windows, macOS, and Linux, with platform-specific details hidden from feature users.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST provide project-wide fixed-width integer, unsigned integer, character, boolean, pointer-size, and size-related type names with documented size expectations.
- **FR-002**: The system MUST provide an engine string value that supports construction from text, copying, moving, comparison, length queries, emptiness checks, and conversion to a standard textual view for diagnostics.
- **FR-003**: The system MUST provide a hashed immutable name value for fast identifier comparison while preserving correct equality semantics for names created from text.
- **FR-004**: The system MUST provide shared and unique ownership pointer vocabulary that developers can use consistently across Core-facing APIs.
- **FR-005**: The system MUST provide memory utilities for allocation, aligned allocation, deallocation, copying, moving, setting, and zeroing memory.
- **FR-006**: The system MUST provide dynamic array and key-value map aliases or wrappers that match the project's public naming conventions and are usable by later engine layers.
- **FR-007**: The public Core foundation primitives MUST be accessible without including or depending on higher engine layers.
- **FR-008**: Each primitive category MUST include verification coverage for normal, boundary, and invalid-input cases appropriate to that category.
- **FR-009**: Public documentation or discoverable comments MUST describe ownership expectations, allocation/deallocation pairing, alignment requirements, and the scope boundaries of this feature.
- **FR-010**: The feature MUST exclude math types, logging/assertion systems, and platform file or process operations; those remain assigned to later roadmap phases.

### Key Entities *(include if feature involves data)*

- **Platform Type Set**: The canonical project vocabulary for fixed-width numeric, character, pointer-size, and size-related values.
- **Engine String**: A textual value used by Core and later engine layers for readable data and diagnostics.
- **Engine Name**: An immutable identifier optimized for repeated comparison while preserving correct equality behavior.
- **Ownership Pointer**: A shared or unique ownership value used to communicate lifetime intent in Core-facing APIs.
- **Memory Utility**: A set of operations for allocation, aligned allocation, deallocation, copying, moving, setting, and zeroing raw memory.
- **Container Vocabulary**: Project-named dynamic array and key-value map abstractions used as the default collection vocabulary.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer can create a Core-only sample using all public primitive categories in under 15 minutes using the names and behavior described by this spec.
- **SC-002**: Verification covers 100% of required primitive categories: platform types, strings, names, ownership pointers, memory utilities, arrays, and maps.
- **SC-003**: The Core foundation verification suite completes with zero failures on Windows, macOS, and Linux.
- **SC-004**: Documented fixed-width type expectations are met for every required type on all supported platforms.
- **SC-005**: At least 95% of boundary-case verification scenarios pass before this feature is considered complete, and any remaining accepted gaps are documented with rationale.
- **SC-006**: No public capability in this feature requires a caller to include or initialize RHI, Backend, Renderer, or Application functionality.

## Assumptions

- Phase 001, the SCons project skeleton, is complete and provides the existing Core layer location and test executable structure.
- This feature is the first implementation phase after the roadmap and should remain limited to Core foundation types and memory.
- The initial string, pointer, array, and map deliverables may use straightforward underlying behavior as long as the public project vocabulary and observable semantics are stable.
- Hash values for names only need to be stable within a single process run unless a later phase explicitly requires persisted or cross-run stable identifiers.
- Unit-style verification is required because the roadmap explicitly calls for tests for all types.
