# Tasks: Vulkan Pipeline & Shader

**Input**: Design documents from `/specs/012-vulkan-pipeline-shader/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/vulkan-pipeline-shader-contract.md, quickstart.md

**Tests**: Required by FR-021 and the contract test coverage. Story test tasks should be written before implementation tasks and should fail before the corresponding implementation is added.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1, US2, US3, US4, US5)
- Include exact file paths in descriptions

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Prepare build visibility, aggregate includes, and shared test helper locations for the 012 Vulkan pipeline/shader slice.

- [X] T001 Inspect existing RHI shader/pipeline descriptors and command buffer contracts in Source/RHI/Public/RHI/FRHIShaderModuleDesc.h, Source/RHI/Public/RHI/FRHIGraphicsPipelineDesc.h, Source/RHI/Public/RHI/FRHIComputePipelineDesc.h, and Source/RHI/Public/RHI/IRHICommandBuffer.h
- [X] T002 Inspect existing Vulkan resource, pipeline layout, command buffer, diagnostics, and device patterns in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanPipelineLayout.h, Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandBuffer.h, Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDiagnostics.h, and Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDevice.h
- [X] T003 [P] Add planned Vulkan shader/pipeline headers to the build-visible aggregate include in Source/Backend/Vulkan/Public/VulkanRHI/VulkanDevice.h
- [X] T004 [P] Confirm new Vulkan private source files are covered by the backend build script in Source/Backend/Vulkan/SConscript
- [X] T005 [P] Add shared 012 test helper declarations for valid shader metadata, bytecode payloads, and pipeline descriptions in Tests/VulkanBackendTests.cpp
- [X] T006 [P] Add shared RHI mock helper declarations for shader metadata and pipeline dynamic state coverage in Tests/RHICoreTests.cpp
- [X] T007 [P] Reserve Vulkan diagnostics fields for shader, graphics pipeline, compute pipeline, cache, binding, and runtime-mode reasons in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDiagnostics.h
- [X] T008 Add matching diagnostics marker stubs for the reserved 012 diagnostics fields in Source/Backend/Vulkan/Private/FVulkanDiagnostics.cpp

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Extend backend-neutral RHI contracts and Vulkan shared primitives that all user stories depend on.

**CRITICAL**: No user story work can begin until this phase is complete.

- [X] T009 Add ERHIShaderBytecodeValidationMode, ERHIRuntimeObjectMode, and ERHIPipelineReuseState enums in Source/RHI/Public/RHI/FRHIShaderModuleDesc.h
- [X] T010 Add FRHIShaderBytecodeDesc, FRHIShaderInterfaceBinding, FRHIShaderConstantRange, and FRHIShaderInterfaceMetadata structs in Source/RHI/Public/RHI/FRHIShaderModuleDesc.h
- [X] T011 Add validation helpers for shader bytecode structure, shader interface metadata, supported stages, and stage visibility in Source/RHI/Public/RHI/FRHIShaderModuleDesc.h
- [X] T012 Extend FRHIShaderModuleDesc with bytecode, explicit interface metadata, validation mode, runtime mode, and stable identity fields in Source/RHI/Public/RHI/FRHIShaderModuleDesc.h
- [X] T013 Extend FRHIPipelineLayoutDesc with small constant-data ranges and duplicate/range validation helpers in Source/RHI/Public/RHI/FRHIPipelineLayoutDesc.h
- [X] T014 Add FRHIDynamicStateRequirements, FRHIMultisampleState, pipeline runtime mode, and reuse state fields to graphics pipeline descriptions in Source/RHI/Public/RHI/FRHIGraphicsPipelineDesc.h
- [X] T015 Add pipeline runtime mode, reuse state, and compatibility summary fields to compute pipeline descriptions in Source/RHI/Public/RHI/FRHIComputePipelineDesc.h
- [X] T016 Add ERHISymbolicCommandType entries for BindGraphicsPipeline and BindComputePipeline in Source/RHI/Public/RHI/IRHICommandBuffer.h
- [X] T017 Add pure virtual BindGraphicsPipeline and BindComputePipeline methods to IRHICommandBuffer in Source/RHI/Public/RHI/IRHICommandBuffer.h
- [X] T018 Update RHIMinimal aggregate includes for new shader interface and pipeline state fields in Source/RHI/Public/RHI/RHIMinimal.h
- [X] T019 [P] Update FMockCommandBuffer with pipeline binding state and new IRHICommandBuffer methods in Tests/RHICoreTests.cpp
- [X] T020 [P] Update FCompletedCommandBuffer or equivalent Vulkan backend test doubles for new IRHICommandBuffer methods in Tests/VulkanBackendTests.cpp
- [X] T021 Update RHI core mock shader module, pipeline layout, graphics pipeline, and compute pipeline classes for new descriptor fields in Tests/RHICoreTests.cpp
- [X] T022 Add foundational RHI tests for shader interface metadata validation and pipeline layout constant ranges in Tests/RHICoreTests.cpp
- [X] T023 Add foundational RHI tests for command buffer pipeline binding interface behavior in Tests/RHICoreTests.cpp
- [X] T024 Add configured pipeline creation limit fields and reset behavior to FVulkanDevice for deterministic failure testing in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDevice.h
- [X] T025 Implement configured pipeline creation limit reset and diagnostics plumbing in Source/Backend/Vulkan/Private/FVulkanDevice.cpp

**Checkpoint**: RHI contracts compile-ready and Vulkan backend has shared diagnostics/configuration needed by all stories.

---

## Phase 3: User Story 1 - Load Shader Modules for Backend Pipelines (Priority: P1) MVP

**Goal**: Developers can create, query, reject, and invalidate shader modules with structurally valid precompiled bytecode and explicit interface metadata.

**Independent Test**: Create an active backend device, load vertex/fragment/compute shader modules with matching metadata, verify stage/interface/lifecycle/validation/runtime diagnostics, and verify invalid bytecode, unsupported stages, metadata mismatch, and shutdown paths fail explicitly.

### Tests for User Story 1

- [X] T026 [P] [US1] Add Vulkan shader module success tests for vertex, fragment, and compute stages in Tests/VulkanBackendTests.cpp
- [X] T027 [P] [US1] Add Vulkan shader module rejection tests for empty, structurally malformed, unsupported, wrong-stage, metadata-incompatible, and post-shutdown inputs in Tests/VulkanBackendTests.cpp
- [X] T028 [P] [US1] Add RHI mock tests for shader module interface metadata query behavior in Tests/RHICoreTests.cpp
- [X] T029 [P] [US1] Add Vulkan shader module lifecycle invalidation tests in Tests/VulkanBackendTests.cpp

### Implementation for User Story 1

- [X] T030 [P] [US1] Create FVulkanShaderModule public interface with desc, interface summary, validation mode, runtime mode, lifecycle, and diagnostics queries in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanShaderModule.h
- [X] T031 [P] [US1] Create FVulkanShaderModule implementation with lightweight structural bytecode validation in Source/Backend/Vulkan/Private/FVulkanShaderModule.cpp
- [X] T032 [P] [US1] Add shader module diagnostics marker implementations in Source/Backend/Vulkan/Private/FVulkanDiagnostics.cpp
- [X] T033 [US1] Implement shader module creation in FVulkanDevice::CreateShaderModule in Source/Backend/Vulkan/Private/FVulkanDevice.cpp
- [X] T034 [US1] Add shader module ownership storage and shutdown invalidation to FVulkanDevice in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDevice.h
- [X] T035 [US1] Invalidate owned shader modules during device shutdown in Source/Backend/Vulkan/Private/FVulkanDevice.cpp
- [X] T036 [US1] Expose shader module runtime/fallback diagnostics from FVulkanDevice creation paths in Source/Backend/Vulkan/Private/FVulkanDevice.cpp
- [X] T037 [US1] Update Vulkan aggregate header to include FVulkanShaderModule in Source/Backend/Vulkan/Public/VulkanRHI/VulkanDevice.h
- [X] T038 [US1] Replace unsupported shader module expectation paths with valid and invalid creation checks in Tests/VulkanBackendTests.cpp
- [X] T039 [US1] Verify US1 tests fail before implementation and pass after implementation in Tests/VulkanBackendTests.cpp

**Checkpoint**: User Story 1 is independently functional and provides the MVP shader module slice.

---

## Phase 4: User Story 2 - Define Pipeline Layouts for Resource Binding (Priority: P1)

**Goal**: Developers can create pipeline layouts that validate descriptor bindings and small constant-data ranges, then use them for shader interface compatibility checks.

**Independent Test**: Create valid layouts from descriptor declarations and constant ranges, verify set order/stage visibility/lifecycle, and reject duplicates, invalid ranges, missing dependencies, and invalidated layouts.

### Tests for User Story 2

- [X] T040 [P] [US2] Add RHI pipeline layout constant range validation tests in Tests/RHICoreTests.cpp
- [X] T041 [P] [US2] Add Vulkan pipeline layout success tests for descriptor bindings plus small constant-data ranges in Tests/VulkanBackendTests.cpp
- [X] T042 [P] [US2] Add Vulkan pipeline layout rejection tests for duplicate bindings, invalid stage visibility, invalid ranges, and post-shutdown creation in Tests/VulkanBackendTests.cpp
- [X] T043 [P] [US2] Add Vulkan invalidated pipeline layout dependency tests for shader/pipeline creation paths in Tests/VulkanBackendTests.cpp

### Implementation for User Story 2

- [X] T044 [US2] Extend FVulkanPipelineLayout to expose constant ranges and updated layout descriptions in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanPipelineLayout.h
- [X] T045 [US2] Implement constant range preservation and FindBinding compatibility in Source/Backend/Vulkan/Private/FVulkanPipelineLayout.cpp
- [X] T046 [US2] Update FVulkanDevice::CreatePipelineLayout validation for duplicate descriptor bindings and invalid constant ranges in Source/Backend/Vulkan/Private/FVulkanDevice.cpp
- [X] T047 [US2] Add shader interface versus pipeline layout compatibility helper declarations in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanPipelineLayout.h
- [X] T048 [US2] Implement shader interface versus pipeline layout compatibility helpers in Source/Backend/Vulkan/Private/FVulkanPipelineLayout.cpp
- [X] T049 [US2] Add pipeline layout diagnostics reason updates for compatibility failures in Source/Backend/Vulkan/Private/FVulkanDiagnostics.cpp
- [X] T050 [US2] Update descriptor set creation compatibility after layout invalidation in Source/Backend/Vulkan/Private/FVulkanDevice.cpp
- [X] T051 [US2] Verify US2 tests fail before implementation and pass after implementation in Tests/VulkanBackendTests.cpp

**Checkpoint**: User Story 2 is independently functional and layout compatibility can support pipeline creation.

---

## Phase 5: User Story 3 - Create Graphics Pipelines for Render Pass Drawing (Priority: P1)

**Goal**: Developers can create triangle-ready graphics pipelines, bind them inside compatible render pass scope, and record draw/indexed draw commands without missing-pipeline diagnostics.

**Independent Test**: Create valid shader modules, compatible layout, render pass/framebuffer scope, and triangle-ready fixed-function state; create and bind a graphics pipeline; verify draw diagnostics plus negative creation/binding paths.

### Tests for User Story 3

- [X] T052 [P] [US3] Add Vulkan graphics pipeline success tests with valid vertex/fragment shaders, layout, render target compatibility, and triangle-ready state in Tests/VulkanBackendTests.cpp
- [X] T053 [P] [US3] Add Vulkan graphics pipeline rejection tests for missing/duplicate/wrong shader stages and interface-layout mismatch in Tests/VulkanBackendTests.cpp
- [X] T054 [P] [US3] Add Vulkan graphics pipeline rejection tests for invalid vertex input, topology, rasterization, depth/stencil, blend, multisample, dynamic viewport/scissor, and render target compatibility in Tests/VulkanBackendTests.cpp
- [X] T055 [P] [US3] Add Vulkan graphics pipeline binding tests for compatible render pass scope and invalid outside-scope or wrong-queue cases in Tests/VulkanBackendTests.cpp
- [X] T056 [P] [US3] Add RHI command buffer draw/indexed draw binding diagnostics tests in Tests/RHICoreTests.cpp
- [X] T057 [P] [US3] Add Vulkan shutdown invalidation tests for graphics pipelines and dependent command binding state in Tests/VulkanBackendTests.cpp

### Implementation for User Story 3

- [X] T058 [P] [US3] Create FVulkanGraphicsPipeline public interface with desc, compatibility summary, runtime mode, reuse state, lifecycle, and diagnostics queries in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanGraphicsPipeline.h
- [X] T059 [P] [US3] Create FVulkanGraphicsPipeline implementation with triangle-ready graphics state validation in Source/Backend/Vulkan/Private/FVulkanGraphicsPipeline.cpp
- [X] T060 [P] [US3] Add graphics pipeline state validation helpers for shader stages, vertex input, topology, fixed-function state, and render target compatibility in Source/Backend/Vulkan/Private/FVulkanGraphicsPipeline.cpp
- [X] T061 [P] [US3] Add graphics pipeline diagnostics marker implementations in Source/Backend/Vulkan/Private/FVulkanDiagnostics.cpp
- [X] T062 [US3] Implement graphics pipeline creation and configured creation failure handling in FVulkanDevice::CreateGraphicsPipeline in Source/Backend/Vulkan/Private/FVulkanDevice.cpp
- [X] T063 [US3] Add graphics pipeline ownership storage and shutdown invalidation to FVulkanDevice in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDevice.h
- [X] T064 [US3] Invalidate owned graphics pipelines during device shutdown in Source/Backend/Vulkan/Private/FVulkanDevice.cpp
- [X] T065 [US3] Add graphics pipeline binding state fields to FVulkanCommandBuffer in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandBuffer.h
- [X] T066 [US3] Implement FVulkanCommandBuffer::BindGraphicsPipeline with recording, queue, render pass, lifecycle, and compatibility validation in Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp
- [X] T067 [US3] Update RecordDraw and RecordDrawIndexed to distinguish missing, compatible, incompatible, wrong-kind, and invalidated graphics pipeline state in Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp
- [X] T068 [US3] Add BindGraphicsPipeline symbolic command recording in Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp
- [X] T069 [US3] Update Vulkan aggregate header to include FVulkanGraphicsPipeline in Source/Backend/Vulkan/Public/VulkanRHI/VulkanDevice.h
- [X] T070 [US3] Replace unsupported graphics pipeline expectations with creation, binding, and draw diagnostic checks in Tests/VulkanBackendTests.cpp
- [X] T071 [US3] Verify US3 tests fail before implementation and pass after implementation in Tests/VulkanBackendTests.cpp

**Checkpoint**: User Story 3 is independently functional and graphics draw commands can validate bound graphics pipelines.

---

## Phase 6: User Story 4 - Create Compute Pipelines for Dispatch Work (Priority: P2)

**Goal**: Developers can create compute pipelines, bind them to compute-capable command buffers, and record dispatch commands without missing-pipeline diagnostics.

**Independent Test**: Create valid and invalid compute shader modules/layouts, create compute pipelines, bind to compute-compatible command buffers, and verify dispatch diagnostics and negative cases.

### Tests for User Story 4

- [X] T072 [P] [US4] Add Vulkan compute pipeline success tests with valid compute shader and compatible layout in Tests/VulkanBackendTests.cpp
- [X] T073 [P] [US4] Add Vulkan compute pipeline rejection tests for wrong-stage shader, multiple shaders, interface-layout mismatch, unsupported queue capability, configured failure, and shutdown in Tests/VulkanBackendTests.cpp
- [X] T074 [P] [US4] Add Vulkan compute pipeline binding tests for compute and graphics queues plus transfer queue rejection in Tests/VulkanBackendTests.cpp
- [X] T075 [P] [US4] Add RHI command buffer dispatch binding diagnostics tests in Tests/RHICoreTests.cpp

### Implementation for User Story 4

- [X] T076 [P] [US4] Create FVulkanComputePipeline public interface with desc, compatibility summary, runtime mode, reuse state, lifecycle, and diagnostics queries in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanComputePipeline.h
- [X] T077 [P] [US4] Create FVulkanComputePipeline implementation with compute shader and layout compatibility validation in Source/Backend/Vulkan/Private/FVulkanComputePipeline.cpp
- [X] T078 [P] [US4] Add compute pipeline diagnostics marker implementations in Source/Backend/Vulkan/Private/FVulkanDiagnostics.cpp
- [X] T079 [US4] Implement compute pipeline creation and configured creation failure handling in FVulkanDevice::CreateComputePipeline in Source/Backend/Vulkan/Private/FVulkanDevice.cpp
- [X] T080 [US4] Add compute pipeline ownership storage and shutdown invalidation to FVulkanDevice in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDevice.h
- [X] T081 [US4] Invalidate owned compute pipelines during device shutdown in Source/Backend/Vulkan/Private/FVulkanDevice.cpp
- [X] T082 [US4] Add compute pipeline binding state fields to FVulkanCommandBuffer in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandBuffer.h
- [X] T083 [US4] Implement FVulkanCommandBuffer::BindComputePipeline with recording, queue, lifecycle, and compatibility validation in Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp
- [X] T084 [US4] Update RecordDispatch to distinguish missing, compatible, incompatible, wrong-kind, and invalidated compute pipeline state in Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp
- [X] T085 [US4] Add BindComputePipeline symbolic command recording in Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp
- [X] T086 [US4] Update Vulkan aggregate header to include FVulkanComputePipeline in Source/Backend/Vulkan/Public/VulkanRHI/VulkanDevice.h
- [X] T087 [US4] Verify US4 tests fail before implementation and pass after implementation in Tests/VulkanBackendTests.cpp

**Checkpoint**: User Story 4 is independently functional and dispatch commands can validate bound compute pipelines.

---

## Phase 7: User Story 5 - Reuse Pipeline Creation Results Deterministically (Priority: P3)

**Goal**: Developers can observe deterministic process-local reuse for equivalent successful graphics and compute pipeline creation requests, while failures and invalidations never become reusable successes.

**Independent Test**: Issue repeated equivalent and non-equivalent pipeline creation requests in the same process, compare reuse diagnostics, invalidate dependencies, and verify no persistent disk cache behavior is required.

### Tests for User Story 5

- [X] T088 [P] [US5] Add Vulkan graphics pipeline process-local reuse success and non-equivalent miss tests in Tests/VulkanBackendTests.cpp
- [X] T089 [P] [US5] Add Vulkan compute pipeline process-local reuse success and non-equivalent miss tests in Tests/VulkanBackendTests.cpp
- [X] T090 [P] [US5] Add Vulkan no-reuse tests for failed, unsupported, configured-failure, and invalidated pipeline requests in Tests/VulkanBackendTests.cpp
- [X] T091 [P] [US5] Add Vulkan device shutdown cache invalidation tests in Tests/VulkanBackendTests.cpp

### Implementation for User Story 5

- [X] T092 [P] [US5] Create FVulkanPipelineCache public process-local cache interface with stable keys, runtime mode, reuse state, generation, and invalidation in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanPipelineCache.h
- [X] T093 [P] [US5] Create FVulkanPipelineCache implementation for graphics and compute pipeline lookup/insert/miss/invalidate behavior in Source/Backend/Vulkan/Private/FVulkanPipelineCache.cpp
- [X] T094 [US5] Integrate graphics pipeline cache lookup and insertion into FVulkanDevice::CreateGraphicsPipeline in Source/Backend/Vulkan/Private/FVulkanDevice.cpp
- [X] T095 [US5] Integrate compute pipeline cache lookup and insertion into FVulkanDevice::CreateComputePipeline in Source/Backend/Vulkan/Private/FVulkanDevice.cpp
- [X] T096 [US5] Add pipeline cache ownership and shutdown invalidation to FVulkanDevice in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDevice.h
- [X] T097 [US5] Add pipeline cache diagnostics marker implementations in Source/Backend/Vulkan/Private/FVulkanDiagnostics.cpp
- [X] T098 [US5] Update Vulkan aggregate header to include FVulkanPipelineCache in Source/Backend/Vulkan/Public/VulkanRHI/VulkanDevice.h
- [X] T099 [US5] Verify no persistent disk cache files are read or written during pipeline reuse tests in Tests/VulkanBackendTests.cpp
- [X] T100 [US5] Verify US5 tests fail before implementation and pass after implementation in Tests/VulkanBackendTests.cpp

**Checkpoint**: User Story 5 is independently functional and process-local pipeline reuse is deterministic.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Regression coverage, documentation consistency, and final verification across all stories.

- [X] T101 [P] Update RHI public contract comments or summaries for shader interface metadata and pipeline binding behavior in Source/RHI/Public/RHI/FRHIShaderModuleDesc.h and Source/RHI/Public/RHI/IRHICommandBuffer.h
- [X] T102 [P] Update Vulkan backend diagnostic comments or summaries for shader/pipeline/cache reasons in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDiagnostics.h
- [X] T103 [P] Add or adjust quickstart-aligned verification notes for 012 in specs/012-vulkan-pipeline-shader/quickstart.md
- [X] T104 Run build verification with conda run -n godot scons and record outcome against specs/012-vulkan-pipeline-shader/quickstart.md
- [X] T105 Run test verification with Build/Mac/Debug/Tests/StonerTest and record outcome against specs/012-vulkan-pipeline-shader/quickstart.md
- [X] T106 Verify RHI abstraction boundaries with rg for forbidden Renderer/Application dependencies in Source/RHI/Public/RHI and Source/Backend/Vulkan/Public/VulkanRHI
- [X] T107 Verify no source shader compilation, automatic reflection, persistent disk cache, material system, render graph scheduling, mesh shader, ray tracing, or visible triangle demo behavior was introduced in Source and Tests
- [X] T108 Update doc/roadmap.md Phase 011 implementation notes after feature completion in doc/roadmap.md
- [X] T109 Generate delivered feature implementation documentation for 012 following doc/SYSTEM_DESIGN.MD in doc/012-vulkan-pipeline-shader.html
- [X] T110 Review all 012 tasks and mark completed checkboxes in specs/012-vulkan-pipeline-shader/tasks.md

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies; can start immediately.
- **Foundational (Phase 2)**: Depends on Setup completion; blocks all user stories.
- **US1 (Phase 3)**: Depends on Foundational; MVP shader module slice.
- **US2 (Phase 4)**: Depends on Foundational and benefits from US1 metadata helpers; can be implemented after RHI metadata contracts exist.
- **US3 (Phase 5)**: Depends on US1 shader modules and US2 layout compatibility.
- **US4 (Phase 6)**: Depends on US1 shader modules and US2 layout compatibility; can proceed in parallel with US3 after foundations.
- **US5 (Phase 7)**: Depends on US3 graphics pipeline and US4 compute pipeline behavior.
- **Polish (Phase 8)**: Depends on all desired user stories being complete.

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Foundational; no dependency on other stories.
- **User Story 2 (P1)**: Can start after Foundational; coordinates with US1 interface metadata but remains independently testable through layout validation.
- **User Story 3 (P1)**: Requires US1 and US2 for meaningful graphics pipeline creation and binding.
- **User Story 4 (P2)**: Requires US1 and US2 for meaningful compute pipeline creation and binding; independent from US3 after shared prerequisites.
- **User Story 5 (P3)**: Requires US3 and US4 successful pipeline creation paths.

### Within Each User Story

- Test tasks should be added first and should fail before implementation.
- Data/contract types before backend objects.
- Backend objects before device factory integration.
- Device factory integration before command binding.
- Command binding before draw/dispatch diagnostic updates.
- Shutdown invalidation after ownership storage is present.

---

## Parallel Opportunities

- Setup tasks T003-T008 can run in parallel after inspection tasks T001-T002.
- Foundational tasks T019-T020 can run in parallel with RHI descriptor updates T009-T018 once method signatures are decided.
- US1 tests T026-T029 can run in parallel before implementation; implementation tasks T030-T032 can run in parallel before T033-T038.
- US2 tests T040-T043 can run in parallel; implementation tasks T044-T049 can run in parallel before T050-T051.
- US3 tests T052-T057 can run in parallel; implementation tasks T058-T061 can run in parallel before T062-T071.
- US4 tests T072-T075 can run in parallel; implementation tasks T076-T078 can run in parallel before T079-T087.
- US5 tests T088-T091 can run in parallel; implementation tasks T092-T093 can run in parallel before T094-T100.
- Polish documentation tasks T101-T103 can run in parallel with final verification preparation.

## Parallel Example: User Story 1

```bash
Task: "T026 [P] [US1] Add Vulkan shader module success tests for vertex, fragment, and compute stages in Tests/VulkanBackendTests.cpp"
Task: "T027 [P] [US1] Add Vulkan shader module rejection tests for empty, structurally malformed, unsupported, wrong-stage, metadata-incompatible, and post-shutdown inputs in Tests/VulkanBackendTests.cpp"
Task: "T028 [P] [US1] Add RHI mock tests for shader module interface metadata query behavior in Tests/RHICoreTests.cpp"
Task: "T029 [P] [US1] Add Vulkan shader module lifecycle invalidation tests in Tests/VulkanBackendTests.cpp"
```

## Parallel Example: User Story 3

```bash
Task: "T052 [P] [US3] Add Vulkan graphics pipeline success tests with valid vertex/fragment shaders, layout, render target compatibility, and triangle-ready state in Tests/VulkanBackendTests.cpp"
Task: "T053 [P] [US3] Add Vulkan graphics pipeline rejection tests for missing/duplicate/wrong shader stages and interface-layout mismatch in Tests/VulkanBackendTests.cpp"
Task: "T054 [P] [US3] Add Vulkan graphics pipeline rejection tests for invalid vertex input, topology, rasterization, depth/stencil, blend, multisample, dynamic viewport/scissor, and render target compatibility in Tests/VulkanBackendTests.cpp"
Task: "T055 [P] [US3] Add Vulkan graphics pipeline binding tests for compatible render pass scope and invalid outside-scope or wrong-queue cases in Tests/VulkanBackendTests.cpp"
```

## Parallel Example: User Story 4

```bash
Task: "T072 [P] [US4] Add Vulkan compute pipeline success tests with valid compute shader and compatible layout in Tests/VulkanBackendTests.cpp"
Task: "T073 [P] [US4] Add Vulkan compute pipeline rejection tests for wrong-stage shader, multiple shaders, interface-layout mismatch, unsupported queue capability, configured failure, and shutdown in Tests/VulkanBackendTests.cpp"
Task: "T074 [P] [US4] Add Vulkan compute pipeline binding tests for compute and graphics queues plus transfer queue rejection in Tests/VulkanBackendTests.cpp"
Task: "T075 [P] [US4] Add RHI command buffer dispatch binding diagnostics tests in Tests/RHICoreTests.cpp"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1 setup tasks.
2. Complete Phase 2 foundational RHI contract tasks.
3. Complete Phase 3 shader module tests and implementation.
4. Verify shader module success, rejection, diagnostics, and invalidation independently.

### Incremental Delivery

1. Deliver US1 to make shader modules real backend objects.
2. Deliver US2 to make layouts compatible with explicit shader interfaces.
3. Deliver US3 to make graphics pipeline binding remove draw missing-pipeline diagnostics.
4. Deliver US4 to make compute pipeline binding remove dispatch missing-pipeline diagnostics.
5. Deliver US5 to add deterministic process-local reuse.
6. Run Phase 8 verification and documentation tasks.

### Validation Gates

- Every task line in this file uses `- [X] T###` checklist format.
- Every user story phase contains independently testable criteria.
- Tests are included because FR-021 and the contract require deterministic coverage.
- Build and test commands are taken from quickstart.md.
