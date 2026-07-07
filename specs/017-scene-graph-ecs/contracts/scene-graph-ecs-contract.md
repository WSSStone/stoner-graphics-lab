# Contract: Scene Graph & ECS Foundation

## Public Boundary

The feature exposes Application-layer scene contracts only. Public contracts may depend on Core math, Core containers, Core strings, and Application diagnostics. They must not expose Renderer frame plans, RHI resources, backend objects, graphics API handles, asset loaders, editor UI, physics, animation, scripting, or serialization formats.

## Result Semantics

All mutating and validation-heavy operations return or expose deterministic scene results:

- `Success`: operation completed and world state changed as requested.
- `InvalidEntity`: handle does not refer to a live entity in the owning world.
- `StaleEntity`: handle generation/version does not match the current slot generation.
- `DuplicateComponent`: add was requested for a single-instance component type already present.
- `MissingComponent`: read, update, replace, or remove was requested for an absent component.
- `InvalidComponentData`: component values are invalid for the requested operation or collection path.
- `HierarchyCycle`: requested parent relationship would create a cycle.
- `InvalidHierarchyOperation`: parent, child, or reparent operation is not valid.
- `CapacityExceeded`: v1 configured entity capacity is exhausted.
- `Unsupported`: operation is intentionally out of v1 scope.

## Entity Lifecycle Contract

### Create Entity

**Given** a world with available capacity  
**When** an entity is created  
**Then** the world returns a live entity handle containing slot identity and generation/version data, records creation order, and includes the entity in live entity queries.

### Destroy Entity

**Given** a live entity handle  
**When** it is destroyed  
**Then** the entity, all descendants, all attached components, and all hierarchy links are removed deterministically, and all corresponding handles become invalid.

### Reuse Slot

**Given** a destroyed slot becomes available  
**When** a new entity reuses that slot  
**Then** the new handle has a different generation/version, and old handles cannot affect the new entity.

## Component Contract

### Add Component

**Given** a live entity without the requested single-instance component type  
**When** a supported transform, mesh, light, or camera component is added  
**Then** the component is present and can be read with the same values, subject to validation normalization if defined by implementation.

### Duplicate Add

**Given** a live entity already has a component of the requested type  
**When** add is requested again  
**Then** the request returns duplicate-component status, the existing component remains unchanged, and diagnostics identify the entity and component type.

### Update or Replace Component

**Given** a live entity has a component  
**When** an explicit update or replace operation is requested with valid values  
**Then** the component changes deterministically and any affected derived state, such as world transform or render summary, becomes dirty or is recomputed on demand.

### Remove Component

**Given** a live entity has a component  
**When** removal is requested  
**Then** the component is absent afterward and render collection no longer treats it as present.

## Hierarchy Contract

### Parent

**Given** valid parent and child entities  
**When** parent assignment is requested  
**Then** the child is removed from its prior parent or root list, inserted into the new parent's child list in insertion order, and the relationship is observable.

### Reparent Default

**Given** a valid child entity is reparented  
**When** no preservation option is specified  
**Then** the entity preserves world transform, recalculates local transform relative to the new parent, and future world transform queries remain stable.

### Reparent Preserve Local

**Given** a valid child entity is reparented with local-preserve requested  
**When** the operation succeeds  
**Then** local transform remains unchanged and world transform is recomputed from the new parent chain.

### Cycle Rejection

**Given** a hierarchy change would parent an entity to itself or to one of its descendants  
**When** the operation is requested  
**Then** it fails with cycle or invalid hierarchy status and previous hierarchy state remains unchanged.

### Operation Order

Transform propagation, recursive destruction, and subtree queries use topological order:

1. Parents before children.
2. Roots in creation order.
3. Siblings in insertion order.

This order is public behavior and must not depend on internal storage layout.

## Render Collection Contract

### Collect Render Summary

**Given** a world with valid transform, mesh, light, and camera components  
**When** render collection is requested  
**Then** the result contains per-category summaries for renderables, lights, and cameras, plus rejected items and diagnostics.

### Accepted Renderable

A renderable item is accepted when it has:

- A live entity handle.
- A valid transform component.
- A valid mesh component with mesh identity.

### Accepted Light

A light item is accepted when it has:

- A live entity handle.
- A valid transform component.
- A valid light component.
- Non-negative intensity.
- Positive range for point-light style lights.

### Accepted Camera

A camera item is accepted when it has:

- A live entity handle.
- A valid transform component.
- A valid camera component.
- Valid projection values and near/far range.

### Rejected Items

Invalid or incomplete scene items are rejected or omitted from accepted categories without preventing valid items from being collected. Rejected items include the entity identity when available, component category, diagnostic code, and deterministic message.

### Ordering

Within each accepted category:

1. Optional explicit sort keys may order items when present.
2. Entity identity is the final tie-breaker.
3. Entity identity comparison is ascending slot index, then generation/version.
4. Ordering must be stable across repeated collection runs and independent of internal storage layout.

## Diagnostics and Inspection Contract

The feature provides deterministic diagnostic records and optional text dumps for entity lifecycle, component operations, hierarchy state, render summaries, and invalid operation outcomes.

Diagnostics and dumps must omit:

- Pointer addresses.
- Native handles.
- Backend objects.
- Platform-specific transient state.
- Nondeterministic timing values.

Repeated equivalent worlds and operation sequences must produce byte-stable inspection output.
