# Tasks: Production Content Integration & Acceptance

**Input**: Design documents from `/specs/028-production-content-acceptance/`
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`,
`contracts/`, `quickstart.md`

**Tests**: Required by the specification. Contract, unit, integration, negative,
native, lifecycle, and evidence tests are written before their implementation
wave and must fail for the intended reason before production code is added.

**Organization**: Tasks are grouped by user story. The real KTX2 producer path
is foundational because every source-to-cooked story depends on it. Deferred is
the full production render gate; Forward is a bounded smoke for the same root.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Different files and no dependency on another incomplete task in the
  same wave.
- **[Story]**: Maps directly to a user story in `spec.md`.
- Every task names exact repository files or directories.

---

## Phase 1: Setup and Baseline Scaffolding

**Purpose**: Establish Feature 028 source, fixture, suite, profile, report, and
evidence ownership before changing shared cooker or Renderer contracts.

- [X] T001 Create Feature 028 content, profile, report, baseline, generated-output, and out-of-band `MAINTAINER_NOTES.md` conventions; specify that validators never open/hash the note and define bounded checked-in versus ignored artifacts in Content/ProductionAcceptance/README.md, Content/ProductionAcceptance/MAINTAINER_NOTES.md, Config/Validation/ProductionContent/README.md, Validation/028/README.md, Validation/028/reports/README.md, Validation/028/Baselines/README.md, Validation/028/CI/README.md, and .gitignore
- [X] T002 [P] Declare the Feature 028 C++ suite, options, and shared fixture helpers in Tests/ProductionContentTests.h, Tests/ProductionContentTestSupport.h, and Tests/ProductionContentTests.cpp
- [X] T003 [P] Create failing Python runner, corpus verifier, image comparator, and report-verifier test skeletons in .github/scripts/test_verify_production_corpus.py, .github/scripts/test_run_production_content_validation.py, .github/scripts/test_compare_production_images.py, and .github/scripts/test_production_acceptance_report.py
- [X] T004 Register the production-content suite, command-line selection, private test include paths, FLIP include boundary, and future helper sources in Tests/Main.cpp and Tests/SConscript
- [X] T005 [P] Create schema-valid canonical Feature 028 regular, medium, and hardware profile skeletons with exact 2-of-20 and 20-of-1,000 warm-up/RSS rules plus a canonically ordered versioned device-class registry requiring unique class/signature records and canonical backend implementation, CPU architecture, adapter family, shader profile, color/depth formats, sample count, and texture-format family in Config/Validation/ProductionContent/Regular.json, Config/Validation/ProductionContent/Medium.json, Config/Validation/ProductionContent/Hardware.json, Config/Validation/ProductionContent/DeviceClasses.json, specs/028-production-content-acceptance/contracts/production-validation-profile.schema.json, and specs/028-production-content-acceptance/contracts/device-class-registry.schema.json
- [X] T006 Capture the pre-change Debug, strict Release, AssetCooker, Runtime Asset Manager, static-model, deferred, forward, Vulkan, Metal, architecture, and validation-script baseline in Validation/028/reports/baseline.md
- [X] T007 Record the exact Khronos corpus and NVIDIA FLIP upstream revisions, selected file paths, expected inventories, and research digests without making license metadata executable in Content/ProductionAcceptance/UPSTREAM.md and ThirdParty/flip/UPSTREAM.md

**Checkpoint**: Feature 028 has stable ownership and a frozen regression baseline.

---

## Phase 2: Foundational Real KTX2 Cook Path

**Purpose**: Correct the current generic texture-cooker selection before any
production-content story can claim strict cooked KTX2 coverage.

**Critical**: Cook/runtime, rendering, tiered-acceptance, and evidence
implementation cannot claim production delivery until an `FTextureAsset`
selects `cooker.ktx2`, publishes an `FKTX2TextureArtifact`, and reloads it
strictly with deterministic DDC evidence. US1 corpus admission may proceed in
parallel after Setup because it does not consume cooked texture output.

- [X] T008 [P] Add failing payload/profile-specific producer-selection, color/normal/data semantic, mip, fallback, and wrong-producer tests in Tests/AssetCookerProductionTextureTests.cpp and Tests/AssetCookerProductionTextureTests.h
- [X] T009 [P] Add failing KTX2 derived-key, clean/warm reuse, corrupt artifact, envelope-type, and strict-loader integration tests in Tests/AssetCookerProductionTextureIntegrationTests.cpp and Tests/AssetCookerProductionTextureIntegrationTests.h
- [X] T010 [P] Add failing target-profile producer-settings and compressed-capability projection tests for every shipping Vulkan/Metal profile in Tests/AssetCookerTargetProfileTests.cpp
- [X] T011 Define deterministic payload/profile-specific cooker selection and selected-parameter evidence in Tools/AssetCooker/Private/FAssetCookerSelection.h and Tools/AssetCooker/Private/FAssetCookerSelection.cpp
- [X] T012 Select `cooker.ktx2` for `FTextureAsset`, construct validated `FTextureCookParameters`, and preserve the existing family producer for all other payloads in Tools/AssetCooker/Private/FAssetCookRunner.cpp
- [X] T013 Register and retain the KTX2 cooker token beside existing cooked and Metal cooker registrations for the entire cook operation in Tools/AssetCooker/Private/FAssetCookRunner.cpp
- [X] T014 Build texture-specific source/version/settings/profile evidence into the derived key without host or unrelated-profile fields in Tools/AssetCooker/Private/FAssetCookerSelection.cpp and Tools/AssetCooker/Private/FAssetCookRunner.cpp
- [X] T015 Publish the actual `FKTX2TextureArtifact` envelope, producer identity, target decision, mip evidence, and payload digest instead of a generic serialized `FTextureAsset` in Tools/AssetCooker/Private/FAssetCookRunner.cpp and Source/Asset/Private/FAssetCookContractCodec.cpp
- [X] T016 Preserve color, tangent-space normal, and data semantics through KTX2 encode/transcode while rejecting sRGB normal/data decisions in Source/Asset/Private/FTextureCookPolicy.cpp, Source/Asset/Private/FKTX2TextureCooker.cpp, and Source/Asset/Private/FKTX2TextureCodec.cpp
- [X] T017 Validate and decode the production KTX2 envelope through the existing strict loader with no generic-texture fallback in Source/Asset/Private/FKTX2TextureLoader.cpp and Source/Asset/Private/FAssetCookedExtensions.cpp
- [X] T018 Update all shipping AssetCooker target profiles with canonical `cooker.ktx2` settings and capability-correct compressed or portable fallback choices in Config/AssetCooker/Profiles/Linux-Vulkan.json, Config/AssetCooker/Profiles/Mac-Metal-Arm64.json, Config/AssetCooker/Profiles/Mac-Metal-X86_64.json, Config/AssetCooker/Profiles/Mac-Vulkan.json, and Config/AssetCooker/Profiles/Windows-Vulkan.json
- [X] T019 Run the focused producer-selection, DDC, publication, strict-loader, profile, and Feature 022 regression gates and record the foundational result in Validation/028/reports/foundation-ktx2.md

**Checkpoint**: Published production textures are real KTX2 artifacts and all
later strict-cooked work can trust their producer/type evidence.

---

## Phase 3: User Story 1 - Admit Representative Production Content (Priority: P1)

**Goal**: Admit a reproducible regular Lantern GLB and hash-pinned external
Sponza package with complete technical provenance, integrity, and coverage.

**Independent Test**: From a clean checkout, validate the regular corpus without
network access and the medium corpus from a pinned cache; reject every missing,
extra, altered, escaping, or normalization-colliding file before import.

### Tests for User Story 1

- [X] T020 [P] [US1] Add failing schema, canonical ordering, unknown-contract-field, package independence, file inventory, digest, tier, and coverage-closure tests; prove absent/changed out-of-band `MAINTAINER_NOTES.md` is never opened or hashed and cannot change canonical output in .github/scripts/test_verify_production_corpus.py
- [X] T021 [P] [US1] Add failing real-package resolver/importer structural tests for embedded GLB and external glTF dependencies in Tests/ProductionContentCorpusTests.cpp and Tests/ProductionContentCorpusTests.h
- [X] T022 [P] [US1] Add failing pinned acquisition, interrupted download, unavailable source, wrong revision, wrong hash, and cache quarantine tests in .github/scripts/test_acquire_production_corpus.py

### Implementation for User Story 1

- [X] T023 [US1] Implement the canonical corpus manifest model, JSON-schema checks, stable first-failure selection, and rejection of unknown canonical contract fields while excluding `MAINTAINER_NOTES.md` from package roots, inventories, and every validator input in .github/scripts/production_content_manifest.py and .github/scripts/verify_production_corpus.py
- [X] T024 [US1] Implement HTTPS pinned-revision medium acquisition into an ignored staging directory with temporary-file cleanup, exact inventory verification, and no fallback URL in .github/scripts/acquire_production_corpus.py
- [X] T025 [US1] Add the exact Lantern `glTF-Binary/Lantern.glb` bytes from revision `bf2bb4a81c73a7ceb53e80df3dec0105c5a3fdef` and its package README under Content/ProductionAcceptance/Regular/Lantern/
- [X] T026 [US1] Create the canonical Lantern and Sponza package/file records with exact sizes, SHA-256 values, roots, source locations, revision, acquisition date, and tiers, with no license/policy fields or maintainer-note digest in Content/ProductionAcceptance/Corpus/corpus-v1.json
- [X] T027 [US1] Declare and validate indexed geometry, primitive/material counts, hierarchy, shared/external/embedded dependencies, 1K/2K, RGB/RGBA, color, normal, and data coverage in Content/ProductionAcceptance/Corpus/coverage-v1.json and .github/scripts/verify_production_corpus.py
- [X] T028 [US1] Reject absolute paths, `.`/`..`, symlink escapes, mixed separators, percent-escape aliases, NFC collisions, case-only collisions, missing files, extra files, and size/hash mismatches in .github/scripts/production_content_manifest.py
- [X] T029 [US1] Route both packages through the existing `IAssetResolver` and glTF/GLB importer with no package-specific parser branch in Tests/ProductionContentCorpusTests.cpp
- [X] T030 [US1] Assert stable typed model, mesh, material, image, texture, and shader identities plus deterministic dependency order for the admitted roots in Tests/ProductionContentCorpusTests.cpp
- [X] T031 [US1] Add at least ten corpus/provenance/path negative fixtures and expected first-failure categories under Tests/Fixtures/ProductionContent/Corpus/ and Tests/Fixtures/ProductionContent/Failures/corpus-cases.json
- [X] T032 [US1] Implement a clean-checkout regular corpus command and optional verified medium acquisition command in .github/scripts/verify_production_corpus.py and Content/ProductionAcceptance/README.md
- [X] T033 [US1] Verify corpus canonical output and imported identity/dependency summaries are byte-identical across 20 runs in .github/scripts/test_verify_production_corpus.py and Tests/ProductionContentCorpusTests.cpp
- [X] T034 [US1] Run the US1 schema, integrity, coverage, resolver/importer, negative, and 20-run determinism gate and record bounded evidence in Validation/028/reports/us1-production-corpus.md

**Checkpoint**: The representative corpus is immutable, reproducible, and
independently useful for importer validation without any automated license policy.

---

## Phase 4: User Story 2 - Reproduce Source-to-Cooked Runtime Content (Priority: P1)

**Goal**: Cook each accepted explicit root into a complete generation, load it
strictly with source unavailable, and prove normalized development/cooked
semantic equivalence and deterministic reuse.

**Independent Test**: Starting with empty DDC/publication roots, clean cook and
publish the regular root, hide source, load its complete typed closure in strict
mode, compare every payload family to development mode, then repeat warm cooking.

### Tests for User Story 2

- [X] T035 [P] [US2] Add failing multi-source-root, explicit-root, default deferred-surface shader, complete dependency closure, and unrelated-output exclusion tests in Tests/ProductionContentCookGraphTests.cpp and Tests/ProductionContentCookGraphTests.h
- [X] T036 [P] [US2] Add failing development-versus-cooked identity, hierarchy, mesh, material, shader, image, KTX2 semantic, and target-selection equivalence tests in Tests/ProductionContentEquivalenceTests.cpp and Tests/ProductionContentEquivalenceTests.h
- [X] T037 [P] [US2] Add failing strict generation bind, source-unavailable, zero-source-participant, wrong-target, corrupt/substituted payload, missing dependency, cancellation, and no-partial-root tests in Tests/ProductionContentStrictRuntimeTests.cpp and Tests/ProductionContentStrictRuntimeTests.h
- [X] T038 [P] [US2] Add failing per-required-target 20-clean-cook determinism, per-root 100-percent warm reuse/strict-no-source/semantic-equivalence, source-mutation, prior-generation preservation, and cross-host normalized-report tests in .github/scripts/test_run_production_content_validation.py

### Implementation for User Story 2

- [X] T039 [US2] Extend production cook request assembly to pass ordered package and repository `Content/` source roots plus one explicit `StaticModel` root in .github/scripts/run_production_content_validation.py
- [X] T040 [US2] Resolve `ShaderProgram:Engine/Shaders/Deferred/Surface` and every selected shader payload into the same graph, failing incomplete material shader closure in Tools/AssetCooker/Private/FAssetCookGraph.cpp and Tests/ProductionContentCookGraphTests.cpp
- [X] T041 [US2] Prevent independently discovered generic images/textures and unrelated repository content from entering an explicit production root generation in Tools/AssetCooker/Private/FAssetCookGraph.cpp and Tools/AssetCooker/Private/FAssetSourceCatalog.cpp
- [X] T042 [US2] Produce and standalone-validate complete Vulkan and Metal generations containing model, mesh, material, shader, image/texture, KTX2, and target-specific shader payloads in .github/scripts/run_production_content_validation.py
- [X] T043 [US2] Build the strict runtime extension registry with existing cooked loaders plus `FKTX2TextureLoader`, validate target-profile evidence, and keep Tools unlinked in Tests/ProductionAssetClosureTestSupport.h and Tests/ProductionAssetClosureTestSupport.cpp
- [X] T044 [US2] Expose bounded resolver, importer, authoring-decoder, source-fallback, and strict-loader execution counters through Source/Asset/Public/Asset/FAssetManagerInspection.h and Source/Asset/Private/FAssetManagerInspection.cpp
- [X] T045 [US2] Add a validation-only complete typed dependency-closure collector with stable role/order and all-or-nothing publication in Tests/ProductionAssetClosureTestSupport.h and Tests/ProductionAssetClosureTestSupport.cpp
- [X] T046 [US2] Implement exact identity, metadata, dependency-role/order, hierarchy, transform, primitive/material association, and shader-interface comparison in Tests/ProductionAssetEquivalence.h and Tests/ProductionAssetEquivalence.cpp
- [X] T047 [US2] Implement normalized finite vertex/index/tangent/bounds comparison using existing import tolerances in Tests/ProductionAssetEquivalence.cpp
- [X] T048 [US2] Implement color/normal/data texture dimension, mip, color-space, target-decision, transcode, and semantic-tolerance comparison in Tests/ProductionAssetEquivalence.cpp
- [X] T049 [US2] Make the regular runner move or revoke source roots after generation validation and before manager construction, then assert all source-participant counters remain zero in .github/scripts/run_production_content_validation.py
- [X] T050 [US2] For every required target profile, run 20 isolated clean imports/cooks of the bounded regular root and compare normalized identities, dependency evidence, manifests, payload evidence, and generation identities in .github/scripts/run_production_content_validation.py
- [X] T051 [US2] For every accepted root, run an unchanged warm cook, assert 100 percent of eligible entries are reused, revoke source access, strict-load the complete closure, and compare every payload-family semantic result with development mode in .github/scripts/run_production_content_validation.py
- [X] T052 [US2] Add at least ten source/cook/publication/runtime corruption cases with exact expected first failures under Tests/Fixtures/ProductionContent/Failures/cook-runtime-cases.json and Tests/ProductionContentStrictRuntimeTests.cpp
- [X] T053 [US2] Inject source mutation during snapshot/cook, fail the operation, and prove the prior valid published generation and `Current.json` remain unchanged in Tests/ProductionContentCookGraphTests.cpp and Tests/AssetCookerInputSnapshotTests.cpp
- [X] T054 [US2] Run the US2 per-target 20-clean-cook gate and every-root clean/warm 100-percent-reuse, standalone-validation, strict-no-source, semantic-equivalence, mutation, and corruption gates and record evidence in Validation/028/reports/us2-cook-runtime-equivalence.md

**Checkpoint**: Features 020-026 form one real, deterministic, source-independent
production delivery path for the admitted root.

---

## Phase 5: User Story 3 - Render One Asset-Backed Composition Across Backends (Priority: P1)

**Goal**: Transactionally realize one strict-cooked model closure and render the
same backend-neutral production composition through full Deferred and bounded
Forward paths on native Vulkan and Metal.

**Independent Test**: Load the regular root only from a published generation,
realize all resources atomically, render GPU-produced Deferred and Forward
readbacks on each available backend, pass semantic plus exact baseline FLIP
gates, and release every owner.

### Tests for User Story 3

- [X] T055 [P] [US3] Add failing complete dependency validation, deterministic planning, shared-resource deduplication, immutable snapshot, and successful multi-node/multi-material realization tests in Tests/RendererStaticModelRealizationTests.cpp and Tests/RendererStaticModelRealizationTests.h
- [X] T056 [P] [US3] Add failing allocation, upload, texture transcode, descriptor, shader, pipeline, cancellation, device-loss, reverse rollback, exactly-once release, and stale-generation tests in Tests/RendererStaticModelRealizationFailureTests.cpp and Tests/RendererStaticModelRealizationFailureTests.h
- [X] T057 [P] [US3] Add failing production Demo configuration, strict root, workload revision, backend parity, Deferred-full, Forward-smoke, and no-Tools-link tests in Tests/ProductionContentDemoTests.cpp and Tests/ProductionContentDemoTests.h
- [X] T058 [P] [US3] Add failing native proof, readback normalization, semantic-probe ordering, exact Accepted-baseline selection with every non-Accepted state rejected, FLIP threshold, mutation rejection, and window-only capture tests in Tests/ProductionImageAcceptanceTests.cpp and Tests/ProductionImageAcceptanceTests.h

### Transactional Renderer Realization

- [X] T059 [US3] Define realization limits, explicit typed dependency sets, request, stages, diagnostics, inspection, immutable draw records, snapshot generation, and result contracts in Source/Renderer/Public/Renderer/FStaticModelRealization.h
- [X] T060 [US3] Validate the complete model/mesh/material/shader/KTX2 dependency set and produce deterministic node, primitive, and resource plans in Source/Renderer/Private/FStaticModelRealizationPlan.h and Source/Renderer/Private/FStaticModelRealizationPlan.cpp
- [X] T061 [US3] Implement a private reverse-order exactly-once RHI ownership transaction with commit/rollback inspection in Source/Renderer/Private/FStaticModelRealizationTransaction.h and Source/Renderer/Private/FStaticModelRealizationTransaction.cpp
- [X] T062 [US3] Compose existing static-mesh buffer realization for all unique mesh AssetId/version pairs and preserve section/index/bounds evidence in Source/Renderer/Private/FStaticModelRealization.cpp
- [X] T063 [US3] Compose KTX2 target transcode and texture realization for all unique texture AssetId/version/target pairs in Source/Renderer/Private/FStaticModelRealization.cpp
- [X] T064 [US3] Convert material/instance/shader assets into immutable Renderer snapshots and bind every primitive material slot and texture resource in Source/Renderer/Private/FStaticModelRealization.cpp
- [X] T065 [US3] Create descriptor and graphics-pipeline resources only after CPU dependency validation and reject interface, format, capability, or limit incompatibility before commit in Source/Renderer/Private/FStaticModelRealization.cpp
- [X] T066 [US3] Publish one immutable `FStaticModelRenderSnapshot` only after every draw is usable and retain all copied CPU state plus RHI ownership until final snapshot release in Source/Renderer/Private/FStaticModelRealization.cpp
- [X] T067 [US3] Implement deterministic bounded realization dumps and stable first-failure diagnostics without native pointers in Source/Renderer/Private/FStaticModelRealization.cpp and Source/Renderer/Public/Renderer/FStaticModelRealization.h
- [X] T068 [US3] Export the aggregate realization contract through Source/Renderer/Public/Renderer/RendererMinimal.h and compile it through Source/Renderer/SConscript
- [X] T069 [US3] Complete successful, shared-dependency, failure-injection, rollback, destroy/recreate, and stale-snapshot tests in Tests/RendererStaticModelRealizationTests.cpp and Tests/RendererStaticModelRealizationFailureTests.cpp

### Backend-Neutral Demo Composition

- [X] T070 [US3] Add production root, workload revision, render path, strict generation, baseline root, device-class-registry, visible-capture, fixed cycle/warm-up, and RSS options with validation that rejects caller-supplied class tokens in Demo/StonerDemo/Private/FDemoConfiguration.h and Demo/StonerDemo/Private/FDemoConfiguration.cpp
- [X] T071 [US3] Implement a responsibility-scoped strict manager session that composes existing cooked loaders plus `FKTX2TextureLoader`, binds one generation, requests the root and closure, waits/cancels deterministically, and hands explicit dependencies to Renderer in Demo/StonerDemo/Private/FProductionContentSession.h and Demo/StonerDemo/Private/FProductionContentSession.cpp
- [X] T072 [US3] Implement versioned backend-neutral model placement, camera, lights, frame token, Deferred inputs, and Forward smoke inputs in Demo/StonerDemo/Private/FProductionContentComposition.h and Demo/StonerDemo/Private/FProductionContentComposition.cpp
- [X] T073 [US3] Integrate the production session and composition as a separate run path without adding AssetCooker ownership or production-resource fields to the triangle path in Demo/StonerDemo/Private/FStonerDemoApplication.h and Demo/StonerDemo/Private/FStonerDemoApplication.cpp
- [X] T074 [US3] Bind aggregate snapshot geometry, material parameters, KTX2 textures, descriptors, and selected shader payloads into the full Deferred plan/executor in Source/Renderer/Private/FDeferredFrameExecutor.cpp and Demo/StonerDemo/Private/FProductionContentComposition.cpp
- [X] T075 [US3] Add the same-root bounded Forward draw plan, native color readback, and visible completion smoke without duplicating the full Deferred matrix in Source/Renderer/Private/FForwardFrameExecutor.cpp and Demo/StonerDemo/Private/FProductionContentComposition.cpp
- [X] T076 [P] [US3] Execute real Vulkan production Deferred attachments, composition color, Forward color, synchronization, and presentation readback with backend proof in Tests/VulkanProductionContentIntegrationTests.cpp and Tests/VulkanProductionContentIntegrationTests.h
- [X] T077 [P] [US3] Execute real Metal production Deferred attachments, composition color, Forward color, synchronization, and presentation readback through public RHI contracts with backend proof in Tests/MetalProductionContentIntegrationTests.cpp and Tests/MetalProductionContentIntegrationTests.h

### Native Image Acceptance and Lifecycle

- [X] T078 [P] [US3] Vendor only the CPU single-header FLIP implementation at commit `b475eb4bf394ab877c42166c9eb0a84a02cc5b14` with exact license, inventory, and hashes in ThirdParty/flip/FLIP.h, ThirdParty/flip/LICENSE, ThirdParty/flip/SHA256SUMS, and ThirdParty/flip/UPSTREAM.md
- [X] T079 [US3] Normalize row pitch, channel order, image origin, finite range, and declared color transfer into canonical LDR RGB in Tests/ProductionImageAcceptance.h and Tests/ProductionImageAcceptance.cpp
- [X] T080 [US3] Implement mandatory nonblank, coverage, orientation, current-frame, primitive/material region, base-color, normal, metallic/roughness, emissive, depth, and normal-attachment probes before perceptual comparison in Tests/ProductionImageAcceptance.cpp
- [X] T081 [US3] Implement schema-validated versioned `Config/Validation/ProductionContent/DeviceClasses.json` loading with canonical ordering and unique class/signature checks, canonical capability-signature construction with all required fields, exact one-class derivation, and exact workload/backend/class lookup that consumes only `Accepted` baselines and rejects Candidate/Calibrated/Reviewed/Superseded with no caller token or nearest/fallback selection in Tests/ProductionImageBaselineRegistry.h and Tests/ProductionImageBaselineRegistry.cpp
- [X] T082 [US3] Wrap CPU LDR-FLIP and report mean, p95, max, bad-pixel threshold, and bad-pixel fraction against immutable limits in Tests/ProductionImageAcceptance.cpp
- [X] T083 [US3] Calibrate candidate Vulkan and Metal baseline policies for every current workload revision and registry-derived device class from 20 same-revision captures plus blank, stale, origin, missing-geometry, material-swap, and color-space mutations, obtain explicit maintainer acceptance, transition selected records to `Accepted`, and retain non-Accepted state fixtures under Content/ProductionAcceptance/Baselines/
- [X] T084 [US3] Run the regular strict load-realize-Deferred/Forward-render-release path for 20 cycles with cycles 1-2 as included warm-up, assert all Asset/Renderer/RHI/native/presentation counters return to baseline and RSS growth from the post-warm-up sample to terminal is at most 16 MiB, and record US3 evidence in Validation/028/reports/us3-native-production-render.md

### Camera Calibration Expansion

- [X] T119 [US3] Amend the Feature 028 specification, plan, data model, research, render contract, task traceability, and quickstart for calibration-only free camera, exact revision-owned View/Projection presets, Sponza v2 invalidation, and fail-closed formal authority in specs/028-production-content-acceptance/
- [X] T120 [P] [US3] Add failing camera matrix validation/derivation, exact revision lookup, Deferred/Forward parity, preview input/reset/snapshot, and formal CLI isolation tests in Tests/ProductionContentDemoTests.cpp and Tests/ProductionCameraPreviewTests.cpp
- [X] T121 [US3] Implement exact code-owned `FProductionCameraPreset` lookup, finite affine orthonormal View validation, positive-X StandardZ Projection validation, and derived camera/ViewProjection/inverse data in Demo/StonerDemo/Private/FProductionCameraPreset.h and Demo/StonerDemo/Private/FProductionCameraPreset.cpp
- [X] T122 [US3] Implement the calibration-only right-drag/WASD/QE/Shift/wheel/reset/snapshot/exit controller and canonical bounded candidate JSON writer in Demo/StonerDemo/Private/FProductionCameraPreview.h and Demo/StonerDemo/Private/FProductionCameraPreview.cpp
- [X] T123 [US3] Add preview-only CLI validation and a strict-cooked native Deferred preview loop that updates camera uniforms/plans without recooking or re-realizing content, while formal native validation rejects overrides and remains input-free, in Demo/StonerDemo/Private/FDemoConfiguration.* and Demo/StonerDemo/Private/FStonerDemoApplication.*
- [X] T124 [US3] Use the Metal preview on the strict Sponza generation to select and explicitly approve an internal atrium-depth View/Projection candidate, freeze it as `production-content-sponza-v2`, and prove Vulkan/Metal plus Deferred/Forward consume the identical preset
- [X] T125 [US3] Replace Sponza v1 runner/probe authority with v2, redefine semantic regions from the approved view, regenerate 20-capture candidate calibration per required backend/device class, reject all mutations, obtain explicit maintainer acceptance, and remove the unaccepted v1 candidate from consumable baseline state
- [ ] T126 [US3] Re-run strict builds, camera/demo/image suites, 20-cycle visible acceptance, 1,000-cycle hardware lifecycle, medium, regression, sanitizer, clean-checkout, and final CI evidence after the v2 camera change
- [X] T127 [US3] Preserve the verified global native winding correction, advance Lantern authority to `production-content-lantern-v2`, move its key light to the intended camera-facing -X surface, and require the corrected normal/material probes without asset-specific triangle rewrites
- [X] T128 [US3] Recalibrate Lantern v2 from 20 Metal and MoltenVK captures, reject blank/stale/origin/missing-geometry/material/color-space/opposite-normal mutations, obtain explicit maintainer acceptance of the current sample-count-one output, register v2 as `Accepted`, and demote Lantern v1 to `Superseded`
- [ ] T129 [US3] Give the serialized two-package visible hardware profile a schema-enforced 60-minute lane budget while retaining 1,000 cycles per package, then rerun both physical M4 Pro backends and final CI
- [X] T130 [US3] Replace Vulkan device-owned strong resource tracking with bounded weak tracking, prune expired pipeline-cache entries, add an active-device transient-release regression, and prove the MoltenVK Lantern 1,000-cycle visible gate returns terminal ownership to zero with no post-warm-up RSS growth
- [X] T131 [US3] Bound long-lifecycle validation bookkeeping by retaining image bytes only for the final authoritative attachment set, counting all window/forward captures separately, capping successful diagnostics while preserving failures and global sequence, and cover the counters and truncation report in Demo/StonerDemo/Private/FProductionWindowCaptureWriter.cpp, Demo/StonerDemo/Private/FDemoDiagnostics.*, and production Demo regressions
- [X] T132 [US3] Reuse one production graphics queue/fence across lifecycle cycles, wait for queue retirement before fence reset, retire completed/invalidated Vulkan submission records without changing completion observation semantics, and prove 128 submissions succeed with a one-buffer command-pool capacity in Demo/StonerDemo/Private/FProductionSubmissionHarness.*, Source/Backend/Vulkan/Private/FVulkanQueue.cpp, and Tests/VulkanBackendTests.cpp
- [X] T133 [US3] Recover Metal visible production presentation from transient `Unavailable`/`NotReady` drawable results and both backends from resize within the existing two-second lifecycle boundary, preserve exact presentation readback comparison, emit the terminal RHI result on failure, and prove the corrected path with strict builds plus visible native acceptance in Demo/StonerDemo/Private/FProductionWindowCaptureWriter.cpp
- [ ] T134 [US3] Prime Deferred and Forward native driver allocation once before the declared lifecycle sequence, require the unmeasured prime to release every owner and reject stale handles, retain exact 20/2 and 1000/20 cycle/RSS contracts, and rerun the twice-failing Linux Lavapipe regular gate without changing the 16 MiB threshold
- [ ] T135 [US3] Release unused Linux/glibc heap pages after complete native teardown at the exact warm-up and terminal comparison cycles before the unchanged `/proc/self/statm` samples, preserve all intermediate/peak evidence plus exact 20/2 and 1000/20 comparisons and the 16 MiB threshold, and rerun the hosted Lavapipe regular and medium gates

**Checkpoint**: The same strict-cooked production root renders through native
Vulkan and Metal with transactional lifetime and semantic/perceptual proof.

---

## Phase 6: User Story 4 - Run Tiered Production Acceptance (Priority: P2)

**Goal**: Provide bounded regular, scheduled/on-demand medium, and explicit
hardware profiles with reproducible commands, honest Unsupported results, and
cost-conscious artifact reuse.

**Independent Test**: Execute each profile entry point and prove exact corpus,
target, cycles, budgets, gates, outputs, cadence, and replacement-lane behavior.

### Tests for User Story 4

- [X] T085 [P] [US4] Add failing regular/medium/hardware profile schema, exact tier membership, exact included 2-of-20 and 20-of-1,000 warm-up boundaries, post-warm-up-to-terminal 16 MiB RSS, 10/30/60 minute tier budgets, cadence, and required-gate tests in .github/scripts/test_run_production_content_validation.py
- [X] T086 [P] [US4] Add failing workflow path-filter, default-branch schedule, manual closeout, hardware-label, artifact producer/consumer, digest revalidation, and unsupported aggregation tests in .github/scripts/test_production_content_workflows.py

### Implementation for User Story 4

- [X] T087 [US4] Implement strict profile parsing, bounded subprocess execution, stable stage reporting, timeout, and exact command construction in .github/scripts/run_production_content_validation.py
- [X] T088 [US4] Implement per-cycle counter sampling, exact included 2-cycle regular and 20-cycle medium/hardware warm-up, peak RSS, post-warm-up-origin to terminal RSS growth, stale-handle checks, and 20/1,000 cycle enforcement in Demo/StonerDemo/Private/FDemoValidationMonitor.h and Demo/StonerDemo/Private/FDemoValidationMonitor.cpp
- [X] T089 [US4] Finalize regular, medium, and hardware profile contents, exact warm-up/RSS rules, target matrices, and canonical device-class registry in Config/Validation/ProductionContent/Regular.json, Config/Validation/ProductionContent/Medium.json, Config/Validation/ProductionContent/Hardware.json, and Config/Validation/ProductionContent/DeviceClasses.json
- [X] T090 [US4] Integrate verified external Sponza acquisition and, for every accepted package root, clean and unchanged warm cooking with 100-percent eligible reuse, source-unavailable strict loading, complete semantic equivalence, aggregate dependency/timing/memory evidence, and 1,000 cycles into the medium runner path in .github/scripts/run_production_content_validation.py
- [X] T091 [US4] Report unavailable backend/device/display/tool capability as `Unsupported` with exact prerequisite and replacement lane, and reject Unsupported as aggregate success in .github/scripts/run_production_content_validation.py
- [X] T092 [P] [US4] Add relevant-path PR/push Windows/macOS/Linux regular build, strict Release, deterministic, and platform-applicable native jobs in .github/workflows/feature-028-production-content.yml
- [X] T093 [US4] Add weekly default-branch and manual feature/release-closeout medium jobs with pinned corpus cache and 30-minute lane budget in .github/workflows/feature-028-production-content.yml
- [X] T094 [P] [US4] Add explicit Windows Vulkan and macOS Vulkan/Metal hardware dispatch jobs required for Feature 028 closeout and reference/render-path changes in .github/workflows/feature-028-production-hardware.yml
- [X] T095 [US4] Share only immutable digest-addressed producer artifacts where valid, and require every consumer to revalidate target profile, manifest, generation, and artifact digests in .github/workflows/feature-028-production-content.yml and .github/scripts/run_production_content_validation.py
- [X] T096 [US4] Document local, scheduled, closeout, and hardware ownership, prerequisites, commands, caches, budgets, and evidence outputs in Validation/028/README.md and Config/Validation/ProductionContent/README.md
- [X] T097 [US4] Run the US4 profile-contract, workflow-static, regular-local, every-root medium clean/warm/strict/equivalence, unsupported, timeout, and artifact-revalidation gates and record evidence in Validation/028/reports/us4-tiered-acceptance.md

**Checkpoint**: Each acceptance tier is reproducible, bounded, honest about
capability, and scheduled at the clarified cadence.

---

## Phase 7: User Story 5 - Inspect and Preserve Acceptance Evidence (Priority: P3)

**Goal**: Emit bounded deterministic and observational evidence that identifies
the exact corpus, generation, backend, workload, result, artifacts, and first
failure without leaking host-private data.

**Independent Test**: Regenerate success, rejection, rollback, Unsupported, and
image-failure reports; validate schema, digests, deterministic 20-run equality,
boundedness, redaction, and actionable reproduction data.

### Tests for User Story 5

- [X] T098 [P] [US5] Add failing acceptance-report schema tests for real generation digest versus failure-only `not-created`, Passed-null versus Failed/Unsupported-object first failure, Unsupported prerequisite/replacement lane, native exact registered device class and measured/not-run FLIP fields, native Passed requiring measured passing FLIP, deterministic-versus-observation separation, canonical ordering, artifact digest/size, 1 MiB report, 64-artifact, 64 MiB item, 256 MiB aggregate bounds, and unknown fields in .github/scripts/test_production_acceptance_report.py
- [X] T099 [P] [US5] Add failing absolute path, username, credential, environment secret, PID, native pointer, device-name identity, full-screen capture, stale capture, and unbounded log privacy tests in .github/scripts/test_production_evidence_privacy.py

### Implementation for User Story 5

- [X] T100 [US5] Implement canonical report construction and schema validation, including Passed generation digest versus failure-only `not-created`, conditional native device/measured-or-not-run FLIP, native Passed/measured-passing-FLIP, result/first-failure, and Unsupported prerequisite/replacement-lane invariants, in .github/scripts/production_acceptance_report.py and specs/028-production-content-acceptance/contracts/production-acceptance-report.schema.json
- [X] T101 [US5] Separate corpus/root/source/target/generation/mode/dependency/workload/backend/result/evidence/first-failure deterministic fields from timing/RSS/device/image observations, while requiring registry-derived class and FLIP observations for Vulkan/Metal reports, in .github/scripts/production_acceptance_report.py
- [X] T102 [US5] Implement stable bounded first-failure selection and reproduction profile mapping across corpus, import, cook, publication, strict load, realization, native, image, lifecycle, timeout, and Unsupported stages; emit null only for Passed and exactly one object for Failed/Unsupported in .github/scripts/production_acceptance_report.py
- [X] T103 [US5] Complete at least 30 targeted negative cases across the Feature 028 failure catalog and assert expected first-failure categories in Tests/Fixtures/ProductionContent/Failures/failure-catalog.json, Tests/ProductionContentTests.cpp, and .github/scripts/test_run_production_content_validation.py
- [X] T104 [US5] Implement evidence digest/size indexing, relative path tokens, canonical ordering, 1 MiB canonical report, 64-artifact, 64 MiB per-artifact, and 256 MiB aggregate limits plus missing/substituted artifact rejection in .github/scripts/production_acceptance_report.py
- [X] T105 [US5] Implement automated redaction and window-only capture validation before artifact publication in .github/scripts/verify_production_evidence.py and .github/scripts/compare_production_images.py
- [X] T106 [US5] Prove normalized correctness reports are byte-identical across 20 equivalent runs while timing, RSS, device, and image observations remain separately identified in .github/scripts/test_run_production_content_validation.py
- [X] T107 [US5] Run the US5 success/failure/Unsupported report, 30-case catalog, artifact-digest, privacy, capture, boundedness, and 20-run determinism gates and record evidence in Validation/028/reports/us5-evidence.md

**Checkpoint**: A new maintainer can explain and reproduce any acceptance result
without exposing host-private state or confusing observations with identities.

---

## Phase 8: Polish, Cross-Cutting Validation, and Closeout

**Purpose**: Prove architecture, regressions, cross-platform automation, medium
scale, and required physical hardware evidence on one final revision.

- [X] T108 [P] Extend architecture verification for Asset-to-Tools/RHI/Renderer/Application/Backend/API leakage, runtime-to-Tools links, Renderer/Application Vulkan/Metal calls, FLIP runtime links, Demo god-class growth, and the FR-044 feature-diff exclusion list covering new importers, skeletal/editor/hot-reload, package/archive, streaming/residency, Meshlet/LOD, virtual geometry, ray tracing, and visual redesign in Tests/verify_architecture.py and Tests/test_verify_architecture.py
- [X] T109 [P] Run local macOS Debug and strict Release production-content builds with warnings as errors and record compiler/test summaries in Validation/028/reports/local-builds.md
- [X] T110 [P] Run Linux ASan/UBSan, applicable TSan, malformed corpus, cancellation, failure-injection, and leak regressions and record commands/results in Validation/028/reports/sanitizers-and-failures.md
- [X] T111 Run all affected Features 018-027 Demo, Deferred, Forward, Asset, AssetCooker, Runtime Manager, Vulkan, and Metal regression suites and record the bounded result in Validation/028/reports/regressions.md
- [X] T112 Run the final-revision Windows/macOS/Linux regular GitHub Actions matrix and record run IDs, revisions, artifact names, and SHA-256 values in Validation/028/CI/README.md
- [X] T113 Run the final-revision medium closeout profile with every accepted package through clean/warm 100-percent reuse, source-unavailable strict loading, complete semantic equivalence, and 1,000 cycles with cycles 1-20 as included warm-up, then record timing, peak RSS, post-warm-up RSS growth, and evidence digests in Validation/028/CI/README.md
- [ ] T114 Run final-revision Windows Vulkan plus physical M4 Pro Vulkan/Metal 1,000-cycle hardware acceptance with GPU readbacks and window-only captures, and record hardware run/artifact/baseline digests in Validation/028/CI/README.md and Validation/028/Baselines/README.md
- [ ] T115 Re-run every command in specs/028-production-content-acceptance/quickstart.md from a clean workspace and record the results in Validation/028/reports/quickstart.md
- [X] T116 [P] Create the delivered Feature 028 system-design document following doc/SYSTEM_DESIGN.MD in doc/028-production-content-acceptance.html
- [ ] T117 Update Feature 028 status, final evidence references, next Phase 029 pointer, and project memory without rewriting historical gaps in doc/roadmap.md, specs/028-production-content-acceptance/spec.md, AGENTS.md, and Validation/028/README.md
- [ ] T118 Validate all Feature 028 JSON/schema files including device-class-registry/image-baseline capability-signature parity, requirement/task traceability, task completion, artifact digests, privacy scans, ASCII/style checks, `git diff --check`, clean generated-output policy, final revision consistency, and explicit FR-044 scope/exclusion evidence in specs/028-production-content-acceptance/, Content/ProductionAcceptance/, Validation/028/, and .github/

**Checkpoint**: Feature 028 is complete only when regular, medium, and required
hardware evidence all reference the same accepted final revision.

---

## Dependencies and Execution Order

### Phase Dependencies

- **Phase 1 - Setup**: Starts immediately.
- **Phase 2 - Real KTX2 Foundation**: Depends on Phase 1 and blocks US2-US5.
- **Phase 3 - US1 Corpus**: Depends on Phase 1 and may proceed in parallel with Phase 2.
- **Phase 4 - US2 Cook/Runtime**: Depends on both Phase 2 and US1 because it consumes admitted roots through real KTX2 cooking.
- **Phase 5 - US3 Rendering**: Depends on US2 strict closure and the Phase 2 KTX2 path.
- **Phase 6 - US4 Tiered Acceptance**: Depends on US1-US3 because profiles orchestrate their gates.
- **Phase 7 - US5 Evidence**: Depends on US1-US4 result producers but its report tests can begin after Phase 2.
- **Phase 8 - Closeout**: Depends on all five user stories.

### User Story Dependencies

```text
Setup --> Real KTX2 Foundation --|
  `----> US1 Corpus -------------+--> US2 Cook/Runtime
                                           |
                                           v
                                     US3 Rendering
                                           |
                                           v
                                  US4 Tiered Gates
                                           |
                                           v
                                     US5 Evidence
                                           |
                                           v
                                        Closeout
```

- **US1** is independently testable as a corpus integrity/import gate.
- **US2** is independently testable as a source-to-strict-cooked semantic gate.
- **US3** is independently testable from one prepared strict generation as a
  transactional native render gate.
- **US4** is independently testable with stubbed profile commands before native
  execution, then integrates the completed US1-US3 commands.
- **US5** is independently testable with synthetic report/result fixtures, then
  validates actual US1-US4 outputs.

### Within Each User Story

- Write the listed tests first and confirm they fail for the intended missing behavior.
- Freeze public/data contracts before implementation that consumes them.
- Validate deterministic CPU behavior before native GPU evidence.
- Validate semantic/readback probes before FLIP.
- Do not create or accept image references until the real strict-cooked native path passes.
- Complete the story checkpoint before checking its tasks complete.

## Parallel Opportunities

- T002, T003, and T005 touch independent suite/script/profile files.
- Phase 2 KTX2 work and US1 corpus admission may proceed in parallel after Setup.
- T008-T010 establish independent KTX2 test fronts before shared implementation.
- T020-T022 cover schema, C++ import, and acquisition independently.
- T035-T038 cover cook graph, equivalence, strict runtime, and runner determinism independently.
- T055-T058 cover Renderer success, Renderer failure, Demo, and image contracts independently.
- T076 and T077 implement Vulkan and Metal integration in separate test units after the common composition is stable.
- T078 can vendor the private validation dependency while native integration is developed.
- T085 and T086 validate profile data and workflow YAML independently.
- T092 and T094 edit separate hosted and hardware workflow files after the shared runner is stable.
- T098 and T099 cover report structure and evidence privacy independently.
- T108-T110 run independent architecture/build/sanitizer closeout fronts.

## Requirement Coverage

| Requirements | Primary tasks | Completion evidence |
|---|---|---|
| FR-001-FR-010 | T020-T034 | Corpus schema, exact inventory, coverage, importer, negatives, determinism |
| FR-011-FR-013 | T021, T029-T030, T035, T040 | Existing resolver/importer and complete typed dependency graph |
| FR-014 | T008-T019, T036, T048 | Real KTX2 producer/artifact and semantic comparison |
| FR-015-FR-022 | T035-T054 | Self-contained generation, strict no-source load, equivalence, reuse, mutation/failure |
| FR-023-FR-031, FR-046-FR-049 | T055-T084, T119-T126 | Asset-root composition, transactional realization, frozen camera preview/presets, native readback, FLIP, lifecycle |
| FR-032-FR-037 | T085-T097, T112-T114 | Regular, medium, hardware profiles and required cadence/evidence |
| FR-038-FR-041 | T098-T107 | Deterministic/observational reports, stable failures, bounded privacy-safe artifacts |
| FR-042 | T020-T022, T031, T035-T038, T052-T053, T055-T058, T069, T080-T083, T103 | At least 30 cross-stage negative cases |
| FR-043-FR-045 | T108, T111-T118 | Architecture, exclusion/regression, documentation, roadmap, and closeout |

| Success criteria | Primary tasks |
|---|---|
| SC-001-SC-002 | T025-T034 |
| SC-003-SC-004 | T038, T050-T051, T054 |
| SC-005-SC-006 | T036-T049, T054 |
| SC-007 | T031, T052, T056, T058, T103, T107 |
| SC-008 | T070-T084, T114 |
| SC-009-SC-010 | T084-T090, T113-T114 |
| SC-011 | T108-T114 |
| SC-012 | T098-T107, T118 |
| SC-013 | T108 |
| SC-014 | T115, T118 |
| SC-015 | T120-T126 |

## Parallel Examples

### User Story 1

```text
Task T020: Corpus schema and canonical manifest tests
Task T021: Real-package C++ resolver/importer tests
Task T022: Pinned medium acquisition tests
```

### User Story 2

```text
Task T035: Explicit-root cook graph tests
Task T036: Payload-family semantic equivalence tests
Task T037: Strict no-source runtime tests
Task T038: Clean/warm determinism runner tests
```

### User Story 3

```text
Task T055: Aggregate realization success tests
Task T056: Aggregate realization failure/rollback tests
Task T057: Production Demo contract tests
Task T058: Native image acceptance tests
```

### User Story 4 and User Story 5

```text
Task T085: Validation profile contract tests
Task T086: Workflow contract tests
Task T098: Acceptance report contract tests
Task T099: Evidence privacy tests
```

## Implementation Strategy

### Planning MVP: Corpus Admission

1. Complete Phase 1 Setup.
2. Execute Phase 2 real KTX2 foundation and US1 corpus admission in parallel.
3. Run the independent US1 corpus gate without waiting for KTX2 completion.
4. Treat this as an inspectable planning MVP, not as Feature 028 completion;
   the roadmap phase is complete only after US2-US5 and closeout.

### First Deliverable: Honest Source-to-Cooked Foundation

1. Complete Setup and the real KTX2 foundation.
2. Complete US1 corpus admission.
3. Complete US2 clean/warm cook plus strict no-source semantic equivalence.
4. Stop and validate that the project has a genuine production delivery path
   before adding visible rendering.

### Second Deliverable: Native Production Rendering

1. Complete the aggregate Renderer transaction.
2. Complete the backend-neutral Demo composition.
3. Pass semantic/readback probes on Vulkan and Metal.
4. Calibrate and review FLIP baselines only after the real path passes.

### Final Deliverable: Durable Acceptance Gate

1. Add regular, medium, and hardware profiles.
2. Add deterministic, privacy-safe evidence.
3. Pass hosted CI, medium closeout, and physical hardware gates on one revision.
4. Update delivered documentation, roadmap, and project memory.

## Notes

- A checked task requires implementation plus its stated tests/evidence.
- `[P]` means file-isolated work after the phase prerequisites, not permission to
  bypass a listed dependency.
- Native success requires requested backend proof and GPU-produced readback; no
  deterministic simulation, semantic oracle, or substituted backend qualifies.
- Automated validation never parses, classifies, approves, or rejects asset
  license terms.
- Keep generated medium content, DDC, generations, logs, and unreviewed captures
  out of authoritative source state.
- Commit after each task or tightly related implementation/test group using the
  project's conventional commit prefixes.
