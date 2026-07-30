# Tasks: KTX2 Cooking & Compression

**Input**: Design documents from
`/specs/022-ktx2-cooking-compression/`
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`,
`contracts/ktx2-asset-api.md`, `contracts/ktx2-container-profile.md`,
`contracts/compressed-rhi-realization-api.md`, and `quickstart.md`

**Tests**: Required. The specification requires byte-identical cross-platform
cooking, independent KTX2 validation, malformed-input and concurrency coverage,
mock-RHI realization, conditional native Vulkan evidence, strict Debug/Release,
Linux sanitizers, and full regression.

**Organization**: Tasks are grouped by user story. Setup and Foundation establish
the pinned codec/runtime, deterministic encoder proof, public boundaries, and
test wiring shared by all stories. Every story ends with an independently
runnable checkpoint.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Safe to execute in parallel because the task edits different files
  and has no dependency on another incomplete task.
- **[Story]**: Maps the task to the corresponding user story in `spec.md`.
- Every task includes an exact target path.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish reproducible third-party provenance, corpus ownership,
and build/test wiring without exposing codec dependencies through public engine
headers.

- [X] T001 [P] Vendor the required private KTX-Software 4.4.2 libktx, DFD, and Basis transcode sources plus exact version, revision, license, notice, build options, and SHA-256 evidence under `ThirdParty/ktx/`
- [X] T002 [P] Vendor the minimal WAMR 2.4.5 interpreter sources plus exact version, revision, license, disabled-feature inventory, and SHA-256 evidence under `ThirdParty/wamr/`
- [X] T003 [P] Add the versioned single-threaded encoder module, exported ABI description, source/toolchain revisions, reproducible build command, licenses, and module checksum under `ThirdParty/stoner-basis-encoder/`
- [X] T004 [P] Define the at-least-18 valid artifact matrix, source facts, expected policy, semantic, quality, mip, size, and digest evidence in `Tests/Fixtures/KTX2/README.md`
- [X] T005 Add deterministic source inputs and checked-in valid/golden ETC1S, UASTC, uncompressed LDR, and uncompressed HDR artifacts under `Tests/Fixtures/KTX2/Valid/` and `Tests/Fixtures/KTX2/Golden/`
- [X] T006 Add bounded malformed KTX2 seed files for header, level-index, DFD, KVD, SGD, Basis payload, overlap, alignment, and truncation mutation coverage under `Tests/Fixtures/KTX2/Invalid/`
- [X] T007 Configure private KTX-Software, WAMR interpreter-only, and encoder-module embedding with scoped third-party warnings and no public include leakage in `Source/Asset/SConscript` and `site_scons/LayerBuilder.py`
- [X] T008 Register Feature 022 Asset and Renderer test translation units plus required private test include paths in `Tests/SConscript`
- [X] T009 Add Feature 022 aggregate test entry points while preserving the existing `--suite asset` behavior in `Tests/AssetKTX2Tests.h`, `Tests/AssetKTX2Tests.cpp`, `Tests/AssetTests.cpp`, and `Tests/AssetTests.h`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Prove the authoritative encoder can satisfy byte identity, then
establish generic cook/load evolution, stable diagnostics, immutable public
types, private adapters, and architecture enforcement.

**Critical**: The deterministic encoder proof is a blocking gate. A mismatch
must be fixed in the canonical path and must not be hidden by digest
normalization, fixture replacement, or weakening SC-002.

- [X] T010 Extend stable KTX2 result categories and `Container`/`Transcode` stages without changing existing enum meanings in `Source/Asset/Public/Asset/EAssetResult.h`
- [X] T011 Extend generic cook requests/results with immutable typed parameters, typed payloads, diagnostics, and all-or-nothing failure defaults, then verify legacy Feature 020 cook participants preserve null-parameter behavior in `Source/Asset/Public/Asset/IAssetCooker.h` and `Tests/AssetCoreTests.cpp`
- [X] T012 Extend generic load requests/results with immutable typed parameters, diagnostics, and backward-compatible null defaults, then verify legacy Feature 020 load participants preserve null-parameter behavior in `Source/Asset/Public/Asset/IAssetLoader.h` and `Tests/AssetCoreTests.cpp`
- [X] T013 Implement normalized KTX2 diagnostic formatting and native-code/path/address redaction in `Source/Asset/Public/Asset/FAssetDiagnostics.h` and `Source/Asset/Private/FAssetDiagnostics.cpp`
- [X] T014 Define compression policy, quality, limits, settings, and typed cook/load parameters with validated defaults in `Source/Asset/Public/Asset/FTextureCook.h`
- [X] T015 Define immutable KTX2 level, info, artifact identity, and inspection/open contracts in `Source/Asset/Public/Asset/FKTX2TextureArtifact.h` and `Source/Asset/Public/Asset/FKTX2TextureCodec.h`
- [X] T016 Define Asset-owned transcode formats, request-scoped mip payloads, exact block footprints, and transactional result contracts in `Source/Asset/Public/Asset/FTextureTranscode.h`
- [X] T017 Export Feature 022 CPU-only public vocabulary without exposing KTX, WAMR, RHI, Renderer, Backend, or Vulkan headers in `Source/Asset/Public/Asset/AssetMinimal.h`
- [X] T018 [P] Define the private `IKTX2Encoder` Strategy contract and declare the narrow memory-only WAMR host adapter, encoder ABI bounds, module hash/version checks, and request-owned instance contract in `Source/Asset/Private/IKTX2Encoder.h` and `Source/Asset/Private/FWamrEncoderRuntime.h`
- [X] T019 [P] Declare private libktx container/transcode ownership wrappers with request-local handles in `Source/Asset/Private/FKTX2ContainerCodec.h`
- [X] T020 Extend the architecture verifier to allow vendored headers only in approved private adapters and reject forbidden dependencies in `Tests/verify_asset_layer.py`
- [X] T021 Add offline provenance validation plus the no-shell pinned `ktx validate 4.4.2` adapter and unit coverage for version/checksum, warnings-as-errors, malformed JSON, timeout, and missing-local-tool behavior in `Tests/verify_ktx2_provenance.py`, `Tests/verify_ktx2_artifacts.py`, and `Tests/test_verify_ktx2_artifacts.py`
- [X] T022 Add a focused test that cooks the golden corpus repeatedly through the authoritative module and compares exact bytes, metadata, diagnostics, and digests in `Tests/AssetKTX2Tests.cpp`
- [X] T023 Implement bounded module loading, interpreter initialization, per-request linear memory, trap/status translation, and range-checked output copy in `Source/Asset/Private/FWamrEncoderRuntime.cpp`
- [X] T024 Implement `FCanonicalBasisEncoder` as the `IKTX2Encoder` Strategy using fixed Balanced/High ETC1S/UASTC profile serialization and an authoritative one-thread/no-SIMD/no-RDO path that accepts canonical settings plus ordered raw mips and returns final compressed KTX2 bytes in `Source/Asset/Private/FCanonicalBasisEncoder.cpp`
- [X] T025 Record canonical module output hashes for every golden input and make any host mismatch fail before broader Feature 022 implementation in `Tests/Fixtures/KTX2/README.md` and `Tests/AssetKTX2Tests.cpp`
- [X] T026 Build the shared foundation, run the focused encoder/provenance proof locally, and add a minimal Windows/macOS/Linux CI digest job that must match the golden module outputs before US1 implementation in `.github/workflows/ci.yml` and `specs/022-ktx2-cooking-compression/quickstart.md`

**Checkpoint**: The checked-in encoder and interpreter produce the expected
authoritative bytes locally, generic APIs remain source-compatible, and Asset
still depends only on Core.

---

## Phase 3: User Story 1 - Cook Portable Texture Artifacts (Priority: P1) MVP

**Goal**: Cook a validated Feature 021 texture into one deterministic,
standards-conforming, inspectable KTX2 artifact with stable identity and full
mip/semantic evidence.

**Independent Test**: Run `StonerTest --suite asset` over all valid fixtures,
emit the current KTX2 outputs, run `Tests/verify_ktx2_artifacts.py` with pinned
`ktx validate 4.4.2`, reopen every output, compare source and artifact
contracts, and verify 20 repeated cooks yield identical bytes and digests
without any GPU.

### Tests for User Story 1

- [X] T027 [US1] Add semantic policy and pre-cook rejection cases for Balanced/High ETC1S, Balanced/High UASTC, uncompressed, HDR, sRGB, normal, data, lossy opt-in, alpha, and base-only/full-chain combinations in `Tests/AssetKTX2Tests.cpp`
- [X] T028 [US1] Add exact canonical metadata, DFD, Basis model, supercompression, level order, alignment, identity, and cook-revision assertions in `Tests/AssetKTX2Tests.cpp`
- [X] T029 [US1] Add uncompressed R8/RG8/RGB8/RGBA8/RGB32F/RGBA16F/RGBA32F cook and reopen cases in `Tests/AssetKTX2Tests.cpp`
- [X] T030 [US1] Add ETC1S/UASTC odd-extent, sub-block terminal mip, straight-alpha, size, color-PSNR, and normal angular-error cases in `Tests/AssetKTX2Tests.cpp`
- [X] T031 [US1] Add same-AssetId/different-cook-revision and runtime-target-profile-does-not-recook regression cases in `Tests/AssetKTX2Tests.cpp`

### Implementation for User Story 1

- [X] T032 [US1] Implement validated defaults, exact Balanced/High parameter mapping, semantic policy resolution, canonical enum tokens, and cook-revision serialization in `Source/Asset/Private/FTextureCookPolicy.cpp`
- [X] T033 [US1] Implement immutable KTX2 artifact construction, exact-byte digest validation, Texture type traits, and source/cooked identity invariants in `Source/Asset/Private/FKTX2TextureArtifact.cpp`
- [X] T034 [US1] Implement checked structural preflight for the 80-byte header, level index, dimensions, 2D scope, offsets, ranges, alignment, overlap, and configured budgets in `Source/Asset/Private/FKTX2Preflight.h` and `Source/Asset/Private/FKTX2Preflight.cpp`
- [X] T035 [US1] Implement host-side canonical key/value insertion, DFD/transfer/alpha/orientation mapping, explicit mip writing, and memory serialization for uncompressed KTX2 in `Source/Asset/Private/FKTX2ContainerCodec.cpp`
- [X] T036 [US1] Implement uncompressed LDR/HDR KTX2 writing without hidden RGB expansion or precision reduction in `Source/Asset/Private/FKTX2ContainerCodec.cpp`
- [X] T037 [US1] Consume complete final KTX2 bytes from the canonical WebAssembly ETC1S/UASTC encoder without host rewriting, then preflight and reopen them for ETC1S BasisLZ and UASTC no-supercompression validation in `Source/Asset/Private/FCanonicalBasisEncoder.cpp` and `Source/Asset/Private/FKTX2ContainerCodec.cpp`
- [X] T038 [US1] Implement CPU-only inspect/open normalization for identity, digests, policy, semantic, transfer, alpha, orientation, dimensions, levels, DFD, SGD, and metadata uniqueness in `Source/Asset/Private/FKTX2TextureCodec.cpp`
- [X] T039 [US1] Reopen every newly cooked byte stream and compare normalized facts with the source texture and resolved settings before success in `Source/Asset/Private/FKTX2TextureCooker.cpp`
- [X] T040 [US1] Implement `FKTX2TextureCooker` typed-request validation, full-mip input checks, transactional output, diagnostics, and generic artifact/payload/digest agreement in `Source/Asset/Private/FKTX2TextureCooker.cpp`
- [X] T041 [US1] Register the KTX2 cooker through the existing scoped extension registry and preserve execution leases in `Source/Asset/Private/AssetModule.cpp`
- [X] T042 [US1] Complete the 18-artifact policy, metadata, mip, size, quality, identity, reopen, 20-run determinism, and pinned independent-validator coverage before the US1 checkpoint in `Tests/AssetKTX2Tests.cpp` and `Tests/verify_ktx2_artifacts.py`

**Checkpoint**: US1 produces one portable immutable KTX2 artifact per cook
policy, passes engine reopen validation and the pinned independent validator,
preserves the source AssetId, and requires no RHI or backend.

---

## Phase 4: User Story 2 - Select a Supported Runtime Representation (Priority: P1)

**Goal**: Select and produce exactly one deterministic semantic-compatible BC,
ETC2/EAC, ASTC 4x4, or uncompressed request-scoped representation from a cooked
artifact and device capability snapshot.

**Independent Test**: Run Asset transcode tests and a 36-or-more-case Renderer
capability matrix; reorder capability records and registrations and verify
selection, diagnostics, footprints, and fallback bytes remain identical.

### Tests for User Story 2

- [X] T043 [P] [US2] Add complete-mip Basis transcode tests for every Asset target format, transfer variant, channel class, odd extent, terminal mip, and exact target footprint in `Tests/AssetKTX2Tests.cpp`
- [X] T044 [P] [US2] Add at least 36 ordered target/capability cases covering every required format, usage rejection, alpha/channel preservation, and permutation independence in `Tests/RendererKTX2TextureTests.h` and `Tests/RendererKTX2TextureTests.cpp`
- [X] T045 [US2] Add fallback-enabled/disabled cases proving uncompressed LDR bytes come from the authoritative Basis payload and create no second Asset or cache record in `Tests/RendererKTX2TextureTests.cpp`

### Implementation for User Story 2

- [X] T046 [US2] Implement Asset transcode target validation for codec, transfer, semantic channels, alpha, HDR, and generic-data restrictions in `Source/Asset/Private/FBasisTextureTranscoder.cpp`
- [X] T047 [US2] Implement checked per-mip block counts, row pitches, byte lengths, aggregate budgets, and all-mips-or-none publication in `Source/Asset/Private/FBasisTextureTranscoder.cpp`
- [X] T048 [US2] Implement request-scoped BC1/3/4/5/7, ETC2 RGB/RGBA, EAC R/RG, ASTC 4x4, and R/RG/RGBA fallback transcoding in `Source/Asset/Private/FTextureTranscode.cpp`
- [X] T049 [US2] Ensure transcode failures drop all temporary levels and expose no registry, static cache, retained handle, or partial payload in `Source/Asset/Private/FTextureTranscode.cpp`
- [X] T050 [US2] Define Renderer-owned target profiles, stable candidate diagnostics, target selection results, and Asset-to-RHI transcode mapping in `Source/Renderer/Public/Renderer/FTextureTargetProfile.h`
- [X] T051 [US2] Implement valid profile checks for duplicates, unknown/depth formats, transfer variants, semantic channels, alpha, and fallback policy in `Source/Renderer/Private/FTextureTargetSelection.cpp`
- [X] T052 [US2] Implement explicit profile-order selection using sampled-image plus copy-destination capabilities without relying on enum, capability, registration, or container order in `Source/Renderer/Private/FTextureTargetSelection.cpp`
- [X] T053 [US2] Implement default desktop BC-before-ASTC-before-ETC2/EAC-before-uncompressed profiles specialized for opaque color, alpha color, two-channel normal/data, and one-channel data in `Source/Renderer/Private/FTextureTargetSelection.cpp`
- [X] T054 [US2] Aggregate Feature 022 selection coverage into the existing `renderer-texture` suite without replacing Feature 021 cases in `Tests/Main.cpp`, `Tests/RendererKTX2TextureTests.cpp`, and `Tests/RendererTextureAssetTests.cpp`
- [X] T055 [US2] Complete request-scope lifetime, fallback provenance, exact-footprint, capability-order, registration-order, and normalized selection diagnostic coverage in `Tests/AssetKTX2Tests.cpp` and `Tests/RendererKTX2TextureTests.cpp`

**Checkpoint**: US2 deterministically selects and produces one complete
request-scoped CPU representation without creating a GPU resource or changing
the authoritative artifact.

---

## Phase 5: User Story 3 - Realize Compressed Textures Through RHI (Priority: P2)

**Goal**: Represent, create, upload, and conditionally validate block-compressed
2D textures through Renderer, RHI, and Vulkan while preserving the Asset
boundary and exact rollback behavior.

**Independent Test**: Run `--suite rhi`, `--suite renderer-texture`, and
`--suite vulkan` with mock capability/upload capture for all required formats,
then run conditional native Vulkan evidence only where both required usages are
advertised.

### Tests for User Story 3

- [X] T056 [US3] Add total `FRHIFormatInfo` table, compressed/uncompressed footprint, overflow, and `GetRHIFormatByteSize` compatibility cases in `Tests/RHICoreTests.cpp`
- [X] T057 [US3] Add compressed upload-region cases for block-aligned origins, edge extents, padded rows, insufficient bytes, odd extents, and 1x1 through 3x3 terminal mips in `Tests/RHICoreTests.cpp`
- [X] T058 [P] [US3] Add mock Renderer realization tests for selected format, all ascending mip uploads, exact descriptors/bytes, capability rejection, and create/upload rollback in `Tests/RendererKTX2TextureTests.cpp`
- [X] T059 [P] [US3] Add Vulkan mapping, synthetic usage-capability, compressed staging, and failure cleanup cases in `Tests/VulkanBackendTests.cpp` and `Tests/VulkanBackendTests.h`
- [X] T060 [P] [US3] Add conditional native Vulkan format-query, compressed create/upload, and raw-block readback or sampled-evidence cases in `Tests/VulkanNativeIntegrationTests.cpp` and `Tests/VulkanNativeIntegrationTests.h`

### Implementation for User Story 3

- [X] T061 [US3] Add every required BC, ETC2/EAC, and ASTC linear/sRGB member plus total validity and depth/color classification in `Source/RHI/Public/RHI/ERHIFormat.h`
- [X] T062 [US3] Implement `FRHIFormatInfo` and checked block-count, row, slice, and total-footprint helpers while keeping `GetRHIFormatByteSize` uncompressed-only in `Source/RHI/Public/RHI/FRHIFormatInfo.h`
- [X] T063 [US3] Replace `SupportedFormats` with normalized per-format usage records and compatibility queries in `Source/RHI/Public/RHI/FRHIDeviceCapabilities.h`
- [X] T064 [US3] Implement block-aware texture upload validation for aligned origins, legal edge extents, row pitch, slices, and exact required bytes in `Source/RHI/Public/RHI/FRHITextureUploadDesc.h`
- [X] T065 [US3] Implement block-aware buffer-copy layout and destination byte-size validation in `Source/RHI/Public/RHI/FRHITextureBufferCopyRegion.h`
- [X] T066 [US3] Export format information and capability vocabulary and migrate mock device/allocation callers from bytes-per-texel assumptions in `Source/RHI/Public/RHI/RHIMinimal.h` and `Source/RHI/Private/RHIModule.cpp`
- [X] T067 [US3] Define the Renderer KTX2 realization request, result, stages, diagnostics, and no-partial-resource contract in `Source/Renderer/Public/Renderer/FKTX2TextureRealization.h`
- [X] T068 [US3] Implement artifact validation, target selection, request-scoped transcode, RHI footprint cross-check, texture creation, ascending upload, and final publication in `Source/Renderer/Private/FKTX2TextureRealization.cpp`
- [X] T069 [US3] Implement exactly-once texture rollback and transient CPU payload release for creation and every mip upload failure in `Source/Renderer/Private/FKTX2TextureRealization.cpp`
- [X] T070 [US3] Export the new target-profile and realization APIs without altering Feature 021 raw-texture behavior in `Source/Renderer/Public/Renderer/RendererMinimal.h`
- [X] T071 [US3] Map every Feature 022 RHI format to its `VkFormat` and derive sampled/copy-destination usage flags from optimal-tiling properties in `Source/Backend/Vulkan/Private/FVulkanPhysicalDevice.cpp` and `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanPhysicalDevice.h`
- [X] T072 [US3] Migrate Vulkan texture allocation, staging, command-copy, and upload validation to common RHI block footprints in `Source/Backend/Vulkan/Private/FVulkanTexture.cpp`, `Source/Backend/Vulkan/Private/FVulkanUploadStaging.cpp`, and `Source/Backend/Vulkan/Private/FVulkanCommandBuffer.cpp`
- [X] T073 [US3] Implement native compressed image creation and tightly packed logical-extent uploads without backend-local footprint recomputation in `Source/Backend/Vulkan/Private/FVulkanDevice.cpp` and `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp`
- [X] T074 [US3] Migrate existing Feature 008/018/019 format, upload, staging, and readback callers to the new format-capability and footprint source of truth in `Tests/RHICoreTests.cpp`, `Tests/VulkanBackendTests.cpp`, `Tests/VulkanNativeIntegrationTests.cpp`, and `Source/Renderer/Private/FTextureAssetRealization.cpp`
- [X] T075 [US3] Complete mock and native agreement, sub-block mip, usage-query, creation/upload failure, exactly-once release, and source/artifact survival coverage in `Tests/RendererKTX2TextureTests.cpp`, `Tests/VulkanBackendTests.cpp`, and `Tests/VulkanNativeIntegrationTests.cpp`

**Checkpoint**: US3 realizes every supported representation through the
backend-neutral contract, and any failure returns no partial RHI texture while
leaving the cooked Asset inspectable.

---

## Phase 6: User Story 4 - Reject Corrupt or Unsafe Cooked Content (Priority: P2)

**Goal**: Inspect, load, and transcode untrusted local KTX2 bytes with bounded
work, checked arithmetic, stable diagnostics, lease safety, and no partial
publication.

**Independent Test**: Run the at-least-40-case mutation matrix, exact limit and
first-value-above cases, corrupt Basis cases, and at-least-eight-way concurrent
cook/inspect/load/transcode tests under normal and sanitizer builds.

### Tests for User Story 4

- [X] T076 [US4] Add an at-least-40-case deterministic matrix covering missing, inaccessible, unsupported, malformed, truncated, contradictory, overlapping, misaligned, and corrupt KTX2 sources with expected result/stage/field/level/limit diagnostics in `Tests/AssetKTX2Tests.cpp` and `Tests/Fixtures/KTX2/README.md`
- [X] T077 [US4] Add exact-limit and first-value-above cases for dimensions, artifact, metadata, key/value count, level bytes, mip count, and aggregate target payload in `Tests/AssetKTX2Tests.cpp`
- [X] T078 [US4] Add corrupt ETC1S/UASTC global-data and per-level failures proving no partial transcode payload escapes in `Tests/AssetKTX2Tests.cpp`
- [X] T079 [US4] Add at-least-eight concurrent cook/inspect/load/transcode, unregister-during-execution, and serial-output equality cases in `Tests/AssetKTX2Tests.cpp`

### Implementation for User Story 4

- [X] T080 [US4] Harden structural preflight with checked `uint64` addition/multiplication and rejection before libktx image-data allocation in `Source/Asset/Private/FKTX2Preflight.cpp`
- [X] T081 [US4] Enforce DFD, KVD, SGD, required-key uniqueness, UTF-8, orientation, alpha, transfer, semantic, and mip-contract consistency during open in `Source/Asset/Private/FKTX2TextureCodec.cpp`
- [X] T082 [US4] Implement bounded source-lease reading, typed expected identity/limits, one-pass open, and all-or-nothing `FKTX2TextureLoader` publication in `Source/Asset/Private/FKTX2TextureLoader.cpp`
- [X] T083 [US4] Register the KTX2 loader while preserving Feature 020 ambiguity, unregister, and active execution-lease behavior in `Source/Asset/Private/AssetModule.cpp`
- [X] T084 [US4] Translate libktx, Basis, and WAMR failures into stable project result/stage/field/level evidence without exposing raw native text in `Source/Asset/Private/FKTX2TextureCodec.cpp`, `Source/Asset/Private/FBasisTextureTranscoder.cpp`, and `Source/Asset/Private/FWamrEncoderRuntime.cpp`
- [X] T085 [US4] Complete malformed, over-limit, corrupt-payload, no-registry-mutation, no-partial-output, lease, and concurrency coverage in `Tests/AssetKTX2Tests.cpp`
- [X] T086 [US4] Re-run the Asset boundary verifier and prove all public Asset and production Asset dependencies remain Core-only in `Tests/verify_asset_layer.py`

**Checkpoint**: Untrusted KTX2 data is rejected before excessive allocation or
publication, and concurrent immutable operations preserve single-request
results and active registration leases.

---

## Phase 7: Polish and Cross-Cutting Validation

**Purpose**: Complete independent standards evidence, reproducible reports,
strict local gates, sanitizer coverage, cross-platform automation, and delivered
feature documentation.

- [X] T087 [P] Audit the early independent-validator adapter against the final generated/golden corpus and normalize stable cross-platform report fields in `Tests/verify_ktx2_artifacts.py`
- [X] T088 [P] Extend validator adapter regression coverage for final report aggregation, mixed input directories, duplicate artifacts, and deterministic ordering in `Tests/test_verify_ktx2_artifacts.py`
- [X] T089 Add test-binary options to emit current KTX2 artifacts, run 20 deterministic cooks, and write normalized determinism/quality/size reports in `Tests/Main.cpp`, `Tests/TestSuiteRegistry.cpp`, and `Tests/AssetKTX2Tests.cpp`
- [X] T090 Reconcile all golden SHA-256 values, source facts, malformed mutation provenance, quality thresholds, and size thresholds in `Tests/Fixtures/KTX2/README.md`
- [X] T091 Provision or build the checksum-verified pinned `ktx` 4.4.2 CLI independently from the engine library, conditionally rebuild and hash-check the encoder WebAssembly module when the pinned toolchain is available, then add provenance, generated/golden validation, focused suites, strict Release, and report-artifact steps after local tests in `.github/workflows/ci.yml`
- [X] T092 Update Linux ASan/UBSan and focused ThreadSanitizer gates for malformed WAMR/libktx inputs and eight concurrent immutable requests in `.github/workflows/ci.yml`
- [X] T093 Add conditional Linux Vulkan compressed-format evidence that fails on capability/behavior disagreement but records unavailable support without treating fallback as native success in `.github/workflows/ci.yml`
- [X] T094 Run the documented strict Debug build, Asset/Renderer-texture/RHI/Vulkan suites, architecture/provenance checks, and full regression on macOS using `specs/022-ktx2-cooking-compression/quickstart.md`
- [X] T095 Run the documented strict Release build, 20-run deterministic report, and available native Vulkan evidence on macOS using `specs/022-ktx2-cooking-compression/quickstart.md`
- [X] T096 Run the pinned independent validator over generated and golden artifacts and record normalized local evidence under `Validation/022/`
- [X] T097 Run Windows and Linux focused suites, full regression, strict Release, and compare 20-run golden digests with macOS through the CI matrix in `.github/workflows/ci.yml`
- [X] T098 Run Linux ASan/UBSan focused/full suites and focused ThreadSanitizer concurrency validation through `.github/workflows/ci.yml`
- [X] T099 Update delivered-feature architecture, dependency provenance, validation evidence, exclusions, and roadmap status in `doc/022-ktx2-cooking-compression.html`, `doc/SYSTEM_DESIGN.MD`, and `doc/roadmap.md`
- [X] T100 Re-run every applicable command in `specs/022-ktx2-cooking-compression/quickstart.md` and reconcile final implementation status against `specs/022-ktx2-cooking-compression/spec.md`, `specs/022-ktx2-cooking-compression/plan.md`, and `specs/022-ktx2-cooking-compression/tasks.md`

---

## Dependencies and Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies. T001-T004 can begin in parallel; fixture
  population and build wiring depend on their corresponding provenance and
  matrix definitions.
- **Foundational (Phase 2)**: Depends on Setup. T021 establishes the independent
  validator, and T022-T026 form a blocking local plus three-platform
  deterministic encoder proof; no story implementation starts until it passes.
- **US1 (Phase 3)**: Depends on Foundation and produces canonical artifacts
  consumed by every later story.
- **US2 (Phase 4)**: Depends on US1 and produces complete request-scoped runtime
  representations without requiring GPU creation.
- **US3 (Phase 5)**: Depends on US2 and adds RHI/Renderer/Vulkan realization.
- **US4 (Phase 6)**: Depends on US2 because corrupt-payload and diagnostic
  coverage exercises the completed transcode path. It can proceed in parallel
  with US3; final hardening must preserve the shared artifact contracts.
- **Polish (Phase 7)**: Depends on all four stories. CI integration intentionally
  follows working local/focused gates.

### User Story Dependency Graph

```text
Setup -> Foundational -> US1 -> US2 -> US3 -> Polish
                                     \-> US4 ---/
```

### Within Each User Story

- Write the listed tests first and confirm they fail for the intended missing
  contract before implementing production behavior.
- Complete public data contracts before private adapters and orchestration.
- Keep Asset codec/transcode work CPU-only; Renderer performs the only
  Asset-to-RHI mapping, and Vulkan remains the only native implementation.
- Publish only complete artifacts, transcode payloads, and RHI textures.
- Treat unavailable native compressed-format support as an explicit skip, never
  as evidence supplied by deterministic fallback mode.

## Parallel Execution Examples

### User Story 1

```text
T027-T031 share Tests/AssetKTX2Tests.cpp and are intentionally serialized.
After policy/artifact contracts stabilize, container work T034-T038 proceeds
before cooker orchestration T039-T041 and final corpus closure T042.
```

### User Story 2

```text
T043 prepares Asset transcode coverage while T044 prepares the separate
Renderer capability matrix. After both are written, complete Asset transcode
T046-T049, then Renderer selection T050-T054, and close with T055.
```

### User Story 3

```text
T056 and T057 are serialized because they share Tests/RHICoreTests.cpp.
T058-T060 can be prepared in parallel across Renderer, Vulkan fallback, and
native integration test files. Production remains ordered from RHI
format/footprint contracts T061-T066, through Renderer T067-T070, to Vulkan
T071-T073 and migration/closure T074-T075.
```

### User Story 4

```text
T076-T079 are separate test groups in one shared file and remain serialized.
Loader work T082-T083 can begin after preflight/open hardening T080-T081 while
diagnostic translation T084 follows the final codec/transcode behavior.
```

## Implementation Strategy

### MVP First

1. Complete Setup and Foundation, including the authoritative encoder proof.
2. Complete US1 through T042.
3. Run `StonerTest --suite asset`,
   `python3 Tests/verify_asset_layer.py`, and
   `python3 Tests/verify_ktx2_provenance.py`, then validate emitted artifacts
   with `python3 Tests/verify_ktx2_artifacts.py`.
4. Review exact artifact bytes, identity/revision separation, normalized
   metadata, and 20-run determinism before adding runtime target formats.

### Incremental Delivery

1. US1 delivers deterministic portable KTX2 cooking and CPU reopen.
2. US2 adds complete request-scoped transcode and deterministic selection.
3. US3 adds block-aware RHI/Vulkan realization and rollback.
4. US4 hardens the completed data path against malformed and concurrent input.
5. Phase 7 integrates the Foundation-established independent oracle into final
   reporting and adds expensive cross-platform gates only after focused local
   behavior is stable.

### Parallel Team Strategy

1. One developer owns the deterministic Asset codec path through US1.
2. After US1 artifact contracts stabilize, a second developer can prepare RHI
   format/footprint tests while the first implements US2 transcode.
3. A third developer can prepare malformed fixtures and native Vulkan tests,
   but final integration waits for the corresponding Asset and RHI contracts.
4. Keep Asset, RHI, Renderer, Vulkan, test, and CI changes in separate logical
   commits to preserve dependency reviewability.

## Notes

- `[P]` is intentionally limited to different files or independently
  preparable test work.
- T026 is the minimal early local and cross-host deterministic gate; T097 is
  the later full-feature cross-host proof. Neither may be satisfied by the
  native Basis encoder.
- Except for T026's intentionally minimal digest job, T091-T098 use CI for
  cross-platform and sanitizer evidence after local gates, not as the primary
  implementation feedback loop.
- Runtime streaming, package manifests, platform-pretranscoded variants,
  cross-request caches, retained handles, and request coalescing remain Feature
  025/026 work and must not be introduced here.
- Commit logical clusters with the repository convention, for example
  `feat(asset): cook deterministic ktx2 texture artifacts`,
  `feat(rhi): add block-compressed texture formats`, or
  `test(vulkan): validate compressed texture uploads`.
