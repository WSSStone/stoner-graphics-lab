# Tasks: Renderer HDR Post-Processing & Output Transform

**Input**: Design documents from `/specs/029-hdr-output-transform/`
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`, `contracts/`, `quickstart.md`

**Tests**: The feature specification requires deterministic, native, lifecycle,
evidence, and human-review validation. Every implementation phase therefore
starts with tests that must fail for the intended reason before production code
is added.

**Organization**: Tasks are grouped by user story so each story can be delivered
and tested as an increment. Contract mapping is: output-pipeline to US1/US3,
output-device-profiles to US2, presentation-readback-lifecycle to the foundation
and US4, and validation-evidence to US5.

**Closeout status (2026-09-04)**: 112/118 tasks recorded complete. The four
`1f46352` +3 EV hidden-background HDR runs passed 1,000/20 lifecycle validation;
the maintainer's separate live-review feedback is preserved in
`Validation/029/HDR/README.md`, but T105 still requires the manually authored
attestation. Hosted run 33847099909 failed strict platform-only compilation.
The local portability repair passes its available checks but changes software
inputs; T112 requires a new frozen SHA and passing platform/sanitizer results.
T101/T102/T104 remain records of completed `1f46352` captures, not evidence for
that future SHA. No old evidence or human decision may be automatically promoted
to the new revision. T103/T105/T106/T112/T117/T118 remain open.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: May run in parallel because it changes different files and has no
  dependency on an unfinished task in the same phase.
- **[US1]`...`[US5]**: User story owning the task.
- Every task names the exact file or directory it changes or the evidence file
  it must produce.

---

## Phase 1: Setup & M0 Color Authority (Shared Infrastructure)

**Purpose**: Freeze every color identity, equation, exposure sample, numerical
tolerance, CPU oracle, and expected vector before RHI, Render Graph, or GPU
implementation begins.

- [X] T001 Freeze the canonical seven-profile constants, native encodings, exposure sample set, CPU tolerance, SDR `2/255`, PQ packed-10, and FP16 HDR decoded-domain tolerance policies in `Config/Validation/OutputTransform/Profiles.json`
- [X] T002 [P] Copy the five frozen JSON contract schemas into runtime validation paths under `Config/Validation/OutputTransform/Schemas/`
- [X] T003 [P] Record the exact ACES `v2.0.0+2025.04.04`/`35e1e6a`, submodule, Khronos PBR Neutral, Narkowicz fit, Extended Reinhard, PQ, gamut-matrix, and native-packer authorities in `Tests/Fixtures/OutputTransform/UPSTREAM.md`
- [X] T004 Declare a compileable private oracle interface, then write and register failing `renderer-output-transform-math` double-precision exposure, negative/non-finite, three-SDR-strategy, ACES2-HDR, gamut, transfer, native-quantization tolerance, XYZ propagation, finite sample-set, and no-expected-value-regeneration tests in `Source/Renderer/Private/FOutputTransformReference.h`, `Tests/RendererOutputTransformMathTests.cpp`, `Tests/RendererOutputTransformMathTests.h`, `.github/scripts/test_verify_output_transform_vectors.py`, `Tests/Main.cpp`, and `Tests/SConscript`
- [X] T005 Implement the complete private double-precision CPU conformance oracle behind the T004 seam from the frozen authorities, including all curves, ACES2 viewing presets, output encodings/decodings, negative policy, local native-step calculation, and RGB-to-XYZ tolerance propagation in `Source/Renderer/Private/FOutputTransformReference.cpp`
- [X] T006 Freeze the immutable vector manifest, at least 32 HDR vectors, at least 16 vectors per profile, exact expected values, provenance digests, and the independent no-regeneration verifier before GPU work; run the registered M0 tests and record their result in `Tests/Fixtures/OutputTransform/manifest-v1.json`, `Tests/Fixtures/OutputTransform/vectors-v1.json`, `.github/scripts/verify_output_transform_vectors.py`, and `Validation/029/CI/m0-color-authority.json`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Add the backend-neutral presentation vocabulary and the minimal
typed Render Graph execution contract required by every user story.

**Critical**: M0 from Phase 1 is immutable input to this phase. No user-story
implementation starts until both M0 and the deterministic mock foundation pass.

### Tests for the foundation

- [X] T007 [P] Write failing exact-pair capability, metadata, generation, unsupported-without-fallback, zero-drawable, and stale-image contract tests in `Tests/RHIPresentationOutputTests.cpp` and `Tests/RHIPresentationOutputTests.h`
- [X] T008 [P] Write failing typed-resource, external-side-effect culling, compiled-transition ordering, and schedule-visitor tests in `Tests/RendererRenderGraphTests.cpp`

### Implementation for the foundation

- [X] T009 Add packed 10-bit presentation format support and preserve distinct linear/nonlinear storage semantics in `Source/RHI/Public/RHI/ERHIFormat.h`
- [X] T010 [P] Define backend-neutral native color-space and native-encoding vocabulary in `Source/RHI/Public/RHI/ERHIPresentationColorSpace.h`
- [X] T011 [P] Define versioned HDR metadata, surface capability snapshots, and immutable resolved presentation state in `Source/RHI/Public/RHI/FRHIHDRMetadata.h`, `Source/RHI/Public/RHI/FRHIPresentationCapabilities.h`, and `Source/RHI/Public/RHI/FRHIResolvedPresentationState.h`
- [X] T012 Extend surface creation and dynamic capability-query contracts with monotonic surface generations in `Source/RHI/Public/RHI/FRHIPresentationSurfaceDesc.h` and `Source/RHI/Public/RHI/IRHIPresentationSurface.h`
- [X] T013 Extend swapchain requests, actual resolved-state reporting, transactional reconfiguration, and stale-generation rejection in `Source/RHI/Public/RHI/FRHISwapchainDesc.h` and `Source/RHI/Public/RHI/IRHISwapchain.h`
- [X] T014 Add deterministic SDR-only defaults and explicit Unsupported responses to RHI test doubles in `Tests/RHIPresentationOutputTests.cpp`
- [X] T015 Add typed format, extent, sample-count, usage, and color-domain sidecar data to graph resources in `Source/Renderer/Public/Renderer/FRenderGraphResource.h` and `Source/Renderer/Public/Renderer/FRenderGraphBuilder.h`
- [X] T016 Add explicit external readback/presentation side effects and immutable compiled pass/transition visitation in `Source/Renderer/Public/Renderer/FRenderGraphPass.h`, `Source/Renderer/Public/Renderer/FRenderGraphCompiler.h`, and `Source/Renderer/Public/Renderer/FRenderGraphExecutor.h`
- [X] T017 Implement typed compatibility validation, culling protection, and interleaved schedule execution without introducing a second graph in `Source/Renderer/Private/FRenderGraphCompiler.cpp` and `Source/Renderer/Private/FRenderGraphExecutor.cpp`
- [X] T018 Register `rhi-presentation-output` and retain the extended `renderer-render-graph` suite in `Tests/Main.cpp`, including their source visibility in `Tests/SConscript`
- [X] T019 Make the new RHI and Render Graph tests pass and record normalized deterministic results in `Validation/029/CI/foundation.json`

**Checkpoint**: Exact presentation pairs and real typed graph scheduling are
available without Renderer color policy or native-backend policy leakage.

---

## Phase 3: User Story 1 - Produce One Formal Display Output (Priority: P1) MVP

**Goal**: Forward and Deferred both hand one canonical RGBA16F linear
Rec.709/D65 SceneColor to one backend-neutral output pipeline that publishes
exactly one formal display output.

**Independent Test**: Submit equivalent bounded Forward and Deferred frames;
each must declare the canonical stages and one formal output. Invalid metadata,
missing resources, duplicate writers, and stale frame identity must fail before
native work or publication.

### Tests for User Story 1

- [X] T020 [P] [US1] Write failing handoff, stage-order, single-writer, invalid-input, and no-partial-publication tests in `Tests/RendererOutputTransformTests.cpp` and `Tests/RendererOutputTransformTests.h`
- [X] T021 [P] [US1] Write failing Forward RGBA16F handoff and formal-output ownership regressions in `Tests/RendererForwardPipelineTests.cpp`
- [X] T022 [P] [US1] Write failing Deferred composition-to-RGBA16F handoff and shared-policy regressions in `Tests/DeferredRenderingTests.cpp`
- [X] T023 [P] [US1] Write failing real typed output-subgraph, execution-order, empty-insertion maximum-three-fullscreen-pass, optional-GPU-copy, presentation-only-zero-CPU-readback, bounded-resource, and constant-full-image-visit tests in `Tests/RendererPostProcessGraphTests.cpp` and `Tests/RendererPostProcessGraphTests.h`

### Implementation for User Story 1

- [X] T024 [P] [US1] Define the immutable canonical SceneColor endpoint and state transitions in `Source/Renderer/Public/Renderer/FHDRSceneColorHandoff.h`
- [X] T025 [P] [US1] Define authored/resolved output settings, the default SDR policy identity, and request invariants in `Source/Renderer/Public/Renderer/FOutputTransformSettings.h`
- [X] T026 [P] [US1] Define immutable plan, stage, output descriptor, fingerprint, and single formal-output identity in `Source/Renderer/Public/Renderer/FOutputTransformPlan.h` and `Source/Renderer/Public/Renderer/FOutputTransformGraphDeclaration.h`
- [X] T027 [P] [US1] Define orthogonal preparation, execution-result, and bounded diagnostic interfaces in `Source/Renderer/Public/Renderer/FHDRPostProcessPipeline.h`, `Source/Renderer/Public/Renderer/FOutputTransformExecutor.h`, and `Source/Renderer/Public/Renderer/FOutputTransformDiagnostics.h`
- [X] T028 [US1] Implement fail-closed handoff and settings validation, including finite RGBA16F linear Rec.709/D65, opaque alpha, exact extent, `sampleCount=1`, and explicit output requirement checks in `Source/Renderer/Private/FOutputTransformSettingsValidator.cpp`
- [X] T029 [US1] Implement plan preparation, canonical stage ordering, fingerprints, and single-writer validation in `Source/Renderer/Private/FHDRPostProcessPipeline.cpp`
- [X] T030 [US1] Declare exposure, tone-map, output encoding, formal readback, and presentation passes through the existing graph in `Source/Renderer/Private/FOutputTransformGraphBuilder.cpp`
- [X] T031 [US1] Implement schedule-driven RHI binding, publication only after all requested terminal operations, stable first-failure handling, and zero CPU readback initiation on presentation-only frames in `Source/Renderer/Private/FOutputTransformExecutor.cpp`
- [X] T032 [P] [US1] Add the fullscreen vertex shader and initial Khronos-PBR-Neutral-to-sRGB output program in `Content/Shaders/PostProcess/Fullscreen.vert`, `Content/Shaders/PostProcess/OutputTransform.frag`, and `Content/Shaders/PostProcess/OutputTransform.shader.json`
- [X] T033 [US1] Offline-compile and validate checked-in shader payloads and stage them through `Content/Shaders/PostProcess/Fullscreen.vert.spv`, `Content/Shaders/PostProcess/OutputTransform.frag.spv`, and `Source/Renderer/SConscript`
- [X] T034 [US1] Replace Forward's formal RGBA8 target with the typed RGBA16F handoff while preserving opaque/transparent order in `Source/Renderer/Public/Renderer/FForwardFramePlan.h`, `Source/Renderer/Public/Renderer/FForwardRenderGraphDeclaration.h`, and `Source/Renderer/Private/FForwardRenderer.cpp`
- [X] T035 [US1] Replace Deferred's RGBA8 composition authority with the typed RGBA16F handoff while preserving lighting/composition order in `Source/Renderer/Public/Renderer/FDeferredRenderGraphDeclaration.h`, `Source/Renderer/Private/FDeferredRenderer.cpp`, and `Content/Shaders/Deferred/Composition.frag`
- [X] T036 [US1] Route both strategies through the same output pipeline and remove renderer-specific formal-output publication in `Source/Renderer/Private/FForwardFrameExecutor.cpp` and `Source/Renderer/Private/FDeferredFrameExecutor.cpp`
- [X] T037 [US1] Register `renderer-output-transform` and `renderer-post-process-graph`, run the US1 suites, and record formal-output, pass/resource-count, full-image-visit, CPU-readback-count, and non-qualifying elapsed-time observations in `Tests/Main.cpp`, `Tests/SConscript`, and `Validation/029/CI/us1-formal-output.json`

**Checkpoint**: User Story 1 is independently usable as the MVP with the
explicit default SDR transform; Forward and Deferred no longer own competing
formal output paths.

---

## Phase 4: User Story 2 - Control Exposure and Output Appearance (Priority: P2)

**Goal**: Resolve explicit exposure, versioned SDR/HDR transforms, seven output
profiles, and exactly-once gamut/transfer ownership with deterministic CPU/GPU
conformance.

**Independent Test**: Run frozen vectors at
`{-16,-8,-1,0,+1,+8,+15,+16}` through all three SDR curves, ACES 2 HDR viewing, all
seven profiles, and applicable Vulkan/Metal offscreen shader readbacks without a
native presentation surface; require the frozen decoded-domain tolerances,
stable identifiers, and results over 20 repeats, and reject unknown or
incompatible selections.

### Tests for User Story 2

- [X] T038 [P] [US2] Write failing runtime Strategy selection, exactly-once exposure binding, frozen-default materialization, unknown-version rejection, and CPU-oracle-to-runtime identity tests in `Tests/RendererOutputTransformMathTests.cpp` and `Tests/RendererOutputTransformMathTests.h`
- [X] T039 [P] [US2] Write failing seven-profile schema, version/default, pair-compatibility, and exactly-once transfer tests in `Tests/OutputDeviceProfileTests.cpp` and `Tests/OutputDeviceProfileTests.h`
- [X] T040 [P] [US2] Write failing frozen-profile/vector/constants-digest drift, tolerance-policy drift, no-regeneration, checked-in SPIR-V identity, deterministic MSL derivation, and 20-repeat tests in `.github/scripts/test_verify_output_transform_vectors.py` and `Tests/MetalShaderDerivationTests.cpp`
- [X] T041 [P] [US2] Write failing offscreen GPU readback conformance and encoded-versus-decoded reporting tests in `Tests/OutputTransformGPUConformanceTests.cpp` and `Tests/OutputTransformGPUConformanceTests.h`

### Implementation for User Story 2

- [X] T042 [P] [US2] Load and validate the already-frozen three-SDR/four-HDR profile registry, native encoding options, comparison domains, and tolerance policy identities in `Source/Renderer/Private/FOutputTransformSettingsValidator.cpp`
- [X] T043 [US2] Bind finite manual exposure exactly once before pre-tonemap insertion and preserve the M0 negative/non-finite/opaque-alpha policy in `Source/Renderer/Private/FOutputTransformShaderParameters.cpp`
- [X] T044 [US2] Register `Sdr.KhronosPbrNeutral.v1`, `Sdr.NarkowiczAcesFit.v1`, and `Sdr.ExtendedReinhardRec709.v1` as distinct runtime Strategies without relabelling the Narkowicz fit as Academy ACES in `Source/Renderer/Private/FOutputTransformSettingsValidator.cpp`
- [X] T045 [US2] Register the pinned official ACES 2 HDR viewing Strategy, 1000/2000-nit presets, Rec.2020 PQ ODTs, and logical linear-HDR scRGB80/Metal-EDR packers in `Source/Renderer/Private/FOutputTransformSettingsValidator.cpp`
- [X] T046 [US2] Implement shared decoded-linear-RGB-nits and matrix-propagated-XYZ comparison helpers using only the M0 tolerance policies in `.github/scripts/output_transform_common.py`
- [X] T047 [US2] Bind the immutable M0 profile, vector-set, constants, and tolerance digests into resolved settings, pipeline keys, normalized diagnostics, and evidence identities in `Source/Renderer/Private/FOutputTransformSettingsValidator.cpp` and `Source/Renderer/Private/FOutputTransformShaderParameters.cpp`
- [X] T048 [P] [US2] Load the checked-in vector set read-only for offscreen GPU conformance and reject any runtime expected-value generation or digest drift in `Tests/OutputTransformGPUConformanceTests.cpp`
- [X] T049 [US2] Extend settings resolution with explicit default materialization, version Strategy lookup, SDR/HDR family exclusion, bounds, and unsupported-profile failure in `Source/Renderer/Private/FOutputTransformSettingsValidator.cpp`
- [X] T050 [P] [US2] Define stable shader parameters, constant digests, profile IDs, and pipeline-key bindings in `Source/Renderer/Private/FOutputTransformShaderParameters.cpp`
- [X] T051 [US2] Implement all frozen SDR curves, ACES2 HDR viewing, gamut conversions, transfers, and distinct scRGB80/Metal EDR packers in `Content/Shaders/PostProcess/OutputTransform.frag`
- [X] T052 [US2] Rebuild the checked-in SPIR-V, run the already-failing T040 derivation/identity tests through the existing Feature 027 MSL/metallib path, and record the immutable outputs in `Content/Shaders/PostProcess/OutputTransform.frag.spv`, `Content/Shaders/PostProcess/OutputTransform.shader.json`, and `Validation/029/CI/shader-derivation.json`
- [X] T053 [US2] Add the output-transform shader identities plus profile/version dependencies to the strict-cooked production closure and DDC inputs in `Demo/StonerDemo/Private/FProductionContentDeferredExecution.cpp` and `Content/Shaders/PostProcess/OutputTransform.shader.json`
- [X] T054 [US2] Implement decoded-domain GPU conformance reporting that never compares raw scRGB and Metal EDR code values in `Tests/OutputTransformGPUConformanceTests.cpp`
- [X] T055 [US2] Register the remaining US2 profile and offscreen GPU suites while retaining the Phase 1 math/vector authority suite in `Tests/Main.cpp` and `Tests/SConscript`
- [X] T056 [US2] Run the CPU/GPU/profile/vector matrix and record normalized per-profile results and the explicit Unreal-parity non-goal in `Validation/029/CI/us2-color-conformance.json`

**Checkpoint**: Every appearance decision is explicit, versioned, deterministic,
decoded in its declared domain, and owned exactly once by Renderer.

---

## Phase 5: User Story 3 - Compose Ordered Post-Processing Stages (Priority: P3)

**Goal**: Expose bounded, domain-correct pre-tonemap and post-tonemap seams for
Feature 030 and diagnostics without adding AA or temporal state.

**Independent Test**: Register no-op and diagnostic operations in permuted
orders; require stable dependency order and correct domains, and reject cycles,
duplicate keys, hazards, invariant changes, or transfer ownership attempts.

### Tests for User Story 3

- [X] T057 [P] [US3] Write failing identity, order, dependency-cycle, bounded-resource, and domain-invariant insertion tests in `Tests/RendererPostProcessInsertionTests.cpp` and `Tests/RendererPostProcessInsertionTests.h`
- [X] T058 [P] [US3] Write failing pre/post graph placement, read/write hazard, external-output, and empty-list equivalence tests in `Tests/RendererPostProcessGraphTests.cpp`
- [X] T059 [P] [US3] Write failing named-stage debug bypass, raw-HDR visualization/readback, non-authoritative evidence, and no-formal-plan-mutation tests in `Tests/RendererOutputTransformTests.cpp`

### Implementation for User Story 3

- [X] T060 [P] [US3] Define insertion points, color-domain contracts, bounded operation resources, stable identities, dependencies, and Strategy/Composite interfaces in `Source/Renderer/Public/Renderer/FPostProcessInsertion.h`
- [X] T061 [US3] Implement deterministic dependency/order validation and reject duplicate keys, cycles, missing dependencies, hazards, invariant changes, and transfer claims in `Source/Renderer/Private/FPostProcessInsertion.cpp`
- [X] T062 [US3] Resolve at most 16 pre-tonemap and 16 post-tonemap operations into immutable plan stages without temporal state in `Source/Renderer/Private/FHDRPostProcessPipeline.cpp`
- [X] T063 [US3] Emit insertion resources, passes, dependencies, transitions, and culling rules around the frozen tone/view stage in `Source/Renderer/Private/FOutputTransformGraphBuilder.cpp`
- [X] T064 [US3] Implement named diagnostic stage selection with HDR-preserving readback or explicit bounded visualization in `Source/Renderer/Private/FOutputTransformExecutor.cpp`
- [X] T065 [P] [US3] Emit stable insertion order/domain/resource diagnostics and explicit non-authoritative bypass records in `Source/Renderer/Public/Renderer/FOutputTransformDiagnostics.h`
- [X] T066 [US3] Register `renderer-post-process-insertion` and execute the US3 independent tests through `Tests/Main.cpp` and `Tests/SConscript`
- [X] T067 [US3] Record the Feature 030 seam contract and proof that Feature 029 owns no jitter, motion-vector, history, reprojection, or AA state in `Validation/029/CI/us3-insertion-contract.json`

**Checkpoint**: TAA can later replace pre-tonemap SceneColor and FXAA can later
filter post-tonemap color without a duplicate post-processing framework.

---

## Phase 6: User Story 4 - Present, Read Back, and Resize Safely (Priority: P4)

**Goal**: Present and read back the same completed formal frame through exact
Vulkan/Metal native modes across resize, minimize, restore, output-mode change,
failure, and teardown.

**Independent Test**: On applicable native paths, verify actual format/color-
space/metadata, same-frame token and exact extent for readback plus present, 100
generation transitions, stable first failure, terminal owner cleanup, and no
Windows HDR authority claim. HDR appearance remains a later human-only US5 gate.

### Tests for User Story 4

- [X] T068 [P] [US4] Extend failing capability-generation, exact-resolution, stale-acquire, reconfigure, zero-drawable, and same-frame terminal-operation tests in `Tests/RHIPresentationOutputTests.cpp`
- [X] T069 [P] [US4] Write failing Vulkan exact surface-pair, packed10/PQ, FP16/linear, metadata, readback/present, and no-hidden-conversion native tests in `Tests/VulkanOutputTransformNativeTests.cpp` and `Tests/VulkanOutputTransformNativeTests.h`
- [X] T070 [P] [US4] Write failing Metal sRGB, `BGR10A2Unorm`/PQ, and FP16/EDR layer tests covering reference-white/headroom, PQ Core Animation color management with `EDRMetadata=nil`, EDR `EDRMetadata=nil`, rejection of native system-tone-mapping metadata, drawable, and readback/present behavior in `Tests/MetalOutputTransformNativeTests.cpp` and `Tests/MetalOutputTransformNativeTests.h`
- [X] T071 [P] [US4] Write failing 100-transition lifecycle, injected-failure, stale-token, paused/restore, and owner-baseline tests in `Tests/OutputPresentationLifecycleTests.cpp` and `Tests/OutputPresentationLifecycleTests.h`

### Implementation for User Story 4

- [X] T072 [P] [US4] Enumerate and retain exact `VkSurfaceFormatKHR` format/color-space pairs and capability generations in `Source/Backend/Vulkan/Private/FVulkanSurface.cpp` and `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanSurface.h`
- [X] T073 [P] [US4] Preserve SRGB versus UNorm responsibility and add packed10/PQ plus FP16/extended-linear mappings in `Source/Backend/Vulkan/Private/FVulkanStruct.h` and `Source/Backend/Vulkan/Private/FVulkanTexture.cpp`
- [X] T074 [US4] Consolidate the real native surface-backed Vulkan swapchain behind RHI, resolve actual state, apply optional metadata, and fail unsupported pairs without SDR fallback in `Source/Backend/Vulkan/Private/FVulkanSwapchain.cpp` and `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp`
- [X] T075 [US4] Publish Vulkan mode generation, exact extent, native encoding, acquire/submission/present identity, and metadata digest in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp` and `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanRuntimeSnapshot.h`
- [X] T076 [P] [US4] Add Metal packed10/FP16 mappings and per-surface HDR/EDR capability/reference-white/headroom snapshots in `Source/Backend/Metal/Private/FMetalFormat.mm` and `Source/Backend/Metal/Private/FMetalCapabilities.mm`
- [X] T077 [US4] Configure `CAMetalLayer` transactionally for SDR explicit transfer, `BGR10A2Unorm` + ITU-R 2100 PQ + EDR opt-in + Core Animation color management with `EDRMetadata=nil`, and extended-linear EDR with `EDRMetadata=nil`; reject `CAEDRMetadata` system tone mapping on both HDR paths in `Source/Backend/Metal/Private/FMetalPresentationContext.mm`
- [X] T078 [US4] Resolve and expose exact Metal drawable format/color space/EDR state/metadata/generation and reject stale drawables in `Source/Backend/Metal/Private/FMetalPresentationSurface.mm` and `Source/Backend/Metal/Private/FMetalSwapchain.mm`
- [X] T079 [US4] Bind Metal native state, readback, submission, and presentation provenance without Renderer-policy duplication in `Source/Backend/Metal/Private/FMetalDevice.mm` and `Source/Backend/Metal/Private/FMetalUploadReadback.mm`
- [X] T080 [US4] Execute acquire, producer, output graph, optional exact copy, Present transition, ordered submission, completion, and present with one frame token in `Source/Renderer/Private/FOutputTransformExecutor.cpp`
- [X] T081 [US4] Remove Forward readback/presentation mutual exclusion and add Deferred presentation while preserving same-frame completion in `Source/Renderer/Private/FForwardFrameExecutor.cpp` and `Source/Renderer/Private/FDeferredFrameExecutor.cpp`
- [X] T082 [US4] Route visible Feature 029 output directly from the formal native image and exclude Feature 028 CPU aspect-fit upload from authority in `Demo/StonerDemo/Private/FDemoBackendFactory.cpp` and `Demo/StonerDemo/Private/FProductionContentDeferredExecution.cpp`
- [X] T083 [US4] Implement transactional extent/profile/display generation invalidation, `Paused` zero-drawable behavior, restored exact resources, and first-failure cleanup in `Source/Renderer/Private/FOutputTransformExecutor.cpp`
- [X] T084 [P] [US4] Add native SDR, Metal HDR non-visual, lifecycle, failure-injection, Windows-no-HDR-claim, and same-frame `ready-for-live-review` request preparation modes in `.github/scripts/run_output_transform_validation.py`
- [X] T085 [P] [US4] Add normalized native probe serialization and terminal-owner inspection in `Demo/StonerDemo/Private/FOutputTransformValidationCommand.cpp`
- [X] T086 [US4] Register the Vulkan, Metal, and lifecycle suites with platform guards in `Tests/Main.cpp` and `Tests/SConscript`
- [X] T087 [US4] Run the applicable native independent tests and 100-transition matrices, recording unsupported hardware as Unsupported and a prepared HDR request as `manual-review-required` rather than success in `Validation/029/CI/us4-native-lifecycle.json`

**Checkpoint**: Presentation and readback share one real completed frame and
generation; no hidden transform, scaling, flip, fallback, or stale output can
become formal evidence.

---

## Phase 7: User Story 5 - Accept a New Formal Image Revision (Priority: P5)

**Goal**: Preserve Feature 028 v2 unchanged, create exact 512x512 SDR v3
Candidates with fresh authority, and require a separate maintainer-authored live
macOS attestation for all four HDR profiles.

**Independent Test**: Missing v3 Accepted records must fail closed as bounded
Candidates; any dimension or normalization attempt must fail before FLIP; only
explicit repository acceptance can admit SDR. HDR automation may prepare a
request but cannot write, infer, score, or replace a maintainer live decision.

### Tests for User Story 5

- [X] T088 [P] [US5] Write failing v3 key, state-machine, exact-dimension, immutable-v2, fresh-authority, and explicit-maintainer-acceptance tests in `Tests/ProductionImageAcceptance.cpp`
- [X] T089 [P] [US5] Write failing output-report, SDR-baseline, HDR-request, and HDR-attestation schema/canonicalization/bounds tests in `.github/scripts/test_verify_output_transform_evidence.py`
- [X] T090 [P] [US5] Write failing no-alignment/crop/scale/warp/resample, mismatch-before-FLIP, one-pixel mutation, and Candidate-only runner tests in `.github/scripts/test_run_output_transform_validation.py`
- [X] T091 [P] [US5] Write failing tests proving no CLI flag, environment variable, prompt default, automation attestation writer, perceptual score, HDR-authority PNG field, or inferred HDR `pass` exists in `.github/scripts/test_hdr_live_review_contract.py`
- [X] T092 [P] [US5] Write failing aggregation tests that reject Windows HDR authority, Feature 028 carry-forward reuse, stale revisions, unmatched attestations, Unsupported-as-pass, and any current `human-attested-fail` as closeout in `.github/scripts/test_aggregate_output_transform_validation.py`

### Implementation for User Story 5

- [X] T093 [P] [US5] Define explicit Lantern and Sponza `production-content-*-v3` workloads with 512x512, `sampleCount=1`, Khronos PBR Neutral v1, sRGB v1, and frozen settings digests in `Config/Validation/OutputTransform/Workloads/Lantern-v3.json` and `Config/Validation/OutputTransform/Workloads/Sponza-v3.json`
- [X] T094 [US5] Add a separate v3 baseline loader/registry keyed by workload, backend, device class, profile, transform, exposure, and settings digest without modifying v2 semantics in `Tests/ProductionImageAcceptance.cpp` and `Config/Validation/OutputTransform/SDR/Baselines-v3.json`
- [X] T095 [US5] Implement exact 512x512 lossless Candidate generation, semantic probes, pre-FLIP dimension rejection, one-pixel mutation, and forbidden-normalization checks in `.github/scripts/run_output_transform_validation.py` and `.github/scripts/compare_output_transform_images.py`
- [X] T096 [US5] Implement canonical bounded evidence/schema/digest/privacy verification and immutable maintainer acceptance-record validation in `.github/scripts/verify_output_transform_evidence.py`
- [X] T097 [US5] Integrate the US4 Metal-only four-profile `ready-for-live-review` request with the separate human-authority handshake while preserving a request schema that cannot express visual acceptance in `.github/scripts/run_output_transform_validation.py`
- [X] T098 [US5] Implement attestation structure, exact request-SHA linkage, immutable supersession, four live observations, acknowledgements, forbidden-field validation, and the rule that every current profile decision must be `pass` for closeout without adding an attestation writer in `.github/scripts/verify_output_transform_evidence.py`
- [X] T099 [US5] Implement deterministic/native/SDR aggregation that may quote only a matching human attestation and explicitly emits no Windows HDR validation claim in `.github/scripts/aggregate_output_transform_validation.py`
- [X] T100 [US5] Prove all Feature 028 v2 baseline/policy/reference files remain byte-identical and record their historical `sampleCount=1`/no-general-post-processing interpretation in `Validation/029/SDR/feature-028-v2-preservation.json`
- [X] T101 [P] [US5] Generate fresh exact-size Lantern and Sponza v3 SDR Candidates on the physical M4 Metal authority and store bounded review manifests in `Validation/029/SDR/M4-Metal/` (fresh `1f46352-20260904-01` calibration/native bundles; earlier working-tree Candidates remain preliminary)
- [X] T102 [P] [US5] Generate fresh exact-size Lantern and Sponza v3 SDR Candidates from same-frame GPU readback on physical Windows discrete Vulkan hardware without carry-forward; permit active Console or RDP sessions with recorded session/native-adapter evidence and successful application-window presentation, without claiming physical-monitor scanout; store bounded review manifests in `Validation/029/SDR/Windows-Vulkan/`
- [ ] T103 [US5] Have the maintainer review and explicitly admit or reject each exact v3 backend/device/workload record through repository changes in `Config/Validation/OutputTransform/SDR/Baselines-v3.json`
- [X] T104 [P] [US5] Run the four-profile PQ1000/PQ2000/EDR1000/EDR2000 non-visual preflight and create the machine-authored request in `Validation/029/HDR/hdr-live-review-request.json`
- [ ] T105 [US5] Have the maintainer personally view all four settled modes on the physical M4 Metal HDR/EDR display and manually author each immutable linked `pass` or `fail` attestation as a new file under `Validation/029/HDR/Attestations/`, with any correction appended as a new superseding record
- [ ] T106 [US5] Verify the accepted SDR records, HDR request/attestation linkage, evidence bounds, and current non-superseded `pass` for all four HDR profiles before recording a successful result in `Validation/029/CI/us5-authority.json`

**Checkpoint**: Changed SDR output has fresh explicit authority, HDR visual
acceptance is a real human observation, and Feature 028 remains immutable
historical evidence.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Integrate CI, enforce scope and evidence boundaries, run all
regressions, and close documentation/roadmap references only after authority is
complete.

- [X] T107 [P] Write failing workflow-YAML-only tests for the required platform matrix, producer/consumer separation, artifact revalidation, sanitizer/Lavapipe lanes, and absence of an automated HDR-acceptance lane in `.github/scripts/test_output_transform_workflows.py`
- [X] T108 Create Windows/macOS/Linux Debug and strict Release, Linux ASan/UBSan/TSan/Lavapipe, Metal non-visual, artifact-producer, artifact-consumer, and aggregate jobs in `.github/workflows/feature-029-hdr-output.yml`
- [X] T109 [P] Enforce JSON <=1 MiB, <=64 artifacts, each <=64 MiB, aggregate <=256 MiB, bounded PNG/JSON-only SDR evidence, and JSON/digest-only HDR authority in `.github/scripts/verify_output_transform_evidence.py`
- [X] T110 [P] Add architecture/scope scans that reject Renderer native API calls, backend-private color policy, runtime shader compilation, temporal/AA state, bloom/DoF/motion-blur/auto-exposure/upscaler/editor scope, and a second post-process graph in `.github/scripts/verify_output_transform_architecture.py`
- [X] T111 Run Feature 013/015/018/019/027/028 regression suites plus all Feature 029 suites and record normalized results in `Validation/029/CI/regressions.json`
- [ ] T112 Run Windows/macOS/Linux strict Release and Linux sanitizer jobs locally or in hosted CI and record exact revisions/run IDs/digests in `Validation/029/CI/README.md`
- [X] T113 [P] Update implementation-facing usage, diagnostic, unsupported-mode, and human-review instructions from verified commands in `specs/029-hdr-output-transform/quickstart.md`
- [X] T114 [P] Update the delivered Feature 029 architecture/evidence documentation without rewriting historical Feature 028 claims in `doc/029-hdr-output-transform.html`
- [X] T115 Reconcile requirement-to-task-to-test-to-evidence coverage for all FR-001 through FR-046 and SC-001 through SC-016 in `specs/029-hdr-output-transform/checklists/requirements.md`
- [X] T116 Run numbering, dependency, anchor, task-reference, and old-phase consistency scans until zero findings and store the normalized scan in `Validation/029/CI/consistency-scan.json`
- [ ] T117 Update Feature 029 completion status, delivered evidence, Feature 030 insertion-contract dependency, and unchanged Feature 031 dependencies only after all gates pass in `doc/roadmap.md`, `specs/002-engine-development-roadmap/spec.md`, and `AGENTS.md`
- [ ] T118 Complete the final same-revision producer/consumer aggregation, require explicit maintainer SDR acceptance and four current non-superseded HDR `pass` observations, and record closeout digests without auto-acceptance in `Validation/029/CI/README.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup & M0)**: No feature-internal dependency; its CPU oracle,
  constants, tolerances, and expected vectors become immutable authority.
- **Phase 2 (Foundation)**: Depends on completed M0 and blocks all user stories.
- **Phase 3 (US1 / MVP)**: Depends on Phase 2; establishes the formal path.
- **Phase 4 (US2)**: Depends on US1's path and may proceed in parallel with US3.
- **Phase 5 (US3)**: Depends on US1's path and may proceed in parallel with the
  CPU/configuration portion of US2.
- **Phase 6 (US4)**: Depends on US1 and US2 because native modes resolve exact
  profile/encoding policy; it consumes, but does not redefine, US3 seams.
- **Phase 7 (US5)**: Depends on US2 and US4. Its default formal workloads use
  empty insertion lists, so US3 is not an image-authority prerequisite, but US3
  must pass before feature closeout. After the shared T088-T100 contracts, the
  T101-T103 SDR authority lane and T104-T105 HDR live-review lane proceed
  independently; T106 is their explicit merge gate.
- **Phase 8 (Polish)**: Depends on all five user stories and both maintainer-
  controlled authority lanes.

### User Story Dependency Graph

```text
Setup -> Foundation -> US1 (formal output MVP)
                            |-> US2 (color authority) -> US4 (native lifecycle) -> US5 (authority)
                            `-> US3 (insertion seams) -------------------------------> Closeout
```

### Within Each User Story

1. Add tests and observe the intended failure before production changes.
2. Freeze public models/contracts before private implementations.
3. Implement CPU/reference and planning behavior before GPU/native execution.
4. Generate or update checked-in shader/config data only from frozen authority.
5. Run the story's independent test and record normalized evidence.
6. Never convert Unsupported, Candidate, or missing human review into success.

---

## Parallel Opportunities

### User Story 1

```text
T020 handoff tests || T021 Forward tests || T022 Deferred tests || T023 graph tests
T024 SceneColor API || T025 settings API || T026 plan API || T027 execution API
T032 shaders may proceed beside T028 validation only after Phase 1 M0 and T025 are frozen
```

### User Story 2

```text
T038 runtime-selection tests || T039 profile tests || T040 authority-drift tests || T041 GPU tests
T042 profile loading || T046 decoded comparison helpers || T050 shader bindings
```

### User Story 3

```text
T057 insertion tests || T058 graph tests || T059 bypass tests
T060 public insertion contract || T065 diagnostic record shape
```

### User Story 4

```text
T068 RHI lifecycle tests || T069 Vulkan tests || T070 Metal tests || T071 matrix tests
T072-T075 Vulkan lane || T076-T079 Metal lane
T084 validation runner || T085 demo probe serialization
```

### User Story 5

```text
T088 C++ baseline tests || T089 schema tests || T090 runner tests || T091 human-boundary tests || T092 aggregation tests
T093 workload configs || T097 HDR request path
T101 physical M4 SDR run || T102 physical Windows SDR run || T104 HDR preflight/request
T101 + T102 -> T103 SDR decision || T104 -> T105 HDR live decisions
T103 + T105 -> T106 authority merge
```

---

## Implementation Strategy

### MVP First

1. Complete Setup and Foundation.
2. Complete US1 with the explicit Khronos PBR Neutral v1 plus sRGB v1 default.
3. Demonstrate one canonical output path for both Forward and Deferred through
   the independent US1 tests.
4. Do not call the feature complete or revise image authority at MVP scope.

### Incremental Delivery

1. **US1** removes competing formal output paths.
2. **US2** binds the already-frozen M0 SDR/HDR color decisions to runtime and
   proves decoded offscreen conformance.
3. **US3** exposes the Feature 030 pre/post seams without temporal ownership.
4. **US4** proves same-frame native presentation/readback and lifecycle safety.
5. **US5** performs fresh SDR authority and human-only macOS HDR acceptance.
6. **Polish** closes cross-platform regression, evidence, documentation, and
   consistency gates at one exact revision.

### Validation Discipline

- M0 tests precede the CPU oracle and vectors; story tests precede runtime,
  shader, native, evidence, and workflow implementation and fail for the missing
  contract rather than build breakage or missing fixtures.
- Feature 028 v2 remains byte-for-byte historical authority.
- Formal v3 image dimensions are exact; no alignment, crop, scale, warp, or
  resampling is ever a permitted comparison step.
- Windows validates fresh SDR only and makes no Feature 029 HDR claim.
- Automated HDR work stops at `ready-for-live-review`; only the maintainer can
  author a linked visual decision after viewing the live Metal output, and any
  current `fail` blocks closeout until an appended superseding attestation gives
  all four profiles current `pass` decisions.
