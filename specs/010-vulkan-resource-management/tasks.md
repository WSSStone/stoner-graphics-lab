# Tasks: Vulkan Resource Management

**Input**: Design documents from `/specs/010-vulkan-resource-management/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/vulkan-resource-management-contract.md, quickstart.md

**Tests**: Test tasks are included because the feature specification requires deterministic success and negative coverage for resource creation, allocation limits, descriptor binding, upload staging, lifecycle invalidation, and shutdown.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Path Conventions

- Vulkan backend public headers: `Source/Backend/Vulkan/Public/VulkanRHI/`
- Vulkan backend private sources: `Source/Backend/Vulkan/Private/`
- Vulkan backend build script: `Source/Backend/Vulkan/SConscript`
- Tests: `Tests/`
- Feature documentation: `specs/010-vulkan-resource-management/`

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish source/test shells and build registration for the Vulkan resource management slice without implementing behavior yet.

- [X] T001 [P] Create public header shell `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanResourceAllocation.h` for allocation mode, limits, and ownership contracts
- [X] T002 [P] Create public header shell `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanMemoryAllocator.h` for real-or-fallback allocator and test limit contracts
- [X] T003 [P] Create public header shell `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanBuffer.h` for backend buffer resource contracts
- [X] T004 [P] Create public header shell `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanTexture.h` for backend texture resource contracts
- [X] T005 [P] Create public header shell `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanSampler.h` for backend sampler resource contracts
- [X] T006 [P] Create public header shell `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDescriptorPool.h` for fixed-capacity descriptor pool contracts
- [X] T007 [P] Create public header shell `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDescriptorSet.h` for descriptor set and retained binding contracts
- [X] T008 [P] Create public header shell `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanUploadStaging.h` for pending upload staging contracts
- [X] T009 [P] Create private source shells `Source/Backend/Vulkan/Private/FVulkanResourceAllocation.cpp` and `Source/Backend/Vulkan/Private/FVulkanMemoryAllocator.cpp`
- [X] T010 [P] Create private source shells `Source/Backend/Vulkan/Private/FVulkanBuffer.cpp`, `Source/Backend/Vulkan/Private/FVulkanTexture.cpp`, and `Source/Backend/Vulkan/Private/FVulkanSampler.cpp`
- [X] T011 [P] Create private source shells `Source/Backend/Vulkan/Private/FVulkanDescriptorPool.cpp` and `Source/Backend/Vulkan/Private/FVulkanDescriptorSet.cpp`
- [X] T012 [P] Create private source shell `Source/Backend/Vulkan/Private/FVulkanUploadStaging.cpp`
- [X] T013 Update aggregate backend header `Source/Backend/Vulkan/Public/VulkanRHI/VulkanDevice.h` to include new resource management public headers
- [X] T014 Update `Source/Backend/Vulkan/SConscript` if needed so new `Source/Backend/Vulkan/Private/*.cpp` files are built by the existing source discovery
- [X] T015 Update `Tests/VulkanBackendTests.h` with resource management result/helper declarations
- [X] T016 Update `Tests/VulkanBackendTests.cpp` with empty resource management test group entry points
- [X] T017 Run `conda run -n godot scons` and fix scaffold build errors in `Source/Backend/Vulkan/` or `Tests/VulkanBackendTests.cpp`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Implement shared resource lifecycle, allocation diagnostics, allocator limits, descriptor pool capacity primitives, and upload metadata used by every user story.

**CRITICAL**: No user story work can begin until this phase is complete.

- [X] T018 Implement allocation mode enum, allocation failure enum, resource kind enum, and allocation record fields in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanResourceAllocation.h`
- [X] T019 Implement allocation record constructors, success/failure helpers, release state, and byte-size query in `Source/Backend/Vulkan/Private/FVulkanResourceAllocation.cpp`
- [X] T020 Implement allocator limit settings, allocation snapshot fields, and diagnostic query declarations in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanMemoryAllocator.h`
- [X] T021 Implement deterministic fallback allocation, real-runtime mode selection placeholder, budget limit checks, allocation-count limit checks, and release accounting in `Source/Backend/Vulkan/Private/FVulkanMemoryAllocator.cpp`
- [X] T022 Extend diagnostics fields for resource allocation mode, fallback reason, allocation failure reason, descriptor pool exhaustion reason, descriptor update rejection reason, and upload rejection reason in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDiagnostics.h`
- [X] T023 Implement resource diagnostic helper functions in `Source/Backend/Vulkan/Private/FVulkanDiagnostics.cpp`
- [X] T024 Add resource test helper declarations for valid/invalid buffer descs, valid/invalid texture descs, sampler descs, pipeline layouts, and diagnostic assertions in `Tests/VulkanBackendTests.h`
- [X] T025 Add resource test helper implementations for valid/invalid buffer descs, valid/invalid texture descs, sampler descs, pipeline layouts, and diagnostic assertions in `Tests/VulkanBackendTests.cpp`
- [X] T026 Add backend device resource configuration declarations for allocation budget, allocation-count limit, descriptor pool capacity, and reset-to-default behavior in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDevice.h`
- [X] T027 Add owned resource container declarations for buffers, textures, samplers, descriptor pools, descriptor sets, and upload requests in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDevice.h`
- [X] T028 Integrate allocator member construction, default descriptor capacity, resource diagnostics, and owned resource invalidation placeholders into `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T029 Run `conda run -n godot scons` and fix foundational build errors in `Source/Backend/Vulkan/` or `Tests/VulkanBackendTests.cpp`

**Checkpoint**: Foundation ready - user story implementation can now begin.

---

## Phase 3: User Story 1 - Create Backend Buffers and Textures (Priority: P1) MVP

**Goal**: Developers can create backend-backed buffers and textures through existing RHI contracts, query descriptions and lifecycle state, and receive explicit failures for invalid descriptions.

**Independent Test**: Create an active backend device, request valid buffer and texture descriptions, verify queryable descriptions/lifecycle/allocation diagnostics, and verify invalid or unsupported descriptions return explicit failures without usable resources.

### Tests for User Story 1

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T030 [P] [US1] Add failing buffer creation success and description preservation tests in `Tests/VulkanBackendTests.cpp`
- [X] T031 [P] [US1] Add failing texture creation success and description preservation tests in `Tests/VulkanBackendTests.cpp`
- [X] T032 [P] [US1] Add failing buffer invalid description rejection tests for zero size and unsupported usage in `Tests/VulkanBackendTests.cpp`
- [X] T033 [P] [US1] Add failing texture invalid description rejection tests for dimensions, format, usage, mips, arrays, and sample count in `Tests/VulkanBackendTests.cpp`
- [X] T034 [P] [US1] Add failing real-or-fallback allocation diagnostic tests for created buffers and textures in `Tests/VulkanBackendTests.cpp`
- [X] T035 [US1] Add failing partial buffer and texture creation cleanup tests in `Tests/VulkanBackendTests.cpp`

### Implementation for User Story 1

- [X] T036 [P] [US1] Implement `FVulkanBuffer` class declaration with RHI buffer interface, desc query, size query, usage query, lifecycle query, allocation query, and invalidate API in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanBuffer.h`
- [X] T037 [US1] Implement `FVulkanBuffer` construction, query behavior, allocation ownership, invalidation, and invalid repeated transition behavior in `Source/Backend/Vulkan/Private/FVulkanBuffer.cpp`
- [X] T038 [P] [US1] Implement `FVulkanTexture` class declaration with RHI texture interface, desc query, dimension query, format query, usage query, lifecycle query, allocation query, and invalidate API in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanTexture.h`
- [X] T039 [US1] Implement `FVulkanTexture` construction, query behavior, allocation ownership, invalidation, and invalid repeated transition behavior in `Source/Backend/Vulkan/Private/FVulkanTexture.cpp`
- [X] T040 [US1] Implement buffer description support validation and unsupported reason mapping in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T041 [US1] Implement texture description support validation and unsupported reason mapping in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T042 [US1] Implement `FVulkanDevice::CreateBuffer` using allocator records, real-or-fallback diagnostics, owned buffer tracking, and partial failure cleanup in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T043 [US1] Implement `FVulkanDevice::CreateTexture` using allocator records, real-or-fallback diagnostics, owned texture tracking, and partial failure cleanup in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T044 [US1] Wire buffer and texture headers into `Source/Backend/Vulkan/Public/VulkanRHI/VulkanDevice.h`
- [X] T045 [US1] Run `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`, then fix US1 failures in `Source/Backend/Vulkan/` or `Tests/VulkanBackendTests.cpp`

**Checkpoint**: User Story 1 is independently functional and testable as the MVP.

---

## Phase 4: User Story 2 - Manage Backend Resource Memory Safely (Priority: P1)

**Goal**: Developers can rely on allocation ownership, deterministic failure limits, cleanup, and shutdown invalidation for created buffers and textures.

**Independent Test**: Configure supported, fallback, budget-limited, and allocation-count-limited paths; verify allocation records, deterministic failure, no partial usable resources, and safe repeated cleanup.

### Tests for User Story 2

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T046 [P] [US2] Add failing allocation record mode, byte size, release state, and diagnostics tests in `Tests/VulkanBackendTests.cpp`
- [X] T047 [P] [US2] Add failing allocation budget limit failure tests for buffers and textures in `Tests/VulkanBackendTests.cpp`
- [X] T048 [P] [US2] Add failing allocation-count limit failure tests for buffers and textures in `Tests/VulkanBackendTests.cpp`
- [X] T049 [P] [US2] Add failing partial allocation cleanup and no usable partial resource tests in `Tests/VulkanBackendTests.cpp`
- [X] T050 [P] [US2] Add failing repeated create/invalidate/release cycle tests in `Tests/VulkanBackendTests.cpp`
- [X] T051 [US2] Add failing device shutdown resource invalidation tests for buffers and textures in `Tests/VulkanBackendTests.cpp`

### Implementation for User Story 2

- [X] T052 [US2] Implement allocator reset, configure budget, configure allocation-count limit, clear limits, and diagnostic state APIs in `Source/Backend/Vulkan/Private/FVulkanMemoryAllocator.cpp`
- [X] T053 [US2] Implement allocation byte estimation helpers for buffer and texture descriptions in `Source/Backend/Vulkan/Private/FVulkanMemoryAllocator.cpp`
- [X] T054 [US2] Implement allocation budget exhaustion and allocation-count exhaustion result mapping in `Source/Backend/Vulkan/Private/FVulkanMemoryAllocator.cpp`
- [X] T055 [US2] Implement `FVulkanDevice` public resource limit configuration APIs in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T056 [US2] Implement resource allocation release on buffer and texture invalidation in `Source/Backend/Vulkan/Private/FVulkanBuffer.cpp` and `Source/Backend/Vulkan/Private/FVulkanTexture.cpp`
- [X] T057 [US2] Implement owned buffer and texture invalidation during device shutdown in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T058 [US2] Add allocation lifecycle diagnostics to `Source/Backend/Vulkan/Private/FVulkanDiagnostics.cpp`
- [X] T059 [US2] Run `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`, then fix US2 failures in `Source/Backend/Vulkan/` or `Tests/VulkanBackendTests.cpp`

**Checkpoint**: Resource allocation ownership and deterministic failure behavior are independently functional.

---

## Phase 5: User Story 3 - Create and Use Backend Samplers (Priority: P1)

**Goal**: Developers can create backend sampler objects for supported descriptions, query lifecycle and desc state, and receive explicit unsupported results for unsupported sampler modes.

**Independent Test**: Create supported samplers, verify preserved desc and lifecycle state, reject unsupported mode combinations, and reject creation after shutdown.

### Tests for User Story 3

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T060 [P] [US3] Add failing sampler creation success and description preservation tests in `Tests/VulkanBackendTests.cpp`
- [X] T061 [P] [US3] Add failing unsupported sampler mode combination tests in `Tests/VulkanBackendTests.cpp`
- [X] T062 [P] [US3] Add failing sampler lifecycle invalidation tests in `Tests/VulkanBackendTests.cpp`
- [X] T063 [US3] Add failing post-shutdown sampler creation rejection tests in `Tests/VulkanBackendTests.cpp`

### Implementation for User Story 3

- [X] T064 [P] [US3] Implement `FVulkanSampler` class declaration with RHI sampler interface, desc query, lifecycle query, unsupported diagnostics, and invalidate API in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanSampler.h`
- [X] T065 [US3] Implement `FVulkanSampler` construction, query behavior, lifecycle invalidation, and invalid repeated transition behavior in `Source/Backend/Vulkan/Private/FVulkanSampler.cpp`
- [X] T066 [US3] Implement sampler description support validation and unsupported reason mapping in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T067 [US3] Implement `FVulkanDevice::CreateSampler` with owned sampler tracking and post-shutdown invalid-state behavior in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T068 [US3] Wire sampler header into `Source/Backend/Vulkan/Public/VulkanRHI/VulkanDevice.h`
- [X] T069 [US3] Run `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`, then fix US3 failures in `Source/Backend/Vulkan/` or `Tests/VulkanBackendTests.cpp`

**Checkpoint**: Backend sampler creation and lifecycle behavior are independently functional.

---

## Phase 6: User Story 4 - Bind Resources Through Descriptor Sets (Priority: P1)

**Goal**: Developers can allocate descriptor sets from fixed-capacity pools, bind buffers/textures/samplers/combined resources, query retained bindings, and receive explicit failures for invalid bindings or pool exhaustion.

**Independent Test**: Create resources, layouts, and descriptor sets from a fixed-capacity pool; update supported binding kinds; verify wrong type, missing binding, invalid index, invalidated resource, pool exhaustion, retained invalidated binding query, and post-shutdown rejection.

### Tests for User Story 4

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T070 [P] [US4] Add failing descriptor pool capacity and descriptor set allocation success tests in `Tests/VulkanBackendTests.cpp`
- [X] T071 [P] [US4] Add failing descriptor pool exhaustion tests in `Tests/VulkanBackendTests.cpp`
- [X] T072 [P] [US4] Add failing missing layout set and invalidated layout rejection tests in `Tests/VulkanBackendTests.cpp`
- [X] T073 [P] [US4] Add failing descriptor update success tests for buffer, texture, sampler, and combined texture-sampler bindings in `Tests/VulkanBackendTests.cpp`
- [X] T074 [P] [US4] Add failing descriptor update rejection tests for wrong type, missing binding, invalid array index, missing resource, and invalidated resource in `Tests/VulkanBackendTests.cpp`
- [X] T075 [P] [US4] Add failing retained binding query tests after referenced resource invalidation in `Tests/VulkanBackendTests.cpp`
- [X] T076 [US4] Add failing descriptor set post-shutdown update and allocation rejection tests in `Tests/VulkanBackendTests.cpp`

### Implementation for User Story 4

- [X] T077 [P] [US4] Implement `FVulkanDescriptorPool` class declaration with fixed capacity, allocated count, lifecycle state, allocation, release, and exhaustion query APIs in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDescriptorPool.h`
- [X] T078 [US4] Implement `FVulkanDescriptorPool` fixed-capacity allocation, exhaustion result, release accounting, and invalidation behavior in `Source/Backend/Vulkan/Private/FVulkanDescriptorPool.cpp`
- [X] T079 [P] [US4] Implement `FVulkanDescriptorSet` class declaration with RHI descriptor set interface, bound record query, retained invalid resource reporting, and invalidate API in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDescriptorSet.h`
- [X] T080 [US4] Implement descriptor set construction, set index query, layout query, bound kind query, bound count query, lifecycle query, and invalidation in `Source/Backend/Vulkan/Private/FVulkanDescriptorSet.cpp`
- [X] T081 [US4] Implement descriptor set binding validation for layout set, binding slot, descriptor type, array index, resource presence, and resource lifecycle in `Source/Backend/Vulkan/Private/FVulkanDescriptorSet.cpp`
- [X] T082 [US4] Implement descriptor update methods for buffers, textures, samplers, and combined texture-sampler resources in `Source/Backend/Vulkan/Private/FVulkanDescriptorSet.cpp`
- [X] T083 [US4] Implement retained bound resource record query behavior after referenced resource invalidation in `Source/Backend/Vulkan/Private/FVulkanDescriptorSet.cpp`
- [X] T084 [US4] Implement descriptor pool configuration and default pool ownership on `FVulkanDevice` in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T085 [US4] Implement `FVulkanDevice::CreateDescriptorSet` using fixed-capacity descriptor pool, missing set rejection, owned set tracking, and post-shutdown invalid-state behavior in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T086 [US4] Implement descriptor pool and descriptor set invalidation during device shutdown in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T087 [US4] Add descriptor diagnostics for pool exhaustion and update rejection in `Source/Backend/Vulkan/Private/FVulkanDiagnostics.cpp`
- [X] T088 [US4] Wire descriptor pool and descriptor set headers into `Source/Backend/Vulkan/Public/VulkanRHI/VulkanDevice.h`
- [X] T089 [US4] Run `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`, then fix US4 failures in `Source/Backend/Vulkan/` or `Tests/VulkanBackendTests.cpp`

**Checkpoint**: Descriptor pool, descriptor set, update, retained invalidation, and exhaustion behavior are independently functional.

---

## Phase 7: User Story 5 - Stage Resource Upload Requests for Later Submission (Priority: P2)

**Goal**: Developers can create pending upload records with CPU-visible staging data and validated destination ranges or regions without claiming GPU execution.

**Independent Test**: Create valid buffer and texture upload requests, verify staged data and metadata, and reject missing data, out-of-bounds ranges, incompatible regions, invalidated destinations, unsupported paths, and post-shutdown requests.

### Tests for User Story 5

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation.**

- [X] T090 [P] [US5] Add failing buffer upload staging success tests for CPU-visible data preservation and destination byte range metadata in `Tests/VulkanBackendTests.cpp`
- [X] T091 [P] [US5] Add failing buffer upload rejection tests for missing data, out-of-bounds ranges, zero-byte ranges, and invalidated destination buffers in `Tests/VulkanBackendTests.cpp`
- [X] T092 [P] [US5] Add failing texture upload staging success tests for CPU-visible data preservation and destination region metadata in `Tests/VulkanBackendTests.cpp`
- [X] T093 [P] [US5] Add failing texture upload rejection tests for missing data, invalid regions, incompatible format expectations, and invalidated destination textures in `Tests/VulkanBackendTests.cpp`
- [X] T094 [P] [US5] Add failing upload record pending state and no-execution-claimed tests in `Tests/VulkanBackendTests.cpp`
- [X] T095 [US5] Add failing upload record invalidation on destination resource invalidation and device shutdown tests in `Tests/VulkanBackendTests.cpp`

### Implementation for User Story 5

- [X] T096 [P] [US5] Implement upload kind enum, upload lifecycle enum, buffer upload range, texture upload region, and upload request declarations in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanUploadStaging.h`
- [X] T097 [US5] Implement upload request construction, staging data ownership, destination metadata queries, pending lifecycle state, and invalidation in `Source/Backend/Vulkan/Private/FVulkanUploadStaging.cpp`
- [X] T098 [US5] Implement buffer upload range validation for source data, destination buffer lifecycle, offset, byte count, and bounds in `Source/Backend/Vulkan/Private/FVulkanUploadStaging.cpp`
- [X] T099 [US5] Implement texture upload region validation for source data, destination texture lifecycle, mip level, array layer, extent, and format compatibility expectations in `Source/Backend/Vulkan/Private/FVulkanUploadStaging.cpp`
- [X] T100 [US5] Add `FVulkanDevice` declarations for staging buffer uploads, staging texture uploads, upload request ownership, and upload diagnostics in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDevice.h`
- [X] T101 [US5] Implement `FVulkanDevice` staging buffer upload and staging texture upload APIs with owned upload request tracking in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T102 [US5] Implement upload request invalidation during destination resource invalidation and device shutdown in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T103 [US5] Add upload rejection diagnostics to `Source/Backend/Vulkan/Private/FVulkanDiagnostics.cpp`
- [X] T104 [US5] Wire upload staging header into `Source/Backend/Vulkan/Public/VulkanRHI/VulkanDevice.h`
- [X] T105 [US5] Run `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`, then fix US5 failures in `Source/Backend/Vulkan/` or `Tests/VulkanBackendTests.cpp`

**Checkpoint**: Upload staging records are independently functional and ready for the future command submission phase.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Verify contract coverage, backend isolation, portability, quickstart readiness, and roadmap state.

- [X] T106 Verify `specs/010-vulkan-resource-management/contracts/vulkan-resource-management-contract.md` is satisfied by `Source/Backend/Vulkan/` and `Tests/VulkanBackendTests.cpp`
- [X] T107 Verify no Renderer or Application public dependency is introduced into `Source/Backend/Vulkan/Public/VulkanRHI/`
- [X] T108 Verify public names follow UE5-style `F*`, `E*`, `I*`, and `T*` naming conventions in `Source/Backend/Vulkan/Public/VulkanRHI/`
- [X] T109 Verify Vulkan-specific includes remain isolated to `Source/Backend/Vulkan/` and do not leak into `Source/RHI/Public/RHI/`
- [X] T110 Review `Tests/VulkanBackendTests.cpp` to ensure real-runtime, fallback allocation, allocation-limit failure, descriptor pool exhaustion, retained invalidated binding, and upload staging modes are deterministic
- [X] T111 Run quickstart build and verification flow from `specs/010-vulkan-resource-management/quickstart.md`
- [X] T112 Run `conda run -n godot scons` through `SConstruct` from the repository root and confirm the build exits with code 0
- [X] T113 Run `Build/Mac/Debug/Tests/StonerTest` from the repository root and confirm the executable exits with code 0
- [X] T114 Update `doc/roadmap.md` Phase 009 status only after implementation and verification are complete
- [X] T115 Review `specs/010-vulkan-resource-management/tasks.md` for completed task checkboxes and consistency before closing the feature

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion - BLOCKS all user stories
- **User Story 1 (Phase 3)**: Depends on Foundational - MVP
- **User Story 2 (Phase 4)**: Depends on Foundational and benefits from US1 resource object shape
- **User Story 3 (Phase 5)**: Depends on Foundational and can run after US1 device factory shape is stable
- **User Story 4 (Phase 6)**: Depends on Foundational and benefits from US1/US3 resources for descriptor binding
- **User Story 5 (Phase 7)**: Depends on US1 resource objects and benefits from US2 lifecycle handling
- **Polish (Phase 8)**: Depends on all desired user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Foundational; recommended MVP because it replaces buffer/texture unsupported placeholders with backend resources.
- **User Story 2 (P1)**: Can start after Foundational, but allocation failure and shutdown tests use US1 resource objects.
- **User Story 3 (P1)**: Can start after Foundational; sampler factory can be validated independently of buffer/texture allocation.
- **User Story 4 (P1)**: Can start after Foundational, but full descriptor binding validation uses US1 buffers/textures and US3 samplers.
- **User Story 5 (P2)**: Depends on US1 buffer/texture objects and lifecycle behavior for destination validation.

### Within Each User Story

- Tests MUST be written and fail before implementation.
- Public headers before private source implementations.
- Resource/allocation/descriptor/upload data models before device factory integration.
- Factory integration before full story smoke tests.
- Story complete before moving to the next priority unless working in parallel after Foundation.

### Parallel Opportunities

- Setup header/source shell tasks T001-T012 can run in parallel.
- Foundational diagnostic and helper tasks T018-T025 can run in parallel after Setup.
- Test tasks inside each user story marked [P] can be written in parallel.
- Public header tasks for buffers, textures, samplers, descriptor pools, descriptor sets, and upload staging can be written in parallel once Foundation is complete.
- US2 and US3 can be implemented in parallel after US1 factory shape is stable.

---

## Parallel Example: User Story 1

```bash
# Launch tests for User Story 1 in parallel:
Task: "Add failing buffer creation success and description preservation tests in Tests/VulkanBackendTests.cpp"
Task: "Add failing texture creation success and description preservation tests in Tests/VulkanBackendTests.cpp"
Task: "Add failing real-or-fallback allocation diagnostic tests for created buffers and textures in Tests/VulkanBackendTests.cpp"

# Launch independent public declarations in parallel:
Task: "Implement FVulkanBuffer class declaration with RHI buffer interface, desc query, size query, usage query, lifecycle query, allocation query, and invalidate API in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanBuffer.h"
Task: "Implement FVulkanTexture class declaration with RHI texture interface, desc query, dimension query, format query, usage query, lifecycle query, allocation query, and invalidate API in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanTexture.h"
```

## Parallel Example: User Story 4

```bash
# Launch tests for User Story 4 in parallel:
Task: "Add failing descriptor pool capacity and descriptor set allocation success tests in Tests/VulkanBackendTests.cpp"
Task: "Add failing descriptor update success tests for buffer, texture, sampler, and combined texture-sampler bindings in Tests/VulkanBackendTests.cpp"
Task: "Add failing retained binding query tests after referenced resource invalidation in Tests/VulkanBackendTests.cpp"

# Launch independent public declarations in parallel:
Task: "Implement FVulkanDescriptorPool class declaration with fixed capacity, allocated count, lifecycle state, allocation, release, and exhaustion query APIs in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDescriptorPool.h"
Task: "Implement FVulkanDescriptorSet class declaration with RHI descriptor set interface, bound record query, retained invalid resource reporting, and invalidate API in Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDescriptorSet.h"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational
3. Complete Phase 3: User Story 1
4. Stop and validate: `conda run -n godot scons` and `Build/Mac/Debug/Tests/StonerTest`
5. Confirm buffer/texture creation, invalid description rejection, and real-or-fallback allocation diagnostics are deterministic

### Incremental Delivery

1. Setup + Foundational -> allocation records, allocator limits, diagnostics, and helper scaffolding ready
2. US1 -> buffer and texture resource MVP
3. US2 -> allocation ownership, deterministic failure, cleanup, and shutdown invalidation
4. US3 -> sampler resources
5. US4 -> fixed-capacity descriptor pools and descriptor sets
6. US5 -> CPU-visible upload staging records
7. Polish -> quickstart, roadmap, isolation, and final verification

### Parallel Team Strategy

After Foundation:

- Developer A: US1 buffer/texture resources
- Developer B: US3 sampler resources after device factory shape is stable
- Developer C: US2 allocation limits and cleanup after US1 allocation record shape is stable
- Developer D: US4 descriptor pool/set after US1 and US3 resources are available
- Developer E: US5 upload staging after US1 resource lifecycle is available

## Notes

- `[P]` tasks = different files or independent test additions with no dependency on incomplete implementation tasks.
- `[US#]` label maps tasks to user stories in `specs/010-vulkan-resource-management/spec.md`.
- Real runtime and deterministic fallback allocation are both valid explicit test outcomes.
- Keep all Vulkan-specific resource allocation detail inside `Source/Backend/Vulkan/`.
- Do not implement real command recording, queue execution of uploads, shader compilation, graphics pipelines, compute pipelines, render passes, framebuffers, or render graph scheduling in this feature.

---

## CR-001 Pre-Asset Hardening Amendment (2026-07-26)

- [X] T116 Add checked allocation counters and exact checked texture-footprint calculation in `Source/Backend/Vulkan/Private/FVulkanMemoryAllocator.cpp`
- [X] T117 Replace copyable allocation records with move-only allocator/epoch-bound ownership tickets and restrict buffer/texture construction to `FVulkanDevice`
- [X] T118 Make wrapper tracking rollback-safe and make host-visible fallback upload storage sparse with explicit allocation-failure results
- [X] T119 Add maintained ownership, overflow, footprint, upload, and shutdown accounting regressions in `Tests/VulkanBackendTests.cpp`
- [X] T120 Synchronize the Feature 010 spec, plan, data model, contract, and task history with the CR-001 amendment and pass strict local build/tests
