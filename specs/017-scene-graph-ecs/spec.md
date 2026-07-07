# Feature Specification: Scene Graph & ECS Foundation

**Feature Branch**: `017-scene-graph-ecs`  
**Created**: 2026-07-07  
**Status**: Draft  
**Input**: User description: "Create the next major feature specification from the roadmap. Roadmap phase: Application Scene Graph & ECS Foundation."

## Clarifications

### Session 2026-07-07

- Q: When an entity with children is destroyed, what should happen to its descendants? → A: Destroying an entity recursively destroys all descendants and invalidates their handles.
- Q: When an entity already has a single-instance component type, what should happen if the developer adds the same component type again? → A: Reject duplicate add; developer must explicitly update or replace the existing component.
- Q: After an entity is destroyed, how should entity identity reuse work while still keeping stale handles safe? → A: Reuse destroyed slots with generation/version checks so stale handles stay invalid.
- Q: When reparenting an entity, should the entity preserve its local transform or preserve its world transform? → A: Default to preserving world transform for manual-editing friendliness, and expose a reparent option to choose preserving local or world transform.
- Q: What ordering key should transform propagation and subtree operations use? → A: Topological ordering: parents before children, roots in creation order, siblings in insertion order; world transforms and recursive destroy/reparent walks follow this order.
- Q: What ordering key should render collection use? → A: Within each category, renderables, lights, and cameras are emitted in ascending entity identity order by slot index then generation; optional sort keys may reorder, but entity identity is the final tie-breaker.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Create and Manage Scene Entities (Priority: P1)

An application developer can create a world, add entities to it, attach basic scene components, remove entities, and detect when an entity handle no longer refers to a valid live entity.

**Why this priority**: A scene cannot be organized, inspected, or later submitted to rendering without stable entity identity and predictable component ownership.

**Independent Test**: Can be fully tested by creating a world, creating several entities, attaching and removing supported components, destroying entities, and verifying that live and stale handles are reported deterministically.

**Acceptance Scenarios**:

1. **Given** an empty world, **When** a developer creates an entity, **Then** the world reports a valid entity handle and includes that entity in its live entity count.
2. **Given** a live entity, **When** a supported component is attached, **Then** the component can be retrieved through that entity and its stored values match the requested values.
3. **Given** a destroyed entity, **When** a developer queries, updates, parents, or adds components through its old handle, **Then** the operation fails safely with a clear invalid-entity result and no unrelated entity is modified.
4. **Given** a destroyed entity slot is reused for a later entity, **When** the old handle is used, **Then** generation or version validation keeps the old handle invalid and prevents it from affecting the new entity.

---

### User Story 2 - Organize Spatial Parent-Child Hierarchies (Priority: P1)

An application developer can organize entities into parent-child relationships and obtain deterministic world-space transforms derived from local transforms.

**Why this priority**: Cameras, lights, meshes, and later editor/runtime features need predictable spatial relationships before the triangle demo grows into a real scene.

**Independent Test**: Can be fully tested by building a small hierarchy with known local transforms, requesting world transforms, reparenting nodes, and verifying final world transforms plus topological operation order: parents before children, roots in creation order, and siblings in insertion order.

**Acceptance Scenarios**:

1. **Given** an entity with a local transform and no parent, **When** its world transform is requested, **Then** the world transform matches its local transform.
2. **Given** a parent entity and a child entity with local transforms, **When** the child world transform is requested, **Then** it reflects the parent transform composed with the child's local transform.
3. **Given** an existing hierarchy, **When** an entity is reparented to another valid parent with the default behavior, **Then** the entity preserves its world transform and its local transform is recalculated relative to the new parent.
4. **Given** an existing hierarchy, **When** an entity is reparented with local-transform preservation requested, **Then** the local transform remains unchanged and the world transform is recomputed from the new parent chain.
5. **Given** a request that would create a parent cycle, **When** the hierarchy change is attempted, **Then** the request is rejected and the previous hierarchy remains intact.

---

### User Story 3 - Collect Render-Relevant Scene Data (Priority: P2)

An application developer can ask the scene foundation to gather render-relevant entities, including mesh instances, lights, and cameras, into a deterministic frame-facing summary that later rendering features can consume.

**Why this priority**: The next visible demo and later renderers need a clean handoff from Application-owned scene data to Renderer-owned frame preparation without coupling Application code to a specific graphics backend.

**Independent Test**: Can be fully tested by creating entities with transform, mesh, light, and camera components, running the render-collection flow, and verifying that accepted and rejected scene items are stable and ordered.

**Acceptance Scenarios**:

1. **Given** an entity with a transform and mesh component, **When** render data is collected, **Then** the entity appears in the renderable summary with its current world transform and mesh identity.
2. **Given** point and directional light components with valid values, **When** render data is collected, **Then** the light summary preserves light type, color, intensity, range where applicable, and world-space position or direction.
3. **Given** a camera component on a valid entity, **When** render data is collected, **Then** the camera summary includes projection intent and world-space camera transform.
4. **Given** an entity missing required render data, **When** render data is collected, **Then** that entity is omitted or reported as rejected without stopping collection for valid entities.
5. **Given** multiple valid items exist in the same render collection category, **When** render data is collected, **Then** the category emits items in ascending entity identity order unless an explicit optional sort key applies, with entity identity as the final tie-breaker.

---

### User Story 4 - Diagnose Invalid Scene Operations (Priority: P3)

An application developer receives stable diagnostics for invalid entities, duplicate or unsupported component operations, broken hierarchy requests, and invalid camera, light, or mesh data.

**Why this priority**: Scene systems are often edited incrementally; deterministic diagnostics prevent silent corruption and make later demos easier to debug.

**Independent Test**: Can be fully tested by applying invalid scene operations in known order and verifying stable result statuses, diagnostic codes, and unchanged prior valid state.

**Acceptance Scenarios**:

1. **Given** an entity already has a component of a single-instance component type, **When** the same component type is added again, **Then** the system rejects the duplicate add, preserves the existing component, and reports that an explicit update or replace operation is required.
2. **Given** invalid light, camera, or mesh reference data, **When** render data is collected, **Then** diagnostics identify the invalid item and valid scene items remain available.
3. **Given** a hierarchy operation targets a missing parent or child, **When** the operation is attempted, **Then** it fails safely and the previous hierarchy remains unchanged.

### Edge Cases

- A world is empty and render-relevant data is collected.
- Entity creation reaches the configured v1 capacity bound.
- A stale entity handle is used after its original entity or one of its ancestors is destroyed.
- A destroyed entity slot is reused for a new entity while stale handles from the old generation still exist.
- An entity is destroyed while it has children; all descendants are recursively destroyed and their handles become invalid.
- A child is reparented from one valid parent to another.
- Reparenting is requested with default world-transform preservation and with explicit local-transform preservation.
- A hierarchy operation attempts to parent an entity to itself or to one of its descendants.
- Local transforms contain zero or non-uniform scale.
- Render collection encounters an entity with a mesh component but no transform component.
- Render collection encounters invalid light range, negative intensity, invalid camera near/far relationship, or missing mesh identity.
- Multiple renderable entities, lights, or cameras have identical optional sort keys or names; ascending entity identity remains the final tie-breaker.
- Component removal is requested for a component that is not present.
- A duplicate component add is requested for an entity that already has that single-instance component type; the existing component remains unchanged.

## Architecture & Design Constraints *(mandatory)*

- **RHI Abstraction**: The feature MUST NOT bypass the RHI layer to call Graphics APIs directly.
- **Design Patterns**: The feature MUST avoid God-classes and utilize Strategy/Composite patterns for orthogonal responsibilities.
- **Advanced Graphics**: The feature MUST consider compatibility with Ray Tracing, Meshlets, and Global Illumination pipelines.
- **Naming Conventions**: The feature's code design MUST adhere to PascalCase, UnrealEngine5-style naming conventions.
- **Cross-Platform Compatibility**: The feature MUST compile and run on all supported platforms (Windows, macOS, Linux). Platform-specific code MUST be isolated behind abstraction layers or conditional compilation guards.
- **Automated Cross-Platform Validation**: Because this feature affects Application-layer runtime data, transform math, deterministic scene ordering, and renderer-facing summaries, it MUST include or update a GitHub Actions or equivalent CI validation path for Windows, macOS, and Linux deterministic build/test coverage, or explicitly document any temporary validation gap with fallback manual verification and follow-up tasks.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST allow an application developer to create and reset a world that owns scene entities and their attached scene components.
- **FR-002**: System MUST create lightweight entity handles with identity and generation/version validation so destroyed slots may be reused without stale handles modifying unrelated new entities.
- **FR-003**: System MUST allow entities to be destroyed safely; destroying an entity MUST recursively destroy all descendants, invalidate their handles, and remove attached components deterministically.
- **FR-004**: System MUST support attaching, reading, explicitly updating, explicitly replacing, and removing transform, mesh, light, and camera components on live entities.
- **FR-005**: System MUST reject duplicate component add requests for single-instance component types, preserve the existing component unchanged, and report invalid entity, missing component, duplicate component, invalid component data, and unsupported operation outcomes with stable result statuses or diagnostics.
- **FR-006**: System MUST support local transforms for entities and world transforms derived from parent-child relationships.
- **FR-007**: System MUST allow entities to be parented, unparented, and reparented while preserving a deterministic hierarchy state; reparenting MUST preserve world transform by default and MUST provide an explicit option to preserve local transform instead.
- **FR-008**: System MUST reject hierarchy changes that would create cycles, self-parenting, or references to invalid parent or child entities.
- **FR-009**: System MUST define deterministic transform propagation and subtree operation ordering as topological order: parents before children, roots in creation order, and siblings in insertion order, independent of the internal storage layout.
- **FR-010**: System MUST provide mesh scene data that can identify renderable objects without requiring ownership of live graphics resources.
- **FR-011**: System MUST provide light scene data for directional and point-light style inputs, including color, intensity, and range where range is meaningful.
- **FR-012**: System MUST provide camera scene data with projection intent, field-of-view or orthographic extent, near/far ranges, and active-camera identification.
- **FR-013**: System MUST provide a render-collection flow that gathers renderable mesh instances, lights, and cameras into deterministic per-category summaries; within each category, items MUST be emitted in ascending entity identity order by slot index then generation, unless an explicit optional sort key applies, and entity identity MUST remain the final tie-breaker.
- **FR-014**: System MUST reject or omit incomplete renderable entities during render collection without preventing valid entities from being collected.
- **FR-015**: System MUST keep Application scene ownership separate from Renderer frame planning, RHI resource ownership, and backend graphics API objects.
- **FR-016**: System MUST include deterministic validation coverage for entity lifecycle, component operations, hierarchy transforms, invalid operation diagnostics, render collection, and stable ordering.
- **FR-017**: System MUST keep v1 scope to one world, basic component storage, transform hierarchy, and render-relevant scene collection; physics, animation, scripting, serialization, editor UI, full archetype query optimization, and multi-world scheduling MUST remain out of scope.

### Key Entities

- **World**: The owning container for live entities, component records, hierarchy relationships, diagnostics, and render-collection summaries.
- **Entity Handle**: A lightweight identifier for an entity owned by a specific world; it includes enough generation or version information to remain invalid after its original entity is destroyed, even if the underlying slot is reused.
- **Transform Component**: Local position, rotation, and scale data used to derive world-space transforms through the hierarchy.
- **Mesh Component**: Renderable mesh identity and related draw-facing metadata that can be collected without owning graphics resources.
- **Light Component**: Light type, color, intensity, range, and scene placement data used to describe scene illumination.
- **Camera Component**: Projection intent, view parameters, near/far ranges, and active-camera participation data.
- **Hierarchy Relationship**: Parent-child linkage between entities, including root entities and ordered children; hierarchy operations use parent-before-child topological order with roots in creation order and siblings in insertion order.
- **Scene Render Summary**: Deterministic collection result containing accepted renderables, accepted lights, cameras, rejected items, and diagnostics; each accepted category has an explicit ordering key independent of internal storage layout.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer can create a world, add at least 100 entities, attach supported components, destroy selected entities, and verify valid versus stale handles without crashes.
- **SC-002**: 100% of covered hierarchy tests produce expected world transforms, topological parent-before-child operation order, root creation ordering, sibling insertion ordering, and cycle-rejection behavior for representative parent-child chains.
- **SC-003**: Render collection for a representative scene containing at least 10 mesh entities, 4 lights, and 2 cameras completes with stable accepted/rejected counts and deterministic per-category entity-identity ordering across 20 repeated runs.
- **SC-004**: Invalid entity, component, hierarchy, camera, light, and mesh operations complete without corrupting prior valid world state in all covered tests.
- **SC-005**: A developer can inspect diagnostics for an invalid scene operation and identify the failing entity or component category within one validation step.
- **SC-006**: The feature behavior is verifiable on Windows, macOS, and Linux through deterministic automated build/test coverage or an explicitly documented temporary validation gap and follow-up task.

## Assumptions

- The primary user is an engine/application developer preparing the project for a richer triangle demo, scene-driven examples, and later editor/runtime tooling.
- The next roadmap feature can still render a hardcoded triangle; this feature is a foundation for organized scenes rather than a prerequisite for drawing the first triangle.
- A single world is sufficient for v1; multiple simultaneous worlds, world streaming, and runtime scene serialization are deferred.
- Basic component storage and deterministic traversal are more important than high-performance archetype queries in this phase.
- Mesh components identify renderable content abstractly; loading mesh assets, owning graphics buffers, and binding live backend resources remain outside this feature.
- Transform, mesh, light, and camera components are enough for the first scene foundation; physics, animation, behavior scripting, audio, UI, and editor gizmos are deferred.
