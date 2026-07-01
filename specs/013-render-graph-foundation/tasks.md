# Tasks: Render Graph Foundation

**Input**: Design documents from `/specs/013-render-graph-foundation/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/render-graph-contract.md, quickstart.md
**Tests**: Required by FR-019 and quickstart negative/representative scenarios.
**Organization**: Tasks are grouped by user story to enable independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel because it touches different files and has no dependency on incomplete tasks in the same phase.
- **[Story]**: Maps to the user story from spec.md (`US1` through `US5`).
- Every task includes an exact repository file path.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create the file skeleton and test harness entry points needed by all render graph stories.

- [X] T001 Create Render Graph public header skeleton `Source/Renderer/Public/Renderer/FRenderGraph.h`
- [X] T002 Create Render Graph builder public header skeleton `Source/Renderer/Public/Renderer/FRenderGraphBuilder.h`
- [X] T003 Create Render Graph compiler public header skeleton `Source/Renderer/Public/Renderer/FRenderGraphCompiler.h`
- [X] T004 Create Render Graph executor public header skeleton `Source/Renderer/Public/Renderer/FRenderGraphExecutor.h`
- [X] T005 Create Render Graph resource public header skeleton `Source/Renderer/Public/Renderer/FRenderGraphResource.h`
- [X] T006 Create Render Graph pass public header skeleton `Source/Renderer/Public/Renderer/FRenderGraphPass.h`
- [X] T007 Create Render Graph diagnostics public header skeleton `Source/Renderer/Public/Renderer/FRenderGraphDiagnostics.h`
- [X] T008 Create Render Graph private source skeleton `Source/Renderer/Private/FRenderGraph.cpp`
- [X] T009 Create Render Graph builder private source skeleton `Source/Renderer/Private/FRenderGraphBuilder.cpp`
- [X] T010 Create Render Graph compiler private source skeleton `Source/Renderer/Private/FRenderGraphCompiler.cpp`
- [X] T011 Create Render Graph executor private source skeleton `Source/Renderer/Private/FRenderGraphExecutor.cpp`
- [X] T012 Create Render Graph resource private source skeleton `Source/Renderer/Private/FRenderGraphResource.cpp`
- [X] T013 Create Render Graph pass private source skeleton `Source/Renderer/Private/FRenderGraphPass.cpp`
- [X] T014 Create Render Graph diagnostics private source skeleton `Source/Renderer/Private/FRenderGraphDiagnostics.cpp`
- [X] T015 [P] Create Renderer render graph test header `Tests/RendererRenderGraphTests.h`
- [X] T016 [P] Create Renderer render graph test source `Tests/RendererRenderGraphTests.cpp`
- [X] T017 Register Renderer render graph tests in `Tests/Main.cpp`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Define shared enums, handles, result types, diagnostics, and mock-test utilities that all user stories depend on.

**CRITICAL**: No user story work can begin until this phase is complete.

- [X] T018 Define `ERenderGraphResult`, `ERenderGraphState`, and common result helpers in `Source/Renderer/Public/Renderer/FRenderGraphDiagnostics.h`
- [X] T019 Define stable pass/resource handle types and invalid-handle helpers in `Source/Renderer/Public/Renderer/FRenderGraphResource.h`
- [X] T020 Define pass type, resource kind, ownership, access type, alias policy, and transition reason enums in `Source/Renderer/Public/Renderer/FRenderGraphPass.h`
- [X] T021 Define diagnostic record, validation category, and deterministic diagnostic formatting declarations in `Source/Renderer/Public/Renderer/FRenderGraphDiagnostics.h`
- [X] T022 Implement diagnostic record storage and stable formatting in `Source/Renderer/Private/FRenderGraphDiagnostics.cpp`
- [X] T023 Define graph, builder, compiler, executor, resource, pass, and compiled graph forward declarations in `Source/Renderer/Public/Renderer/RendererMinimal.h`
- [X] T024 [P] Define mock render graph RHI resource and command record helpers in `Tests/RendererRenderGraphTests.cpp`
- [X] T025 [P] Add Renderer render graph test result entry point declaration in `Tests/RendererRenderGraphTests.h`
- [X] T026 Add Renderer render graph test runner call in `Tests/Main.cpp`

**Checkpoint**: Foundation ready; user story implementation can now begin.

---

## Phase 3: User Story 1 - Declare Render Work as a Graph (Priority: P1) MVP

**Goal**: Developers can declare passes, virtual resources, reads/writes, and dependency edges, then compile a deterministic graph order.

**Independent Test**: Declare multiple passes with virtual resources and verify read/write relationships and compiled ordering.

### Tests for User Story 1

- [X] T027 [US1] Add graph declaration success tests in `Tests/RendererRenderGraphTests.cpp`
- [X] T028 [US1] Add dependency ordering and stable tie-break tests in `Tests/RendererRenderGraphTests.cpp`
- [X] T029 [US1] Add cycle, read-before-write, invalid handle, and cross-graph handle rejection tests in `Tests/RendererRenderGraphTests.cpp`

### Implementation for User Story 1

- [X] T030 [P] [US1] Define `FRenderGraphPassDesc`, access declaration structs, and pass query API in `Source/Renderer/Public/Renderer/FRenderGraphPass.h`
- [X] T031 [P] [US1] Define `FRenderGraphResourceDesc` and resource query API in `Source/Renderer/Public/Renderer/FRenderGraphResource.h`
- [X] T032 [US1] Define `FRenderGraph` declaration, compile, reset, and query API in `Source/Renderer/Public/Renderer/FRenderGraph.h`
- [X] T033 [US1] Define `FRenderGraphBuilder` resource/pass/output declaration API in `Source/Renderer/Public/Renderer/FRenderGraphBuilder.h`
- [X] T034 [US1] Implement pass storage and access validation helpers in `Source/Renderer/Private/FRenderGraphPass.cpp`
- [X] T035 [US1] Implement resource storage and description validation helpers in `Source/Renderer/Private/FRenderGraphResource.cpp`
- [X] T036 [US1] Implement graph-owned pass/resource/output collections and lifecycle reset in `Source/Renderer/Private/FRenderGraph.cpp`
- [X] T037 [US1] Implement builder pass/resource/access/output declaration behavior in `Source/Renderer/Private/FRenderGraphBuilder.cpp`
- [X] T038 [US1] Define compiler API and compiled schedule data structures in `Source/Renderer/Public/Renderer/FRenderGraphCompiler.h`
- [X] T039 [US1] Implement graph validation, dependency edge construction, cycle rejection, and stable topological scheduling in `Source/Renderer/Private/FRenderGraphCompiler.cpp`
- [X] T040 [US1] Expose compiled schedule and validation diagnostics from `FRenderGraph` in `Source/Renderer/Private/FRenderGraph.cpp`

**Checkpoint**: User Story 1 is independently testable as the MVP.

---

## Phase 4: User Story 2 - Manage Virtual Resource Lifetimes (Priority: P1)

**Goal**: Developers can compile virtual resources into first-use/last-use lifetime summaries and aliasing eligibility diagnostics.

**Independent Test**: Create virtual resources across passes and verify lifetime, imported/exported handling, and aliasing eligibility/rejection.

### Tests for User Story 2

- [X] T041 [US2] Add transient, imported, exported, and side-effect resource lifetime tests in `Tests/RendererRenderGraphTests.cpp`
- [X] T042 [US2] Add aliasing eligibility and rejection reason tests in `Tests/RendererRenderGraphTests.cpp`
- [X] T043 [US2] Add execution validation test proving alias-eligible resources receive separate backing storage in `Tests/RendererRenderGraphTests.cpp`

### Implementation for User Story 2

- [X] T044 [P] [US2] Define resource lifetime and aliasing decision structs in `Source/Renderer/Public/Renderer/FRenderGraphResource.h`
- [X] T045 [P] [US2] Add lifetime and aliasing query accessors to compiled graph API in `Source/Renderer/Public/Renderer/FRenderGraphCompiler.h`
- [X] T046 [US2] Implement first-use and last-use lifetime analysis in `Source/Renderer/Private/FRenderGraphCompiler.cpp`
- [X] T047 [US2] Implement imported/exported/no-alias exclusions in `Source/Renderer/Private/FRenderGraphCompiler.cpp`
- [X] T048 [US2] Implement aliasing eligibility and deterministic rejection reasons in `Source/Renderer/Private/FRenderGraphCompiler.cpp`
- [X] T049 [US2] Add lifetime and aliasing diagnostics emission in `Source/Renderer/Private/FRenderGraphDiagnostics.cpp`

**Checkpoint**: User Story 2 works independently after compiling graphs with resources.

---

## Phase 5: User Story 3 - Produce Synchronization and Transition Plans (Priority: P1)

**Goal**: Developers can compile inspectable transition plans and execution can emit transitions matching those plans.

**Independent Test**: Build graphs with read-after-write, write-after-read, write-after-write, graphics-to-compute, and compute-to-graphics usage and verify planned/emitted transitions.

### Tests for User Story 3

- [X] T050 [US3] Add transition plan generation tests for all required transition reasons in `Tests/RendererRenderGraphTests.cpp`
- [X] T051 [US3] Add redundant transition elision tests in `Tests/RendererRenderGraphTests.cpp`
- [X] T052 [US3] Add execution transition emission matching compiled plan tests in `Tests/RendererRenderGraphTests.cpp`
- [X] T053 [US3] Add incompatible resource usage and layout rejection tests in `Tests/RendererRenderGraphTests.cpp`

### Implementation for User Story 3

- [X] T054 [P] [US3] Define transition state and transition record structs in `Source/Renderer/Public/Renderer/FRenderGraphCompiler.h`
- [X] T055 [P] [US3] Add transition plan query API to `FRenderGraph` in `Source/Renderer/Public/Renderer/FRenderGraph.h`
- [X] T056 [US3] Implement resource usage/layout compatibility validation in `Source/Renderer/Private/FRenderGraphCompiler.cpp`
- [X] T057 [US3] Implement transition plan generation for read/write and pass-kind changes in `Source/Renderer/Private/FRenderGraphCompiler.cpp`
- [X] T058 [US3] Implement redundant transition elision in `Source/Renderer/Private/FRenderGraphCompiler.cpp`
- [X] T059 [US3] Define executor API for transition emission in `Source/Renderer/Public/Renderer/FRenderGraphExecutor.h`
- [X] T060 [US3] Implement planned transition emission through RHI-facing command context in `Source/Renderer/Private/FRenderGraphExecutor.cpp`

**Checkpoint**: User Story 3 compiles and emits resource transitions independently.

---

## Phase 6: User Story 4 - Cull Unused Work and Execute the Compiled Graph (Priority: P2)

**Goal**: Developers can mark outputs, cull unused work, resolve transient resources, validate imports, execute scheduled passes, and fail fast on execution errors.

**Independent Test**: Create a graph with required and unused branches, compile with explicit outputs, verify culling, execute with mock RHI, and verify fail-fast behavior.

### Tests for User Story 4

- [X] T061 [US4] Add output-based culling and side-effect preservation tests in `Tests/RendererRenderGraphTests.cpp`
- [X] T062 [US4] Add zero-output rejection and zero-output side-effect acceptance tests in `Tests/RendererRenderGraphTests.cpp`
- [X] T063 [US4] Add transient resource resolution and missing/invalid imported resource tests in `Tests/RendererRenderGraphTests.cpp`
- [X] T064 [US4] Add execution ordering and first-pass-failure stop tests in `Tests/RendererRenderGraphTests.cpp`
- [X] T065 [US4] Add reset and invalidation execution rejection tests in `Tests/RendererRenderGraphTests.cpp`

### Implementation for User Story 4

- [X] T066 [P] [US4] Define execution context, imported resource binding, and pass callback contracts in `Source/Renderer/Public/Renderer/FRenderGraphExecutor.h`
- [X] T067 [P] [US4] Add culling result query API to compiled graph structures in `Source/Renderer/Public/Renderer/FRenderGraphCompiler.h`
- [X] T068 [US4] Implement output reachability, unused branch culling, and side-effect preservation in `Source/Renderer/Private/FRenderGraphCompiler.cpp`
- [X] T069 [US4] Implement zero-output validation rules in `Source/Renderer/Private/FRenderGraphCompiler.cpp`
- [X] T070 [US4] Implement transient resource resolution and separate backing storage tracking in `Source/Renderer/Private/FRenderGraphExecutor.cpp`
- [X] T071 [US4] Implement caller-supplied imported resource validation in `Source/Renderer/Private/FRenderGraphExecutor.cpp`
- [X] T072 [US4] Implement scheduled pass callback invocation and declared-resource access filtering in `Source/Renderer/Private/FRenderGraphExecutor.cpp`
- [X] T073 [US4] Implement fail-fast execution failure diagnostics in `Source/Renderer/Private/FRenderGraphExecutor.cpp`
- [X] T074 [US4] Wire `FRenderGraph::Execute`, reset, and invalidation behavior to executor state in `Source/Renderer/Private/FRenderGraph.cpp`

**Checkpoint**: User Story 4 executes compiled graphs through mock RHI behavior.

---

## Phase 7: User Story 5 - Inspect Graph Structure for Debugging (Priority: P3)

**Goal**: Developers can request deterministic text debug dumps for successful and failed graph states.

**Independent Test**: Compile representative valid and invalid graphs and compare debug output for pass order, resources, lifetimes, culling, aliasing, transitions, and diagnostics.

### Tests for User Story 5

- [X] T075 [US5] Add successful graph debug dump content and byte-stability tests in `Tests/RendererRenderGraphTests.cpp`
- [X] T076 [US5] Add failed graph debug dump diagnostic context tests in `Tests/RendererRenderGraphTests.cpp`

### Implementation for User Story 5

- [X] T077 [P] [US5] Define debug dump API on `FRenderGraph` and compiled graph structures in `Source/Renderer/Public/Renderer/FRenderGraph.h`
- [X] T078 [US5] Implement deterministic pass/resource/dependency dump formatting in `Source/Renderer/Private/FRenderGraphDiagnostics.cpp`
- [X] T079 [US5] Implement culling/lifetime/aliasing/transition/diagnostic dump sections in `Source/Renderer/Private/FRenderGraphDiagnostics.cpp`
- [X] T080 [US5] Wire debug dump output for failed and successful graph states in `Source/Renderer/Private/FRenderGraph.cpp`

**Checkpoint**: User Story 5 provides deterministic text inspection for valid and invalid graphs.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Finish integration, boundaries, documentation, and verification across all stories.

- [X] T081 Update `Source/Renderer/Public/Renderer/RendererMinimal.h` to include or forward the final Render Graph public API surface
- [X] T082 Verify Renderer layer build auto-discovers all new private sources via `Source/Renderer/SConscript`
- [X] T083 Verify test build includes Renderer render graph tests via `Tests/SConscript`
- [X] T084 Run `conda run -n godot scons` and record result in `specs/013-render-graph-foundation/quickstart.md`
- [X] T085 Run `Build/Mac/Debug/Tests/StonerTest` and record result in `specs/013-render-graph-foundation/quickstart.md`
- [X] T086 Record representative graph declare/compile/inspect/execute elapsed time against the 60-second SC-001 target in `specs/013-render-graph-foundation/quickstart.md`
- [X] T087 Run backend-boundary check from `specs/013-render-graph-foundation/quickstart.md` and record result in `specs/013-render-graph-foundation/quickstart.md`
- [X] T088 Review public naming and UE5-style conventions across `Source/Renderer/Public/Renderer/`
- [X] T089 Review no material, shader permutation, forward/deferred renderer, scene graph, presentation, async compute overlap, or backing-storage reuse scope creep in `Source/Renderer/`
- [X] T090 Update implementation status notes in `specs/013-render-graph-foundation/spec.md`
- [X] T091 Update roadmap phase status and implementation notes in `doc/roadmap.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies.
- **Foundational (Phase 2)**: Depends on Setup completion and blocks all user stories.
- **US1 (Phase 3)**: Depends on Foundational phase; MVP.
- **US2 (Phase 4)**: Depends on US1 compiled graph declarations and schedule.
- **US3 (Phase 5)**: Depends on US1 access declarations and schedule; can proceed in parallel with US2 after US1 if capacity allows.
- **US4 (Phase 6)**: Depends on US1 schedule, US2 resource planning, and US3 transition planning.
- **US5 (Phase 7)**: Depends on compiled metadata from US1-US4.
- **Polish (Phase 8)**: Depends on desired user stories being complete.

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Foundational; no dependency on other stories.
- **User Story 2 (P1)**: Depends on US1 graph/resource/pass declaration and compilation.
- **User Story 3 (P1)**: Depends on US1 graph/resource/pass declaration and compilation.
- **User Story 4 (P2)**: Depends on US1, US2, and US3.
- **User Story 5 (P3)**: Depends on compiled and execution metadata from US1-US4.

### Parallel Opportunities

- Setup skeleton tasks touching separate files can be parallelized after agreeing on names.
- Foundational mock-test helper and public enum/result declarations can proceed in parallel.
- US1 test tasks T027-T029 are sequential because they share `Tests/RendererRenderGraphTests.cpp`.
- US2 test tasks T041-T043 are sequential because they share `Tests/RendererRenderGraphTests.cpp`.
- US3 test tasks T050-T053 are sequential because they share `Tests/RendererRenderGraphTests.cpp`.
- US4 test tasks T061-T065 are sequential because they share `Tests/RendererRenderGraphTests.cpp`.
- US5 test tasks T075-T076 are sequential because they share `Tests/RendererRenderGraphTests.cpp`.

---

## Parallel Example: User Story 1

```bash
Task: "T030 [P] [US1] Define FRenderGraphPassDesc, access declaration structs, and pass query API in Source/Renderer/Public/Renderer/FRenderGraphPass.h"
Task: "T031 [P] [US1] Define FRenderGraphResourceDesc and resource query API in Source/Renderer/Public/Renderer/FRenderGraphResource.h"
```

## Parallel Example: User Story 2

```bash
Task: "T044 [P] [US2] Define resource lifetime and aliasing decision structs in Source/Renderer/Public/Renderer/FRenderGraphResource.h"
Task: "T045 [P] [US2] Add lifetime and aliasing query accessors to compiled graph API in Source/Renderer/Public/Renderer/FRenderGraphCompiler.h"
```

## Parallel Example: User Story 3

```bash
Task: "T054 [P] [US3] Define transition state and transition record structs in Source/Renderer/Public/Renderer/FRenderGraphCompiler.h"
Task: "T055 [P] [US3] Add transition plan query API to FRenderGraph in Source/Renderer/Public/Renderer/FRenderGraph.h"
```

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1 setup.
2. Complete Phase 2 foundational declarations and mock helpers.
3. Complete Phase 3 US1 tests and implementation.
4. Stop and validate graph declaration, dependency ordering, and invalid declaration paths.

### Incremental Delivery

1. US1: graph declaration and deterministic compilation.
2. US2: resource lifetime and aliasing eligibility.
3. US3: transition planning and transition emission.
4. US4: culling, resource resolution, execution, and fail-fast behavior.
5. US5: deterministic debug dump.
6. Polish: build/test/boundary verification and roadmap/spec updates.

### Team Parallel Strategy

After US1 stabilizes:

- Developer A: US2 lifetime and aliasing planning.
- Developer B: US3 transition planning and emission.
- Developer C: executor contract prep and US4 tests.

---

## Notes

- Tests are included because FR-019 explicitly requires render graph coverage.
- `[P]` tasks are marked only when the task can be started without depending on an incomplete same-phase task and when file conflicts are manageable.
- Keep Renderer graph public contracts backend-agnostic.
- Do not implement real backing-storage alias reuse in this feature.
- Commit after each completed phase or coherent task group using the project conventional commit style.
