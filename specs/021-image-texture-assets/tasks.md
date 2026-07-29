# Tasks: Image & Texture Asset Foundation

**Input**: Design documents from `/specs/021-image-texture-assets/`
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`,
`contracts/image-texture-asset-api.md`, and `quickstart.md`

**Tests**: Required. The specification requires deterministic CPU import and mip
coverage, Asset boundary enforcement, mock-RHI realization coverage, native
upload/readback smoke coverage where available, three-platform CI, and Linux
ASan/UBSan validation.

**Organization**: Tasks are grouped by user story. The Setup and Foundational
phases establish the shared boundaries that every story needs; each user-story
phase then ends with its own independently runnable validation checkpoint.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Safe to execute in parallel because the task edits different files
  and has no incomplete-task dependency.
- **[Story]**: Maps the task to the corresponding user story in `spec.md`.
- Every task includes an exact target path.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish reproducible decoder provenance, fixture ownership, and
the build/test wiring required before Image/Texture code is introduced.

- [x] T001 [P] Vendor pinned stb_image 2.30 with upstream provenance, SHA-256, and license in `ThirdParty/stb/stb_image.h`, `ThirdParty/stb/VERSION`, and `ThirdParty/stb/LICENSE.md`
- [x] T002 [P] Define the fixture manifest with source digests, decoded facts, a minimum 12-file valid / 30-case negative coverage matrix, and asymmetric JPEG APP1/PNG eXIf orientation expectations in `Tests/Fixtures/Images/README.md`
- [x] T003 Add at least 12 valid PNG, JPEG, and Radiance HDR fixtures covering grayscale, straight alpha, odd dimensions, HDR range, asymmetric JPEG APP1 orientation, and asymmetric PNG eXIf orientation under `Tests/Fixtures/Images/Valid/`
- [x] T004 Add the malformed and unsupported binary seeds required for the at-least-30-case bounded mutation matrix under `Tests/Fixtures/Images/Invalid/`
- [x] T005 Configure Asset-private third-party compilation and warning isolation for stb in `Source/Asset/SConscript` and `site_scons/LayerBuilder.py`
- [x] T006 Register the Feature 021 source and focused test targets without changing the existing `--suite asset` command in `Source/Asset/SConscript` and `Tests/SConscript`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Establish Asset-only types, generic import bridging, diagnostics,
and architecture enforcement. No user story starts before this phase completes.

**Critical**: Asset production code must still depend only on Core. It must not
include RHI, Renderer, Backend, graphics API, or third-party decoder headers
outside the approved private wrapper.

- [x] T007 Extend image-specific failure results and import stages while preserving existing Feature 020 meanings in `Source/Asset/Public/Asset/EAssetResult.h` and `Source/Asset/Public/Asset/FAssetDiagnostics.h`
- [x] T008 Implement stable normalized image diagnostic construction and inspection output in `Source/Asset/Private/FAssetDiagnostics.cpp` and `Source/Asset/Private/FAssetInspection.cpp`
- [x] T009 Define immutable import parameters and the backward-compatible generic request bridge in `Source/Asset/Public/Asset/FAssetImportRequest.h` and `Source/Asset/Public/Asset/IAssetImporter.h`
- [x] T010 Route request-based imports through existing selection and execution leases while preserving the legacy importer overload in `Source/Asset/Public/Asset/FAssetDispatch.h` and `Source/Asset/Private/FAssetDispatch.cpp`
- [x] T011 Add checked bounded source-byte access suitable for probes and decoders in `Source/Asset/Public/Asset/FAssetSource.h` and `Source/Asset/Private/FAssetSource.cpp`
- [x] T012 Define checked extent, canonical format, semantic, origin, alpha, mip-policy, HDR-layout, settings, and limits types in `Source/Asset/Public/Asset/FImageTypes.h`
- [x] T013 Define immutable tightly packed mip validation and checked byte-size helpers in `Source/Asset/Public/Asset/FImageMip.h` and `Source/Asset/Private/FImageValidation.h`
- [x] T014 Export the new generic-import and image vocabulary from the public Asset aggregate without exposing decoder implementation in `Source/Asset/Public/Asset/AssetMinimal.h`
- [x] T015 Update Feature 020 regression coverage for legacy importer compatibility, lease preservation, and request dispatch in `Tests/AssetCoreTests.cpp` and `Tests/AssetCoreTests.h`
- [x] T016 Extend Asset-boundary verification to permit only the private stb wrapper and reject forbidden layer/third-party includes elsewhere in `Tests/verify_asset_layer.py`
- [x] T017 Add the Feature 021 aggregate suite registration while retaining Feature 020 coverage under `--suite asset` in `Tests/AssetTests.h`, `Tests/AssetTests.cpp`, `Tests/TestSuiteRegistry.cpp`, and `Tests/TestSuiteRegistryTests.cpp`
- [x] T018 Verify the shared base compiles and the Asset boundary remains clean with `Tests/verify_asset_layer.py` and `Tests/AssetCoreTests.cpp`

**Checkpoint**: Asset import vocabulary, checked input access, diagnostics, test
aggregation, and boundary enforcement are ready. User-story work may begin.

---

## Phase 3: User Story 1 - Import Source Images as Stable Assets (Priority: P1) MVP

**Goal**: Import a supported PNG, JPEG, or HDR source into one immutable Image
asset plus one dependent Texture asset with stable identity, metadata, source
provenance, canonical base pixels, and atomic registry publication.

**Independent Test**: Run `StonerTest --suite asset` with the valid fixture
corpus and verify identical Image/Texture identities, metadata, decoded base
bytes, content digests, and diagnostics across repeated imports without a GPU.

### Tests for User Story 1

- [x] T019 [US1] Add deterministic assertions for every fixture in the at-least-12 valid matrix, including canonical format, metadata, JPEG APP1/PNG eXIf top-left-oriented base pixels, inspection transform evidence, and exact CPU evidence in `Tests/AssetImageTextureTests.h` and `Tests/AssetImageTextureTests.cpp`
- [x] T020 [US1] Add source-probe tests for absent, uppercase, and misleading extensions in `Tests/AssetImageTextureTests.cpp`
- [x] T021 [US1] Add source/texture identity, provenance, digest, dependency, same-source multi-semantic, and one-HDR-source default-RGBA16F/explicit-RGBA32F/explicit-RGB32F distinct-version regression cases in `Tests/AssetImageTextureTests.cpp`

### Implementation for User Story 1

- [x] T022 [US1] Implement immutable `FImageAsset` payload validation and base-level content-digest evidence in `Source/Asset/Public/Asset/FImageAsset.h` and `Source/Asset/Private/FImageAsset.cpp`
- [x] T023 [US1] Implement immutable `FTextureAsset` payload validation, image dependency, chain invariants, and texture content-digest evidence in `Source/Asset/Public/Asset/FTextureAsset.h` and `Source/Asset/Private/FImageAsset.cpp`
- [x] T024 [US1] Declare bounded format inspection results and metadata vocabulary in `Source/Asset/Public/Asset/FImageInspection.h` and `Source/Asset/Private/FImageContainerInspector.h`
- [x] T025 [US1] Implement PNG signature, chunk, CRC, palette/transparency, transfer/profile, eXIf orientation, feature-policy, and limit inspection in `Source/Asset/Private/FImageContainerInspector.cpp`
- [x] T026 [US1] Implement JPEG signature, marker, dimensions, APP1 orientation, color-metadata, and limit inspection in `Source/Asset/Private/FImageContainerInspector.cpp`
- [x] T027 [US1] Implement Radiance HDR signature, header, dimensions, orientation, and bounded metadata inspection in `Source/Asset/Private/FImageContainerInspector.cpp`
- [x] T028 [US1] Define the decoder-neutral decoded-raster contract and canonical conversion entry points in `Source/Asset/Private/FImageDecode.h` and `Source/Asset/Private/FImageDecode.cpp`
- [x] T029 [US1] Implement the sole `STB_IMAGE_IMPLEMENTATION` wrapper with `STBI_NO_STDIO`, `STBI_NO_SIMD`, and PNG/JPEG/HDR-only configuration in `Source/Asset/Private/FStbImageDecode.cpp`
- [x] T030 [US1] Implement checked LDR canonicalization for gray, gray-plus-alpha, RGB, RGBA, palette expansion, and 1/2/4/8/16-bit normalization in `Source/Asset/Private/FImageDecode.cpp`
- [x] T031 [US1] Implement finite HDR canonicalization with default RGBA16F plus explicit RGBA32F/RGB32F range validation in `Source/Asset/Private/FImageDecode.cpp`
- [x] T032 [US1] Implement DX/Unreal top-left orientation normalization without leaving decoder-specific orientation state in payloads in `Source/Asset/Private/FImageOrientation.h` and `Source/Asset/Private/FImageOrientation.cpp`
- [x] T033 [US1] Implement `FImageAssetImporter` format hints, <=64 KiB side-effect-free probes, typed parameter validation, and deterministic PNG/JPEG/HDR selection in `Source/Asset/Public/Asset/FImageImport.h` and `Source/Asset/Private/FImageImport.cpp`
- [x] T034 [US1] Implement `FAssetImportService::ImportAndRegister` candidate validation and one-batch Image/Texture metadata publication with all-or-nothing outputs in `Source/Asset/Public/Asset/FImageImport.h`, `Source/Asset/Private/FImageImport.cpp`, and `Source/Asset/Private/FAssetRegistry.cpp`
- [x] T035 [US1] Register the production image importer through the existing scoped extension registry without changing dispatch tie behavior in `Source/Asset/Private/AssetModule.cpp` and `Source/Asset/Private/FAssetExtensionRegistry.cpp`
- [x] T036 [US1] Complete valid source import, canonical base-payload, stable identity, provenance, atomic-publication, and deterministic image/texture/semantic/color/alpha/mip/importer/diagnostic inspection-summary plus native-address-redaction coverage in `Tests/AssetImageTextureTests.cpp`

**Checkpoint**: Supported source images import into deterministic CPU-only Image
and Texture assets; no RHI device is needed for the US1 test suite.

---

## Phase 4: User Story 2 - Preserve Texture Meaning Across Mip Levels (Priority: P1)

**Goal**: Generate deterministic complete or explicit base-only mip chains that
preserve Color, Normal, and Data semantics.

**Independent Test**: Generate chains from color, tangent-normal, and
multi-channel data fixtures; inspect every level and compare bytes/digests over
20 repeated runs, including odd and non-square dimensions.

### Tests for User Story 2

- [x] T037 [US2] Add sRGB linear-light, straight-alpha, and linear color mip cases with exact canonical CPU-byte references in `Tests/AssetImageTextureTests.cpp`
- [x] T038 [US2] Add normal-vector reconstruction, renormalization, zero-vector fallback, no-sRGB, and decoded `abs(length - 1.0) <= 0.015` cases in `Tests/AssetImageTextureTests.cpp`
- [x] T039 [US2] Add generic-data exact-byte, odd/non-square, one-pixel, base-only, full-chain, limit, and 20-run digest determinism cases in `Tests/AssetImageTextureTests.cpp`

### Implementation for User Story 2

- [x] T040 [US2] Define mip-generation validation, semantic dispatch, and fixed extent-recurrence helpers in `Source/Asset/Private/FImageMipGenerator.h` and `Source/Asset/Private/FImageValidation.cpp`
- [x] T041 [US2] Implement fixed-order scalar full-footprint color resampling with checked literal transfer tables and independently averaged straight alpha in `Source/Asset/Private/FImageMipGenerator.cpp`
- [x] T042 [US2] Implement fixed-order linear UNorm and float data resampling without color transfer, reassociation, or fused multiply-add opportunities in `Source/Asset/Private/FImageMipGenerator.cpp`
- [x] T043 [US2] Implement RG/RGB/RGBA tangent-space normal filtering, +Z reconstruction, deterministic normalization, and zero-vector fallback in `Source/Asset/Private/FImageMipGenerator.cpp`
- [x] T044 [US2] Integrate full-chain-default and explicit-base-only texture construction with checked per-mip and total-chain limits in `Source/Asset/Private/FImageImport.cpp` and `Source/Asset/Private/FImageValidation.cpp`
- [x] T045 [US2] Complete semantic mip, chain validation, overflow, and repeated-run digest coverage in `Tests/AssetImageTextureTests.cpp`

**Checkpoint**: US1 import now yields validated semantic mip chains while
retaining deterministic CPU-only behavior and stable Asset evidence.

---

## Phase 5: User Story 3 - Realize Texture Assets Through Renderer and RHI (Priority: P2)

**Goal**: Realize a validated CPU Texture asset as a sampled RHI texture through
Renderer, upload every mip synchronously, and release all GPU state on failure
without giving Asset any graphics dependency.

**Independent Test**: Use a mock RHI device to verify descriptors, ascending mip
upload regions, RGB/RG expansion, capability rejection, and rollback; run
supplementary native Vulkan upload/readback coverage where enabled.

### Tests for User Story 3

- [x] T046 [P] [US3] Add mock-RHI format, descriptor, per-mip upload, unsupported-format, and injected-failure contract cases in `Tests/RHICoreTests.cpp` and `Tests/RHICoreTests.h`
- [x] T047 [P] [US3] Add Renderer realization tests for Asset-to-RHI mapping, RGB/RG expansion, top-left upload content without a second source-format V flip, ascending uploads, and rollback in `Tests/RendererTextureAssetTests.h` and `Tests/RendererTextureAssetTests.cpp`
- [x] T048 [P] [US3] Add Vulkan fallback/native upload and readback smoke coverage using one-LSB UNorm, FP16 `max(1e-3, abs(expected) * 1e-3)`, and FP32 `max(1e-6, abs(expected) * 1e-6)` tolerances in `Tests/VulkanBackendTests.cpp`, `Tests/VulkanBackendTests.h`, `Tests/VulkanNativeIntegrationTests.cpp`, and `Tests/VulkanNativeIntegrationTests.h`

### Implementation for User Story 3

- [x] T049 [US3] Add the portable formats required by the contract, byte-size rules, and format capability reporting in `Source/RHI/Public/RHI/ERHIFormat.h`, `Source/RHI/Public/RHI/FRHIDeviceCapabilities.h`, and `Source/RHI/Private/RHIModule.cpp`
- [x] T050 [US3] Define validated synchronous single-mip texture upload vocabulary in `Source/RHI/Public/RHI/FRHITextureUploadDesc.h` and add `IRHIDevice::UploadTexture` in `Source/RHI/Public/RHI/IRHIDevice.h`
- [x] T051 [US3] Implement deterministic mock/fallback texture upload validation, footprint recording, and sample-ready completion behavior in `Source/RHI/Private/RHIModule.cpp` and `Tests/RHICoreTests.cpp`
- [x] T052 [US3] Implement Vulkan texture-format/capability mapping for R8G8, RGBA8 sRGB, and RGBA32F in `Source/Backend/Vulkan/Private/FVulkanPhysicalDevice.cpp` and `Source/Backend/Vulkan/Private/FVulkanTexture.cpp`
- [x] T053 [US3] Implement Vulkan synchronous staging, copy, CopyDestination-to-ShaderReadOnly transition, completion wait, and failure cleanup in `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDevice.h`, `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`, and `Source/Backend/Vulkan/Private/FVulkanNativeContext.cpp`
- [x] T054 [US3] Define Renderer-owned realization request, result, diagnostic, and no-partial-resource contract in `Source/Renderer/Public/Renderer/FTextureAssetRealization.h`
- [x] T055 [US3] Implement Asset payload validation and portable Asset-to-RHI format planning without modifying Asset bytes in `Source/Renderer/Private/FTextureAssetRealization.cpp`
- [x] T056 [US3] Implement temporary RGB and grayscale/RG-to-RGBA expansion with alpha-one or straight-alpha semantics in `Source/Renderer/Private/FTextureAssetRealization.cpp`
- [x] T057 [US3] Implement sampled/copy-destination creation, ascending synchronous uploads, diagnostic stages, and request-owned rollback in `Source/Renderer/Private/FTextureAssetRealization.cpp`
- [x] T058 [US3] Export the realization API through the Renderer public aggregate and register its focused tests in `Source/Renderer/Public/Renderer/RendererMinimal.h`, `Tests/RendererTextureAssetTests.cpp`, and `Tests/TestSuiteRegistry.cpp`
- [x] T059 [US3] Complete mock-RHI realization, injected upload failure, device invalidation, CPU-payload survival, and tolerance-governed Vulkan smoke/readback coverage in `Tests/RendererTextureAssetTests.cpp`, `Tests/VulkanBackendTests.cpp`, and `Tests/VulkanNativeIntegrationTests.cpp`

**Checkpoint**: Renderer can realize supported immutable texture assets through
RHI, and failure leaves no published GPU resource or Asset-side GPU ownership.

---

## Phase 6: User Story 4 - Reject Unsafe Inputs and Extend Formats Safely (Priority: P3)

**Goal**: Make all unsafe source and realization failures bounded, diagnosable,
atomic, and deterministic while allowing future format importers to use the
Feature 020 extension lifecycle unchanged.

**Independent Test**: Mutate checked-in seeds to create missing, truncated,
overflowing, non-finite, and conflicting inputs; register a synthetic future
importer; verify normalized errors, no registry publication, and unchanged
architecture boundaries.

### Tests for User Story 4

- [x] T060 [US4] Add an at-least-30-case bounded mutation matrix for missing, inaccessible `AccessDenied` distinct from `NotFound` and `Unsupported`, truncated, bad-CRC, contradictory, unsupported-profile, and over-limit sources in `Tests/AssetImageTextureTests.cpp`
- [x] T061 [US4] Add non-finite/HDR-range, invalid semantic/color-space, malformed-chain, invalid-row-pitch, and every-limit-at-boundary/first-value-above-before-allocation diagnostic cases in `Tests/AssetImageTextureTests.cpp`
- [x] T062 [US4] Add equal-confidence ambiguity, scoped synthetic importer, at-least-eight-request concurrent import/mip, unregister-during-execution, and atomic-conflict cases in `Tests/AssetImageTextureTests.cpp`

### Implementation for User Story 4

- [x] T063 [US4] Normalize inspect/decode/validate/mip failure conversion, required subjects, fields, limits, and participant evidence in `Source/Asset/Private/FImageContainerInspector.cpp`, `Source/Asset/Private/FImageDecode.cpp`, `Source/Asset/Private/FImageValidation.cpp`, and `Source/Asset/Private/FImageMipGenerator.cpp`
- [x] T064 [US4] Enforce checked uint64 arithmetic before reads, allocation, row-pitch, canonical conversion, and mip-chain accumulation in `Source/Asset/Private/FImageValidation.cpp` and `Source/Asset/Private/FImageImport.cpp`
- [x] T065 [US4] Preserve Feature 020 ambiguity and execution-lease behavior for image importer probe/registration paths in `Source/Asset/Private/FAssetDispatch.cpp`, `Source/Asset/Private/FAssetExtensionRegistry.cpp`, and `Source/Asset/Private/FImageImport.cpp`
- [x] T066 [US4] Expand Asset inspection output so malformed or rejected image requests report stable result, stage, source/asset subject, field, limit, and no native error text in `Source/Asset/Private/FAssetInspection.cpp`
- [x] T067 [US4] Complete unsafe-input, at-least-eight-request concurrent extension-lifecycle, atomic-publication, single-request-output equality, and normalized-diagnostic coverage in `Tests/AssetImageTextureTests.cpp`
- [x] T068 [US4] Run the Asset boundary verifier and ensure public Asset headers remain free of RHI, Renderer, Backend, graphics API, and stb includes in `Tests/verify_asset_layer.py`

**Checkpoint**: Invalid image inputs fail before publication with bounded work and
stable diagnostics; future importers can participate without altering existing
format, registry, or Renderer contracts.

---

## Phase 7: Polish and Cross-Cutting Validation

**Purpose**: Finish repository integration, deterministic evidence, strict
cross-platform gates, and feature documentation after all stories are complete.

- [x] T069 [P] Record all 12-or-more valid fixture digests, decoded expectations, and 30-or-more negative mutation provenance in `Tests/Fixtures/Images/README.md`
- [x] T070 Update Asset-oriented test selection and native-upload wording in `.github/workflows/ci.yml` and `specs/021-image-texture-assets/quickstart.md`
- [x] T071 Add the Linux ThreadSanitizer build option and focused Asset-suite at-least-eight-request concurrency gate alongside ASan/UBSan coverage in `site_scons/BuildConfig.py` and `.github/workflows/ci.yml`
- [x] T072 Add a deterministic 20-run all-valid-fixture report with exact normalized CPU payload/digest/diagnostic comparisons in `Tests/AssetImageTextureTests.cpp`
- [x] T073 Remove unexplained strict-warning regressions in `Source/Asset/SConscript`, `Source/RHI/SConscript`, `Source/Renderer/SConscript`, and `Source/Backend/Vulkan/SConscript`
- [x] T074 Run the documented Debug Asset suite and full regression on macOS using `specs/021-image-texture-assets/quickstart.md`
- [x] T075 Run the documented strict Release build and `Tests/verify_asset_layer.py` on macOS using `specs/021-image-texture-assets/quickstart.md`
- [x] T076 Run the documented Windows and Linux focused Asset suite plus full regression and record platform-specific outcomes in `specs/021-image-texture-assets/quickstart.md`
- [x] T077 Run Linux ASan/UBSan focused Asset and full regression gates plus the focused ThreadSanitizer concurrency gate using `specs/021-image-texture-assets/quickstart.md`
- [x] T078 Run GitHub Actions Windows/macOS/Linux Debug and strict Release gates, confirm native Vulkan evidence remains supplementary, and record any unsupported native environment in `specs/021-image-texture-assets/quickstart.md`
- [x] T079 Update delivered-feature documentation, validation evidence, and architecture notes in `doc/021-image-texture-assets.html` and `doc/SYSTEM_DESIGN.MD`
- [x] T080 Re-run all quickstart commands and reconcile final status against `specs/021-image-texture-assets/spec.md`, `specs/021-image-texture-assets/plan.md`, and `specs/021-image-texture-assets/tasks.md`

---

## Dependencies and Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies. T001 and T002 may begin together; the
  fixture additions require the fixture policy in T002, and build wiring follows
  the vendored decoder in T001.
- **Foundational (Phase 2)**: Depends on Phase 1. It blocks all user stories.
- **US1 (Phase 3)**: Depends on the shared Foundation. It provides valid
  immutable base assets for all later work.
- **US2 (Phase 4)**: Depends on US1 payload and import construction. It adds
  semantic mips without requiring graphics.
- **US3 (Phase 5)**: Depends on US2's validated Texture chain and owns all
  Renderer/RHI/Vulkan realization work.
- **US4 (Phase 6)**: Depends on US1's importer and validation path. It can
  begin after US1, but should finish after US2/US3 so diagnostics cover the
  complete path.
- **Polish (Phase 7)**: Depends on all four user stories.

### User Story Dependency Graph

```text
Setup -> Foundational -> US1 -> US2 -> US3 -> Polish
                              \-> US4 -/
```

### Within Each User Story

- Write the listed test cases before the implementation tasks they cover, and
  confirm they fail for the intended missing behavior.
- Complete payload/contract work before dispatch, integration, or publication.
- Complete the story checkpoint before treating the next story as delivered.
- Do not mark a native Vulkan smoke gap as a substitute for its mock-RHI test.

## Parallel Execution Examples

### User Story 1

```text
After the Foundational phase, complete the focused Asset test additions T019
through T021 in their shared file, then complete the ordered import path T022
through T035 before T036 closes the story.
```

### User Story 2

```text
Before the mip implementation, complete the reference test groups T037 through
T039 in their shared file. T040 through T044 share mip-generation code and
remain intentionally ordered.
```

### User Story 3

```text
T046, T047, and T048 can be prepared in parallel as RHI, Renderer, and Vulkan
test coverage. The production path remains ordered: RHI format/upload contract
(T049-T051), Vulkan implementation (T052-T053), then Renderer realization
(T054-T058).
```

### User Story 4

```text
T060, T061, and T062 are separate test groups but intentionally share the
focused Asset test file. T063 through T066 are ordered because their diagnostics
are emitted by the validation path they harden.
```

## Implementation Strategy

### MVP First

1. Complete Setup and Foundation.
2. Complete US1 through T036.
3. Run `StonerTest --suite asset` and `python3 Tests/verify_asset_layer.py`.
4. Review canonical payload bytes, registry atomicity, identities, and
   deterministic diagnostics before adding mip or graphics work.

### Incremental Delivery

1. US1 delivers CPU image/texture asset import with no GPU dependency.
2. US2 adds semantic, deterministic mip chains to the same immutable payloads.
3. US3 adds the Renderer/RHI/Vulkan realization boundary and rollback.
4. US4 finishes failure and extension hardening across the established paths.
5. Phase 7 makes all validation reproducible across supported platforms.

### Parallel Team Strategy

1. One developer completes the shared Foundation.
2. After US1 has stable payload interfaces, a second developer may prepare US3
   test scaffolding while the first completes US2.
3. A third developer may prepare US4 mutation and extension tests after US1,
   then integrate them only when the corresponding diagnostic paths exist.
4. Keep RHI, Renderer, and Vulkan ownership in separate commits to preserve
   the constitution dependency direction.

## Notes

- `[P]` is used only for distinct-file or independently preparable test work;
  it is intentionally absent from shared implementation files.
- `T078` uses CI as a cross-platform verification gate, not as a replacement
  for the required focused local/mock coverage.
- Commit logical clusters with the repository convention, for example
  `feat(asset): import canonical png jpeg and hdr payloads` or
  `test(renderer): cover texture realization rollback`.
