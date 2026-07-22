# Tasks: Triangle Demo Integration Milestone

**Input**: Design documents from `/specs/018-triangle-demo-integration/`
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`, `contracts/`, `quickstart.md`

**Tests**: The specification explicitly requires deterministic integration, native Vulkan integration, endurance, failure-injection, regression, and cross-platform CI coverage. Test tasks are therefore included and must be written before their corresponding implementation tasks.

**Organization**: Tasks are grouped by user story so each story can be implemented and validated as an incremental milestone. Shared build, runtime-mode, memory, and RHI contracts are blocking foundations.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel because it changes different files and does not depend on unfinished work in the same phase
- **[Story]**: Maps the task to a user story in `spec.md`
- Every task names the exact file or directory it changes

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Add the standalone demo build shape, graphics dependency discovery, and reproducible shader inputs without changing runtime behavior.

- [X] T001 Create the `StonerDemo` executable and reusable demo-runtime library targets, link existing engine layers, and register the demo build from the root in `Demo/StonerDemo/SConscript` and `SConstruct`
- [X] T002 [P] Implement optional GLFW, Vulkan loader/SDK, MoltenVK, GLSL compiler, and SPIR-V validator discovery with controlled unavailable results in `site_scons/GraphicsDependencyDetect.py`
- [X] T003 Integrate graphics dependency results, platform link flags, and checked-in shader-copy/optional compile-and-validate builders into `Demo/StonerDemo/SConscript`
- [X] T004 [P] Add the three-position/three-color triangle GLSL sources and checked-in reproducible SPIR-V payloads in `Demo/StonerDemo/Shaders/Triangle.vert`, `Demo/StonerDemo/Shaders/Triangle.frag`, `Demo/StonerDemo/Shaders/Triangle.vert.spv`, and `Demo/StonerDemo/Shaders/Triangle.frag.spv`
- [X] T005 [P] Create retained-evidence directory guidance without fabricating validation artifacts in `Validation/018/README.md`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Establish backend-neutral runtime proof, process-memory measurement, presentation contracts, and command capabilities used by every story.

**Critical**: No user story implementation begins until these contracts compile and their deterministic contract tests fail for the expected unimplemented behavior.

- [X] T006 [P] Add runtime-mode and stable runtime-snapshot contract tests covering deterministic/native proof and address-free live counts in `Tests/RHICoreTests.cpp`
- [X] T007 [P] Add process resident-memory success and controlled-unavailable tests for supported desktop platforms in `Tests/CorePlatformTests.cpp`
- [X] T008 Add presentation surface, swapchain image, acquire/present synchronization, vertex binding, viewport/scissor, clear-value, and upload contract tests in `Tests/RHICoreTests.cpp`
- [X] T009 Define explicit deterministic/native execution vocabulary and backend-neutral runtime proof in `Source/RHI/Public/RHI/ERHIRuntimeMode.h`, `Source/RHI/Public/RHI/FRHIRuntimeSnapshot.h`, and `Source/RHI/Public/RHI/RHIMinimal.h`
- [X] T010 [P] Implement cross-platform process resident-memory querying behind Core in `Source/Core/Public/Core/FPlatformMemory.h`, `Source/Core/Private/FPlatformMemory.cpp`, `Source/Core/Public/Core/CoreMinimal.h`, and `Source/Core/SConscript`
- [X] T011 Define presentation surface and swapchain descriptions without native API types in `Source/RHI/Public/RHI/FRHIPresentationSurfaceDesc.h`, `Source/RHI/Public/RHI/FRHISwapchainDesc.h`, and `Source/RHI/Public/RHI/IRHIPresentationSurface.h`
- [X] T012 Extend device and swapchain interfaces for explicit runtime mode, surface creation, imported swapchain textures, normalized acquire/present results, and runtime snapshots in `Source/RHI/Public/RHI/IRHIDevice.h`, `Source/RHI/Public/RHI/IRHISwapchain.h`, and `Source/RHI/Public/RHI/ERHIResult.h`
- [X] T013 Extend buffer/resource descriptions and command recording for host upload/copy, vertex-buffer binding, viewport/scissor, and render-pass clear values in `Source/RHI/Public/RHI/FRHIBufferDesc.h`, `Source/RHI/Public/RHI/IRHIBuffer.h`, `Source/RHI/Public/RHI/IRHICommandBuffer.h`, and `Source/RHI/Public/RHI/FRHIRenderPassDesc.h`
- [X] T014 Extend queue submission contracts with wait/signal semaphores and completion fences while preserving deterministic behavior in `Source/RHI/Public/RHI/IRHICommandQueue.h`, `Source/RHI/Public/RHI/IRHISemaphore.h`, and `Source/RHI/Public/RHI/IRHIFence.h`
- [X] T015 Update deterministic RHI implementations and existing mocks for the new contracts so all pre-018 regression tests compile and pass in `Tests/RHICoreTests.cpp`, `Tests/VulkanBackendTests.cpp`, and `Source/Backend/Vulkan/Private/VulkanDevice.cpp`

**Checkpoint**: The project builds with or without native graphics dependencies; public RHI contracts express the complete triangle/presentation path and existing deterministic tests remain green.

---

## Phase 3: User Story 1 - Launch the First Visible Engine Demo (Priority: P1) MVP

**Goal**: Build and launch one standalone native desktop window that presents a stable RGB triangle through Application -> Renderer/render graph -> RHI -> Vulkan, rejecting deterministic fallback as visible success.

**Independent Test**: On either Windows or macOS, launch `StonerDemo --mode interactive`, verify one non-degenerate RGB-interpolated triangle is first presented within 5,000 milliseconds, inspect the log for native runtime proof and measured startup duration, then close normally. This proves the functional MVP on one platform; full US1 completion remains gated by both-platform evidence in US4. Deterministic integration tests independently verify the same stage ordering without claiming pixels.

### Tests for User Story 1

- [ ] T016 [P] [US1] Add failing private-driver tests for native-window selection, opaque platform handle availability, framebuffer pixel extent, callback translation, Escape, and dependency-unavailable behavior in `Tests/ApplicationWindowInputTests.cpp`
- [X] T017 [P] [US1] Add failing native Vulkan tests for real instance/device selection, runtime proof, buffers, shader modules, graphics pipeline, offscreen target, commands, synchronization, and final-zero snapshots in `Tests/VulkanNativeIntegrationTests.h` and `Tests/VulkanNativeIntegrationTests.cpp`
- [X] T018 [P] [US1] Add failing Renderer executor tests for imported output resolution, per-pass interleaved transitions, clear/bind/viewport/scissor/three-vertex draw order, and invalid binding rejection in `Tests/RendererForwardPipelineTests.cpp`
- [ ] T019 [P] [US1] Add failing demo contract tests for native-required rejection of fallback, triangle geometry/shader validation, initialization order, one-frame order, injected-clock 5,000-millisecond first-present boundary, and normal shutdown in `Tests/TriangleDemoIntegrationTests.h` and `Tests/TriangleDemoIntegrationTests.cpp`
- [X] T020 [US1] Register the new test suites in `Tests/Main.cpp` and `Tests/SConscript`, sharing demo runtime sources without linking `Demo/StonerDemo/Private/Main.cpp`

### Implementation for User Story 1

- [X] T021 [P] [US1] Replace the GLFW placeholder with a private GLFW 3.4 driver that owns initialization/window lifetime, uses `GLFW_NO_API`, translates callbacks, reports framebuffer extent, and exposes only `FPlatformWindow` in `Source/Application/Private/FGlfwWindowDriver.cpp` and `Source/Application/Private/FWindowDriver.h`
- [X] T022 [US1] Add explicit real/headless driver selection and real-window lifecycle accessors while preserving deterministic defaults in `Source/Application/Public/Application/FWindow.h`, `Source/Application/Private/FWindow.cpp`, and `Source/Application/SConscript`
- [X] T023 [P] [US1] Add shared native Vulkan parent ownership, loader capability state, portability-enumeration setup, and stable live-object accounting in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanNativeContext.h`, `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp`, and `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanRuntimeSnapshot.h`
- [X] T024 [US1] Implement native instance, physical-device, queue-family, logical-device, extension, and MoltenVK portability selection behind explicit runtime mode in the consolidated native facade at `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanNativeContext.h` and `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp`
- [X] T025 [US1] Implement Application-window-backed native Vulkan surface ownership and compatibility checks without exposing native handles upward in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanNativeContext.h` and `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp`
- [X] T026 [US1] Implement native buffer allocation/upload and imported/native color texture ownership while retaining deterministic paths in `Source/Backend/Vulkan/Private/FVulkanMemoryAllocator.cpp`, `Source/Backend/Vulkan/Private/FVulkanResourceAllocation.cpp`, `Source/Backend/Vulkan/Private/FVulkanBuffer.cpp`, `Source/Backend/Vulkan/Private/FVulkanUploadStaging.cpp`, and `Source/Backend/Vulkan/Private/FVulkanTexture.cpp`
- [X] T027 [P] [US1] Implement strict checked-in SPIR-V loading, magic/alignment/stage/entry-point validation, native shader modules, pipeline layouts, render passes, framebuffers, and graphics pipelines in `Source/Backend/Vulkan/Private/FVulkanShaderModule.cpp`, `Source/Backend/Vulkan/Private/FVulkanPipelineLayout.cpp`, `Source/Backend/Vulkan/Private/FVulkanRenderPass.cpp`, `Source/Backend/Vulkan/Private/FVulkanFramebuffer.cpp`, and `Source/Backend/Vulkan/Private/FVulkanGraphicsPipeline.cpp`
- [X] T028 [US1] Implement native command pools/buffers, interleaved barriers, render scope, viewport/scissor, vertex binding, draw, semaphore/fence waits and signals, queue submit, and idle coordination in `Source/Backend/Vulkan/Private/FVulkanCommandPool.cpp`, `Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp`, `Source/Backend/Vulkan/Private/FVulkanCommandSubmission.cpp`, `Source/Backend/Vulkan/Private/FVulkanSemaphore.cpp`, `Source/Backend/Vulkan/Private/FVulkanFence.cpp`, and `Source/Backend/Vulkan/Private/FVulkanQueue.cpp`
- [X] T029 [US1] Implement native swapchain creation, selected format/extent/image import, synchronized acquire/submit/present, and normalized native results in the consolidated native facade at `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp`
- [X] T030 [US1] Expose native visible and native-headless execution through `FVulkanNativeContext` and the backend build without leaking Vulkan types into RHI in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanNativeContext.h`, `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp`, and `Source/Backend/Vulkan/SConscript`
- [X] T031 [P] [US1] Define the backend-neutral forward execution input/result and frame-context binding contract in `Source/Renderer/Public/Renderer/FForwardFrameExecutor.h` and `Source/Renderer/Public/Renderer/RendererMinimal.h`
- [X] T032 [US1] Implement `FForwardFrameExecutor` to validate the frame plan, resolve the imported output, interleave render-graph transitions per pass, and record the triangle draw through RHI in `Source/Renderer/Private/FForwardFrameExecutor.cpp` and `Source/Renderer/SConscript`
- [X] T033 [P] [US1] Define demo modes, lifecycle/frame stages, result mapping, immutable configuration defaults, stable diagnostics, and the Demo Application lifecycle Composite over independently owned triangle resources, presentation state, and frame-context children in `Demo/StonerDemo/Private/FDemoConfiguration.h`, `Demo/StonerDemo/Private/FDemoDiagnostics.h`, and `Demo/StonerDemo/Private/FStonerDemoApplication.h`
- [ ] T034 [US1] Complete strict CLI parsing for interactive/native startup plus triangle payload validation, two-frame default coordination, forward-plan-to-native-RHI binding, native initialization, monotonic first-present measurement, visible frame loop, Escape/close handling, and Composite reverse-order shutdown in `Demo/StonerDemo/Private/FStonerDemoApplication.cpp`
- [X] T035 [US1] Add the minimal composition-root entry point, stable exit-code mapping, and no silent native-to-deterministic fallback in `Demo/StonerDemo/Private/Main.cpp`
- [X] T036 [US1] Run the deterministic US1 suites and a local native macOS interactive smoke build, recording only reproducible observations and unresolved environment limitations in `specs/018-triangle-demo-integration/quickstart.md`

**Checkpoint**: The US1 functional MVP is independently demonstrable on one supported display platform, and deterministic/native tests distinguish modeled execution from real backend execution. US1 is not fully complete until T059 and T060 provide both-platform timing and visual evidence.

---

## Phase 4: User Story 2 - Keep Rendering Through Window Changes (Priority: P2)

**Goal**: Preserve a valid, responsive frame lifecycle through resize, zero drawable extent, minimize/restore, and recoverable stale presentation.

**Independent Test**: Run 20 resize/minimize/restore cycles; events continue while paused, no acquire/submit/present occurs at zero extent, each non-zero return creates one new presentation generation, and the triangle resumes within two seconds with no stale-resource use.

### Tests for User Story 2

- [X] T037 [P] [US2] Add deterministic injected-clock tests for zero-extent startup, paused event/exit handling, restore, 20 resize cycles, per-cycle 2,000-millisecond recovery boundaries, generation invalidation, and no draw submission while paused in `Tests/TriangleDemoIntegrationTests.cpp`
- [ ] T038 [P] [US2] Add Vulkan swapchain tests for out-of-date/suboptimal acquire/present normalization, old-generation invalidation, image-indexed synchronization, and partial recreation failure in `Tests/VulkanBackendTests.cpp`
- [ ] T039 [P] [US2] Add real-window event tests for logical-size versus framebuffer-size changes, minimize/restore, close while paused, and callback ordering in `Tests/ApplicationWindowInputTests.cpp`

### Implementation for User Story 2

- [X] T040 [P] [US2] Complete framebuffer-size, iconify, restore, focus, and close callback translation without blocking native event polling in `Source/Application/Private/FGlfwWindowDriver.cpp` and `Source/Application/Private/FWindow.cpp`
- [X] T041 [US2] Add presentation generation tracking, stale-image invalidation, wait-idle recreation, format-dependent object rebuild, and zero-extent unavailable state in `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp` and `Demo/StonerDemo/Private/FStonerDemoApplication.cpp`
- [X] T042 [US2] Implement `Running -> PresentationPaused -> RecreatingPresentation -> Running` coordination, acquired-image discard, resize counters, monotonic restored-extent-to-first-replacement-present timing, and recoverable-result handling in `Demo/StonerDemo/Private/FStonerDemoApplication.cpp`
- [ ] T043 [US2] Execute deterministic recovery tests and a local macOS 20-cycle resize/minimize/restore smoke, asserting and documenting every recovery duration is no greater than 2,000 milliseconds in `specs/018-triangle-demo-integration/quickstart.md`

**Checkpoint**: US2 can be validated independently using deterministic event injection and one real display smoke without restarting the process.

---

## Phase 5: User Story 3 - Exit Cleanly and Diagnose Failures (Priority: P2)

**Goal**: Provide deterministic bounded execution, stable first-failure diagnostics, endurance memory/resource gates, and dependency-safe cleanup for normal and partial-failure paths.

**Independent Test**: Run bounded headless validation plus injected failures at every documented startup/frame/report stage; verify exact exit categories, byte-stable primary diagnostics, configured frame accounting, bounded post-warm-up RSS growth, and final zero live resources.

### Tests for User Story 3

- [X] T044 [US3] Add configuration and exit-code tests for defaults, all CLI options, malformed/unknown values, invalid extents, zero budgets, warm-up/sample constraints, and unwritable output paths in `Tests/TriangleDemoIntegrationTests.cpp`
- [X] T045 [US3] Add failure-injection tests for window, runtime, shader, upload, pipeline, acquire, record, submit, present, memory, report, shutdown, and first-failure ownership in `Tests/TriangleDemoIntegrationTests.cpp`
- [X] T046 [US3] Add validation-monitor tests for warm-up exclusion, sampling cadence, first/final five medians, absolute/percentage limits, insufficient samples, bounded peak counts, final-zero gates, and presentation timing fields in `Tests/TriangleDemoIntegrationTests.cpp`
- [X] T047 [US3] Add 20-run byte-stability tests for deterministic frame-stage ordering, result categories, normalized diagnostics, timing fields, and validation report output in `Tests/TriangleDemoIntegrationTests.cpp`

### Implementation for User Story 3

- [X] T048 [P] [US3] Implement canonical CLI modes, profile defaults, validation rules, and exit-code mapping in `Demo/StonerDemo/Private/FDemoConfiguration.cpp`, `Demo/StonerDemo/Private/FDemoConfiguration.h`, and `Demo/StonerDemo/Private/Main.cpp`
- [X] T049 [P] [US3] Implement monotonic stable diagnostics, primary-failure ownership, native-address redaction, stage/result normalization, and deterministic formatting in `Demo/StonerDemo/Private/FDemoDiagnostics.cpp` and `Demo/StonerDemo/Private/FDemoDiagnostics.h`
- [X] T050 [P] [US3] Implement memory sampling, median/growth calculations, first-present and per-recovery timing gates, peak/final runtime snapshots, resource-lifecycle gates, validation report serialization, and write-failure handling in `Demo/StonerDemo/Private/FDemoValidationMonitor.cpp` and `Demo/StonerDemo/Private/FDemoValidationMonitor.h`
- [X] T051 [US3] Integrate bounded `headless`, `validate`, and `headless-vulkan` termination, failure injection seams, sample collection, idempotent partial shutdown, final snapshot, report writing, and first-error exit behavior in `Demo/StonerDemo/Private/FStonerDemoApplication.cpp`
- [X] T052 [US3] Run the 4,096-frame deterministic endurance profile and all US3 failure suites locally, retaining generated logs only under the build tree and updating commands/results in `specs/018-triangle-demo-integration/quickstart.md`

**Checkpoint**: US3 independently proves bounded success, deterministic failure diagnosis, leak gates, and clean shutdown without requiring visible presentation.

---

## Phase 6: User Story 4 - Validate the Integration Across Supported Platforms (Priority: P3)

**Goal**: Build and run deterministic validation on Windows/macOS/Linux, execute real no-window Vulkan through Linux Lavapipe, and retain human-confirmed visible evidence for Windows and macOS.

**Independent Test**: The CI matrix builds `StonerDemo` and passes all deterministic/regression tests on three platforms; Linux additionally completes a native software-Vulkan report; retained Windows/macOS screenshot-log pairs share run IDs and satisfy the evidence contract.

### Tests for User Story 4

- [X] T053 [P] [US4] Add a native headless Vulkan integration case that proves software-device selection and executes real buffer upload, shader/pipeline, offscreen target, command submit/fence wait, and final-zero shutdown without surface/swapchain creation in `Tests/VulkanNativeIntegrationTests.cpp`
- [X] T054 [P] [US4] Add validation-report conformance checks for platform/mode proof, software-device marker, evidence run ID, first-present and per-recovery timing limits, forbidden native addresses, and required fields in `Tests/TriangleDemoIntegrationTests.cpp`

### Implementation for User Story 4

- [X] T055 [US4] Complete `NativeHeadless` offscreen color-target execution and ensure it cannot emit surface, swapchain, screenshot, or visible-success claims in `Demo/StonerDemo/Private/FStonerDemoApplication.cpp` and `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
- [X] T056 [P] [US4] Add reusable CI commands for deterministic and native-headless profiles, explicit Lavapipe ICD resolution, and validation-log artifact collection in `.github/scripts/run_triangle_demo_validation.py`
- [X] T057 [US4] Update the Windows/macOS/Linux GitHub Actions matrix to build `StonerDemo`, run all regressions and deterministic validation, provision Linux Vulkan/Mesa/glslang tools, execute Lavapipe native headless validation, and upload success/failure reports in `.github/workflows/ci.yml`
- [ ] T058 [US4] Run or inspect the complete GitHub Actions matrix and record the commit/run identity plus deterministic and Linux Lavapipe outcomes in `Validation/018/completion.md`
- [ ] T059 [US4] Perform the required 10,000-frame macOS real-window validation, confirm first presentation within 5,000 milliseconds, execute 20 resize/minimize/restore cycles each recovering within 2,000 milliseconds, confirm RGB triangle pixels, and retain the same-run screenshot and normalized timing log in `Validation/018/macOS/triangle.png` and `Validation/018/macOS/triangle.log`
- [X] T060 [US4] Perform the required 10,000-frame Windows real-window validation, confirm first presentation within 5,000 milliseconds, execute 20 resize/minimize/restore cycles each recovering within 2,000 milliseconds, confirm RGB triangle pixels, and retain the same-run screenshot and normalized timing log in `Validation/018/Windows/triangle.png` and `Validation/018/Windows/triangle.log`
- [ ] T061 [US4] Verify both visible evidence pairs, Linux report, and CI identity against the validation contract and finalize references with no accepted gaps in `Validation/018/completion.md`

**Checkpoint**: US4 and Feature 018 are complete only after CI is green and both real-window evidence pairs exist and pass manual review.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Close documentation, architecture, naming, regression, and reproducibility requirements across all stories.

- [X] T062 [P] Update the roadmap milestone status and Feature 018 implementation notes in `doc/roadmap.md`
- [X] T063 [P] Create the Feature 018 system-design summary following the established HTML structure and `SYSTEM_DESIGN.MD` rules in `doc/018-triangle-demo-integration.html`
- [X] T064 [P] Update active technology, recent-change, current-plan, runtime-mode, CI, and evidence memory after implementation in `AGENTS.md`
- [X] T065 Audit Strategy and lifecycle Composite boundaries, public headers for raw `Vk*`/GLFW types, normalized output for native addresses, backend downcasts, and naming violations; fix any findings in `Source/Application/Public/`, `Source/Renderer/Public/`, `Source/RHI/Public/`, and `Demo/StonerDemo/Private/`
- [X] T066 Run the full local macOS build, `StonerTest`, deterministic 4,096-frame validation, shader verification, boundary scans, and `git diff --check`, then synchronize verified commands and outcomes in `specs/018-triangle-demo-integration/quickstart.md`
- [X] T067 Reconcile every FR-001 through FR-021 and SC-001 through SC-009 with implementation/tests/evidence, mark all completed tasks, and record any genuine external blocker in `specs/018-triangle-demo-integration/tasks.md` and `Validation/018/completion.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: Starts immediately; T003 depends on T001 and T002.
- **Foundational (Phase 2)**: Depends on Setup; tests T006-T008 precede contracts T009-T014, and T015 reconciles all deterministic implementations. This phase blocks every user story.
- **US1 (Phase 3)**: Depends on Foundational and delivers the MVP. Tests T016-T020 must fail for the intended missing behavior before T021-T035 implement it; T036 validates the story.
- **US2 (Phase 4)**: Depends on the US1 frame/presentation path. Tests T037-T039 precede T040-T042; T043 validates recovery independently.
- **US3 (Phase 5)**: Depends on the US1 application lifecycle and foundational memory/runtime snapshots, but can proceed in parallel with US2 after US1. Tests T044-T047 precede T048-T051; T052 validates bounded/failure behavior.
- **US4 (Phase 6)**: Depends on US1 native execution and US3 validation reports; CI configuration may begin after Foundational, but T058-T061 require US1-US3 complete.
- **Polish (Phase 7)**: Depends on all desired stories. T067 cannot complete until CI and both manual evidence pairs pass.

### User Story Dependency Graph

```text
Setup -> Foundational -> US1 (visible/native MVP)
                           |-> US2 (presentation recovery) --|
                           |-> US3 (diagnostics/endurance) ---|-> US4 (platform validation) -> Polish
```

### Parallel Opportunities

- T002, T004, and T005 can proceed alongside T001 before T003 integrates the build.
- Foundational memory test T007 can proceed independently; T006 and T008 are sequential because both edit `Tests/RHICoreTests.cpp`; T010 can proceed alongside RHI contracts T009 and T011 after their tests fail as expected.
- US1 tests T016-T019 target separate suites. After native context T023, resource work T026, shader/pipeline work T027, and Renderer contract T031 can proceed in parallel before command/demo integration.
- US2 tests T037-T039 are independent; Application callback work T040 can proceed alongside backend recreation T041.
- US3 tests T044-T047 are sequential because they share `Tests/TriangleDemoIntegrationTests.cpp`; implementations T048-T050 touch separable responsibilities and can proceed in parallel before T051 integrates them.
- US4 tests T053-T054 and CI helper T056 can proceed in parallel; Windows and macOS evidence tasks T059-T060 are independent platform runs.
- Documentation tasks T062-T064 can proceed in parallel after implementation behavior stabilizes.

## Parallel Examples

### User Story 1

```text
Task T016: Application native-window tests in Tests/ApplicationWindowInputTests.cpp
Task T017: Native backend tests in Tests/VulkanNativeIntegrationTests.cpp
Task T018: Renderer executor tests in Tests/RendererForwardPipelineTests.cpp
Task T019: Demo orchestration tests in Tests/TriangleDemoIntegrationTests.cpp
```

### User Story 2

```text
Task T040: GLFW framebuffer/minimize callback implementation
Task T041: Vulkan presentation-generation recreation implementation
```

### User Story 3

```text
Task T048: Configuration and exit-code implementation
Task T049: Stable diagnostics implementation
Task T050: Validation monitor and report implementation
```

### User Story 4

```text
Task T053: Linux native-headless Vulkan test
Task T054: Validation report conformance test
Task T056: Cross-platform CI validation helper
```

## Implementation Strategy

### MVP First: US1

1. Complete Setup and Foundational contracts while preserving all existing tests.
2. Write and observe the US1 tests failing for native behavior.
3. Implement the private GLFW/native Vulkan/RHI/Renderer path and demo composition root.
4. Run deterministic tests and one local macOS native smoke.
5. Stop at the US1 checkpoint if a smaller reviewable milestone is needed.

### Incremental Delivery

1. **Foundation**: Explicit modes, memory measurement, and minimal RHI execution/presentation vocabulary.
2. **US1**: Real visible RGB triangle with strict native proof.
3. **US2**: Resize/minimize/recovery without invalid reuse.
4. **US3**: Bounded modes, deterministic failures, memory/resource gates, clean partial shutdown.
5. **US4**: Three-platform CI, Linux Lavapipe, and retained Windows/macOS presentation evidence.
6. **Polish**: Architecture audit, system documentation, full traceability, and final verification.

### Completion Discipline

- Keep deterministic mode explicit; never convert a missing native dependency into visible success.
- Write each listed test before its corresponding implementation and confirm it fails for the intended reason.
- Commit after a coherent task group using the project's conventional commit style.
- Generated build logs remain under `Build/`; only contract-required validation evidence belongs in `Validation/018/`.
- T059 and T060 require human observation on their respective platforms and cannot be marked complete from headless or simulated output.
