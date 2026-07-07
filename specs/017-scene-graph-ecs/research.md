# Research: Scene Graph & ECS Foundation

## Decision: Authoritative entities and components are stored flat; hierarchy is relationship data

**Rationale**: ECS identity and component access should not depend on tree-shaped ownership. Flat entity slots with component records make entity validation, stale-handle checks, and component queries deterministic and cache-friendly. Parent-child relationships express spatial hierarchy as data, and systems derive ordered views when they need hierarchy semantics.

**Alternatives considered**:
- Tree-owned entity nodes: rejected because it couples identity, ownership, traversal, and transform concerns into one structure and makes ECS-style component iteration harder.
- Pure flat entities with no hierarchy index: rejected because transform propagation, recursive destruction, and future scene inspection need explicit parent-child relationships.

## Decision: Entity handles use slot identity plus generation/version validation

**Rationale**: Slot reuse keeps the world bounded and avoids unbounded identifier growth, while generation/version validation ensures stale handles remain invalid after destruction. This directly satisfies the clarification that destroyed slots may be reused safely.

**Alternatives considered**:
- Never reuse entity IDs until world reset: simpler but wastes capacity and does not exercise stale-handle safety under realistic reuse.
- Monotonically increasing IDs with hard exhaustion: simple but less useful for long-running worlds and repeated create/destroy tests.

## Decision: Destroying an entity recursively destroys descendants

**Rationale**: Recursive destruction keeps hierarchy ownership clear for v1 and avoids orphan children whose world transforms or parent state become surprising after a parent is removed. It gives tests a concrete stale-handle rule for all descendants.

**Alternatives considered**:
- Detach children to roots and preserve world transforms: more editor-friendly for some tools but adds extra transform policy complexity to destruction.
- Reject destruction when children exist: safe but cumbersome for basic scene operations and recursive cleanup.

## Decision: Duplicate add is rejected; update and replace are explicit operations

**Rationale**: Transform, mesh, light, and camera components are single-instance component types in v1. Rejecting duplicate add prevents accidental overwrites and makes mutation intent testable. Explicit update/replace operations provide the intended mutation paths.

**Alternatives considered**:
- Automatically replace on duplicate add: convenient but hides mistakes and weakens diagnostics.
- Allow multiple same-type components everywhere: not needed for v1 and complicates component semantics before a full query system exists.

## Decision: Reparenting preserves world transform by default and exposes a local-preserve option

**Rationale**: Preserving world transform is more friendly for manual scene editing and avoids visual jumps when moving an entity between parents. Providing an explicit local-preserve option keeps deterministic runtime behavior available for callers that want hierarchy-relative transforms unchanged.

**Alternatives considered**:
- Always preserve local transform: simple but less useful for editor-like operations.
- Force callers to always choose: explicit but too noisy for the common editing-friendly path.

## Decision: Transform and subtree operations use explicit topological ordering keys

**Rationale**: World transforms and recursive operations need parents before children. Roots use creation order, and siblings use insertion order, so repeated operations are deterministic without exposing internal storage layout.

**Alternatives considered**:
- Creation order for all operations: cache-friendly but not sufficient for parent-before-child transform propagation.
- Internal storage order: rejected because public behavior would change when storage is compacted or optimized.

## Decision: Render collection emits per-category ascending entity identity order

**Rationale**: Render collection is not inherently a scene-tree traversal. It should consume updated world transforms and component data, then emit renderables, lights, and cameras in deterministic category-specific summaries. Ordering by entity identity, slot index then generation, is stable and independent of internal storage layout. Optional sort keys may reorder items, but entity identity remains the final tie-breaker.

**Alternatives considered**:
- Hierarchy pre-order for render collection: rejected because rendering collection may later use flat component queries or spatial indexes and does not need tree semantics.
- Internal array order: rejected because it leaks implementation details into public behavior.
- Renderer-specific order only: deferred because Renderer sorting belongs to later frame preparation, while Application still needs stable summary output.

## Decision: Spatial acceleration structures are deferred and treated as derived indexes later

**Rationale**: Octrees, BVHs, grids, and other spatial indexes can help rendering collection, light queries, physics broadphase, audio, AI, and editor selection, but each domain has different bounds and filtering needs. v1 should not make a global spatial index authoritative. The world and scene summary must leave room for future derived indexes without changing entity storage.

**Alternatives considered**:
- Add an octree in v1: rejected as premature and likely to entangle render, physics, and editor concerns.
- Use hierarchy as a spatial acceleration structure: rejected because parent-child relationships express transform ownership, not spatial partitioning.

## Decision: Diagnostics and dumps are deterministic and process-local

**Rationale**: Scene and ECS bugs are often ordering or stale-state bugs. Stable result codes, diagnostic categories, subject identifiers, and text dumps make regression tests and future documentation reliable without requiring a database, asset catalog, or live renderer.

**Alternatives considered**:
- Human-readable logs only: rejected because tests need structured and repeatable validation.
- Pointer/address-based dumps: rejected because they are not stable across runs or platforms.
