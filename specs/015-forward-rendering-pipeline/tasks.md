# Tasks: Forward Rendering Pipeline

**Input**: Design documents from `/specs/015-forward-rendering-pipeline/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/forward-rendering-contract.md, quickstart.md
**Tests**: Required by spec success criteria and quickstart representative/negative scenarios.
**Organization**: Tasks are grouped by user story to enable independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel because it touches different files and has no dependency on incomplete tasks in the same phase.
- **[Story]**: Maps to the user story from spec.md (`US1` through `US3`).
- Every task includes an exact repository file path.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create the forward renderer file skeleton and test harness entry points needed by all stories.

- [X] T001 Create Forward Renderer public header skeleton in `Source/Renderer/Public/Renderer/FForwardRenderer.h`
- [X] T002 Create Forward Frame Plan public header skeleton in `Source/Renderer/Public/Renderer/FForwardFramePlan.h`
- [X] T003 Create Forward View Data public header skeleton in `Source/Renderer/Public/Renderer/FForwardViewData.h`
- [X] T004 Create Forward Light Data public header skeleton in `Source/Renderer/Public/Renderer/FForwardLightData.h`
- [X] T005 Create Mesh Draw Command public header skeleton in `Source/Renderer/Public/Renderer/FMeshDrawCommand.h`
- [X] T006 Create Forward Render Graph Declaration public header skeleton in `Source/Renderer/Public/Renderer/FForwardRenderGraphDeclaration.h`
- [X] T007 Create Forward Diagnostics public header skeleton in `Source/Renderer/Public/Renderer/FForwardDiagnostics.h`
- [X] T008 Create Forward Renderer private source skeleton in `Source/Renderer/Private/FForwardRenderer.cpp`
- [X] T009 Create Forward Frame Plan private source skeleton in `Source/Renderer/Private/FForwardFramePlan.cpp`
- [X] T010 Create Forward View Data private source skeleton in `Source/Renderer/Private/FForwardViewData.cpp`
- [X] T011 Create Forward Light Data private source skeleton in `Source/Renderer/Private/FForwardLightData.cpp`
- [X] T012 Create Mesh Draw Command private source skeleton in `Source/Renderer/Private/FMeshDrawCommand.cpp`
- [X] T013 Create Forward Render Graph Declaration private source skeleton in `Source/Renderer/Private/FForwardRenderGraphDeclaration.cpp`
- [X] T014 Create Forward Diagnostics private source skeleton in `Source/Renderer/Private/FForwardDiagnostics.cpp`
- [X] T015 [P] Create Renderer forward pipeline test header in `Tests/RendererForwardPipelineTests.h`
- [X] T016 [P] Create Renderer forward pipeline test source in `Tests/RendererForwardPipelineTests.cpp`
- [X] T017 Register Renderer forward pipeline tests in `Tests/Main.cpp`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Define shared result types, configuration, diagnostics, identifiers, and test builders that all forward rendering stories depend on.

**CRITICAL**: No user story work can begin until this phase is complete.

- [X] T018 Define `EForwardResult`, `EForwardValidationState`, diagnostic severity, and diagnostic category enums in `Source/Renderer/Public/Renderer/FForwardDiagnostics.h`
- [X] T019 Define forward diagnostic record, stable code, subject identity, and deterministic formatting declarations in `Source/Renderer/Public/Renderer/FForwardDiagnostics.h`
- [X] T020 Implement diagnostic storage, stable ordering, merge behavior, and formatting helpers in `Source/Renderer/Private/FForwardDiagnostics.cpp`
- [X] T021 Define forward renderer configuration with default point light limit 4, ambient fallback policy, sky flag, and transparent sort policy in `Source/Renderer/Public/Renderer/FForwardRenderer.h`
- [X] T022 Define stable object, mesh, material, pass, output, and frame identifier structs in `Source/Renderer/Public/Renderer/FForwardFramePlan.h`
- [X] T023 Define pass stage enum for depth, opaque, sky/background, and transparent work in `Source/Renderer/Public/Renderer/FForwardFramePlan.h`
- [X] T024 Define reusable math/value helpers needed by forward view and light validation in `Source/Renderer/Public/Renderer/FForwardViewData.h`
- [X] T025 Implement finite-value, positive-extent, and stable id validation helpers in `Source/Renderer/Private/FForwardViewData.cpp`
- [X] T026 Define shared forward test fixture builders for views, outputs, lights, material bindings, and draw candidates in `Tests/RendererForwardPipelineTests.cpp`
- [X] T027 [P] Add forward pipeline test entry point declaration in `Tests/RendererForwardPipelineTests.h`
- [X] T028 Add forward pipeline test runner call in `Tests/Main.cpp`
- [X] T029 Export forward rendering public headers through `Source/Renderer/Public/Renderer/RendererMinimal.h`

**Checkpoint**: Foundation ready; user story implementation can now begin.

---

## Phase 3: User Story 1 - Render Lit Opaque Geometry (Priority: P1) MVP

**Goal**: Developers can prepare a deterministic forward frame plan and render graph-compatible declarations for valid opaque geometry, material bindings, view data, output targets, and one directional light.

**Independent Test**: Declare a camera, output target, opaque mesh items, material data, and one directional light, then request a frame plan and verify depth-before-opaque pass order, final output declaration, stable draw ordering, and rejection diagnostics for invalid opaque materials.

### Tests for User Story 1

- [X] T030 [US1] Add valid opaque frame preparation test with one view, one output, one directional light, and four opaque draws in `Tests/RendererForwardPipelineTests.cpp`
- [X] T031 [US1] Add depth-before-opaque pass order and final color target declaration test in `Tests/RendererForwardPipelineTests.cpp`
- [X] T032 [US1] Add stable opaque draw identity and ordering test across 20 repeated frame preparations in `Tests/RendererForwardPipelineTests.cpp`
- [X] T033 [US1] Add invalid view data and missing output target rejection tests in `Tests/RendererForwardPipelineTests.cpp`
- [X] T034 [US1] Add invalid or incomplete opaque material binding rejection diagnostic tests in `Tests/RendererForwardPipelineTests.cpp`

### Implementation for User Story 1

- [X] T035 [P] [US1] Define `FForwardViewData`, viewport extent, camera position, and view-projection validation API in `Source/Renderer/Public/Renderer/FForwardViewData.h`
- [X] T036 [P] [US1] Define `FForwardOutputTarget`, color/depth target summary, format intent, and extent validation API in `Source/Renderer/Public/Renderer/FForwardFramePlan.h`
- [X] T037 [P] [US1] Define `FMeshDrawCommand`, draw candidate, pass mask, stable sort key, and validation state API in `Source/Renderer/Public/Renderer/FMeshDrawCommand.h`
- [X] T038 [P] [US1] Define `FForwardFramePlan`, pass record, accepted/rejected draw lists, and frame query API in `Source/Renderer/Public/Renderer/FForwardFramePlan.h`
- [X] T039 [P] [US1] Define `FForwardRenderGraphDeclaration`, pass declaration, resource declaration, access declaration, and graph output summary API in `Source/Renderer/Public/Renderer/FForwardRenderGraphDeclaration.h`
- [X] T040 [US1] Implement view validation and diagnostics in `Source/Renderer/Private/FForwardViewData.cpp`
- [X] T041 [US1] Implement output target validation and diagnostics in `Source/Renderer/Private/FForwardFramePlan.cpp`
- [X] T042 [US1] Implement opaque mesh draw command construction, material binding compatibility checks, and stable draw identity in `Source/Renderer/Private/FMeshDrawCommand.cpp`
- [X] T043 [US1] Implement frame plan assembly for depth and opaque stages in `Source/Renderer/Private/FForwardFramePlan.cpp`
- [X] T044 [US1] Implement render graph-compatible color, depth, opaque pass, and output declarations in `Source/Renderer/Private/FForwardRenderGraphDeclaration.cpp`
- [X] T045 [US1] Implement `FForwardRenderer::PrepareFrame` MVP flow for valid view/output/opaque/directional-light inputs in `Source/Renderer/Private/FForwardRenderer.cpp`
- [X] T046 [US1] Implement deterministic frame debug dump sections for view, output, pass order, opaque draws, declarations, and diagnostics in `Source/Renderer/Private/FForwardDiagnostics.cpp`
- [X] T047 [US1] Expose forward renderer prepare/reset/invalidate/query APIs in `Source/Renderer/Public/Renderer/FForwardRenderer.h`

**Checkpoint**: User Story 1 is independently testable as the MVP.

---

## Phase 4: User Story 2 - Evaluate Basic Material Lighting (Priority: P2)

**Goal**: Developers can validate full PBR-style material inputs, prepare directional and point light data, select point lights by deterministic influence, and use constant ambient-only fallback when no lights are accepted.

**Independent Test**: Prepare representative full PBR-style material inputs, one directional light, several point lights within and beyond the configured limit, and no-light fallback cases, then verify lighting data, accepted/rejected counts, diagnostics, and frame declarations.

### Tests for User Story 2

- [X] T048 [US2] Add full PBR-style surface input success and missing-input rejection tests in `Tests/RendererForwardPipelineTests.cpp`
- [X] T049 [US2] Add directional light validation and multiple-primary directional light diagnostic tests in `Tests/RendererForwardPipelineTests.cpp`
- [X] T050 [US2] Add point light default limit 4 selection and accepted/rejected count tests in `Tests/RendererForwardPipelineTests.cpp`
- [X] T051 [US2] Add configurable point light limit override and zero-limit behavior tests in `Tests/RendererForwardPipelineTests.cpp`
- [X] T052 [US2] Add point light influence ordering tests using distance, effectiveness, and stable tie-breakers in `Tests/RendererForwardPipelineTests.cpp`
- [X] T053 [US2] Add invalid point light range, intensity, position, and identity diagnostics tests in `Tests/RendererForwardPipelineTests.cpp`
- [X] T054 [US2] Add no accepted lights ambient-only fallback diagnostic test in `Tests/RendererForwardPipelineTests.cpp`

### Implementation for User Story 2

- [X] T055 [P] [US2] Define `FForwardDirectionalLight`, `FForwardPointLight`, `FForwardLightSet`, and light validation API in `Source/Renderer/Public/Renderer/FForwardLightData.h`
- [X] T056 [P] [US2] Define `FForwardPBRSurfaceInputs`, extension slot records, and material input validation declarations in `Source/Renderer/Public/Renderer/FMeshDrawCommand.h`
- [X] T057 [US2] Implement directional light validation, primary-light selection, and multiple-primary diagnostics in `Source/Renderer/Private/FForwardLightData.cpp`
- [X] T058 [US2] Implement point light field validation and diagnostic emission in `Source/Renderer/Private/FForwardLightData.cpp`
- [X] T059 [US2] Implement deterministic point light influence scoring using view distance and effectiveness in `Source/Renderer/Private/FForwardLightData.cpp`
- [X] T060 [US2] Implement configurable point light limit selection, default limit 4, accepted/rejected lists, and stable tie-breakers in `Source/Renderer/Private/FForwardLightData.cpp`
- [X] T061 [US2] Implement full PBR-style material input validation for base color, metallic, roughness, normal, occlusion, emissive, alpha, and extension slots in `Source/Renderer/Private/FMeshDrawCommand.cpp`
- [X] T062 [US2] Integrate light set preparation and PBR validation into `FForwardRenderer::PrepareFrame` in `Source/Renderer/Private/FForwardRenderer.cpp`
- [X] T063 [US2] Implement ambient-only fallback plan behavior and exactly-one fallback diagnostic for valid geometry with no accepted lights in `Source/Renderer/Private/FForwardFramePlan.cpp`
- [X] T064 [US2] Extend render graph-compatible declarations with light data and material resource requirement summaries in `Source/Renderer/Private/FForwardRenderGraphDeclaration.cpp`
- [X] T065 [US2] Extend debug dumps with PBR input validation, light selection, accepted/rejected counts, and ambient fallback decisions in `Source/Renderer/Private/FForwardDiagnostics.cpp`

**Checkpoint**: User Story 2 works independently after the MVP frame planner exists.

---

## Phase 5: User Story 3 - Compose Sky and Transparent Geometry (Priority: P3)

**Goal**: Developers can include sky/background behavior and transparent draw declarations after opaque work, with transparent draws sorted by camera-space depth descending, stable material id, and stable object id.

**Independent Test**: Prepare a scene with sky/background data and transparent objects at distinct and equal camera-space depths, then verify stage order, sorting, tie-breakers, compatible material validation, and debug dumps.

### Tests for User Story 3

- [X] T066 [US3] Add sky/background declaration test that verifies background does not overwrite opaque geometry order in `Tests/RendererForwardPipelineTests.cpp`
- [X] T067 [US3] Add transparent draw camera-space depth descending sort test in `Tests/RendererForwardPipelineTests.cpp`
- [X] T068 [US3] Add equal-depth transparent tie-breaker tests by stable material id and stable object id in `Tests/RendererForwardPipelineTests.cpp`
- [X] T069 [US3] Add incompatible transparent material rejection diagnostic test in `Tests/RendererForwardPipelineTests.cpp`
- [X] T070 [US3] Add no-renderable-geometry clear/background valid plan test in `Tests/RendererForwardPipelineTests.cpp`
- [X] T071 [US3] Add repeated transparent and sky frame dump stability test across 20 preparations in `Tests/RendererForwardPipelineTests.cpp`

### Implementation for User Story 3

- [X] T072 [P] [US3] Define environment background mode, environment resource requirement, and background declaration API in `Source/Renderer/Public/Renderer/FForwardFramePlan.h`
- [X] T073 [P] [US3] Extend `FMeshDrawCommand` public API with transparent pass compatibility, camera-space depth key, stable material id, and stable object id accessors in `Source/Renderer/Public/Renderer/FMeshDrawCommand.h`
- [X] T074 [US3] Implement environment background validation and fallback clear/background behavior in `Source/Renderer/Private/FForwardFramePlan.cpp`
- [X] T075 [US3] Implement transparent material compatibility checks and rejection diagnostics in `Source/Renderer/Private/FMeshDrawCommand.cpp`
- [X] T076 [US3] Implement camera-space depth computation and transparent sorting by depth, material id, and object id in `Source/Renderer/Private/FMeshDrawCommand.cpp`
- [X] T077 [US3] Integrate sky/background and transparent stages into `FForwardRenderer::PrepareFrame` after opaque and light preparation in `Source/Renderer/Private/FForwardRenderer.cpp`
- [X] T078 [US3] Extend render graph-compatible declarations with sky/background and transparent pass/resource summaries in `Source/Renderer/Private/FForwardRenderGraphDeclaration.cpp`
- [X] T079 [US3] Extend debug dumps with sky/background summary, transparent sort keys, tie-breaker decisions, and transparent diagnostics in `Source/Renderer/Private/FForwardDiagnostics.cpp`

**Checkpoint**: All user stories are independently functional.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Finish integration, boundaries, documentation, and verification across all stories.

- [X] T080 Verify `Source/Renderer/Public/Renderer/RendererMinimal.h` exports the final forward rendering public API surface
- [X] T081 Verify Renderer layer build auto-discovers all new private sources via `Source/Renderer/SConscript`
- [X] T082 Verify test build includes Renderer forward pipeline tests via `Tests/SConscript`
- [X] T083 Run `conda run -n godot scons` and record result in `specs/015-forward-rendering-pipeline/quickstart.md`
- [X] T084 Run `Build/Mac/Debug/Tests/StonerTest` and record result in `specs/015-forward-rendering-pipeline/quickstart.md`
- [X] T085 Record representative forward frame preparation elapsed time against the under-1-second SC-001 target in `specs/015-forward-rendering-pipeline/quickstart.md`
- [X] T086 Record 20-run deterministic frame plan, draw order, diagnostics, and debug dump stability results in `specs/015-forward-rendering-pipeline/quickstart.md`
- [X] T087 Run backend/presentation boundary check from `specs/015-forward-rendering-pipeline/quickstart.md` and record result in `specs/015-forward-rendering-pipeline/quickstart.md`
- [X] T088 Verify or document Windows, macOS, and Linux build/test compatibility evidence in `specs/015-forward-rendering-pipeline/quickstart.md`
- [X] T089 Review public naming and UE5-style conventions across `Source/Renderer/Public/Renderer/`
- [X] T090 Review no GPU execution, window/swapchain presentation, shadow mapping, post-processing, deferred rendering, Application scene ownership, runtime shader compilation, or shader file loading scope creep in `Source/Renderer/`
- [X] T091 Update implementation status notes in `specs/015-forward-rendering-pipeline/spec.md`
- [X] T092 Update roadmap phase status and implementation notes in `doc/roadmap.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies.
- **Foundational (Phase 2)**: Depends on Setup completion and blocks all user stories.
- **US1 (Phase 3)**: Depends on Foundational phase; MVP.
- **US2 (Phase 4)**: Depends on US1 frame plan, draw command, output, and declaration foundations.
- **US3 (Phase 5)**: Depends on US1 frame planning and benefits from US2 draw/material validation, but sky/no-geometry work can be developed after Foundational if kept isolated.
- **Polish (Phase 6)**: Depends on desired user stories being complete.

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Foundational; no dependency on other stories.
- **User Story 2 (P2)**: Depends on US1 public frame/draw/view/output foundations.
- **User Story 3 (P3)**: Depends on US1 frame planning and transparent draw foundations; integrates with US2 material validation when all stories are complete.

### Parallel Opportunities

- Setup skeleton tasks touching separate files can be parallelized after agreeing on names.
- Foundational diagnostics, configuration, identifiers, and test helper declarations can proceed in parallel by file.
- US1 public API tasks T035 through T039 can run in parallel.
- US2 public API tasks T055 and T056 can run in parallel.
- US3 public API tasks T072 and T073 can run in parallel.
- Test tasks inside each story share `Tests/RendererForwardPipelineTests.cpp`, so they should be edited sequentially or carefully batched.

---

## Parallel Example: User Story 1

```bash
Task: "T035 [P] [US1] Define FForwardViewData, viewport extent, camera position, and view-projection validation API in Source/Renderer/Public/Renderer/FForwardViewData.h"
Task: "T036 [P] [US1] Define FForwardOutputTarget, color/depth target summary, format intent, and extent validation API in Source/Renderer/Public/Renderer/FForwardFramePlan.h"
Task: "T037 [P] [US1] Define FMeshDrawCommand, draw candidate, pass mask, stable sort key, and validation state API in Source/Renderer/Public/Renderer/FMeshDrawCommand.h"
Task: "T038 [P] [US1] Define FForwardFramePlan, pass record, accepted/rejected draw lists, and frame query API in Source/Renderer/Public/Renderer/FForwardFramePlan.h"
Task: "T039 [P] [US1] Define FForwardRenderGraphDeclaration, pass declaration, resource declaration, access declaration, and graph output summary API in Source/Renderer/Public/Renderer/FForwardRenderGraphDeclaration.h"
```

## Parallel Example: User Story 2

```bash
Task: "T055 [P] [US2] Define FForwardDirectionalLight, FForwardPointLight, FForwardLightSet, and light validation API in Source/Renderer/Public/Renderer/FForwardLightData.h"
Task: "T056 [P] [US2] Define FForwardPBRSurfaceInputs, extension slot records, and material input validation declarations in Source/Renderer/Public/Renderer/FMeshDrawCommand.h"
```

## Parallel Example: User Story 3

```bash
Task: "T072 [P] [US3] Define environment background mode, environment resource requirement, and background declaration API in Source/Renderer/Public/Renderer/FForwardFramePlan.h"
Task: "T073 [P] [US3] Extend FMeshDrawCommand public API with transparent pass compatibility, camera-space depth key, stable material id, and stable object id accessors in Source/Renderer/Public/Renderer/FMeshDrawCommand.h"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup.
2. Complete Phase 2: Foundational.
3. Complete Phase 3: User Story 1.
4. Stop and validate US1 independently with the opaque frame preparation tests.

### Incremental Delivery

1. Setup plus Foundational creates forward renderer shells, diagnostics, ids, and fixture builders.
2. US1 delivers deterministic headless opaque frame preparation and render graph-compatible declarations.
3. US2 adds full PBR-style input validation, directional/point light selection, and no-light fallback.
4. US3 adds sky/background and transparent composition ordering.
5. Polish verifies build, tests, deterministic dumps, boundary checks, and roadmap/spec status.

### Parallel Team Strategy

With multiple developers:

1. Team completes Setup and Foundational together.
2. After Foundational, one developer can start US1 public API and tests while another prepares US2/US3 test fixtures in separate planned sections.
3. After US1 frame planning exists, US2 and US3 can proceed with careful coordination around `FMeshDrawCommand` and `Tests/RendererForwardPipelineTests.cpp`.

## Notes

- [P] tasks = different files, no dependency on incomplete tasks.
- [Story] label maps task to a specific user story for traceability.
- Each user story should be independently completable and testable.
- Tests should be added before implementation for each user story and should fail before the corresponding implementation lands.
- Commit after each task or logical group using the project conventional commit style.
- Avoid backend API, presentation, runtime shader compilation, scene ownership, or deferred rendering scope creep in this feature.
