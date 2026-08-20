# Tasks: Native Metal Backend

**Input**: Design documents from `/specs/027-metal-backend/`
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`,
`contracts/`, `quickstart.md`

**Tests**: Required by the specification. Contract and integration tests are
written before each implementation wave and must fail for the intended reason
before production code is added.

**Organization**: Tasks are grouped by user story. Foundational M0-M2a work is
shared because real Metal graphics, presentation, and strict cooked loading all
depend on the same backend-neutral payload, capability, build, process, and
offline shader contracts.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Different files and no dependency on another incomplete task in the
  same wave.
- **[Story]**: Maps directly to a user story in `spec.md`.
- Every task names exact repository files or directories.

---

## Phase 1: Setup And Evidence Scaffolding

**Purpose**: Establish source, test, fixture, and evidence ownership before
changing shared RHI or build contracts.

- [X] T001 Create Feature 027 fixture, report, capture, and CI artifact directories with local-output ignore rules in Tests/Fixtures/Metal/README.md, Validation/027/README.md, Validation/027/reports/README.md, Validation/027/captures/README.md, Validation/027/CI/README.md, and .gitignore
- [X] T002 [P] Define conformance, shader, presentation, failure, comparison, and malformed fixture provenance in Tests/Fixtures/Metal/Conformance/README.md, Tests/Fixtures/Metal/Shaders/README.md, Tests/Fixtures/Metal/Presentation/README.md, and Tests/Fixtures/Metal/Failures/README.md
- [X] T003 [P] Create the Feature 027 architecture/contract verifier skeleton, public RHI signature/overload extraction against specs/027-metal-backend/contracts/rhi-operation-matrix.md, and failing verifier unit tests in Tests/verify_metal_backend.py and Tests/test_verify_metal_backend.py
- [X] T004 [P] Create Feature 027 suite result/options declarations in Tests/MetalTests.h and Tests/MetalTestSupport.h
- [X] T005 Register Feature 027 suite selection, arguments, and future helper paths in Tests/Main.cpp, Tests/MetalTests.cpp, and Tests/SConscript
- [X] T006 Capture the pre-change Debug/strict Release, affected Vulkan, Asset, Demo, and architecture baseline plus the generated one-row-per-overload RHI matrix seed in Validation/027/reports/baseline.md and Validation/027/reports/rhi-operation-matrix.md; stop if the extracted API differs from specs/027-metal-backend/contracts/rhi-operation-matrix.md

**Checkpoint**: Every implementation and evidence artifact has an auditable
location; the existing baseline is frozen before shared migrations.

---

## Phase 2: Foundational Build, RHI, Process, And Shader Contracts

**Purpose**: Complete M0, M1, and M2a prerequisites that block every user story;
production cooker/DDC/publication/runtime composition remains M2b in User Story 3.

**Critical**: No user-story implementation begins until typed shader payloads,
capability limits, private Objective-C++ builds, deterministic MSL derivation,
and offline compiler boundaries are in place.

### Objective-C++ And Platform Isolation

- [X] T007 [P] Add failing `.mm` discovery, ARC isolation, Apple-framework leakage, non-macOS exclusion, deployment-target 12.0, and unguarded-availability tests in Tests/test_verify_metal_backend.py and Tests/test_verify_architecture.py
- [X] T008 Teach layer source discovery and architecture scanning to recognize private `.mm` files without treating them as portable C++ in site_scons/LayerBuilder.py and Tests/verify_architecture.py
- [X] T009 Replace the Metal stub with macOS-only Objective-C++/ARC compilation, API-free public includes, private Apple framework linkage, `MACOSX_DEPLOYMENT_TARGET=12.0`, and availability warnings as errors in Source/Backend/Metal/SConscript
- [X] T010 Add conditional Metal public includes/library linkage, propagate and assert `MACOSX_DEPLOYMENT_TARGET=12.0` for macOS Test/Demo compile and link actions, and exclude Apple implementation units on unsupported hosts in Tests/SConscript and Demo/StonerDemo/SConscript

### Bounded Process Execution

- [X] T011 [P] Add failing explicit-executable/argv, UTF-8 argument, bounded stdout/stderr, timeout, exit-code, missing-executable, and no-shell tests in Tests/CorePlatformProcessTests.cpp and Tests/CorePlatformProcessTests.h
- [X] T012 Define source-compatible bounded process request/result/limit contracts beside dynamic-module APIs in Source/Core/Public/Core/FPlatformProcess.h
- [X] T013 Implement no-shell process creation, pipe capture, timeout termination, and deterministic diagnostics on POSIX and Win32 in Source/Core/Private/FPlatformProcess.cpp and Source/Core/Private/FPlatformProcessInternal.h

### Backend-Neutral Shader Payload Migration

- [X] T014 [P] Add failing typed-byte SPIR-V/MetalLibrary validation, canonical native-binding metadata, digest, alignment, wrong-format, entry-point, and legacy migration tests in Tests/RHICoreTests.cpp
- [X] T015 Define `ERHIShaderPayloadFormat`, immutable byte payloads, canonical `FRHINativeBindingMap`, format-specific validation, and SPIR-V checked byte-to-word views in Source/RHI/Public/RHI/ERHIShaderPayloadFormat.h, Source/RHI/Public/RHI/FRHINativeBindingMap.h, and Source/RHI/Public/RHI/FRHIShaderModuleDesc.h
- [X] T016 Migrate Vulkan shader creation, native contexts, native offscreen session, and pipeline reuse keys to checked typed SPIR-V bytes in Source/Backend/Vulkan/Private/FVulkanDevice.cpp, Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp, Source/Backend/Vulkan/Private/FVulkanNativeOffscreenSession.cpp, and Source/Backend/Vulkan/Private/FVulkanPipelineCache.cpp
- [X] T017 Migrate Renderer conversion plus RHI, Vulkan, deferred, and demo test callers from `.Words` to typed bytes in Source/Renderer/Private/FShaderAssetConversion.cpp, Tests/RendererMaterialShaderAssetTests.cpp, Tests/VulkanBackendTests.cpp, Tests/VulkanNativeIntegrationTests.cpp, Tests/DeferredNativeIntegrationTests.cpp, and Tests/RendererForwardPipelineTests.cpp

### Capability Vocabulary

- [X] T018 Add failing resource-size, texture-dimension, per-stage binding, constant-range, sample-count, and compute-dispatch capability invariant tests in Tests/RHICoreTests.cpp after T014
- [X] T019 Extend backend-neutral device capabilities and canonical validation/dump helpers in Source/RHI/Public/RHI/FRHIDeviceCapabilities.h and Source/RHI/Private/RHIModule.cpp
- [X] T020 Populate the new capability vocabulary truthfully in Vulkan device discovery and preserve existing capability regressions in Source/Backend/Vulkan/Private/FVulkanDevice.cpp, Source/Backend/Vulkan/Private/FVulkanPhysicalDevice.cpp, and Tests/VulkanBackendTests.cpp

### Asset/Profile Compatibility

- [X] T021 [P] Add failing v1-read/v2-write, MSL-source-versus-MetalLibrary, canonical binding-entry/digest evidence, architecture, deployment-target, toolchain-evidence, and old-generation compatibility tests in Tests/AssetCookerProfileTests.cpp, Tests/AssetCookerPayloadCodecTests.cpp, and Tests/AssetMaterialShaderTests.cpp
- [X] T022 Add explicit `MetalLibrary` payload format, immutable `FShaderNativeBindingEvidence`, and v2-compatible Metal target requirements without GPU ownership in Source/Asset/Public/Asset/FMaterialShaderTypes.h, Source/Asset/Public/Asset/FShaderNativeBindingEvidence.h, and Source/Asset/Public/Asset/FAssetTargetProfile.h
- [X] T023 Implement canonical v2 Metal profile parsing/writing while retaining v1 Vulkan readers in Source/Asset/Private/FAssetTargetProfileCodec.cpp and Source/Asset/Private/FAssetTargetProfile.cpp
- [X] T024 Implement v2 shader payload envelope plus canonical binding-entry/digest evidence validation and retain v1 SPIR-V decoding in Source/Asset/Private/FMaterialShaderCookedCodec.cpp, Source/Asset/Private/FShaderPayloadValidation.cpp, and Source/Asset/Private/FAssetCookedExtensions.cpp

### Deterministic SPIR-V To MSL Derivation

- [X] T025 Vendor exactly `spirv_cross.cpp`, `spirv_cross_parsed_ir.cpp`, `spirv_parser.cpp`, `spirv_cfg.cpp`, `spirv_glsl.cpp`, `spirv_msl.cpp`, and their required headers at commit `a0fba56c34a6700f1724bf9b751da5b488a3775c` with license, provenance, complete file inventory, and source hashes in ThirdParty/spirv-cross/LICENSE, ThirdParty/spirv-cross/UPSTREAM.md, and ThirdParty/spirv-cross/SHA256SUMS
- [X] T026 Build the pinned SPIRV-Cross sources as a private AssetCooker-only static library with no runtime/Asset include leakage and enforce `MACOSX_DEPLOYMENT_TARGET=12.0` on macOS AssetCooker compile/link actions in Tools/AssetCooker/SConscript and site_scons/LayerBuilder.py
- [X] T027 [P] Add failing binding collision/limit, stage/set/binding/array mapping, malformed SPIR-V, volatile-source normalization, and twenty-repeat derivation tests in Tests/MetalShaderDerivationTests.cpp and Tests/MetalShaderDerivationTests.h
- [X] T028 Implement the sole canonical `metal-direct-binding-v1` slot assignment, reserved ranges, target-limit checks, ordered Asset evidence entries, and SHA-256 policy evidence in Tools/AssetCooker/Private/FMetalBindingMap.h and Tools/AssetCooker/Private/FMetalBindingMap.cpp
- [X] T029 Implement pinned SPIRV-Cross validation/reflection, explicit MSL bindings, MSL 2.4 options, UTF-8/LF normalization, and deterministic diagnostics in Tools/AssetCooker/Private/FSpirvCrossMslDeriver.h and Tools/AssetCooker/Private/FSpirvCrossMslDeriver.cpp
- [X] T030 Implement the runtime-independent canonical derivation/native-library evidence codec and binding-map invariants in Tools/AssetCooker/Private/FMetalShaderEvidenceCodec.h and Tools/AssetCooker/Private/FMetalShaderEvidenceCodec.cpp; use specs/027-metal-backend/contracts/metal-shader-evidence.schema.json only as a Python test oracle in Tests/test_verify_metal_backend.py, never as a runtime dependency

### Native Metal Library Finalization

- [X] T031 [P] Add failing toolchain-doctor, explicit `xcrun metal`/`metallib` argv, timeout, compiler failure, empty output, evidence mismatch, and Windows/Linux HostUnsupported tests in Tests/MetalShaderCompilerTests.cpp and Tests/MetalShaderCompilerTests.h
- [X] T032 Implement macOS-only AIR/metallib finalization, bounded output capture, complete toolchain evidence, cleanup, and non-macOS fail-closed behavior in Tools/AssetCooker/Private/FMetalLibraryCompiler.h and Tools/AssetCooker/Private/FMetalLibraryCompiler.cpp

**Checkpoint**: M0, M1, and M2a contracts compile on all hosts; Vulkan remains
green; cross-platform MSL derivation is deterministic; native finalization
primitives work only on an eligible macOS toolchain. M2 is not complete until
User Story 3 supplies DDC, publication, strict loading, and runtime transfer.

---

## Phase 3: User Story 1 - Execute Existing RHI Workloads On Metal (Priority: P1) MVP

**Goal**: Provide a real native Metal device implementing every applicable
current public RHI operation with GPU-produced offscreen evidence.

**Independent Test**: Build a test metallib offline, run complete Metal backend
conformance for device/resource/descriptor/graphics/compute/transfer/sync
contracts, and verify real GPU readback plus exact ownership cleanup.

### Tests For User Story 1

- [X] T033 [US1] Add authoritative conformance inputs and goldens in Tests/Fixtures/Metal/Conformance/triangle.vert.spv, Tests/Fixtures/Metal/Conformance/triangle.frag.spv, Tests/Fixtures/Metal/Conformance/compute.comp.spv, Tests/Fixtures/Metal/Conformance/expected-msl.json, Tests/Fixtures/Metal/Conformance/expected-readback.json, and Tests/Fixtures/Metal/Conformance/fixture-manifest.json
- [X] T034 [P] [US1] Add failing deterministic adapter selection, explicit registry-ID precedence, capability rejection, owner identity, foreign/stale object, and partial-init tests in Tests/MetalDeviceTests.cpp and Tests/MetalDeviceTests.h
- [X] T035 [P] [US1] Add failing buffer/texture/sampler storage policy, alignment, upload, subresource, row-padding, readback, destruction, and Intel-managed-memory tests in Tests/MetalResourceTests.cpp and Tests/MetalResourceTests.h
- [X] T036 [P] [US1] Add failing Asset-evidence-to-RHI binding-map consumption, no-runtime-slot-recomputation, descriptor compatibility, shader-library, graphics/compute pipeline, render-pass, framebuffer, and reuse-key tests in Tests/MetalPipelineTests.cpp and Tests/MetalPipelineTests.h
- [X] T037 [P] [US1] Add failing command-state, render/compute/blit encoder, copy/barrier, queue-order, fence/semaphore epoch, timeout, reset, and in-flight retention tests in Tests/MetalCommandTests.cpp and Tests/MetalCommandTests.h
- [X] T038 [P] [US1] Add failing real-device graphics, compute, transfer, synchronization, and readback conformance tests guarded by an honest Metal-device probe in Tests/MetalNativeIntegrationTests.cpp and Tests/MetalNativeIntegrationTests.h

### Device And Capabilities

- [X] T039 [US1] Define API-free backend config, selection, diagnostics, inspection, and factory contracts in Source/Backend/Metal/Public/MetalRHI/FMetalBackendConfig.h, Source/Backend/Metal/Public/MetalRHI/FMetalBackendDiagnostics.h, Source/Backend/Metal/Public/MetalRHI/FMetalBackendInspection.h, and Source/Backend/Metal/Public/MetalRHI/FMetalDeviceFactory.h
- [X] T040 [US1] Implement generation-safe device ownership, lifecycle, live-object counters, in-flight counters, and terminal failure capture in Source/Backend/Metal/Private/FMetalDeviceOwnerState.h and Source/Backend/Metal/Private/FMetalDeviceOwnerState.cpp
- [X] T041 [US1] Implement adapter enumeration, baseline filtering, explicit registry-ID selection, canonical ranking, and stable candidate diagnostics in Source/Backend/Metal/Private/FMetalAdapter.h and Source/Backend/Metal/Private/FMetalAdapter.mm
- [X] T042 [US1] Map GPU families, formats, samples, resource/binding/constant/dispatch limits, queue support, and normalized capability evidence in Source/Backend/Metal/Private/FMetalCapabilities.h, Source/Backend/Metal/Private/FMetalCapabilities.mm, Source/Backend/Metal/Private/FMetalFormat.h, and Source/Backend/Metal/Private/FMetalFormat.mm
- [X] T043 [US1] Compose native device/queue creation, baseline validation, child factories, failure unwind, and publication-after-ready in Source/Backend/Metal/Private/FMetalDevice.h and Source/Backend/Metal/Private/FMetalDevice.mm

### Resources And Bindings

- [X] T044 [P] [US1] Implement buffer allocation, checked ranges, shared/managed/private storage, mapping, and pending-destroy retention in Source/Backend/Metal/Private/FMetalBuffer.h and Source/Backend/Metal/Private/FMetalBuffer.mm
- [X] T045 [P] [US1] Implement texture/sampler creation, usage/format/mip/subresource checks, and lifecycle ownership in Source/Backend/Metal/Private/FMetalTexture.h, Source/Backend/Metal/Private/FMetalTexture.mm, Source/Backend/Metal/Private/FMetalSampler.h, and Source/Backend/Metal/Private/FMetalSampler.mm
- [X] T046 [US1] Implement bounded staging, private/managed upload, coherency notifications, texture row layout, blit readback, and completion visibility in Source/Backend/Metal/Private/FMetalUploadReadback.h and Source/Backend/Metal/Private/FMetalUploadReadback.mm
- [X] T047 [US1] Implement `FRHINativeBindingMap` canonical/digest/capability validation without slot recomputation, pipeline layouts, descriptor snapshots, direct resource binding, and dynamic-range checks in Source/Backend/Metal/Private/FMetalBindingMapValidator.h, Source/Backend/Metal/Private/FMetalBindingMapValidator.cpp, Source/Backend/Metal/Private/FMetalPipelineLayout.h, Source/Backend/Metal/Private/FMetalPipelineLayout.mm, Source/Backend/Metal/Private/FMetalDescriptorSet.h, and Source/Backend/Metal/Private/FMetalDescriptorSet.mm

### Shaders, Pipelines, And Render Scope

- [X] T048 [US1] Implement strict `MetalLibrary` byte/profile/digest/interface/entry validation and native `MTLLibrary` ownership without runtime source compilation in Source/Backend/Metal/Private/FMetalShaderLibrary.h and Source/Backend/Metal/Private/FMetalShaderLibrary.mm
- [X] T049 [P] [US1] Implement graphics pipeline validation, vertex/raster/depth/blend/attachment/sample state mapping, and complete reuse keys in Source/Backend/Metal/Private/FMetalGraphicsPipeline.h and Source/Backend/Metal/Private/FMetalGraphicsPipeline.mm
- [X] T050 [P] [US1] Implement compute pipeline validation, entry/interface/layout checks, dispatch-limit validation, and reuse keys in Source/Backend/Metal/Private/FMetalComputePipeline.h and Source/Backend/Metal/Private/FMetalComputePipeline.mm
- [X] T051 [US1] Implement render-pass/framebuffer attachment compatibility, load/store/clear actions, extent/sample checks, and ownership in Source/Backend/Metal/Private/FMetalRenderPass.h, Source/Backend/Metal/Private/FMetalRenderPass.mm, Source/Backend/Metal/Private/FMetalFramebuffer.h, and Source/Backend/Metal/Private/FMetalFramebuffer.mm

### Commands, Submission, And Synchronization

- [X] T052 [US1] Implement immutable command records, begin/end/render-scope state, retained object identities, reset rules, and fail-before-submit behavior in Source/Backend/Metal/Private/FMetalCommandBuffer.h and Source/Backend/Metal/Private/FMetalCommandBuffer.mm
- [X] T053 [P] [US1] Implement render encoder pipeline/descriptor/vertex/index/viewport/scissor/draw translation in Source/Backend/Metal/Private/FMetalRenderCommandEncoder.h and Source/Backend/Metal/Private/FMetalRenderCommandEncoder.mm
- [X] T054 [P] [US1] Implement compute encoder pipeline/descriptor/dispatch translation and threadgroup checks in Source/Backend/Metal/Private/FMetalComputeCommandEncoder.h and Source/Backend/Metal/Private/FMetalComputeCommandEncoder.mm
- [X] T055 [P] [US1] Implement blit encoder buffer/texture copies, texture-to-buffer readback, logical barriers, managed synchronization, and conservative encoder splits in Source/Backend/Metal/Private/FMetalBlitCommandEncoder.h and Source/Backend/Metal/Private/FMetalBlitCommandEncoder.mm
- [X] T056 [US1] Integrate render/compute/blit encoders and complete all applicable `IRHICommandBuffer` operations in Source/Backend/Metal/Private/FMetalCommandBuffer.mm
- [X] T057 [US1] Implement logical graphics/compute/transfer queues, command-buffer encoding/commit, wait/signal ordering, and stable submission records in Source/Backend/Metal/Private/FMetalQueue.h and Source/Backend/Metal/Private/FMetalQueue.mm
- [X] T058 [US1] Implement monotonic event-backed fences/semaphores, CPU condition waits, reset epochs, timeout, and stale/foreign rejection in Source/Backend/Metal/Private/FMetalSynchronization.h and Source/Backend/Metal/Private/FMetalSynchronization.mm
- [X] T059 [US1] Retain all resources/sync objects through terminal native completion and release them exactly once in Source/Backend/Metal/Private/FMetalSubmission.h and Source/Backend/Metal/Private/FMetalSubmission.mm
- [X] T060 [US1] Implement admission stop, accepted-work drain, child invalidation, zero-count audit, and device teardown ordering in Source/Backend/Metal/Private/FMetalDevice.mm

### US1 Integration And Gate

- [X] T061 [US1] Register all Metal private/public sources, framework flags, test suites, native availability definitions, and emitted deployment-target verification for Backend/Test/Demo actions in Source/Backend/Metal/SConscript, Source/Backend/SConscript, Tests/SConscript, Tests/MetalTests.cpp, and Tests/Main.cpp
- [X] T062 [US1] Run the focused Metal device/resource/pipeline/command/native suites and record GPU probe plus conformance results in Validation/027/reports/us1-rhi-conformance.json
- [ ] T063 [US1] Verify every row frozen by T006 is now native-pass or genuine capability-limited with requirement/API/test/device-capability evidence, fail on any new or missing public overload, and update statuses without changing the baseline inventory in Validation/027/reports/rhi-operation-matrix.md
- [X] T064 [US1] Run affected RHI/Vulkan/Renderer regressions after the shared contract migration and record the checkpoint in Validation/027/reports/us1-regression.md

**Checkpoint**: US1 is a real offscreen Metal RHI MVP. It is not yet a complete
Feature 027 because presentation, strict cooked Asset integration, shared demos,
and closeout evidence remain.

---

## Phase 4: User Story 2 - Present And Recover A Visible macOS Window (Priority: P1)

**Goal**: Present native Metal output through a Backend-owned `CAMetalLayer`
attached to an Application-owned window and recover across desktop lifecycle
changes.

**Independent Test**: Attach to a real GLFW Cocoa view, render/present bounded
frames, exercise resize/minimize/restore/drawable absence/close, and verify
correct layer ownership, frame completion, and clean detach.

### Tests For User Story 2

- [X] T065 [US2] Add resize, zero-extent, scale-change, temporary no-drawable, frame-slot, stale-generation, close, and expected-capture fixtures in Tests/Fixtures/Metal/Presentation/lifecycle-cases.json, Tests/Fixtures/Metal/Presentation/expected-frame.rgba8, and Tests/Fixtures/Metal/Presentation/fixture-manifest.json
- [X] T066 [P] [US2] Add failing surface/layer ownership, invalid-window, main-thread attach/detach, drawable size, frame-slot, and shutdown contract tests in Tests/MetalPresentationTests.cpp and Tests/MetalPresentationTests.h
- [X] T067 [P] [US2] Add failing real-window resize/minimize/restore/no-drawable/present/close integration tests behind an explicit visible-test switch in Tests/MetalPresentationIntegrationTests.cpp and Tests/MetalPresentationIntegrationTests.h

### Implementation For User Story 2

- [X] T068 [US2] Implement validated GLFW Cocoa-view borrowing, main-thread Backend-owned `CAMetalLayer` attach, partial-init rollback, and stable identities in Source/Backend/Metal/Private/FMetalPresentationContext.h and Source/Backend/Metal/Private/FMetalPresentationContext.mm
- [X] T069 [US2] Implement RHI presentation-surface lifecycle and borrowed-window validity checks in Source/Backend/Metal/Private/FMetalPresentationSurface.h and Source/Backend/Metal/Private/FMetalPresentationSurface.mm
- [X] T070 [US2] Implement swapchain-equivalent configuration, bounded frame slots, per-frame drawable ownership, format/count validation, and generation tracking in Source/Backend/Metal/Private/FMetalSwapchain.h and Source/Backend/Metal/Private/FMetalSwapchain.mm
- [X] T071 [US2] Implement logical/framebuffer extent, display scale, `drawableSize`, colorspace, resize generation, and stale-frame rejection in Source/Backend/Metal/Private/FMetalPresentationContext.mm
- [X] T072 [US2] Integrate drawable texture rendering, ordered command-buffer presentation, completion release, and in-flight ownership in Source/Backend/Metal/Private/FMetalQueue.mm and Source/Backend/Metal/Private/FMetalSwapchain.mm
- [X] T073 [US2] Implement bounded paused/unavailable/reconfiguring recovery for zero extent, minimize, occlusion, and temporary `nextDrawable == nil` without polling loops in Source/Backend/Metal/Private/FMetalPresentationContext.mm
- [X] T074 [US2] Implement drain, drawable release, layer device clear, detach-before-window-destroy, and exact shutdown audit in Source/Backend/Metal/Private/FMetalPresentationContext.mm and Source/Backend/Metal/Private/FMetalDevice.mm
- [X] T075 [US2] Add a real-window presentation helper with explicit user-controlled lifecycle actions and normalized output in Tests/Helpers/MetalPresentationProbe.mm and Tests/SConscript
- [ ] T076 [US2] Run the focused presentation contracts and one bounded local real-window smoke, recording device/layer/lifecycle evidence in Validation/027/reports/us2-presentation-smoke.json

**Checkpoint**: Native Metal presentation is usable and lifecycle-correct without
transferring window/view ownership out of Application.

---

## Phase 5: User Story 3 - Cook And Consume Asset-Backed Metal Library Payloads (Priority: P1)

**Goal**: Complete M2b: produce deterministic normalized MSL on all hosts, finalize native
libraries only on macOS, publish strict cooked generations, and consume them
without runtime source compilation or Asset GPU ownership.

**Independent Test**: Repeat derivation and final cooking twenty times, load the
result through strict cooked mode, create native graphics/compute pipelines, and
fail closed for every host/profile/evidence/payload mismatch.

### Tests For User Story 3

- [X] T077 [US3] Add derivation/native evidence goldens, malformed payloads, strict-generation cases, and profile expectations in Tests/Fixtures/Metal/Shaders/derivation-cases.json, Tests/Fixtures/Metal/Shaders/native-evidence-golden.json, Tests/Fixtures/Metal/Shaders/malformed-payloads.json, Tests/Fixtures/Metal/Shaders/strict-generation-cases.json, Tests/Fixtures/AssetCooker/Contracts/Profiles/Mac-Metal-Arm64.expected.json, and Tests/Fixtures/AssetCooker/Contracts/Profiles/Mac-Metal-X86_64.expected.json
- [X] T078 [P] [US3] Add failing cooker registration, complete derived-key invalidation, source snapshot, cross-host derivation, and twenty-repeat determinism tests in Tests/MetalShaderCookerTests.cpp and Tests/MetalShaderCookerTests.h
- [X] T079 [P] [US3] Add failing macOS finalization, native-library digest/evidence, non-macOS publication rejection, rollback, and immutable old-generation tests in Tests/MetalShaderPublicationTests.cpp and Tests/MetalShaderPublicationTests.h
- [X] T080 [P] [US3] Add failing strict cooked selection, wrong backend/CPU/profile/stage/entry/interface/version/digest, missing payload, zero source fallback, and post-Asset-release pipeline tests in Tests/MetalShaderRuntimeTests.cpp and Tests/MetalShaderRuntimeTests.h

### Implementation For User Story 3

- [X] T081 [US3] Implement `IAssetCooker` composition for SPIR-V-to-MSL derivation and optional native finalization in Tools/AssetCooker/Private/FMetalShaderCooker.h and Tools/AssetCooker/Private/FMetalShaderCooker.cpp
- [X] T082 [US3] Add transformation, binding-policy, profile, Apple toolchain, and native-library evidence to cook inputs and DDC keys in Tools/AssetCooker/Private/FCookInputSnapshot.cpp, Source/Asset/Public/Asset/FAssetDerivedKey.h, Source/Asset/Public/Asset/FAssetCookContractCodec.h, and Source/Asset/Private/FAssetCookContractCodec.cpp
- [X] T083 [US3] Add canonical macOS 12.0/MSL 2.4 arm64 and x86_64 Metal profiles with final-library producer requirements and verify every AssetCooker finalization invocation receives deployment target 12.0 in Config/AssetCooker/Profiles/Mac-Metal-Arm64.json, Config/AssetCooker/Profiles/Mac-Metal-X86_64.json, and Tests/MetalShaderCompilerTests.cpp
- [X] T084 [US3] Register Metal shader cooking, expose an explicit toolchain doctor, and preserve no-shell invocation in Tools/AssetCooker/Private/AssetCookerModule.cpp, Tools/AssetCooker/Private/FAssetCookCli.cpp, and Tools/AssetCooker/Private/FAssetCookRunner.cpp
- [X] T085 [US3] Publish only complete v2 `MetalLibrary` envelopes/manifests and reject source-only/failed finalization without changing `Current.json` in Source/Asset/Private/FAssetCookedExtensions.cpp, Tools/AssetCooker/Private/FCookedGenerationPublisher.cpp, and Source/Asset/Private/FAssetCookManifestCodec.cpp
- [X] T086 [US3] Select target-compatible Metal payloads, validate Asset-owned canonical binding evidence, and copy payload bytes plus every binding-map field into typed `FRHIShaderModuleDesc`/`FRHINativeBindingMap` values without leaking Asset handles into Backend in Source/Asset/Private/FShaderPayloadSelector.cpp and Source/Renderer/Private/FShaderAssetConversion.cpp
- [X] T087 [US3] Enforce strict cooked Metal payload validation and zero GLSL/SPIR-V/MSL source fallback through the runtime manager in Source/Asset/Private/FCookedAssetLoadingStrategy.cpp and Source/Asset/Private/FShaderDependencyLoader.cpp
- [X] T088 [US3] Run twenty local deterministic derivations and record canonical MSL/evidence digests in Validation/027/reports/us3-derivation-determinism.json
- [ ] T089 [US3] Run twenty eligible macOS final cooks, strict loads, and graphics/compute pipeline creations under one exact architecture/deployment/Xcode/SDK/compiler/profile/input tuple; prove identical metallib digests, DDC keys, evidence digests, outcomes, and reports while recording cross-tuple comparisons as derivation-only in Validation/027/reports/us3-native-cook-determinism.json
- [X] T090 [US3] Verify Windows/Linux finalization returns HostUnsupported and cannot publish a valid Metal generation in Validation/027/reports/us3-nonmac-finalization.md
- [X] T091 [US3] Run Feature 023/025/026 material, cooker, publication, and runtime-manager regressions and record the checkpoint in Validation/027/reports/us3-asset-regression.md

**Checkpoint**: Production runtime Metal pipelines consume only strict cooked
native libraries derived from the existing authoritative shader chain.

---

## Phase 6: User Story 4 - Run Backend-Neutral Triangle And Deferred Validation (Priority: P2)

**Goal**: Run shared triangle and deferred compositions through explicit Metal
or Vulkan selection without backend-specific Renderer/Application algorithms.

**Independent Test**: Execute both compositions natively on Metal, rerun Vulkan/
MoltenVK, and compare normalized semantic/readback/image evidence within declared
orientation, color, depth, normal, and image tolerances.

### Tests For User Story 4

- [X] T092 [P] [US4] Add failing explicit backend parse, unavailable backend, no-silent-fallback, shared-composition, and unchanged-scene/material tests in Tests/TriangleDemoIntegrationTests.cpp and Tests/TriangleDemoIntegrationTests.h
- [X] T093 [P] [US4] Add failing Metal deferred GBuffer/world-normal/depth/light/final-output tests using `metal-vulkan-tolerance-v1` from specs/027-metal-backend/contracts/validation-evidence.md in Tests/DeferredNativeIntegrationTests.cpp and Tests/DeferredNativeIntegrationTests.h
- [X] T094 [P] [US4] Add failing orientation, colorspace, row-padding, depth-policy, normal-decoding, exact semantic-threshold, whole-image `99.5%`/maximum-error, and unmatched-evidence normalization tests for `metal-vulkan-tolerance-v1` in Tests/MetalBackendComparisonTests.cpp and Tests/MetalBackendComparisonTests.h

### Implementation For User Story 4

- [X] T095 [US4] Add explicit backend enum/config parsing, observable selection, and stable initialization failure categories in Demo/StonerDemo/Private/FDemoConfiguration.h and Demo/StonerDemo/Private/FDemoConfiguration.cpp
- [X] T096 [US4] Implement an API-neutral Demo RHI factory that composes Vulkan/MoltenVK or Metal without silent substitution in Demo/StonerDemo/Private/FDemoBackendFactory.h and Demo/StonerDemo/Private/FDemoBackendFactory.cpp
- [X] T097 [US4] Refactor the demo application to receive the selected RHI factory/device while preserving shared scene, material, frame planning, and lifecycle code in Demo/StonerDemo/Private/FStonerDemoApplication.h and Demo/StonerDemo/Private/FStonerDemoApplication.cpp
- [X] T098 [US4] Load strict cooked Metal Shader Assets and execute the existing triangle composition through shared RHI submission/presentation in Demo/StonerDemo/Private/FStonerDemoApplication.cpp and Demo/StonerDemo/SConscript
- [X] T099 [US4] Route the existing deferred surface/graph/execution and real attachment readback through selected Metal RHI objects without Renderer forks in Tests/DeferredNativeIntegrationTests.cpp and Source/Renderer/Private/FDeferredRenderer.cpp
- [X] T100 [US4] Implement canonical Metal/Vulkan semantic and image comparison reports with the frozen `metal-vulkan-tolerance-v1` thresholds, observed maxima/ratios, and independent native evidence references in Tests/MetalBackendComparison.cpp and Tests/MetalBackendComparison.h
- [ ] T101 [US4] Run native Metal triangle/deferred probes and store normalized evidence in Validation/027/reports/us4-metal-triangle.json and Validation/027/reports/us4-metal-deferred.json
- [ ] T102 [US4] Rerun Vulkan/MoltenVK triangle/deferred native probes after Demo/RHI changes and store evidence in Validation/027/reports/us4-vulkan-regression.json
- [ ] T103 [US4] Produce the accepted cross-backend comparison with tolerance provenance in Validation/027/reports/us4-metal-vulkan-comparison.json
- [X] T104 [US4] Verify Renderer and Application contain no Metal imports or backend-specific rendering branches using Tests/verify_metal_backend.py and Validation/027/reports/us4-architecture.md

**Checkpoint**: Metal and Vulkan are explicit peers executing the same rendering
logic; comparisons are semantic and native on both sides.

---

## Phase 7: User Story 5 - Diagnose Failures And Preserve Cross-Platform Builds (Priority: P3)

**Goal**: Make every Metal failure bounded, stable, inspectable, and ownership-
clean while retaining Windows/Linux build and derivation support without Apple
SDK assumptions.

**Independent Test**: Inject every required lifecycle failure, repeat normalized
diagnostics, run 10,000 ownership cycles, and build/test shared code on all three
platforms with honest native-unavailable results.

### Tests For User Story 5

- [X] T105 [P] [US5] Add failing device/resource/pipeline/command/submission/sync/drawable/present/shutdown injection and exact unwind tests in Tests/MetalFailureInjectionTests.cpp and Tests/MetalFailureInjectionTests.h
- [X] T106 [P] [US5] Add failing stable diagnostic ordering, truncation, redaction, object/frame identity, capability reason, recovery state, and twenty-repeat tests in Tests/MetalDiagnosticsTests.cpp and Tests/MetalDiagnosticsTests.h
- [X] T107 [P] [US5] Add failing Windows/Linux no-Apple-header/link, explicit unsupported selection, cross-platform MSL derivation, and no-native-pass tests in Tests/test_verify_metal_backend.py
- [X] T108 [P] [US5] Add failing 10,000-iteration resource/pipeline/command lifecycle, in-flight shutdown, ownership-zero, and RSS protocol tests with iterations 1-1,000 as warm-up, samples every 100 iterations from 1,100-10,000, first/final ten-sample medians, and allowed growth `max(16 MiB, 5%)` in Tests/MetalLifecycleStressTests.cpp and Tests/MetalLifecycleStressTests.h

### Implementation For User Story 5

- [X] T109 [US5] Implement deterministic named failure points and one-shot/sequence injection without changing normal native paths in Source/Backend/Metal/Private/FMetalFailureInjector.h and Source/Backend/Metal/Private/FMetalFailureInjector.cpp
- [X] T110 [US5] Implement bounded normalized diagnostic collection with stable operation/object/frame/result/recovery fields and native-address redaction in Source/Backend/Metal/Private/FMetalDiagnostics.h and Source/Backend/Metal/Private/FMetalDiagnostics.cpp
- [X] T111 [US5] Implement immutable device/resource/pipeline/submission/presentation ownership snapshots and zero-count audits in Source/Backend/Metal/Private/FMetalInspection.h and Source/Backend/Metal/Private/FMetalInspection.cpp
- [X] T112 [US5] Provide API-free unsupported Metal factory/selection behavior on Windows/Linux without compiling or linking Apple implementation units in Source/Backend/Metal/Public/MetalRHI/FMetalDeviceFactory.h, Source/Backend/Metal/Private/FMetalDeviceFactoryUnsupported.cpp, and Source/Backend/Metal/SConscript
- [X] T113 [US5] Enforce Backend-only Apple API usage, Tools-only SPIRV-Cross, no Asset/RHI/Renderer/Application native ownership, and complete `.mm` scanning in Tests/verify_metal_backend.py and Tests/verify_architecture.py
- [X] T114 [US5] Run all failure points and twenty repeated normalized traces, recording terminal states and ownership counters in Validation/027/reports/us5-failure-determinism.json
- [X] T115 [US5] Run the 10,000-iteration Release lifecycle/RSS gate and record all 90 samples, warm-up/sample intervals, first/final medians, absolute/relative growth, computed `max(16 MiB, 5%)` threshold, and result in Validation/027/reports/us5-lifecycle-stress.json
- [X] T116 [US5] Run shared Windows/macOS/Linux build-isolation and derivation probes through the validation runner and record unavailable-versus-native classifications in Validation/027/reports/us5-cross-platform.md

**Checkpoint**: Failures are reproducible and clean; unsupported hosts remain
first-class build/derivation environments without false Metal execution claims.

---

## Phase 8: Polish, CI, Hardware Acceptance, And Closeout

**Purpose**: Prove every FR/SC, collect both Mac architecture evidence, preserve
all affected regressions, and close Feature 027 honestly.

- [X] T117 [P] Complete FR-001-FR-045 and SC-001-SC-010 trace checks, evidence-tier validation, native-device proof, and forbidden-scope checks in Tests/verify_metal_backend.py and Tests/test_verify_metal_backend.py
- [X] T118 [P] Implement normalized deterministic/native/visible/comparison orchestration, Metal-device probing, schema validation, timeout, cleanup, and runner unit tests in .github/scripts/run_metal_validation.py and .github/scripts/test_run_metal_validation.py
- [X] T119 Add the ten-job Windows/macOS-arm64/Linux Debug/strict Release, Linux ASan/UBSan/TSan, and macOS Intel hosted build/cook matrix with twenty-repeat deterministic MSL derivation on every supported host, unique always-uploaded artifacts, and honest native-unavailable handling in .github/workflows/feature-027-metal-backend.yml; add required fail-on-unavailable GPU native-offscreen jobs using `[self-hosted, macOS, metal, arm64]` and `[self-hosted, macOS, metal, x86_64]` in .github/workflows/feature-027-metal-hardware.yml
- [X] T120 Run local macOS Debug and strict Release builds plus all focused Feature 027 and affected full-regression suites and record results in Validation/027/reports/local-regression.md
- [ ] T121 Dispatch, watch, and download both workflows using the exact commands from quickstart section 10 and record all hosted and required hardware job conclusions, runner architectures/labels, probe results, head SHA, and artifact digests in Validation/027/CI/README.md; for any unavailable hardware lane also record the gap owner, exact manual diagnostic command, and follow-up gate while leaving closeout blocked
- [ ] T122 Require the automated GPU-capable arm64 hardware job to pass offscreen conformance, shader cook, triangle/deferred comparison, failure, and lifecycle gates; cross-check its artifact with a local M4 Pro run and record accepted evidence in Validation/027/reports/native-arm64.json
- [ ] T123 Require the automated GPU-capable x86_64 hardware job to pass equivalent native Intel Metal gates and record architecture/OS/device/capability plus GPU-readback evidence in Validation/027/reports/native-x86_64.json
- [ ] T124 Run the 3,000-frame/20-cycle real-window acceptance, save an oriented capture and lifecycle log, and record exit/RSS/layer-detach evidence in Validation/027/captures/visible-acceptance.json
- [ ] T125 Validate all checked-in reports against specs/027-metal-backend/contracts/metal-validation-report.schema.json and record artifact SHA-256 values in Validation/027/CI/README.md
- [ ] T126 Document delivered architecture, shader pipeline, ownership, presentation, validation tiers, exclusions, and evidence in doc/027-metal-backend.html following doc/SYSTEM_DESIGN.MD
- [ ] T127 Mark Feature 027 complete, retain Feature 028 as Production Content Integration, and update active project memory in doc/roadmap.md and AGENTS.md only after T121-T126 pass
- [ ] T128 Execute every applicable command in specs/027-metal-backend/quickstart.md, run `git diff --check`, remove unresolved markers/local outputs, and resolve discrepancies in their owning files

---

## Dependencies And Execution Order

### Phase Dependencies

- **Setup (Phase 1)** starts immediately.
- **Foundation (Phase 2)** depends on Setup and blocks every user story.
- **US1 (Phase 3)** depends on Foundation and provides the native offscreen RHI
  MVP used by presentation and integrations.
- **US2 (Phase 4)** depends on US1 device, queue, resource, and synchronization
  completion.
- **US3 (Phase 5)** depends on Foundation and may proceed beside US1/US2; US1
  may use an offline-built test metallib, but production strict loading requires
  US3.
- **US4 (Phase 6)** depends on US1, US2, and US3.
- **US5 (Phase 7)** depends on the native boundaries from US1/US2 and the shader
  evidence from US3; its test scaffolding can begin earlier.
- **Closeout (Phase 8)** depends on all five user stories.

### User Story Dependency Graph

```text
Setup -> Foundation -> US1 -> US2 ---\
                    \-> US3 --------+-> US4 -> Closeout
                         \----------+-> US5 ---/
```

### Within Each User Story

- Fixture tasks precede tests that consume them.
- Tests must fail for the intended missing behavior before implementation.
- Device ownership precedes child objects; resources precede bindings/pipelines;
  pipelines and resources precede command submission.
- Presentation attach/size/frame ownership precedes present and shutdown.
- Derivation/finalization precedes publication; publication precedes strict load.
- Native evidence is collected only after a runtime device probe succeeds.

### Parallel Opportunities

- Setup T002-T004 are separate files after T001.
- Foundation test waves T007/T011/T014/T021/T027/T031 are independent; T018
  follows T014 because both edit `Tests/RHICoreTests.cpp`;
  their implementation waves remain ordered by the contracts they verify.
- US1 tests T034-T038 can be authored together after fixture T033.
- US1 buffers T044 and textures/samplers T045 are parallel after device T043;
  graphics T049 and compute T050 are parallel after shader/binding completion;
  encoder tasks T053-T055 are parallel after command records T052.
- US2 tests T066/T067, US3 tests T078-T080, US4 tests T092-T094, and US5 tests
  T105-T108 are parallel within their phases.
- Closeout verifier T117 and validation runner T118 are parallel before workflow
  composition T119.

## Parallel Example: User Story 1

```text
Wave 1: T033 conformance fixtures
Wave 2: T034, T035, T036, T037, T038 failing tests
Wave 3: T039, T040, T041, T042, T043 device foundation
Wave 4: T044 and T045 resources
Wave 5: T046, T047, T048
Wave 6: T049 and T050, then T051 and T052
Wave 7: T053, T054, T055, then T056-T060
Join: T061-T064 integration and evidence
```

## Parallel Example: User Story 3

```text
Wave 1: T077 fixtures
Wave 2: T078, T079, T080 failing tests
Wave 3: T081-T087 in dependency order
Wave 4: T088-T091 derivation, native cook, non-macOS, and regression evidence
```

## Implementation Strategy

### MVP First

1. Complete T001-T032.
2. Complete US1 T033-T064.
3. Stop and validate real Metal offscreen RHI conformance.
4. Do not claim Feature 027 complete or production-ready at this checkpoint.

### Incremental Solo Order

1. T001-T032: build/RHI/process/shader foundation.
2. T033-T064: native Metal RHI MVP.
3. T065-T076: presentation and desktop lifecycle.
4. T077-T091: strict cooked Metal shader path can proceed after US2 or beside it.
5. T092-T104: shared triangle/deferred and backend comparison.
6. T105-T116: diagnostics, failure, stress, and unsupported hosts.
7. T117-T128: CI, dual-architecture hardware acceptance, docs, and closeout.

### Multi-Agent Strategy

- One owner controls shared RHI/build migrations T007-T024.
- A Tools owner handles SPIRV-Cross/cooking T025-T032 and US3 after contracts
  freeze.
- A Backend owner handles US1, then presentation US2.
- Renderer/Demo integration starts only after RHI and strict shader contracts
  freeze; validation tooling may proceed in parallel on separate files.
- Merge at phase checkpoints and rerun affected gates before opening the next
  shared-contract wave.

## Notes

- `MetalLibrary` bytes are final runtime payloads; normalized `MSL` is evidence,
  never a production runtime compilation input.
- `macos-26` and `macos-26-intel` prove CPU architecture build/cook coverage.
  They count as native only when an actual Metal-device probe succeeds.
- No deterministic output, semantic oracle, or silent MoltenVK fallback may
  satisfy a native Metal gate.
- Apple frameworks and Objective-C++ remain private to Metal Backend; SPIRV-
  Cross remains private to AssetCooker Tools.
- iOS lifecycle, argument-buffer optimization, mesh shaders, ray tracing,
  meshlets, streaming/residency, and Asset GPU ownership remain excluded.
