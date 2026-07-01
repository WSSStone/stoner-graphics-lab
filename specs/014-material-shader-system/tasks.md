# Tasks: Material & Shader System

**Input**: Design documents from `/specs/014-material-shader-system/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/material-shader-contract.md, quickstart.md
**Tests**: Required by FR-022 and quickstart representative/negative scenarios.
**Organization**: Tasks are grouped by user story to enable independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel because it touches different files and has no dependency on incomplete tasks in the same phase.
- **[Story]**: Maps to the user story from spec.md (`US1` through `US4`).
- Every task includes an exact repository file path.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create the file skeleton and test harness entry points needed by all material/shader stories.

- [X] T001 Create Material public header skeleton in `Source/Renderer/Public/Renderer/FMaterial.h`
- [X] T002 Create Material Instance public header skeleton in `Source/Renderer/Public/Renderer/FMaterialInstance.h`
- [X] T003 Create Material Parameter Set public header skeleton in `Source/Renderer/Public/Renderer/FMaterialParameterSet.h`
- [X] T004 Create Material Resource Requirement public header skeleton in `Source/Renderer/Public/Renderer/FMaterialResourceRequirement.h`
- [X] T005 Create Material Shader Binding public header skeleton in `Source/Renderer/Public/Renderer/FMaterialShaderBinding.h`
- [X] T006 Create Shader Library public header skeleton in `Source/Renderer/Public/Renderer/FShaderLibrary.h`
- [X] T007 Create Shader Permutation public header skeleton in `Source/Renderer/Public/Renderer/FShaderPermutation.h`
- [X] T008 Create Material Diagnostics public header skeleton in `Source/Renderer/Public/Renderer/FMaterialDiagnostics.h`
- [X] T009 Create Material private source skeleton in `Source/Renderer/Private/FMaterial.cpp`
- [X] T010 Create Material Instance private source skeleton in `Source/Renderer/Private/FMaterialInstance.cpp`
- [X] T011 Create Material Parameter Set private source skeleton in `Source/Renderer/Private/FMaterialParameterSet.cpp`
- [X] T012 Create Material Resource Requirement private source skeleton in `Source/Renderer/Private/FMaterialResourceRequirement.cpp`
- [X] T013 Create Material Shader Binding private source skeleton in `Source/Renderer/Private/FMaterialShaderBinding.cpp`
- [X] T014 Create Shader Library private source skeleton in `Source/Renderer/Private/FShaderLibrary.cpp`
- [X] T015 Create Shader Permutation private source skeleton in `Source/Renderer/Private/FShaderPermutation.cpp`
- [X] T016 Create Material Diagnostics private source skeleton in `Source/Renderer/Private/FMaterialDiagnostics.cpp`
- [X] T017 [P] Create Renderer material/shader test header in `Tests/RendererMaterialShaderTests.h`
- [X] T018 [P] Create Renderer material/shader test source in `Tests/RendererMaterialShaderTests.cpp`
- [X] T019 Register Renderer material/shader tests in `Tests/Main.cpp`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Define shared enums, identifiers, parameter values, diagnostics, and test helpers that all user stories depend on.

**CRITICAL**: No user story work can begin until this phase is complete.

- [X] T020 Define `EMaterialResult`, validation state, diagnostic severity, and diagnostic category enums in `Source/Renderer/Public/Renderer/FMaterialDiagnostics.h`
- [X] T021 Define diagnostic record, stable diagnostic code, and deterministic formatting declarations in `Source/Renderer/Public/Renderer/FMaterialDiagnostics.h`
- [X] T022 Implement diagnostic storage, stable ordering, and formatting helpers in `Source/Renderer/Private/FMaterialDiagnostics.cpp`
- [X] T023 Define material domain, blend mode, render state summary, and material identity structs in `Source/Renderer/Public/Renderer/FMaterial.h`
- [X] T024 Define abstract Renderer-level resource reference identifiers and access intent enums in `Source/Renderer/Public/Renderer/FMaterialResourceRequirement.h`
- [X] T025 Define scalar, vector, color-like, and resource-reference parameter value types in `Source/Renderer/Public/Renderer/FMaterialParameterSet.h`
- [X] T026 Implement parameter value equality, type queries, and deterministic value formatting in `Source/Renderer/Private/FMaterialParameterSet.cpp`
- [X] T027 Define shader identifier, shader record identifier, and shader variant identifier structs in `Source/Renderer/Public/Renderer/FShaderLibrary.h`
- [X] T028 Define common material/shader forward declarations and include exports in `Source/Renderer/Public/Renderer/RendererMinimal.h`
- [X] T029 [P] Add material/shader test entry point declaration in `Tests/RendererMaterialShaderTests.h`
- [X] T030 [P] Add material/shader test utility builders for parameters, shader records, and resource references in `Tests/RendererMaterialShaderTests.cpp`
- [X] T031 Add Renderer material/shader test runner call in `Tests/Main.cpp`

**Checkpoint**: Foundation ready; user story implementation can now begin.

---

## Phase 3: User Story 1 - Define Reusable Materials (Priority: P1) MVP

**Goal**: Developers can define reusable materials with shader reference, domain, blend mode, render behavior, typed parameters, validation, diagnostics, and deterministic inspection.

**Independent Test**: Create materials for opaque surface, masked surface, translucent surface, post-process, UI, and decal usage, then inspect and validate deterministic material summaries.

### Tests for User Story 1

- [X] T032 [US1] Add valid material definition tests covering all supported domains and blend modes in `Tests/RendererMaterialShaderTests.cpp`
- [X] T033 [US1] Add duplicate parameter name and unsupported domain/blend rejection tests in `Tests/RendererMaterialShaderTests.cpp`
- [X] T034 [US1] Add material validation diagnostic subject and stable code tests in `Tests/RendererMaterialShaderTests.cpp`
- [X] T035 [US1] Add material inspection dump byte-stability tests across 20 repeated dumps in `Tests/RendererMaterialShaderTests.cpp`

### Implementation for User Story 1

- [X] T036 [P] [US1] Define `FMaterialDesc`, `FMaterial`, and material query API in `Source/Renderer/Public/Renderer/FMaterial.h`
- [X] T037 [P] [US1] Define `FMaterialParameter`, `FMaterialParameterSet`, and parameter set query API in `Source/Renderer/Public/Renderer/FMaterialParameterSet.h`
- [X] T038 [US1] Implement parameter insertion, duplicate-name rejection, and deterministic ordering in `Source/Renderer/Private/FMaterialParameterSet.cpp`
- [X] T039 [US1] Implement material construction, reset, invalidation, and query behavior in `Source/Renderer/Private/FMaterial.cpp`
- [X] T040 [US1] Implement material domain/blend compatibility validation in `Source/Renderer/Private/FMaterial.cpp`
- [X] T041 [US1] Implement material default parameter validation and diagnostic emission in `Source/Renderer/Private/FMaterial.cpp`
- [X] T042 [US1] Implement material inspection dump sections in `Source/Renderer/Private/FMaterialDiagnostics.cpp`
- [X] T043 [US1] Expose material validation and dump APIs through `Source/Renderer/Public/Renderer/FMaterial.h`

**Checkpoint**: User Story 1 is independently testable as the MVP.

---

## Phase 4: User Story 2 - Override Parameters Per Material Instance (Priority: P2)

**Goal**: Developers can create material instances, inherit from materials or instances, apply nearest-override precedence, reject invalid overrides, and detect inheritance cycles.

**Independent Test**: Create a base material and several instance chains with different overrides, then resolve effective values and validate failure cases.

### Tests for User Story 2

- [X] T044 [US2] Add material instance no-override inheritance tests in `Tests/RendererMaterialShaderTests.cpp`
- [X] T045 [US2] Add nearest-override precedence tests for scalar, vector, color-like, and resource-reference parameters in `Tests/RendererMaterialShaderTests.cpp`
- [X] T046 [US2] Add unknown root parameter and override type mismatch rejection tests in `Tests/RendererMaterialShaderTests.cpp`
- [X] T047 [US2] Add material instance inheritance cycle detection tests in `Tests/RendererMaterialShaderTests.cpp`
- [X] T048 [US2] Add invalidated parent rejection and instance dump stability tests in `Tests/RendererMaterialShaderTests.cpp`

### Implementation for User Story 2

- [X] T049 [P] [US2] Define `FMaterialInstanceDesc`, `FMaterialInstance`, parent reference, and override query API in `Source/Renderer/Public/Renderer/FMaterialInstance.h`
- [X] T050 [P] [US2] Add override storage and resolved-parameter API declarations to `Source/Renderer/Public/Renderer/FMaterialParameterSet.h`
- [X] T051 [US2] Implement parent-chain traversal and root material discovery in `Source/Renderer/Private/FMaterialInstance.cpp`
- [X] T052 [US2] Implement nearest-override effective parameter resolution in `Source/Renderer/Private/FMaterialInstance.cpp`
- [X] T053 [US2] Implement unknown root parameter and override type mismatch validation in `Source/Renderer/Private/FMaterialInstance.cpp`
- [X] T054 [US2] Implement deterministic inheritance cycle detection diagnostics in `Source/Renderer/Private/FMaterialInstance.cpp`
- [X] T055 [US2] Implement invalidated parent rejection and instance invalidation behavior in `Source/Renderer/Private/FMaterialInstance.cpp`
- [X] T056 [US2] Implement material instance inspection dump sections in `Source/Renderer/Private/FMaterialDiagnostics.cpp`

**Checkpoint**: User Story 2 works independently after material definitions exist.

---

## Phase 5: User Story 3 - Select Shader Variants Deterministically (Priority: P3)

**Goal**: Developers can register precompiled shader records in memory, validate per-shader permutation flags, select variants deterministically, and receive clear diagnostics for missing records, flags, variants, or required parameters.

**Independent Test**: Register shader records with variant flags, request permutations for material configurations, and verify stable selection and clear rejection for unavailable choices.

### Tests for User Story 3

- [X] T057 [US3] Add explicit in-memory shader record registration and duplicate shader identity tests in `Tests/RendererMaterialShaderTests.cpp`
- [X] T058 [US3] Add canonical permutation key stability tests across reordered flags and 20 repeated resolutions in `Tests/RendererMaterialShaderTests.cpp`
- [X] T059 [US3] Add unknown permutation flag rejection tests before variant lookup in `Tests/RendererMaterialShaderTests.cpp`
- [X] T060 [US3] Add missing shader record, missing variant, and missing required parameter diagnostics tests in `Tests/RendererMaterialShaderTests.cpp`
- [X] T061 [US3] Add material shader binding success and invalidated shader record rejection tests in `Tests/RendererMaterialShaderTests.cpp`

### Implementation for User Story 3

- [X] T062 [P] [US3] Define `FShaderPermutation` canonical flag set API in `Source/Renderer/Public/Renderer/FShaderPermutation.h`
- [X] T063 [P] [US3] Define `FShaderRecord`, `FShaderVariant`, and `FShaderLibrary` registration/query API in `Source/Renderer/Public/Renderer/FShaderLibrary.h`
- [X] T064 [P] [US3] Define `FMaterialShaderBinding` result and binding diagnostics API in `Source/Renderer/Public/Renderer/FMaterialShaderBinding.h`
- [X] T065 [US3] Implement deterministic permutation canonicalization and equality in `Source/Renderer/Private/FShaderPermutation.cpp`
- [X] T066 [US3] Implement shader record registration, duplicate rejection, and invalidation behavior in `Source/Renderer/Private/FShaderLibrary.cpp`
- [X] T067 [US3] Implement per-shader allowed permutation flag validation in `Source/Renderer/Private/FShaderLibrary.cpp`
- [X] T068 [US3] Implement shader variant lookup and missing-variant diagnostics in `Source/Renderer/Private/FShaderLibrary.cpp`
- [X] T069 [US3] Implement required parameter matching against material or instance effective parameters in `Source/Renderer/Private/FMaterialShaderBinding.cpp`
- [X] T070 [US3] Implement material-to-shader binding success/failure result assembly in `Source/Renderer/Private/FMaterialShaderBinding.cpp`
- [X] T071 [US3] Implement shader library, permutation, and binding inspection dump sections in `Source/Renderer/Private/FMaterialDiagnostics.cpp`

**Checkpoint**: User Story 3 resolves shader variants deterministically for valid materials and rejects invalid shader requests.

---

## Phase 6: User Story 4 - Declare Render Graph Resource Needs (Priority: P4)

**Goal**: Developers can extract stable abstract resource requirements from materials and instances so render graph pass declaration code can consume them before execution.

**Independent Test**: Resolve a material instance with texture/resource parameters and verify the requirement summary can be used in a render graph declaration flow.

### Tests for User Story 4

- [X] T072 [US4] Add resource requirement extraction tests for material resource parameters in `Tests/RendererMaterialShaderTests.cpp`
- [X] T073 [US4] Add resource-reference override replacement tests for material instances in `Tests/RendererMaterialShaderTests.cpp`
- [X] T074 [US4] Add empty resource requirement success tests for materials without resource parameters in `Tests/RendererMaterialShaderTests.cpp`
- [X] T075 [US4] Add live RHI resource and graph-local handle exclusion tests in `Tests/RendererMaterialShaderTests.cpp`
- [X] T076 [US4] Add render graph declaration consumption smoke test using `FRenderGraphBuilder` in `Tests/RendererMaterialShaderTests.cpp`

### Implementation for User Story 4

- [X] T077 [P] [US4] Define `FMaterialResourceReference` and `FMaterialResourceRequirement` API in `Source/Renderer/Public/Renderer/FMaterialResourceRequirement.h`
- [X] T078 [P] [US4] Add resource-reference parameter helper declarations in `Source/Renderer/Public/Renderer/FMaterialParameterSet.h`
- [X] T079 [US4] Implement abstract resource-reference parameter validation and formatting in `Source/Renderer/Private/FMaterialParameterSet.cpp`
- [X] T080 [US4] Implement material resource requirement extraction in `Source/Renderer/Private/FMaterialResourceRequirement.cpp`
- [X] T081 [US4] Implement instance-aware resource requirement extraction using resolved parameters in `Source/Renderer/Private/FMaterialResourceRequirement.cpp`
- [X] T082 [US4] Implement live resource and graph-local handle exclusion checks in `Source/Renderer/Private/FMaterialResourceRequirement.cpp`
- [X] T083 [US4] Implement deterministic resource requirement ordering and diagnostics in `Source/Renderer/Private/FMaterialResourceRequirement.cpp`
- [X] T084 [US4] Add render graph resource requirement declaration helpers or examples in `Source/Renderer/Public/Renderer/FMaterialResourceRequirement.h`
- [X] T085 [US4] Implement resource requirement inspection dump sections in `Source/Renderer/Private/FMaterialDiagnostics.cpp`

**Checkpoint**: User Story 4 provides stable resource-needs summaries consumable by render graph declaration code.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Finish integration, boundaries, documentation, and verification across all stories.

- [X] T086 Update `Source/Renderer/Public/Renderer/RendererMinimal.h` to include or forward the final Material & Shader public API surface
- [X] T087 Verify Renderer layer build auto-discovers all new private sources via `Source/Renderer/SConscript`
- [X] T088 Verify test build includes Renderer material/shader tests via `Tests/SConscript`
- [X] T089 Run `conda run -n godot scons` and record result in `specs/014-material-shader-system/quickstart.md`
- [X] T090 Run `Build/Mac/Debug/Tests/StonerTest` and record result in `specs/014-material-shader-system/quickstart.md`
- [X] T091 Record representative material library validation/inspection/resource-summary elapsed time against the 60-second SC-004 target in `specs/014-material-shader-system/quickstart.md`
- [X] T092 Run backend-boundary check from `specs/014-material-shader-system/quickstart.md` and record result in `specs/014-material-shader-system/quickstart.md`
- [X] T093 Review public naming and UE5-style conventions across `Source/Renderer/Public/Renderer/`
- [X] T094 Review no runtime shader compilation, shader file loading, visual material editor, PBR model, concrete forward pass, scene graph, presentation, or live resource ownership scope creep in `Source/Renderer/`
- [X] T095 Update implementation status notes in `specs/014-material-shader-system/spec.md`
- [X] T096 Update roadmap phase status and implementation notes in `doc/roadmap.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies.
- **Foundational (Phase 2)**: Depends on Setup completion and blocks all user stories.
- **US1 (Phase 3)**: Depends on Foundational phase; MVP.
- **US2 (Phase 4)**: Depends on US1 material and parameter definitions.
- **US3 (Phase 5)**: Depends on US1 material and parameter definitions; can proceed in parallel with US2 after US1 if capacity allows.
- **US4 (Phase 6)**: Depends on US1 parameters, US2 effective instance resolution, and US3 binding/resource context.
- **Polish (Phase 7)**: Depends on desired user stories being complete.

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Foundational; no dependency on other stories.
- **User Story 2 (P2)**: Depends on US1 material definitions and parameter sets.
- **User Story 3 (P3)**: Depends on US1 material definitions and parameter sets.
- **User Story 4 (P4)**: Depends on US1, US2, and the render graph foundation; the smoke test also uses existing `FRenderGraphBuilder`.

### Parallel Opportunities

- Setup skeleton tasks touching separate files can be parallelized after agreeing on names.
- Foundational diagnostics, parameter values, shader identifiers, and test helper declarations can proceed in parallel by file.
- US1 public header definitions T036 and T037 can run in parallel.
- US2 public API tasks T049 and T050 can run in parallel.
- US3 public API tasks T062, T063, and T064 can run in parallel.
- US4 public API tasks T077 and T078 can run in parallel.
- Test tasks inside each story share `Tests/RendererMaterialShaderTests.cpp`, so they should be edited sequentially or carefully batched.

---

## Parallel Example: User Story 1

```bash
Task: "T036 [P] [US1] Define FMaterialDesc, FMaterial, and material query API in Source/Renderer/Public/Renderer/FMaterial.h"
Task: "T037 [P] [US1] Define FMaterialParameter, FMaterialParameterSet, and parameter set query API in Source/Renderer/Public/Renderer/FMaterialParameterSet.h"
```

## Parallel Example: User Story 2

```bash
Task: "T049 [P] [US2] Define FMaterialInstanceDesc, FMaterialInstance, parent reference, and override query API in Source/Renderer/Public/Renderer/FMaterialInstance.h"
Task: "T050 [P] [US2] Add override storage and resolved-parameter API declarations to Source/Renderer/Public/Renderer/FMaterialParameterSet.h"
```

## Parallel Example: User Story 3

```bash
Task: "T062 [P] [US3] Define FShaderPermutation canonical flag set API in Source/Renderer/Public/Renderer/FShaderPermutation.h"
Task: "T063 [P] [US3] Define FShaderRecord, FShaderVariant, and FShaderLibrary registration/query API in Source/Renderer/Public/Renderer/FShaderLibrary.h"
Task: "T064 [P] [US3] Define FMaterialShaderBinding result and binding diagnostics API in Source/Renderer/Public/Renderer/FMaterialShaderBinding.h"
```

## Parallel Example: User Story 4

```bash
Task: "T077 [P] [US4] Define FMaterialResourceReference and FMaterialResourceRequirement API in Source/Renderer/Public/Renderer/FMaterialResourceRequirement.h"
Task: "T078 [P] [US4] Add resource-reference parameter helper declarations in Source/Renderer/Public/Renderer/FMaterialParameterSet.h"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup.
2. Complete Phase 2: Foundational.
3. Complete Phase 3: User Story 1.
4. Stop and validate material definition, validation, diagnostics, and deterministic dump behavior independently.

### Incremental Delivery

1. Complete Setup + Foundational to establish shared Renderer material/shader infrastructure.
2. Add User Story 1 to define reusable materials and parameter sets.
3. Add User Story 2 to support material instance inheritance and overrides.
4. Add User Story 3 to register shader records and resolve shader variants.
5. Add User Story 4 to expose render graph resource requirements.
6. Run polish verification and update quickstart/spec/roadmap notes.

### Parallel Team Strategy

With multiple developers:

1. Team completes Setup + Foundational together.
2. After US1 is complete, one developer can work on US2 instance resolution while another works on US3 shader library selection.
3. US4 should start after US2 effective parameter resolution is stable.

## Notes

- [P] tasks = different files, no dependencies on incomplete tasks in the same phase.
- [Story] label maps task to a specific user story for traceability.
- Each user story should be independently completable and testable.
- Tests are included because FR-022 explicitly requires them.
- Verify tests fail before implementing each story behavior.
- Commit after each task or logical group using the project's conventional commit style.
- Avoid backend-specific graphics API concepts in Renderer public material/shader contracts.
