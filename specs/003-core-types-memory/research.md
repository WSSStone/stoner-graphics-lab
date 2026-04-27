# Research: Core Foundation Types & Memory

**Feature**: 003-core-types-memory  
**Date**: 2026-04-27  
**Status**: Complete

## Research Tasks

This feature introduces the first real Core layer primitives. The research phase resolves the implementation choices needed before designing tasks.

---

## Decision 1: Initial Primitive Implementation Strategy

**Decision**: Use standard-library-backed initial implementations and aliases where they satisfy the public contract.

**Rationale**: Phase 002 must establish a stable vocabulary without expanding into a full custom container/string project. The roadmap allows `FString`, `TArray`, and `TMap` to wrap or alias standard behavior initially. This keeps the feature completable in one speckit cycle while preserving public names that later phases can evolve behind.

**Alternatives Considered**:

- Fully custom string, array, and map implementations from day one (rejected - too much scope for the first Core phase and duplicates later learning opportunities).
- Use raw standard-library names directly in public engine code (rejected - violates the project vocabulary goal and makes later migration harder).

---

## Decision 2: Public Header Organization

**Decision**: Create one public header per primitive category under `Source/Core/Public/Core/`, and update `CoreMinimal.h` to aggregate the stable public Core foundation.

**Rationale**: Individual headers keep dependencies clear and make future growth easier. `CoreMinimal.h` preserves the existing include-chain pattern used by later skeleton layers.

**Alternatives Considered**:

- Put all primitives directly in `CoreMinimal.h` (rejected - creates a large catch-all header and makes responsibilities harder to maintain).
- Create nested subdirectories for each primitive family immediately (rejected - unnecessary structure for the current small scope).

---

## Decision 3: `FString` Scope

**Decision**: Implement `FString` as an owning string value with construction, copy/move, comparison, length/empty queries, and diagnostic view access.

**Rationale**: The spec needs a usable engine string, not a full text processing library. This scope supports Core diagnostics and future APIs while avoiding encoding conversion, localization, formatting, and allocator customization for now.

**Alternatives Considered**:

- Alias `FString` directly to `std::string` (rejected - an alias cannot grow stable project-specific behavior without later public API disruption).
- Implement UTF conversion and formatting now (rejected - belongs in future text/localization or logging work).

---

## Decision 4: `FName` Collision-Safe Identity

**Decision**: Make `FName` immutable and store both original text and a hash. Equality first compares hash, then compares text to preserve correctness if two names collide.

**Rationale**: This provides fast common-case comparison while satisfying the spec's collision correctness requirement. It avoids a global intern table in the first pass, keeping ownership and lifecycle simple.

**Alternatives Considered**:

- Process-wide intern table like Unreal's production `FName` (rejected for this phase - introduces global registry lifecycle and thread-safety concerns).
- Hash-only equality (rejected - incorrect under collisions).

---

## Decision 5: Memory Utility Semantics

**Decision**: Provide deterministic `FMemory` functions for allocation, aligned allocation, deallocation, copying, moving, setting, and zeroing. Zero-size allocation returns `nullptr`; invalid alignment requests fail deterministically without corrupting memory.

**Rationale**: Low-level systems need predictable behavior across platforms. Explicit zero-size and invalid-input semantics simplify tests and reduce ambiguous caller expectations.

**Alternatives Considered**:

- Let platform allocators define zero-size behavior (rejected - observable behavior would vary).
- Add custom allocator tracking in this phase (rejected - useful later, but not required for the foundation vocabulary).

---

## Decision 6: Ownership and Container Vocabulary

**Decision**: Define `TSharedPtr`, `TUniquePtr`, `TArray`, and `TMap` as project-named aliases around standard ownership and collection facilities for this phase.

**Rationale**: The feature goal is vocabulary and consistency. Aliases are enough to let future layers use project names immediately, while preserving room for custom wrappers or allocators in later features.

**Alternatives Considered**:

- Custom smart pointer reference counting (rejected - too much risk and scope before the engine has real ownership requirements).
- Custom containers with bespoke allocators (rejected - better deferred until allocator and performance requirements are clearer).

---

## Decision 7: Verification Strategy

**Decision**: Add self-contained Core foundation verification to the existing `StonerTest` executable.

**Rationale**: The skeleton already builds one test executable and does not include a third-party test framework. A simple local test harness keeps dependencies at zero and is enough to validate the primitive behavior required by the spec.

**Alternatives Considered**:

- Add a third-party unit test framework (rejected - new dependency is unnecessary for this small foundation feature).
- Create a separate test binary for Core only (rejected - adds build complexity before the current test structure needs it).

---

## Open Questions

None. All planning decisions are resolved.
