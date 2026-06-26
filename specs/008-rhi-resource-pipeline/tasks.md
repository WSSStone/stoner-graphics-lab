# Tasks: RHI Resource & Pipeline Interfaces

**Input**: Design documents from `/specs/008-rhi-resource-pipeline/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/rhi-resource-pipeline-api.md, quickstart.md

**Tests**: Test tasks are included because the feature specification requires mock-based coverage for every public RHI resource and pipeline contract, with at least one success path and one negative path for each contract.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Path Conventions

- RHI public headers: `Source/RHI/Public/RHI/`
- RHI private sources: `Source/RHI/Private/`
- Tests: `Tests/`
- Feature documentation: `specs/008-rhi-resource-pipeline/`

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create header shells and test harness placeholders for the RHI resource/pipeline slice without implementing behavior yet.

- [X] T001 [P] Create public header shell `Source/RHI/Public/RHI/ERHIResourceUsage.h` for composable buffer and texture usage flags
- [X] T002 [P] Create public header shell `Source/RHI/Public/RHI/ERHITextureDimension.h` for 1D, 2D, 3D, cube, and array texture dimensions
- [X] T003 [P] Create public header shell `Source/RHI/Public/RHI/ERHISamplerMode.h` for sampler filter, address, and comparison mode enums
- [X] T004 [P] Create public header shell `Source/RHI/Public/RHI/ERHIShaderStage.h` for shader stage classifications
- [X] T005 [P] Create public header shell `Source/RHI/Public/RHI/ERHIDescriptorType.h` for descriptor categories and shader visibility flags
- [X] T006 [P] Create public header shell `Source/RHI/Public/RHI/ERHIPipelineState.h` for topology, rasterizer, blend, depth-stencil, attachment, and lifecycle enums
- [X] T007 [P] Create descriptor header shells `Source/RHI/Public/RHI/FRHIBufferDesc.h`, `Source/RHI/Public/RHI/FRHITextureDesc.h`, and `Source/RHI/Public/RHI/FRHISamplerDesc.h`
- [X] T008 [P] Create descriptor header shells `Source/RHI/Public/RHI/FRHIShaderModuleDesc.h`, `Source/RHI/Public/RHI/FRHIDescriptorBinding.h`, and `Source/RHI/Public/RHI/FRHIPipelineLayoutDesc.h`
- [X] T009 [P] Create descriptor header shells `Source/RHI/Public/RHI/FRHIGraphicsPipelineDesc.h`, `Source/RHI/Public/RHI/FRHIComputePipelineDesc.h`, `Source/RHI/Public/RHI/FRHIRenderPassDesc.h`, and `Source/RHI/Public/RHI/FRHIFramebufferDesc.h`
- [X] T010 [P] Create interface header shells `Source/RHI/Public/RHI/IRHIBuffer.h`, `Source/RHI/Public/RHI/IRHITexture.h`, and `Source/RHI/Public/RHI/IRHISampler.h`
- [X] T011 [P] Create interface header shells `Source/RHI/Public/RHI/IRHIShaderModule.h`, `Source/RHI/Public/RHI/IRHIPipelineLayout.h`, and `Source/RHI/Public/RHI/IRHIDescriptorSet.h`
- [X] T012 [P] Create interface header shells `Source/RHI/Public/RHI/IRHIGraphicsPipeline.h`, `Source/RHI/Public/RHI/IRHIComputePipeline.h`, `Source/RHI/Public/RHI/IRHIRenderPass.h`, and `Source/RHI/Public/RHI/IRHIFramebuffer.h`
- [X] T013 Update `Source/RHI/Public/RHI/RHIMinimal.h` to include the new RHI resource/pipeline public header shells
- [X] T014 Add empty resource/pipeline test sections and result counters in `Tests/RHICoreTests.cpp`
- [X] T015 Run `conda run -n godot scons` from the repository root and fix scaffold build errors in `Source/RHI/Public/RHI/` or `Tests/RHICoreTests.cpp`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Establish shared flags, value objects, lifecycle contracts, helper validation, and device factory shape needed by all user stories.

**CRITICAL**: No user story work can begin until this phase is complete.

- [X] T016 Implement shared `ERHIResourceLifecycleState` values Valid and Invalidated in `Source/RHI/Public/RHI/ERHIPipelineState.h`
- [X] T017 Implement composable buffer usage flags and helper operators in `Source/RHI/Public/RHI/ERHIResourceUsage.h`
- [X] T018 Implement composable texture usage flags and helper operators in `Source/RHI/Public/RHI/ERHIResourceUsage.h`
- [X] T019 Implement texture dimension enum values in `Source/RHI/Public/RHI/ERHITextureDimension.h`
- [X] T020 Implement sampler filter, mip filter, address mode, and compare mode enum values in `Source/RHI/Public/RHI/ERHISamplerMode.h`
- [X] T021 Implement shader stage values and stage visibility flags in `Source/RHI/Public/RHI/ERHIShaderStage.h`
- [X] T022 Implement descriptor type enum values in `Source/RHI/Public/RHI/ERHIDescriptorType.h`
- [X] T023 Implement primitive topology, rasterizer, blend, depth-stencil, attachment role, load/store, and sample count enums in `Source/RHI/Public/RHI/ERHIPipelineState.h`
- [X] T024 Declare forward references and `TRHIObjectResult` factory return usage for new RHI interfaces in `Source/RHI/Public/RHI/IRHIDevice.h`
- [X] T025 Extend `IRHIDevice` with factory methods for buffers, textures, samplers, shader modules, pipeline layouts, descriptor sets, graphics pipelines, compute pipelines, render passes, and framebuffers in `Source/RHI/Public/RHI/IRHIDevice.h`
- [X] T026 Add shared mock resource/pipeline base lifecycle helpers in `Tests/RHICoreTests.cpp`
- [X] T027 Add shared mock validation helpers for usage flags, dimensions, shader stages, descriptor bindings, and Invalidated objects in `Tests/RHICoreTests.cpp`
- [X] T028 Add aggregate header isolation smoke checks for resource/pipeline headers in `Tests/RHICoreTests.cpp`
- [X] T029 Run `conda run -n godot scons` from the repository root and fix foundational build errors in `Source/RHI/Public/RHI/`, `Tests/RHICoreTests.cpp`, or `Source/RHI/Public/RHI/IRHIDevice.h`

**Checkpoint**: Foundation ready - user story implementation can now begin.

---

## Phase 3: User Story 1 - Describe and Create RHI Resources (Priority: P1) MVP

**Goal**: Developers can describe buffers, textures, and samplers through stable contracts and receive valid objects or explicit failures from a mock device.

**Independent Test**: Use a mock device to create valid buffers, textures, and samplers, query preserved descriptions and lifecycle state, and reject invalid size, dimension, format, usage, and sampler descriptions.

### Tests for User Story 1

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T030 [P] [US1] Add failing buffer description success and zero-size failure tests in `Tests/RHICoreTests.cpp`
- [X] T031 [P] [US1] Add failing buffer composable usage and incompatible usage combination tests in `Tests/RHICoreTests.cpp`
- [X] T032 [P] [US1] Add failing texture 1D, 2D, 3D, cube, and array description success tests in `Tests/RHICoreTests.cpp`
- [X] T033 [P] [US1] Add failing texture zero dimension, invalid mip/layer, non-square cube, unsupported format, and incompatible usage tests in `Tests/RHICoreTests.cpp`
- [X] T034 [P] [US1] Add failing sampler description success and unsupported sampler mode failure tests in `Tests/RHICoreTests.cpp`
- [X] T035 [US1] Add failing mock device factory creation and shutdown-state rejection tests for buffers, textures, and samplers in `Tests/RHICoreTests.cpp`

### Implementation for User Story 1

- [X] T036 [P] [US1] Implement `FRHIBufferDesc` fields and validation helper declarations in `Source/RHI/Public/RHI/FRHIBufferDesc.h`
- [X] T037 [P] [US1] Implement `FRHITextureDesc` fields and validation helper declarations in `Source/RHI/Public/RHI/FRHITextureDesc.h`
- [X] T038 [P] [US1] Implement `FRHISamplerDesc` fields and validation helper declarations in `Source/RHI/Public/RHI/FRHISamplerDesc.h`
- [X] T039 [P] [US1] Declare `IRHIBuffer` description query, size query, usage query, lifecycle query, and invalidation APIs in `Source/RHI/Public/RHI/IRHIBuffer.h`
- [X] T040 [P] [US1] Declare `IRHITexture` description query, dimension/format query, usage query, lifecycle query, and invalidation APIs in `Source/RHI/Public/RHI/IRHITexture.h`
- [X] T041 [P] [US1] Declare `IRHISampler` description query, lifecycle query, and invalidation APIs in `Source/RHI/Public/RHI/IRHISampler.h`
- [X] T042 [US1] Implement mock buffer, texture, and sampler classes in `Tests/RHICoreTests.cpp`
- [X] T043 [US1] Implement mock device buffer, texture, and sampler factory behavior in `Tests/RHICoreTests.cpp`
- [X] T044 [US1] Run `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`, then fix US1 failures in `Source/RHI/Public/RHI/` or `Tests/RHICoreTests.cpp`

**Checkpoint**: User Story 1 is independently functional and testable as the MVP.

---

## Phase 4: User Story 2 - Bind Resources Through Pipeline Layouts and Descriptor Sets (Priority: P1)

**Goal**: Developers can define multi-set pipeline layouts and update descriptor sets with compatible buffers, textures, samplers, and combined texture-sampler resources.

**Independent Test**: Use mock pipeline layouts and descriptor sets to verify set index + binding slot lookup, valid updates, incompatible descriptor rejection, invalid array indices, and Invalidated resource rejection.

### Tests for User Story 2

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T045 [P] [US2] Add failing descriptor binding declaration and duplicate binding rejection tests in `Tests/RHICoreTests.cpp`
- [X] T046 [P] [US2] Add failing multi-set pipeline layout creation and binding lookup tests in `Tests/RHICoreTests.cpp`
- [X] T047 [P] [US2] Add failing descriptor set creation for specific set index and missing set rejection tests in `Tests/RHICoreTests.cpp`
- [X] T048 [P] [US2] Add failing buffer, texture, sampler, and combined texture-sampler descriptor update success tests in `Tests/RHICoreTests.cpp`
- [X] T049 [P] [US2] Add failing wrong descriptor type, missing binding, invalid array index, and Invalidated resource update rejection tests in `Tests/RHICoreTests.cpp`
- [X] T050 [US2] Add failing mock device factory creation and shutdown-state rejection tests for pipeline layouts and descriptor sets in `Tests/RHICoreTests.cpp`

### Implementation for User Story 2

- [X] T051 [P] [US2] Implement `FRHIDescriptorBinding` set index, binding slot, descriptor type, array count, and shader visibility fields in `Source/RHI/Public/RHI/FRHIDescriptorBinding.h`
- [X] T052 [P] [US2] Implement `FRHIPipelineLayoutDesc` descriptor set layout and binding collection fields in `Source/RHI/Public/RHI/FRHIPipelineLayoutDesc.h`
- [X] T053 [P] [US2] Declare `IRHIPipelineLayout` description query, binding lookup, set count, lifecycle query, and invalidation APIs in `Source/RHI/Public/RHI/IRHIPipelineLayout.h`
- [X] T054 [P] [US2] Declare `IRHIDescriptorSet` set index query, update APIs, bound resource query APIs, lifecycle query, and invalidation APIs in `Source/RHI/Public/RHI/IRHIDescriptorSet.h`
- [X] T055 [US2] Implement mock pipeline layout validation and binding lookup in `Tests/RHICoreTests.cpp`
- [X] T056 [US2] Implement mock descriptor set update validation and bound resource storage in `Tests/RHICoreTests.cpp`
- [X] T057 [US2] Integrate mock device pipeline layout and descriptor set factory behavior in `Tests/RHICoreTests.cpp`
- [X] T058 [US2] Run `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`, then fix US2 failures in `Source/RHI/Public/RHI/` or `Tests/RHICoreTests.cpp`

**Checkpoint**: User Stories 1 and 2 both work independently and together for resource binding.

---

## Phase 5: User Story 3 - Define Graphics and Compute Pipelines (Priority: P1)

**Goal**: Developers can describe shader modules, graphics pipelines, and compute pipelines through RHI contracts and validate required stage/layout/render-target rules.

**Independent Test**: Use mock shader modules and pipeline objects to create valid compute and graphics pipelines, query preserved descriptions, and reject missing stages, wrong stages, Invalidated dependencies, and incompatible render target state.

### Tests for User Story 3

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T059 [P] [US3] Add failing shader module opaque payload identity, entry point, stage, and missing-field rejection tests in `Tests/RHICoreTests.cpp`
- [X] T060 [P] [US3] Add failing compute pipeline success, non-compute shader rejection, missing shader rejection, and multiple compute stage rejection tests in `Tests/RHICoreTests.cpp`
- [X] T061 [P] [US3] Add failing graphics pipeline vertex/fragment success and missing required stage rejection tests in `Tests/RHICoreTests.cpp`
- [X] T062 [P] [US3] Add failing graphics pipeline state preservation tests for vertex input, topology, rasterization, blend, depth-stencil, and render target compatibility in `Tests/RHICoreTests.cpp`
- [X] T063 [P] [US3] Add failing Invalidated shader module, Invalidated pipeline layout, and unsupported attachment format rejection tests in `Tests/RHICoreTests.cpp`
- [X] T064 [US3] Add failing mock device factory creation and shutdown-state rejection tests for shader modules, graphics pipelines, and compute pipelines in `Tests/RHICoreTests.cpp`

### Implementation for User Story 3

- [X] T065 [P] [US3] Implement `FRHIShaderModuleDesc` stage, entry point, opaque payload identity, and debug name fields in `Source/RHI/Public/RHI/FRHIShaderModuleDesc.h`
- [X] T066 [P] [US3] Implement graphics state value objects and `FRHIGraphicsPipelineDesc` fields in `Source/RHI/Public/RHI/FRHIGraphicsPipelineDesc.h`
- [X] T067 [P] [US3] Implement `FRHIComputePipelineDesc` compute shader and pipeline layout fields in `Source/RHI/Public/RHI/FRHIComputePipelineDesc.h`
- [X] T068 [P] [US3] Declare `IRHIShaderModule` description query, stage query, lifecycle query, and invalidation APIs in `Source/RHI/Public/RHI/IRHIShaderModule.h`
- [X] T069 [P] [US3] Declare `IRHIGraphicsPipeline` description query, layout query, lifecycle query, and invalidation APIs in `Source/RHI/Public/RHI/IRHIGraphicsPipeline.h`
- [X] T070 [P] [US3] Declare `IRHIComputePipeline` description query, layout query, lifecycle query, and invalidation APIs in `Source/RHI/Public/RHI/IRHIComputePipeline.h`
- [X] T071 [US3] Implement mock shader module, graphics pipeline, and compute pipeline validation classes in `Tests/RHICoreTests.cpp`
- [X] T072 [US3] Integrate mock device shader module and pipeline factory behavior in `Tests/RHICoreTests.cpp`
- [X] T073 [US3] Run `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`, then fix US3 failures in `Source/RHI/Public/RHI/` or `Tests/RHICoreTests.cpp`

**Checkpoint**: User Stories 1, 2, and 3 complete the P1 RHI resource/binding/pipeline MVP.

---

## Phase 6: User Story 4 - Model Render Passes and Framebuffers (Priority: P2)

**Goal**: Developers can model single-subpass render passes and compatible framebuffers using texture attachments.

**Independent Test**: Use mock render pass and framebuffer objects to validate attachment roles, load/store behavior, format/dimension/sample compatibility, and Invalidated texture/render pass rejection.

### Tests for User Story 4

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T074 [P] [US4] Add failing single-subpass render pass success tests for color and optional depth-stencil attachments in `Tests/RHICoreTests.cpp`
- [X] T075 [P] [US4] Add failing render pass empty attachment, unsupported format, invalid sample count, and multi-subpass exclusion tests in `Tests/RHICoreTests.cpp`
- [X] T076 [P] [US4] Add failing framebuffer success tests with compatible texture attachments and dimensions in `Tests/RHICoreTests.cpp`
- [X] T077 [P] [US4] Add failing framebuffer attachment count, format, dimension, sample count, Invalidated texture, and Invalidated render pass rejection tests in `Tests/RHICoreTests.cpp`
- [X] T078 [US4] Add failing mock device factory creation and shutdown-state rejection tests for render passes and framebuffers in `Tests/RHICoreTests.cpp`

### Implementation for User Story 4

- [X] T079 [P] [US4] Implement attachment description, attachment role, load/store behavior, and `FRHIRenderPassDesc` fields in `Source/RHI/Public/RHI/FRHIRenderPassDesc.h`
- [X] T080 [P] [US4] Implement attachment references, dimensions, and `FRHIFramebufferDesc` fields in `Source/RHI/Public/RHI/FRHIFramebufferDesc.h`
- [X] T081 [P] [US4] Declare `IRHIRenderPass` description query, attachment query, lifecycle query, and invalidation APIs in `Source/RHI/Public/RHI/IRHIRenderPass.h`
- [X] T082 [P] [US4] Declare `IRHIFramebuffer` description query, render pass query, dimension query, attachment count query, lifecycle query, and invalidation APIs in `Source/RHI/Public/RHI/IRHIFramebuffer.h`
- [X] T083 [US4] Implement mock render pass validation and lifecycle behavior in `Tests/RHICoreTests.cpp`
- [X] T084 [US4] Implement mock framebuffer compatibility validation and lifecycle behavior in `Tests/RHICoreTests.cpp`
- [X] T085 [US4] Integrate mock device render pass and framebuffer factory behavior in `Tests/RHICoreTests.cpp`
- [X] T086 [US4] Run `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`, then fix US4 failures in `Source/RHI/Public/RHI/` or `Tests/RHICoreTests.cpp`

**Checkpoint**: Render target contracts are independently functional and ready for future Vulkan and render graph planning.

---

## Phase 7: User Story 5 - Validate Resource and Pipeline Lifecycles With Mocks (Priority: P2)

**Goal**: Developers can run deterministic mock-based tests that exercise every new public contract, including Invalidated lifecycle behavior and aggregate header isolation.

**Independent Test**: Run the project test executable and confirm every resource/pipeline contract has success and negative coverage without a graphics backend.

### Tests for User Story 5

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation where coverage is still missing.**

- [X] T087 [P] [US5] Add failing lifecycle matrix tests for Valid to Invalidated transitions across all resource and pipeline-family mock objects in `Tests/RHICoreTests.cpp`
- [X] T088 [P] [US5] Add failing aggregate renderer-facing smoke test that describes resources, binds descriptors, creates a pipeline, creates a render pass/framebuffer pair, and validates the complete mock setup in `Tests/RHICoreTests.cpp`
- [X] T089 [P] [US5] Add failing public header isolation scan assertions for new RHI resource/pipeline headers in `Tests/RHICoreTests.cpp`
- [X] T090 [US5] Add failing verification that every public contract introduced by `specs/008-rhi-resource-pipeline/contracts/rhi-resource-pipeline-api.md` has at least one success path and one negative path in `Tests/RHICoreTests.cpp`

### Implementation for User Story 5

- [X] T091 [US5] Implement cross-object Invalidated lifecycle behavior for all resource and pipeline-family mocks in `Tests/RHICoreTests.cpp`
- [X] T092 [US5] Implement renderer-facing smoke flow helper using mock resources, descriptors, graphics pipeline, render pass, and framebuffer in `Tests/RHICoreTests.cpp`
- [X] T093 [US5] Update `Source/RHI/Public/RHI/RHIMinimal.h` to include every finalized public RHI resource/pipeline header
- [X] T094 [US5] Review `Source/RHI/Public/RHI/` headers to remove any Backend, Renderer, Application, platform-windowing, or concrete graphics API public dependencies
- [X] T095 [US5] Run `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`, then fix US5 failures in `Source/RHI/Public/RHI/` or `Tests/RHICoreTests.cpp`

**Checkpoint**: All user stories are independently functional and collectively validate the RHI resource/pipeline contracts.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Verify contract coverage, naming, architecture isolation, quickstart readiness, and roadmap state.

- [X] T096 Verify `specs/008-rhi-resource-pipeline/contracts/rhi-resource-pipeline-api.md` is satisfied by public headers in `Source/RHI/Public/RHI/`
- [X] T097 Verify no public RHI resource/pipeline header includes Backend, Renderer, Application, graphics API, native windowing, or platform surface headers in `Source/RHI/Public/RHI/`
- [X] T098 Verify public names follow UE5-style `I*`, `F*`, and `E*` naming conventions in `Source/RHI/Public/RHI/`
- [X] T099 Review `Tests/RHICoreTests.cpp` to ensure every public RHI resource/pipeline contract has at least one success path and one negative path
- [X] T100 Run the quickstart build and verification flow from `specs/008-rhi-resource-pipeline/quickstart.md`
- [X] T101 Run `conda run -n godot scons` from the repository root and confirm `Build/<Platform>/Debug/Tests/StonerTest` exits with code 0
- [X] T102 Update `doc/roadmap.md` Phase 007 status only after implementation and verification are complete
- [X] T103 Review `specs/008-rhi-resource-pipeline/tasks.md` for completed task checkboxes and consistency before closing the feature

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion - BLOCKS all user stories
- **User Story 1 (Phase 3)**: Depends on Foundational - MVP
- **User Story 2 (Phase 4)**: Depends on Foundational and benefits from US1 resources for descriptor binding tests
- **User Story 3 (Phase 5)**: Depends on Foundational and benefits from US2 pipeline layouts for pipeline creation tests
- **User Story 4 (Phase 6)**: Depends on Foundational and benefits from US1 textures for framebuffer tests
- **User Story 5 (Phase 7)**: Depends on all new contracts for complete lifecycle and smoke-flow coverage
- **Polish (Phase 8)**: Depends on all desired user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Foundational; recommended MVP because resources are needed by descriptor sets and framebuffers
- **User Story 2 (P1)**: Can start after Foundational, but full binding tests use US1 mock resources
- **User Story 3 (P1)**: Can start after Foundational, but full pipeline tests use US2 pipeline layouts
- **User Story 4 (P2)**: Can start after Foundational, but full framebuffer tests use US1 texture mocks
- **User Story 5 (P2)**: Integrates all prior stories for lifecycle matrix and smoke-flow validation

### Within Each User Story

- Tests MUST be written and fail before implementation
- Public descriptors and enums before interface declarations
- Interface declarations before mock behavior implementation
- Device factory extensions before story integration through mock device
- Story complete before moving to next priority in solo-agent mode
- Run `conda run -n godot scons` and `StonerTest` at each story checkpoint

### Parallel Opportunities

- T001-T012 can run in parallel during Setup
- T017-T023 can run in parallel after T016 during Foundational
- Descriptor/value object headers in each story can be implemented in parallel with unrelated interface headers
- Test additions marked [P] within a story can be drafted in parallel, but coordinate same-file edits to `Tests/RHICoreTests.cpp`
- After Foundational, US1 and the shell work for US2/US3/US4 can proceed in parallel if teams coordinate dependencies
- Polish checks T096-T099 can run in parallel after implementation is complete

---

## Parallel Example: User Story 1

```bash
# Draft US1 tests in parallel:
Task: "Add failing buffer description success and zero-size failure tests in Tests/RHICoreTests.cpp"
Task: "Add failing texture 1D, 2D, 3D, cube, and array description success tests in Tests/RHICoreTests.cpp"
Task: "Add failing sampler description success and unsupported sampler mode failure tests in Tests/RHICoreTests.cpp"

# Implement independent US1 headers in parallel:
Task: "Implement FRHIBufferDesc fields and validation helper declarations in Source/RHI/Public/RHI/FRHIBufferDesc.h"
Task: "Implement FRHITextureDesc fields and validation helper declarations in Source/RHI/Public/RHI/FRHITextureDesc.h"
Task: "Implement FRHISamplerDesc fields and validation helper declarations in Source/RHI/Public/RHI/FRHISamplerDesc.h"
```

## Parallel Example: User Story 2

```bash
# Draft US2 tests in parallel:
Task: "Add failing descriptor binding declaration and duplicate binding rejection tests in Tests/RHICoreTests.cpp"
Task: "Add failing multi-set pipeline layout creation and binding lookup tests in Tests/RHICoreTests.cpp"
Task: "Add failing descriptor set creation for specific set index and missing set rejection tests in Tests/RHICoreTests.cpp"

# Implement independent US2 headers in parallel:
Task: "Implement FRHIDescriptorBinding set index, binding slot, descriptor type, array count, and shader visibility fields in Source/RHI/Public/RHI/FRHIDescriptorBinding.h"
Task: "Implement FRHIPipelineLayoutDesc descriptor set layout and binding collection fields in Source/RHI/Public/RHI/FRHIPipelineLayoutDesc.h"
Task: "Declare IRHIPipelineLayout description query, binding lookup, set count, lifecycle query, and invalidation APIs in Source/RHI/Public/RHI/IRHIPipelineLayout.h"
```

## Parallel Example: User Story 3

```bash
# Draft US3 tests in parallel:
Task: "Add failing shader module opaque payload identity, entry point, stage, and missing-field rejection tests in Tests/RHICoreTests.cpp"
Task: "Add failing compute pipeline success, non-compute shader rejection, missing shader rejection, and multiple compute stage rejection tests in Tests/RHICoreTests.cpp"
Task: "Add failing graphics pipeline vertex/fragment success and missing required stage rejection tests in Tests/RHICoreTests.cpp"

# Implement independent US3 headers in parallel:
Task: "Implement FRHIShaderModuleDesc stage, entry point, opaque payload identity, and debug name fields in Source/RHI/Public/RHI/FRHIShaderModuleDesc.h"
Task: "Implement graphics state value objects and FRHIGraphicsPipelineDesc fields in Source/RHI/Public/RHI/FRHIGraphicsPipelineDesc.h"
Task: "Implement FRHIComputePipelineDesc compute shader and pipeline layout fields in Source/RHI/Public/RHI/FRHIComputePipelineDesc.h"
```

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational
3. Complete Phase 3: User Story 1
4. Stop and validate buffer, texture, and sampler contracts independently
5. Continue to descriptor binding only after resource creation is stable

### Incremental Delivery

1. Setup + Foundational -> shared enums, lifecycle, factory shape, and test helpers
2. US1 -> resources and samplers
3. US2 -> pipeline layouts and descriptor sets
4. US3 -> shader modules and graphics/compute pipelines
5. US4 -> render passes and framebuffers
6. US5 -> lifecycle matrix, smoke flow, aggregate isolation
7. Polish -> quickstart and roadmap verification

### Parallel Team Strategy

With multiple developers:

1. Team completes Setup + Foundational together
2. Developer A: resource descriptors/interfaces and US1 tests
3. Developer B: descriptor layout headers and US2 tests after foundational enum shape
4. Developer C: pipeline descriptor headers and US3 tests after layout shape stabilizes
5. Developer D: render pass/framebuffer headers and US4 tests after texture descriptor shape stabilizes
6. Integrate through US5 smoke flow and final quickstart validation

## Notes

- [P] tasks = different files or independent test slices; same-file test tasks still require careful merge coordination in `Tests/RHICoreTests.cpp`
- [US] label maps task to a specific user story for traceability
- Each user story is independently completable and testable at its checkpoint
- Verify tests fail before implementing each story
- Commit after each task or logical group when requested
- Avoid adding concrete backend, renderer, application, native windowing, or graphics API dependencies in public RHI headers
