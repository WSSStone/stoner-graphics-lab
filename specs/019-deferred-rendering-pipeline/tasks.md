# Tasks: Deferred Rendering Pipeline

**Input**: Design documents from `/specs/019-deferred-rendering-pipeline/`
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`, `contracts/`, `quickstart.md`

**Tests**: The specification explicitly requires deterministic frame/graph/executor tests, real Vulkan offscreen readback, semantic probe tolerances, failure/cleanup coverage, regression validation, and cross-platform CI. Test tasks are included and must be written before their corresponding implementation tasks.

**Organization**: Tasks are grouped by user story. Shared shader-build and RHI command capabilities are blocking foundations. US1 delivers the executable offscreen deferred MVP; US2 adds scalable local-light behavior and comparison evidence; US3 completes diagnostics and failure ownership; US4 closes forward coexistence and platform validation.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel because it changes different files and does not depend on unfinished work in the same phase
- **[Story]**: Maps the task to a user story in `spec.md`
- Every task names the exact file or directory it changes

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Prepare reproducible Renderer-owned shader assets and retained Feature 019 validation locations without changing runtime behavior.

- [ ] T001 Add checked-in deferred GLSL/SPIR-V discovery, optional compile/validation, and asset-copy builders to `Source/Renderer/SConscript`
- [ ] T002 [P] Document generated-versus-retained evidence rules, required Linux artifact names, and no-screenshot policy in `Validation/019/README.md`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Add the general RHI command vocabulary required by all deferred stories before introducing Renderer behavior.

**Critical**: No user-story implementation begins until the command contracts compile, deterministic mocks support them, and the new contract tests fail only for intentionally missing deferred behavior.

- [ ] T003 Add failing RHI contract tests for descriptor-set binding, index-buffer binding, explicit render-pass clear values, texture-to-buffer copy validation, and symbolic command records in `Tests/RHICoreTests.cpp`
- [ ] T004 [P] Add failing Vulkan command tests for descriptor/index binding, clear-value compatibility, copy usages/ranges, lifecycle rejection, and deterministic diagnostics in `Tests/VulkanBackendTests.cpp`
- [ ] T005 Define backend-neutral index type and texture-to-buffer copy region contracts in `Source/RHI/Public/RHI/ERHIIndexType.h` and `Source/RHI/Public/RHI/FRHITextureBufferCopyRegion.h`
- [ ] T006 Extend symbolic command types and `IRHICommandBuffer` with descriptor-set binding, index-buffer binding, clear-value render-pass begin, and texture-to-buffer copy operations in `Source/RHI/Public/RHI/IRHICommandBuffer.h` and `Source/RHI/Public/RHI/RHIMinimal.h`
- [ ] T007 Update deterministic test doubles for the new pure-virtual commands and stable validation behavior in `Tests/RHICoreTests.cpp`, `Tests/VulkanBackendTests.cpp`, and `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp`
- [ ] T008 Implement deterministic Vulkan descriptor/index/clear/readback command validation and symbolic records in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanCommandBuffer.h` and `Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp`
- [ ] T009 Run the full local build and `StonerTest` to prove the RHI additions preserve existing contracts, recording any required compatibility adjustment in `specs/019-deferred-rendering-pipeline/quickstart.md`

**Checkpoint**: Shared RHI contracts express every command required by deferred execution, all existing mocks compile, and the pre-019 regression suite remains green.

---

## Phase 3: User Story 1 - Render Opaque Scenes Through Deferred Shading (Priority: P1) MVP

**Goal**: Prepare and execute one complete deferred frame with surface data, directional/point/spot lighting, composition, and real Linux Vulkan offscreen pixel readback through Renderer -> RHI -> Vulkan.

**Independent Test**: Submit a fixed view with opaque/masked materials and one light of each supported type; verify the deterministic surface/pass/resource plan, execute it offscreen through real RHI bindings on Lavapipe, and confirm at least 12 named intermediate/final probes satisfy their semantic thresholds without using forward opaque lighting.

### Tests for User Story 1

- [ ] T010 [US1] Add failing surface-layout, view/output, opaque/masked material, empty/ambient/emissive, canonical pass-order, and deterministic plan tests in `Tests/DeferredRenderingTests.h` and `Tests/DeferredRenderingTests.cpp`
- [ ] T011 [P] [US1] Add failing real-runtime, surface attachment, shader/pipeline, submit/fence, texture-to-buffer readback, at-least-12-probe, LDR `2/255`, depth `1e-4`, normal-dot `0.999`, scalar `1e-3`, non-finite rejection, and final-zero tests in `Tests/DeferredNativeIntegrationTests.h` and `Tests/DeferredNativeIntegrationTests.cpp`
- [ ] T012 [US1] Extend `Tests/DeferredRenderingTests.cpp` with failing executor binding and command-order cases for surface, directional, point, spot, composition, and validation-readback stages
- [ ] T013 [US1] Register the deferred deterministic and native suites in `Tests/Main.cpp` and `Tests/SConscript`

### Implementation for User Story 1

- [ ] T014 [P] [US1] Define stable deferred result, stage, severity, subject, and diagnostic record vocabulary in `Source/Renderer/Public/Renderer/FDeferredDiagnostics.h` and `Source/Renderer/Private/FDeferredDiagnostics.cpp`
- [ ] T015 [P] [US1] Implement the three-color-plus-depth semantic layout, compatibility identity, ranges, clear values, and validation in `Source/Renderer/Public/Renderer/FDeferredSurfaceData.h` and `Source/Renderer/Private/FDeferredSurfaceData.cpp`
- [ ] T016 [P] [US1] Define directional, point, and spot input/acceptance records with finite-value, range, direction, and cone validation in `Source/Renderer/Public/Renderer/FDeferredLightData.h` and `Source/Renderer/Private/FDeferredLightData.cpp`
- [ ] T017 [US1] Define deferred configuration, frame inputs, draw records, pass records, input fingerprints, and valid/invalid frame-plan state in `Source/Renderer/Public/Renderer/FDeferredFramePlan.h` and `Source/Renderer/Private/FDeferredFramePlan.cpp`
- [ ] T018 [US1] Implement `FDeferredRenderer` preparation for opaque/masked materials, shared PBR semantics, ambient/emissive/no-light outcomes, one surface sequence, supported light records, and exactly one composition output in `Source/Renderer/Public/Renderer/FDeferredRenderer.h` and `Source/Renderer/Private/FDeferredRenderer.cpp`
- [ ] T019 [US1] Translate valid deferred plans into explicit surface, lighting, composition, and readback render-graph resources/accesses/dependencies/culling decisions in `Source/Renderer/Public/Renderer/FDeferredRenderGraphDeclaration.h` and `Source/Renderer/Private/FDeferredRenderGraphDeclaration.cpp`
- [ ] T020 [US1] Implement transactional RHI binding validation, interleaved graph transitions, render scopes, descriptor/index/geometry binding, stage draws, composition, and readback recording in `Source/Renderer/Public/Renderer/FDeferredFrameExecutor.h` and `Source/Renderer/Private/FDeferredFrameExecutor.cpp`
- [ ] T021 [P] [US1] Add reviewable surface, fullscreen, directional, point, spot, and composition GLSL plus checked-in matching SPIR-V payloads in `Source/Renderer/Shaders/Deferred/`
- [ ] T022 [US1] Implement reusable native offscreen lifecycle ownership for images, buffers, descriptors, shaders, pipelines, render passes, framebuffers, commands, fences, and staging readback in `Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.h` and `Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp`
- [ ] T023 [US1] Expose backend-neutral offscreen RHI wrappers, runtime proof, submission completion, mapped readback summaries, and reverse-order shutdown through `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanNativeContext.h` and `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp`
- [ ] T024 [US1] Build the fixed native reference scene and implement at least 12 named probe decoders/assertions using LDR `2/255`, depth `1e-4`, normal-dot `0.999`, metallic/roughness/occlusion `1e-3`, and non-finite failure rules without native-address output in `Tests/DeferredNativeIntegrationTests.cpp`
- [ ] T025 [US1] Run the complete deterministic suite plus Linux Lavapipe native test, then reconcile the MVP commands and expected probe fields in `specs/019-deferred-rendering-pipeline/quickstart.md`

**Checkpoint**: US1 is complete when a valid deferred frame is independently preparable, graph-declared, RHI-recorded, native-submitted offscreen, read back, and semantically validated with zero final deferred objects.

---

## Phase 4: User Story 2 - Scale Local Lighting Independently of Geometry (Priority: P2)

**Goal**: Process every valid view-affecting directional, point, and spot light deterministically, cull only proven outside-view local lights, render bounded sphere/cone volumes, and produce a reproducible forward/deferred comparison baseline.

**Independent Test**: Prepare equivalent scenes at 0, 16, 64, and 256 local-light tiers; verify outside-view/boundary/camera-inside/near-plane classifications, stable identity tie-breaking, constant deferred surface geometry work, complete median/p95 reports after at least 100 measured frames, and rejection of mismatched fingerprints.

### Tests for User Story 2

- [ ] T026 [US2] Add failing no-light-cap, influence-boundary, outside-view, boundary-touching, camera-enclosing, near-plane, equal-key identity, and constant-surface-work tests in `Tests/DeferredRenderingTests.cpp`
- [ ] T027 [P] [US2] Add failing 0/16/64/256-tier, warm-up, 100-sample, median/p95, workload, fingerprint-mismatch, incomplete-run, and crossover-classification tests in `Tests/RendererComparisonTests.h` and `Tests/RendererComparisonTests.cpp`
- [ ] T028 [US2] Register the comparison suite in `Tests/Main.cpp` and `Tests/SConscript`

### Implementation for User Story 2

- [ ] T029 [US2] Implement deterministic frustum intersection, influence keys, stable identity tie-breaking, and outside/camera-inside/near-plane volume classification in `Source/Renderer/Public/Renderer/FDeferredLightVolume.h` and `Source/Renderer/Private/FDeferredLightVolume.cpp`
- [ ] T030 [US2] Integrate uncapped directional/point/spot acceptance, culling records, canonical type order, and volume classifications into `Source/Renderer/Private/FDeferredLightData.cpp` and `Source/Renderer/Private/FDeferredRenderer.cpp`
- [ ] T031 [US2] Add reusable sphere/cone volume geometry, outside/inside cull-depth pipeline variants, bounded scissors, indexed draws, exact range/cone fragment rejection, and additive accumulation to `Source/Renderer/Private/FDeferredFrameExecutor.cpp` and `Source/Renderer/Shaders/Deferred/`
- [ ] T032 [US2] Define normalized scene/view/material/light fingerprints, workload records, timing samples, tier summaries, validity state, and crossover classification in `Source/Renderer/Public/Renderer/FRendererComparisonReport.h` and `Source/Renderer/Private/FRendererComparisonReport.cpp`
- [ ] T033 [US2] Implement equivalent-input validation, warm-up exclusion, median/p95 aggregation, four required tiers, and no-speedup-gate reporting in `Tests/RendererComparisonTests.cpp`
- [ ] T034 [US2] Execute all four comparison tiers and local-light boundary cases, then synchronize exact report fields and interpretation commands in `specs/019-deferred-rendering-pipeline/quickstart.md`

**Checkpoint**: US2 is complete when local-light work is bounded and deterministic at all required edge cases and a valid four-tier comparison report records scaling/crossover without treating timing as a correctness gate.

---

## Phase 5: User Story 3 - Inspect and Diagnose Deferred Frames (Priority: P2)

**Goal**: Produce deterministic, actionable plans/reports and preserve first-failure ownership and zero-live cleanup across invalid input and partial execution failures.

**Independent Test**: Exercise valid and invalid view/output/layout/material/light/binding/graph/record/submit/readback inputs, repeat equivalent preparation 20 times, and confirm the first actionable diagnostic, byte-stable normalized report, stopped dependent stages, idempotent cleanup, and zero final live resources.

### Tests for User Story 3

- [ ] T035 [US3] Add failing invalid-input diagnostic, accepted/rejected count, graph dump, first-error ownership, native-address exclusion, and 20-run byte-stability tests in `Tests/DeferredRenderingTests.cpp`
- [ ] T036 [P] [US3] Add failing partial-initialization, record/submit/fence/copy/map/decode/probe failure, no-later-success, idempotent shutdown, and zero-live tests in `Tests/DeferredNativeIntegrationTests.cpp`

### Implementation for User Story 3

- [ ] T037 [US3] Implement ordered diagnostic aggregation, first-actionable-error ownership, normalized subject/reason formatting, and native-address exclusion in `Source/Renderer/Private/FDeferredDiagnostics.cpp`
- [ ] T038 [US3] Implement the human-readable deferred frame dump with layout, passes, resources, draw/light decisions, composition, transparent handoff, and stable result categories in `Source/Renderer/Private/FDeferredFramePlan.cpp`
- [ ] T039 [US3] Enforce stop-on-first-failure stage state, no dependent success records, stable command counts, and cleanup diagnostics in `Source/Renderer/Private/FDeferredFrameExecutor.cpp`
- [ ] T040 [US3] Implement partial-state-safe reverse-order native release, bounded completion wait, readback decode failure ownership, and final live-object snapshots in `Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp` and `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp`
- [ ] T041 [US3] Run repeated deterministic and injected native failure suites and record normalized report examples and triage guidance in `specs/019-deferred-rendering-pipeline/quickstart.md`

**Checkpoint**: US3 is complete when every required failure identifies one primary stage/subject, no dependent stage claims success, equivalent reports are byte-stable, and cleanup leaves zero live deferred objects.

---

## Phase 6: User Story 4 - Preserve Existing Renderer Workflows (Priority: P3)

**Goal**: Keep forward rendering as the unchanged default, hand transparent draws to the existing forward-transparent path after deferred composition, and validate the feature across the required CI matrix.

**Independent Test**: Select forward and deferred explicitly on equivalent inputs; verify forward produces no deferred resources, transparent ordering remains established after composition, all existing regressions pass, all three CI jobs pass deterministic coverage, and Linux additionally passes Lavapipe readback/comparison artifact generation.

### Tests for User Story 4

- [ ] T042 [US4] Add failing forward-default/non-mutation, explicit deferred selection, shared material semantics, post-composition transparent ordering, and no-deferred-resource forward regression tests in `Tests/RendererForwardPipelineTests.cpp`
- [ ] T043 [P] [US4] Add validation-script tests for profile arguments, missing runtime/report failure, probe/tier parsing, timeout handling, and artifact pass/fail rules in `.github/scripts/test_run_deferred_validation.py`

### Implementation for User Story 4

- [ ] T044 [US4] Implement optional forward-transparent handoff after composition while preserving existing camera-depth/material/object tie-breaking in `Source/Renderer/Private/FDeferredRenderer.cpp` and `Source/Renderer/Private/FDeferredFrameExecutor.cpp`
- [ ] T045 [US4] Add explicit renderer strategy selection coverage and forward/deferred shared-input adapters without changing the forward default in `Source/Renderer/Public/Renderer/FDeferredRenderer.h` and `Tests/RendererForwardPipelineTests.cpp`
- [ ] T046 [US4] Implement deterministic, native-Lavapipe, semantic-probe, comparison-tier, timeout, normalized-report, and failure-artifact orchestration in `.github/scripts/run_deferred_validation.py`
- [ ] T047 [US4] Extend the Windows/macOS/Linux CI matrix with deferred deterministic validation, Linux Lavapipe native readback/comparison, and always-upload Feature 019 artifacts in `.github/workflows/ci.yml`
- [ ] T048 [US4] Run the full local macOS build, complete `StonerTest`, deterministic deferred profile, shader validation when available, and forward/triangle regressions; record verified results in `specs/019-deferred-rendering-pipeline/quickstart.md`
- [ ] T049 [US4] Trigger the feature-branch CI, verify all three deterministic jobs plus Linux native gates, and download exact Linux reports into `Validation/019/Linux/deferred-readback-report.txt` and `Validation/019/Linux/renderer-comparison-report.txt`
- [ ] T050 [US4] Verify CI run/commit identity, artifact digests, probe/tier counts, forward regressions, and final-zero resources against the validation contract in `Validation/019/completion.md`

**Checkpoint**: US4 and the functional feature are complete when forward behavior is unchanged, transparent handoff is correct, the three-platform matrix is green, and retained Linux artifacts satisfy every native and comparison gate.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Close documentation, architecture, naming, reproducibility, and full requirement traceability.

- [ ] T051 [P] Update Deferred Rendering roadmap status and implementation notes after all completion gates pass in `doc/roadmap.md`
- [ ] T052 [P] Create the Feature 019 system-design summary following `doc/SYSTEM_DESIGN.MD` and the established HTML structure in `doc/019-deferred-rendering-pipeline.html`
- [ ] T053 [P] Update Feature 019 active technology, delivered behavior, CI evidence, and current-plan memory after implementation in `AGENTS.md`
- [ ] T054 Audit Strategy/lifecycle Composite boundaries, Renderer/RHI public headers for raw `Vk*` types or backend downcasts, shader/format contracts for API-specific leakage, native reports for addresses, and UE5-style naming; fix findings in `Source/Renderer/`, `Source/RHI/Public/`, and `Source/Backend/Vulkan/`
- [ ] T055 Run shader reproducibility checks, the complete local macOS build/test/profile workflow, deterministic boundary scans, and `git diff --check`, then finalize verified commands in `specs/019-deferred-rendering-pipeline/quickstart.md`
- [ ] T056 Reconcile FR-001 through FR-025 and SC-001 through SC-010 against implementation, tests, CI, and retained artifacts; mark completed work and document any genuine external blocker in `specs/019-deferred-rendering-pipeline/tasks.md` and `Validation/019/completion.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: Starts immediately; T001 and T002 are independent.
- **Foundational (Phase 2)**: Depends on Setup. T003/T004 precede T005-T008; T007 reconciles test doubles after interface changes; T009 validates the foundation. This phase blocks all user stories.
- **US1 (Phase 3)**: Depends on Foundational and delivers the native offscreen MVP. T010-T013 establish failing tests; T014-T016 define independent vocabulary; T017-T020 integrate planning/graph/execution; T021 may proceed alongside Renderer models; T022-T024 implement native resources and probes; T025 validates the story.
- **US2 (Phase 4)**: Depends on US1's valid frame/executor path. T026/T027 precede T029-T033; T028 registers the new suite; T034 validates the full tier matrix.
- **US3 (Phase 5)**: Depends on US1 but can proceed in parallel with US2 after the US1 checkpoint. T035/T036 precede T037-T040; T041 validates failure behavior.
- **US4 (Phase 6)**: Depends on US1-US3 functional behavior and US2 comparison output. T042/T043 precede T044-T047; T048 validates locally; T049/T050 require pushed CI and retained artifacts.
- **Polish (Phase 7)**: Depends on all desired stories. T051-T053 may proceed in parallel only after behavior/evidence stabilizes; T056 is the final traceability gate.

### User Story Dependency Graph

```text
Setup -> Foundational -> US1 (deferred native MVP)
                           |-> US2 (bounded lights + comparison) --|
                           |-> US3 (diagnostics + cleanup) --------|-> US4 (coexistence + CI) -> Polish
```

### Parallel Opportunities

- T002 can proceed alongside T001 because it only creates validation guidance.
- T003 and T004 target separate test suites and may be authored together before the shared interfaces change.
- T011 targets native integration while T010 targets deterministic Renderer behavior.
- T014, T015, and T016 define separate diagnostics/surface/light files after the foundation is stable; T017 begins only after their contracts exist.
- T021 owns shader assets and may proceed alongside C++ frame-model work before pipeline/interface integration.
- T026 and T027 target separate local-light and comparison suites after US1.
- US2 and US3 may proceed in parallel after US1 because their primary production/test files differ; integrations into shared renderer/executor files must be serialized.
- T035 and T036 target deterministic and native failure suites separately.
- T042 and T043 target C++ regression and Python validation-script tests separately.
- T051, T052, and T053 update separate documentation/memory files after completion evidence is stable.

## Parallel Examples

### User Story 1

```text
Task T010: Deterministic surface/frame/graph tests in Tests/DeferredRenderingTests.cpp
Task T011: Native offscreen/readback tests in Tests/DeferredNativeIntegrationTests.cpp
Task T021: Deferred GLSL and checked-in SPIR-V assets under Source/Renderer/Shaders/Deferred/
```

### User Story 2

```text
Task T026: Local-light boundary and stable-order tests in Tests/DeferredRenderingTests.cpp
Task T027: Four-tier comparison contract tests in Tests/RendererComparisonTests.cpp
```

### User Story 3

```text
Task T035: Deterministic diagnostics/report tests in Tests/DeferredRenderingTests.cpp
Task T036: Native partial-failure/cleanup tests in Tests/DeferredNativeIntegrationTests.cpp
```

### User Story 4

```text
Task T042: Forward/deferred coexistence tests in Tests/RendererForwardPipelineTests.cpp
Task T043: CI validation helper tests in .github/scripts/test_run_deferred_validation.py
```

## Implementation Strategy

### MVP First: US1

1. Complete Setup and the general RHI foundation while preserving all regressions.
2. Write and observe US1 deterministic/native tests failing for the intended missing behavior.
3. Implement surface semantics, frame planning, graph declaration, RHI execution, shaders, and native offscreen ownership.
4. Validate at least 12 semantic probes through Lavapipe and final-zero cleanup.
5. Stop at the US1 checkpoint for a reviewable, actually rendered deferred MVP.

### Incremental Delivery

1. **Foundation**: General descriptor/index/clear/readback RHI commands.
2. **US1**: Correct opaque deferred frame through real offscreen Vulkan.
3. **US2**: Uncapped bounded local-light volumes and reproducible comparison baseline.
4. **US3**: Stable diagnostics, deterministic dumps, first-failure ownership, and cleanup.
5. **US4**: Forward coexistence, transparent handoff, three-platform CI, and retained Linux artifacts.
6. **Polish**: Architecture audit, system documentation, full validation, and traceability.

### Completion Discipline

- Write each listed test before its corresponding implementation and confirm it fails for the intended missing behavior.
- Keep forward as the unchanged default; never report deterministic fallback as Linux native success.
- Keep raw Vulkan types and calls inside `Source/Backend/Vulkan/`; Renderer consumes RHI interfaces only.
- Do not add tiled/clustered lighting, shadows, SSAO, SSR, anti-aliasing, editor UI, new backends, or visible screenshot gates.
- Treat performance timings as baseline evidence; only incomplete/non-equivalent reports fail.
- Commit coherent task groups using the project's conventional commit style.
- Generated build outputs remain under `Build/`; only contract-required reports and completion evidence belong in `Validation/019/`.
