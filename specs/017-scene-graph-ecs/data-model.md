# Data Model: Scene Graph & ECS Foundation

## World

**Purpose**: Owns all live scene entities, component records, hierarchy links, diagnostics, and render collection summaries for one process-local scene.

**Fields**:
- `EntitySlots`: ordered slot records containing generation/version, live state, creation order, and component presence.
- `FreeSlots`: reusable destroyed slot indices.
- `TransformComponents`: single-instance transform records keyed by entity.
- `MeshComponents`: single-instance mesh records keyed by entity.
- `LightComponents`: single-instance light records keyed by entity.
- `CameraComponents`: single-instance camera records keyed by entity.
- `Hierarchy`: parent-child relationship records.
- `Diagnostics`: deterministic diagnostic records from invalid operations and collection validation.

**Validation rules**:
- A world owns entity handles it creates; handles from another world are invalid for this world.
- One world is in scope for v1; multi-world scheduling is out of scope.
- Reset clears entity slots, components, hierarchy, render summaries, diagnostics, and free slot state.

**State transitions**:
- Empty -> Active after first entity creation.
- Active -> Reset after reset call; all previous handles become invalid.

## Entity Handle

**Purpose**: Lightweight stable reference to an entity slot in a world.

**Fields**:
- `SlotIndex`: numeric identity slot.
- `Generation`: generation or version value for stale-handle validation.

**Validation rules**:
- Valid only when the owning world has a live slot at `SlotIndex` and the stored generation matches.
- Destroyed slots may be reused with a new generation.
- Stale handles must not read, update, destroy, parent, or add components to a reused slot.

**State transitions**:
- Invalid/default -> Live when returned by successful entity creation.
- Live -> Stale when the entity or one of its ancestors is recursively destroyed.
- Stale remains stale even if the slot is reused with a newer generation.

## Entity Slot

**Purpose**: Internal world-owned entity lifetime record.

**Fields**:
- `Generation`: current generation or version.
- `bLive`: whether the slot currently hosts a live entity.
- `CreationOrder`: monotonic order assigned when an entity is created.
- `Parent`: optional parent entity identity.
- `Children`: ordered child identities in insertion order.
- Component presence bits for transform, mesh, light, and camera.

**Validation rules**:
- A live slot can have at most one transform, mesh, light, and camera component in v1.
- Destroying a slot recursively destroys all descendants before the slot becomes reusable.
- Reuse increments or otherwise changes generation before issuing a new handle.

## Transform Component

**Purpose**: Stores local transform and derived world transform data for spatial hierarchy.

**Fields**:
- `LocalPosition`
- `LocalRotation`
- `LocalScale`
- `WorldTransform`
- `bWorldTransformValid`

**Validation rules**:
- Local transform values must be finite.
- Zero or non-uniform scale is allowed but must remain deterministic.
- World transform is derived using topological parent-before-child order.

**State transitions**:
- Missing -> Present on successful add.
- Present -> Present with new values on explicit update/replace.
- Present -> Missing on removal or entity destruction.
- World transform becomes dirty after local transform update, reparent, or ancestor transform change.

## Mesh Component

**Purpose**: Identifies a renderable scene item without owning live graphics resources.

**Fields**:
- `MeshId`: stable abstract mesh identity.
- `MaterialId`: optional stable abstract material identity or binding reference.
- `Bounds`: optional local-space bounds or future-bounds placeholder.
- `OptionalSortKey`: optional collection sort key before entity identity tie-break.

**Validation rules**:
- Mesh identity must be present for an item to be accepted as renderable.
- A mesh component without a valid transform is rejected or omitted during render collection.
- The component must not store backend handles, RHI resources, or GPU allocations.

## Light Component

**Purpose**: Describes scene lighting inputs for render collection.

**Fields**:
- `LightType`: directional or point-light style.
- `Color`
- `Intensity`
- `Range`: meaningful for point lights.
- `OptionalSortKey`: optional collection sort key before entity identity tie-break.

**Validation rules**:
- Intensity must be non-negative.
- Point-light range must be positive.
- Directional lights use transform orientation for direction; point lights use transform position.
- Invalid lights are reported as rejected without blocking valid lights.

## Camera Component

**Purpose**: Describes scene camera inputs for render collection.

**Fields**:
- `ProjectionType`: perspective or orthographic.
- `FieldOfView` or `OrthographicExtent`
- `NearPlane`
- `FarPlane`
- `bActiveCamera`
- `OptionalSortKey`: optional collection sort key before entity identity tie-break.

**Validation rules**:
- Near and far plane values must be positive with near less than far.
- Perspective field of view and orthographic extent must be positive.
- Camera collection reports valid cameras deterministically; active-camera policy remains data-only in v1.

## Hierarchy Relationship

**Purpose**: Represents parent-child relationships independently of entity/component storage layout.

**Fields**:
- Root entity identities in creation order.
- Child identity lists in insertion order.
- Parent identity per child.

**Validation rules**:
- Self-parenting is rejected.
- Parent cycles are rejected.
- Invalid parent or child handles are rejected.
- Reparent preserves world transform by default and recalculates local transform relative to the new parent.
- Reparent can explicitly preserve local transform and recompute world transform from the new parent chain.
- Transform propagation and subtree operations use topological ordering: parents before children, roots in creation order, siblings in insertion order.

**State transitions**:
- Root -> Child when parented.
- Child -> Root when unparented.
- Child under parent A -> Child under parent B when reparented.
- Any state -> Destroyed when entity or ancestor is destroyed.

## Scene Render Summary

**Purpose**: Deterministic per-frame or per-query output that later Renderer work can consume.

**Fields**:
- `AcceptedRenderables`: mesh scene items with entity identity, world transform, mesh identity, material identity, and optional bounds.
- `AcceptedLights`: light scene items with entity identity, world transform-derived placement, type, color, intensity, and range.
- `AcceptedCameras`: camera scene items with entity identity, world transform, projection data, and active flag.
- `RejectedItems`: invalid or incomplete render collection records.
- `Diagnostics`: stable diagnostic records.
- `DebugDump`: optional deterministic text summary.

**Ordering rules**:
- Each accepted category is ordered by optional explicit sort key when present.
- Entity identity, slot index then generation, is the final tie-breaker.
- Ordering is independent of internal storage layout.

## Diagnostics

**Purpose**: Captures stable failure and warning information for invalid scene operations.

**Fields**:
- `Code`: stable diagnostic code.
- `Severity`: info, warning, or error style category.
- `Subject`: entity/component/category identity.
- `Message`: deterministic human-readable summary.

**Validation rules**:
- Diagnostics must not include pointer addresses, native handles, platform-specific state, or nondeterministic values.
- Repeated equivalent invalid operations produce stable codes and ordering.
