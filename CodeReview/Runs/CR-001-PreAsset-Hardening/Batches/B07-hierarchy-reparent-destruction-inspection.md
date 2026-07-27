# B07-S10: Hierarchy, Reparenting, And Destruction Inspection

## Scope

Inspected Feature 017 hierarchy operations, transform propagation, reparent
preservation modes, subtree order, recursive destruction, and existing scene
ECS tests.

Production files read:

- `Source/Application/Public/Application/FWorld.h`
- `Source/Application/Private/FWorld.cpp`
- `Source/Application/Public/Application/FTransformComponent.h`
- `Source/Core/Public/Core/FTransform.h`

Supporting files read:

- `Tests/ApplicationSceneEcsTests.cpp`
- `specs/017-scene-graph-ecs/spec.md`
- `specs/017-scene-graph-ecs/contracts/scene-graph-ecs-contract.md`

## Requirements Checked

- Feature 017 FR-003: destroying an entity recursively destroys descendants and
  invalidates their handles.
- Feature 017 FR-006: world transforms are derived from parent-child
  relationships.
- Feature 017 FR-007: reparenting preserves world transform by default and
  offers an explicit local-preserve option; unrepresentable TRS results fail
  without mutation.
- Feature 017 FR-008: hierarchy changes reject invalid entities, cycles, and
  self-parenting.
- Feature 017 FR-009: transform propagation and subtree operations use
  topological parent-before-child ordering with roots in creation order and
  siblings in insertion order.
- Hierarchy Contract: parent assignment, reparent default/local modes, cycle
  rejection, and operation order are public behavior independent of storage.

## Finding

### `CR001-B07-F006`

`ComputeWorldTransform` stops propagation when the immediate parent exists but
does not have a transform component. It returns the child local transform
directly instead of continuing through the parent chain, so a legal
`Root(transform) -> Group(no transform) -> Child(transform)` hierarchy loses the
root transform. `SetParent` also treats transformless parents as having no
parent-world transform during preserve-world/local feasibility checks.

Impact: transformless grouping entities can produce incorrect descendant world
transforms and renderer-facing scene summaries. Reparenting through such groups
can silently preserve or recompute the wrong world state.

Status: Accepted, S2.

## Non-Findings

- Direct transformed parent-child composition is covered by
  `Tests/ApplicationSceneEcsTests.cpp`.
- Root creation order and sibling insertion order are covered by the
  topological-order test.
- Self/descendant cycle rejection rejects mutation before changing parent state.
- Preserve-world reparenting recalculates the child local transform for direct
  transformed parents and preserves the observed world transform.
- Preserve-local reparenting leaves local transform unchanged and recomputes
  direct transformed parent world state.
- Unrepresentable rotated non-uniform preserve-world reparenting returns
  `InvalidHierarchyOperation` before mutation.
- Recursive destruction builds subtree order, removes parent/root references,
  clears component state, increments generations, and invalidates descendants.
