# Tasks: Scene Graph & ECS Foundation

**Input**: Design documents from `/specs/017-scene-graph-ecs/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/scene-graph-ecs-contract.md, quickstart.md

**Tests**: Required by spec FR-016, SC-001 through SC-006, quickstart Required Test Coverage, and the constitution cross-platform validation gate. Test tasks are listed before implementation tasks in each user story.

**Organization**: Tasks are grouped by user story so each story can be implemented and validated as an independent increment.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel because it touches different files and does not depend on incomplete tasks.
- **[Story]**: User story label for story phases only.
- Every task includes an exact repository path.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Prepare the Application scene/ECS file surface and test registration points without implementing story behavior.

- [X] T001 Create placeholder scene/ECS public headers in Source/Application/Public/Application/ESceneResult.h, Source/Application/Public/Application/ESceneComponentType.h, Source/Application/Public/Application/ESceneLightType.h, Source/Application/Public/Application/ESceneProjectionType.h, Source/Application/Public/Application/FEntity.h, Source/Application/Public/Application/FWorld.h, Source/Application/Public/Application/FTransformComponent.h, Source/Application/Public/Application/FMeshComponent.h, Source/Application/Public/Application/FLightComponent.h, Source/Application/Public/Application/FCameraComponent.h, Source/Application/Public/Application/FEntityHierarchy.h, Source/Application/Public/Application/FSceneDiagnostics.h, Source/Application/Public/Application/FSceneRenderSummary.h, and Source/Application/Public/Application/FRenderSystem.h
- [X] T002 Create placeholder scene/ECS implementation files in Source/Application/Private/ESceneResult.cpp, Source/Application/Private/FEntity.cpp, Source/Application/Private/FWorld.cpp, Source/Application/Private/FTransformComponent.cpp, Source/Application/Private/FMeshComponent.cpp, Source/Application/Private/FLightComponent.cpp, Source/Application/Private/FCameraComponent.cpp, Source/Application/Private/FEntityHierarchy.cpp, Source/Application/Private/FSceneDiagnostics.cpp, Source/Application/Private/FSceneRenderSummary.cpp, and Source/Application/Private/FRenderSystem.cpp
- [X] T003 [P] Create Application scene/ECS test declaration file in Tests/ApplicationSceneEcsTests.h
- [X] T004 Create Application scene/ECS test source file and include it in the existing auto-discovered test build at Tests/ApplicationSceneEcsTests.cpp
- [X] T005 Register RunApplicationSceneEcsTests in the test executable entry point in Tests/Main.cpp

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Define shared result, identity, component, diagnostics, and world scaffolding required before any user story can be completed.

**Critical**: No user story work can complete until this phase is done.

- [X] T006 Define scene result/status values and stable result names in Source/Application/Public/Application/ESceneResult.h
- [X] T007 Implement scene result/status string conversion helpers in Source/Application/Private/ESceneResult.cpp
- [X] T008 [P] Define scene component, light, and projection enum contracts in Source/Application/Public/Application/ESceneComponentType.h, Source/Application/Public/Application/ESceneLightType.h, and Source/Application/Public/Application/ESceneProjectionType.h
- [X] T009 [P] Define generation-safe entity handle fields, comparison, invalid handle state, and identity ordering in Source/Application/Public/Application/FEntity.h
- [X] T010 Implement FEntity validity helpers and slot-index-then-generation ordering in Source/Application/Private/FEntity.cpp
- [X] T011 [P] Define transform, mesh, light, and camera component value types in Source/Application/Public/Application/FTransformComponent.h, Source/Application/Public/Application/FMeshComponent.h, Source/Application/Public/Application/FLightComponent.h, and Source/Application/Public/Application/FCameraComponent.h
- [X] T012 Implement component validation and deterministic defaults after component value contracts are defined in Source/Application/Private/FTransformComponent.cpp, Source/Application/Private/FMeshComponent.cpp, Source/Application/Private/FLightComponent.cpp, and Source/Application/Private/FCameraComponent.cpp
- [X] T013 [P] Define deterministic diagnostic record and diagnostic collection contracts in Source/Application/Public/Application/FSceneDiagnostics.h
- [X] T014 Implement diagnostic append, clear, stable code text, and byte-stable dump helpers in Source/Application/Private/FSceneDiagnostics.cpp
- [X] T015 Define FWorld public API skeleton for world reset, entity lifecycle, components, hierarchy, render collection, diagnostics, and debug dumps in Source/Application/Public/Application/FWorld.h
- [X] T016 Implement FWorld internal slot storage, ownership validation scaffolding, reset behavior, and capacity result plumbing in Source/Application/Private/FWorld.cpp
- [X] T017 Export scene/ECS public headers from the Application umbrella include in Source/Application/Public/Application/ApplicationMinimal.h

**Checkpoint**: Shared public contracts compile and story phases can implement behavior independently.

---

## Phase 3: User Story 1 - Create and Manage Scene Entities (Priority: P1) MVP

**Goal**: Developers can create worlds and entities, attach/read/update/replace/remove supported components, destroy entities, reuse slots safely, and reject stale handles.

**Independent Test**: Create a world, create at least 100 entities, add/remove supported components, destroy selected entities, force slot reuse, and verify live versus stale handles and component state deterministically.

### Tests for User Story 1

- [X] T018 [P] [US1] Add entity creation, live-count, reset, and capacity-bound tests in Tests/ApplicationSceneEcsTests.cpp
- [X] T019 [P] [US1] Add component add/read/update/replace/remove and missing-component tests in Tests/ApplicationSceneEcsTests.cpp
- [X] T020 [P] [US1] Add destroy, slot reuse, and stale-handle rejection tests in Tests/ApplicationSceneEcsTests.cpp

### Implementation for User Story 1

- [X] T021 [US1] Implement entity creation, live-count queries, configured v1 capacity handling, and reset invalidation in Source/Application/Private/FWorld.cpp
- [X] T022 [US1] Implement entity destruction for leaf entities, free-slot reuse, generation increments, and stale-handle rejection in Source/Application/Private/FWorld.cpp
- [X] T023 [US1] Implement transform component add/read/update/replace/remove operations with duplicate-add rejection in Source/Application/Private/FWorld.cpp
- [X] T024 [US1] Implement mesh, light, and camera component add/read/update/replace/remove operations with duplicate-add rejection in Source/Application/Private/FWorld.cpp
- [X] T025 [US1] Implement component presence bits and cleanup on entity destruction in Source/Application/Private/FWorld.cpp
- [X] T026 [US1] Add deterministic entity/component diagnostics for invalid entity, stale entity, duplicate component, and missing component outcomes in Source/Application/Private/FWorld.cpp

**Checkpoint**: User Story 1 is functional and testable without hierarchy or render collection.

---

## Phase 4: User Story 2 - Organize Spatial Parent-Child Hierarchies (Priority: P1)

**Goal**: Developers can parent, unparent, reparent, recursively destroy subtrees, and obtain deterministic world transforms using public ordering keys.

**Independent Test**: Build a small hierarchy with known local transforms, query world transforms, reparent with both preserve modes, reject cycles, destroy a parent recursively, and verify topological ordering.

### Tests for User Story 2

- [X] T027 [P] [US2] Add local-to-world transform propagation tests for roots, parents, children, zero scale, and non-uniform scale in Tests/ApplicationSceneEcsTests.cpp
- [X] T028 [P] [US2] Add parent, unparent, reparent-preserve-world, and reparent-preserve-local tests in Tests/ApplicationSceneEcsTests.cpp
- [X] T029 [P] [US2] Add self-parent, descendant-cycle, invalid-parent, recursive-destroy, and topological-order tests in Tests/ApplicationSceneEcsTests.cpp

### Implementation for User Story 2

- [X] T030 [P] [US2] Define hierarchy relationship types and reparent preservation options in Source/Application/Public/Application/FEntityHierarchy.h
- [X] T031 [US2] Implement parent, unparent, reparent, root ordering, child insertion ordering, and cycle detection helpers in Source/Application/Private/FEntityHierarchy.cpp
- [X] T032 [US2] Integrate hierarchy ownership and subtree cleanup with FWorld entity lifecycle in Source/Application/Private/FWorld.cpp
- [X] T033 [US2] Implement transform dirtying, parent-before-child world transform propagation, and world transform queries in Source/Application/Private/FWorld.cpp
- [X] T034 [US2] Implement default world-transform-preserving reparent and explicit local-transform-preserving reparent in Source/Application/Private/FWorld.cpp
- [X] T035 [US2] Implement subtree traversal and recursive destruction order as parents before children, roots in creation order, siblings in insertion order in Source/Application/Private/FWorld.cpp

**Checkpoint**: User Story 2 is functional and testable with US1 lifecycle/components but without render collection.

---

## Phase 5: User Story 3 - Collect Render-Relevant Scene Data (Priority: P2)

**Goal**: Developers can collect renderables, lights, and cameras into deterministic backend-neutral summaries for later renderer consumption.

**Independent Test**: Create entities with transform, mesh, light, and camera data, collect summaries across 20 repeated runs, and verify accepted/rejected counts plus per-category entity identity ordering.

### Tests for User Story 3

- [X] T036 [P] [US3] Add renderable collection tests for transform-plus-mesh acceptance and missing-transform or missing-mesh rejection in Tests/ApplicationSceneEcsTests.cpp
- [X] T037 [P] [US3] Add light and camera collection tests for valid values, invalid ranges, invalid projection values, and active-camera data in Tests/ApplicationSceneEcsTests.cpp
- [X] T038 [P] [US3] Add repeated-run deterministic ordering tests for at least 10 mesh entities, 4 lights, 2 cameras, optional sort keys, and entity identity tie-breaks in Tests/ApplicationSceneEcsTests.cpp

### Implementation for User Story 3

- [X] T039 [P] [US3] Define accepted renderable, accepted light, accepted camera, rejected item, and summary contracts in Source/Application/Public/Application/FSceneRenderSummary.h
- [X] T040 [US3] Implement deterministic summary storage, clear, count, ordering, and dump helpers in Source/Application/Private/FSceneRenderSummary.cpp
- [X] T041 [P] [US3] Define renderer-facing scene collection entry point in Source/Application/Public/Application/FRenderSystem.h
- [X] T042 [US3] Implement render collection over FWorld scene data without exposing Renderer frame plans or RHI resources in Source/Application/Private/FRenderSystem.cpp
- [X] T043 [US3] Integrate FWorld render collection API with FRenderSystem and FSceneRenderSummary in Source/Application/Private/FWorld.cpp
- [X] T044 [US3] Implement per-category ordering by optional sort key followed by entity slot index and generation tie-break in Source/Application/Private/FSceneRenderSummary.cpp
- [X] T045 [US3] Implement invalid renderable, light, and camera rejection records without stopping valid item collection in Source/Application/Private/FRenderSystem.cpp

**Checkpoint**: User Story 3 is functional and testable as an Application-owned scene summary.

---

## Phase 6: User Story 4 - Diagnose Invalid Scene Operations (Priority: P3)

**Goal**: Developers receive stable result statuses, diagnostic codes, and debug dumps for invalid scene operations without corrupting prior valid state.

**Independent Test**: Apply invalid operations in known order and verify result statuses, diagnostic codes, unchanged valid state, and byte-stable dumps.

### Tests for User Story 4

- [X] T046 [P] [US4] Add invalid operation diagnostics tests for invalid entity, duplicate component, missing component, unsupported operation, and capacity exceeded in Tests/ApplicationSceneEcsTests.cpp
- [X] T047 [P] [US4] Add invalid hierarchy and invalid component data state-preservation tests in Tests/ApplicationSceneEcsTests.cpp
- [X] T048 [P] [US4] Add byte-stable diagnostics, world dump, hierarchy dump, and render summary dump tests across repeated equivalent operation sequences in Tests/ApplicationSceneEcsTests.cpp

### Implementation for User Story 4

- [X] T049 [US4] Implement stable diagnostic codes and subject formatting for lifecycle, component, hierarchy, and render collection failures in Source/Application/Private/FSceneDiagnostics.cpp
- [X] T050 [US4] Add FWorld diagnostic accessors, clear helpers, and invalid-operation recording integration in Source/Application/Private/FWorld.cpp
- [X] T051 [US4] Implement byte-stable world, hierarchy, component, and render summary dump text with no pointer addresses or platform-specific transient values in Source/Application/Private/FWorld.cpp
- [X] T052 [US4] Verify invalid operations preserve previous valid world, component, hierarchy, and render summary state in Source/Application/Private/FWorld.cpp

**Checkpoint**: User Story 4 completes deterministic diagnostics and inspection coverage.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Validate integration, boundaries, cross-platform CI, and documentation after selected user stories are implemented.

- [X] T053 [P] Run public Application boundary leakage check from quickstart.md against Source/Application/Public/Application and address any matches in Source/Application/Public/Application
- [X] T054 [P] Verify naming and public API consistency with PascalCase and UnrealEngine5-style conventions in Source/Application/Public/Application
- [X] T055 [P] Update GitHub Actions or equivalent CI matrix to continue Windows, macOS, and Linux SCons build plus deterministic StonerTest execution in .github/workflows/ci.yml
- [X] T056 Document any temporary automated cross-platform validation gap with fallback manual commands and follow-up tasks in specs/017-scene-graph-ecs/quickstart.md
- [X] T057 Run local build with conda run -n godot scons and fix any Application scene/ECS build failures in Source/Application
- [X] T058 Run the local StonerTest executable and fix any scene/ECS regression failures in Tests/ApplicationSceneEcsTests.cpp
- [X] T059 [P] Update feature documentation summary for scene graph and ECS foundation in doc/017-scene-graph-ecs.html
- [X] T060 [P] Remove stale placeholders and verify all 017 design artifacts reference implemented behavior and exclude out-of-scope physics, animation, scripting, serialization, editor UI, authoritative spatial indexes, archetype optimization, and multi-world scheduling in specs/017-scene-graph-ecs

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies.
- **Foundational (Phase 2)**: Depends on Setup and blocks all user stories.
- **User Story 1 (Phase 3)**: Depends on Foundational.
- **User Story 2 (Phase 4)**: Depends on Foundational and uses US1 entity lifecycle/component behavior for full validation.
- **User Story 3 (Phase 5)**: Depends on Foundational and uses US1 components plus US2 world transforms for full validation.
- **User Story 4 (Phase 6)**: Depends on Foundational and can be finished after the invalid paths for US1, US2, and US3 exist.
- **Polish (Phase 7)**: Depends on all desired user stories for the release increment.

### User Story Dependencies

- **US1 (P1)**: First MVP increment after Foundational; no story dependency.
- **US2 (P1)**: Can start after Foundational but reaches full independent test coverage once US1 lifecycle and transform components exist.
- **US3 (P2)**: Can start after Foundational but reaches full independent test coverage once US1 components and US2 world transforms exist.
- **US4 (P3)**: Can start diagnostic scaffolding after Foundational, then complete after invalid paths from US1 through US3 are implemented.

### Within Each User Story

- Tests are written before implementation.
- Public contracts and value types precede implementation.
- FWorld integration follows helper/component implementation.
- Story checkpoint validation happens before moving to the next priority story.

---

## Parallel Opportunities

- T003 and T004 can start after T001/T002 create the file surface.
- T008, T009, T011, and T013 can run in parallel because they define separate public contracts.
- US1 tests T018, T019, and T020 can be written in parallel.
- US2 tests T027, T028, and T029 can be written in parallel.
- US3 tests T036, T037, and T038 can be written in parallel.
- US4 tests T046, T047, and T048 can be written in parallel.
- Public-summary work T039 and T041 can run in parallel after US3 tests are drafted.
- Polish checks T053, T054, T055, T059, and T060 can run in parallel once implementation is available.

---

## Parallel Example: User Story 1

```text
Task: "T018 [P] [US1] Add entity creation, live-count, reset, and capacity-bound tests in Tests/ApplicationSceneEcsTests.cpp"
Task: "T019 [P] [US1] Add component add/read/update/replace/remove and missing-component tests in Tests/ApplicationSceneEcsTests.cpp"
Task: "T020 [P] [US1] Add destroy, slot reuse, and stale-handle rejection tests in Tests/ApplicationSceneEcsTests.cpp"
```

## Parallel Example: User Story 2

```text
Task: "T027 [P] [US2] Add local-to-world transform propagation tests for roots, parents, children, zero scale, and non-uniform scale in Tests/ApplicationSceneEcsTests.cpp"
Task: "T028 [P] [US2] Add parent, unparent, reparent-preserve-world, and reparent-preserve-local tests in Tests/ApplicationSceneEcsTests.cpp"
Task: "T029 [P] [US2] Add self-parent, descendant-cycle, invalid-parent, recursive-destroy, and topological-order tests in Tests/ApplicationSceneEcsTests.cpp"
```

## Parallel Example: User Story 3

```text
Task: "T036 [P] [US3] Add renderable collection tests for transform-plus-mesh acceptance and missing-transform or missing-mesh rejection in Tests/ApplicationSceneEcsTests.cpp"
Task: "T037 [P] [US3] Add light and camera collection tests for valid values, invalid ranges, invalid projection values, and active-camera data in Tests/ApplicationSceneEcsTests.cpp"
Task: "T038 [P] [US3] Add repeated-run deterministic ordering tests for at least 10 mesh entities, 4 lights, 2 cameras, optional sort keys, and entity identity tie-breaks in Tests/ApplicationSceneEcsTests.cpp"
```

## Parallel Example: User Story 4

```text
Task: "T046 [P] [US4] Add invalid operation diagnostics tests for invalid entity, duplicate component, missing component, unsupported operation, and capacity exceeded in Tests/ApplicationSceneEcsTests.cpp"
Task: "T047 [P] [US4] Add invalid hierarchy and invalid component data state-preservation tests in Tests/ApplicationSceneEcsTests.cpp"
Task: "T048 [P] [US4] Add byte-stable diagnostics, world dump, hierarchy dump, and render summary dump tests across repeated equivalent operation sequences in Tests/ApplicationSceneEcsTests.cpp"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1 and Phase 2.
2. Write US1 tests T018 through T020 and confirm they fail against scaffolding.
3. Implement US1 tasks T021 through T026.
4. Validate with local build and StonerTest, focused on entity lifecycle and component operations.

### Incremental Delivery

1. Deliver US1 for safe entity/component ownership.
2. Add US2 hierarchy and transform propagation without changing US1 behavior.
3. Add US3 render collection summaries on top of stable world transforms.
4. Add US4 diagnostics and dump stability across all invalid paths.
5. Finish Polish with boundary checks, cross-platform CI validation, docs, and quickstart verification.

### Parallel Team Strategy

1. Complete Setup and Foundational contracts together.
2. Draft story tests in parallel while keeping implementation serialized around FWorld.cpp merge points.
3. Assign separate implementation streams to FEntityHierarchy.cpp, FSceneRenderSummary.cpp, FRenderSystem.cpp, and FSceneDiagnostics.cpp after FWorld public contracts stabilize.
4. Reconcile FWorld integration tasks in priority order: US1, US2, US3, US4.

---

## Notes

- `[P]` tasks use separate files or independent test sections and can be parallelized with normal merge discipline.
- `FWorld.cpp` tasks are intentionally not marked `[P]` because they share central world state.
- Keep public Application scene contracts backend-neutral and free of Renderer frame plans, RHI resources, native handles, and platform window types.
- Commit after each story checkpoint or logical task group using the project conventional commit style.
