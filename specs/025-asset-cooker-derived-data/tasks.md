# Tasks: Asset Cooker, Manifest & Derived Data

**Input**: Design documents from `/specs/025-asset-cooker-derived-data/`
**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`,
`contracts/`, `quickstart.md`

**Tests**: Feature 025 explicitly requires contract, equivalence, corruption,
failure-injection, concurrency, determinism, scale, sanitizer, and
cross-platform validation. Test tasks are mandatory and precede the related
implementation tasks.

**Organization**: Shared filesystem transactions, target evidence, cooked
payloads, and manifest codecs are blocking foundations. Tasks then follow the
five user stories so each acceptance boundary remains independently testable.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: May run in parallel because it changes different files and has no
  dependency on another incomplete task in the same wave.
- **[Story]**: Maps the task to a user story from `spec.md`.
- Every task names the exact file or directory it changes.

---

## Phase 1: Setup And Validation Scaffolding

**Purpose**: Establish the standalone tool target, reproducible fixture layout,
and evidence structure without changing runtime behavior.

- [X] T001 Create the standalone `StonerAssetCooker` library/executable targets and public/private include boundaries in `Tools/AssetCooker/SConscript`
- [X] T002 Register the Asset Cooker tool as an opt-in SCons target without adding runtime-to-Tools linkage in `SConstruct`
- [X] T003 [P] Create the licensed representative, mutation, corruption, concurrency, and scale corpus layout plus authoring rules in `Tests/Fixtures/AssetCooker/README.md`
- [X] T004 [P] Establish Feature 025 evidence and normalized-report conventions in `Validation/025/README.md`, `Validation/025/reports/README.md`, and `Validation/025/artifacts/README.md`
- [X] T005 Define the fixture-manifest schema and initial provenance, license, expected identity, payload family, profile, and expected-result records in `Validation/025/fixture-manifest.json`
- [X] T006 Ignore local cooker staging, DDC, and published-output roots while retaining checked-in normalized evidence in `.gitignore`

**Checkpoint**: The tool can be selected by the build, and every future test
artifact has an owned, auditable location.

---

## Phase 2: Foundational Contracts And Transactions

**Purpose**: Complete Plan M0-M2 primitives shared by every user story.

**Critical**: T007-T017 complete first. Profile/key work T018-T027 and cooked
contract work T028-T044 may then proceed as ownership-separated tracks. T045
joins both and blocks all user-story implementation.

### Core Filesystem Transactions

- [X] T007 [P] Add failing bounded enumeration, regular-file query, canonical containment, same-volume move, atomic replacement, durable write, and safe-remove tests in `Tests/CorePlatformFileTransactionTests.cpp` and `Tests/CorePlatformFileTransactionTests.h`
- [X] T008 [P] Add failing lease timeout, exclusivity, owner metadata, move ownership, crash release, and non-inheritance tests in `Tests/CorePlatformFileLeaseTests.cpp` and `Tests/CorePlatformFileLeaseTests.h`
- [X] T009 [P] Add a subprocess lease holder/probe with stable exit categories in `Tests/Helpers/PlatformFileLeaseProbe.cpp`
- [X] T010 Add symlink/junction escape, Unicode/long-path, permission, existing-destination, and cross-volume fixtures to `Tests/CorePlatformFileTransactionTests.cpp`
- [X] T011 Register focused `core-file-transaction` and subprocess probe builds in `Tests/Main.cpp` and `Tests/SConscript`
- [X] T012 Define source-compatible result-bearing filesystem transaction, containment, enumeration, and durability contracts in `Source/Core/Public/Core/FPlatformFileSystem.h`
- [X] T013 Implement shared validation, bounded traversal, diagnostics, and legacy-call routing in `Source/Core/Private/FPlatformFileSystem.cpp` and `Source/Core/Private/FPlatformFileSystemInternal.h`
- [X] T014 Implement POSIX same-volume moves, durable writes, atomic replacement, containment, and recursive removal in `Source/Core/Private/FPlatformFileSystemPosix.cpp`
- [X] T015 Implement Win32 long-path-aware moves, durable writes, replace-existing semantics, reparse-point containment, and recursive removal in `Source/Core/Private/FPlatformFileSystemWindows.cpp`
- [X] T016 Define a move-only bounded-wait cross-process lease with owner metadata in `Source/Core/Public/Core/FPlatformFileLease.h`
- [X] T017 Implement descriptor-owned POSIX `flock` and Win32 sharing-handle lease ownership behind Core-private platform guards in `Source/Core/Private/FPlatformFileLease.cpp`

### Target Profiles And Derived Keys

- [X] T018 [P] Add failing strict profile parse/write, producer-settings ordering/schema/validation, rename equivalence, malformed input, normalized digest, and cooker-projection tests in `Tests/AssetCookerProfileTests.cpp` and `Tests/AssetCookerProfileTests.h`
- [X] T019 [P] Add failing domain separation, ambiguous-boundary, evidence-completeness, schema-versioned BuildPolicy producer-settings projection, irrelevant-field reuse, relevant-field invalidation, and golden-key tests in `Tests/AssetCookerDerivedKeyTests.cpp` and `Tests/AssetCookerDerivedKeyTests.h`
- [X] T020 Add canonical profile and derived-key golden fixtures plus their provenance/hash records in `Tests/Fixtures/AssetCooker/Contracts/Profiles/`, `Tests/Fixtures/AssetCooker/Contracts/DerivedKeys/`, and `Validation/025/fixture-manifest.json`
- [X] T021 [P] Define schema revision, display identity, normalized effective configuration, capabilities, fallback policy, unique sorted schema-versioned producer settings, and projection evidence in `Source/Asset/Public/Asset/FAssetTargetProfile.h`
- [X] T022 [P] Define immutable derived-key identity and complete inspectable evidence records in `Source/Asset/Public/Asset/FAssetDerivedKey.h`
- [X] T023 Implement bounded strict canonical target-profile JSON parsing/writing including ordered producer-settings records in `Source/Asset/Private/FAssetTargetProfileCodec.h` and `Source/Asset/Private/FAssetTargetProfileCodec.cpp`
- [X] T024 Implement type-tagged, length-delimited, domain-separated SHA-256 key construction over schema-versioned profile BuildPolicy producer-settings projections in `Source/Asset/Private/FAssetDerivedKeyBuilder.h` and `Source/Asset/Private/FAssetDerivedKeyBuilder.cpp`
- [X] T025 Replace the string target profile with typed effective-profile evidence while retaining source compatibility in `Source/Asset/Public/Asset/IAssetCooker.h` and `Source/Asset/Public/Asset/IAssetLoader.h`
- [X] T026 Migrate Feature 022 texture cook policy and KTX2 cooker to validated schema-versioned producer settings and per-cooker profile projections without changing portable semantics in `Source/Asset/Public/Asset/FTextureCook.h`, `Source/Asset/Private/FTextureCookPolicy.cpp`, and `Source/Asset/Private/FKTX2TextureCooker.cpp`
- [X] T027 Register profile/key sources and focused suites in `Source/Asset/SConscript`, `Tests/AssetTests.cpp`, `Tests/AssetTests.h`, and `Tests/SConscript`

### Cooked Payload And Manifest Contracts

- [X] T028 [P] Add failing `SGCOOK01` round-trip, byte-order, truncation, substitution, digest, unknown-codec, unknown-schema, and size-limit tests in `Tests/AssetCookerPayloadCodecTests.cpp` and `Tests/AssetCookerPayloadCodecTests.h`
- [X] T029 [P] Add failing canonical manifest ordering, locator containment, generation hash, dependency/source evidence, duplicate identity, and limits tests in `Tests/AssetCookerManifestTests.cpp` and `Tests/AssetCookerManifestTests.h`
- [X] T030 [P] Add failing source-import versus cooked-load normalized equivalence tests for every Feature 021-024 payload family in `Tests/AssetCookerEquivalenceTests.cpp` and `Tests/AssetCookerEquivalenceTests.h`
- [X] T031 Add valid and malformed envelope/manifest golden fixtures plus their provenance/hash records in `Tests/Fixtures/AssetCooker/Contracts/Payloads/`, `Tests/Fixtures/AssetCooker/Contracts/Manifests/`, and `Validation/025/fixture-manifest.json`
- [X] T032 Implement schema and golden-fixture verification with unit coverage in `Tests/verify_asset_cooker_contracts.py` and `Tests/test_verify_asset_cooker_contracts.py`
- [X] T033 [P] Define bounded cooked-envelope header, payload bytes, digests, codec/schema identity, and validation result types in `Source/Asset/Public/Asset/FAssetCookedPayload.h`
- [X] T034 [P] Define manifest header, selection, source, dependency, asset record, generation identity, and canonical ordering types in `Source/Asset/Public/Asset/FAssetCookManifest.h`
- [X] T035 Define the Asset-public typed profile/envelope/manifest facade and implement little-endian `SGCOOK01` parsing, writing, digest verification, and hard limits behind it in `Source/Asset/Public/Asset/FAssetCookContractCodec.h`, `Source/Asset/Private/FAssetCookedPayloadCodec.h`, and `Source/Asset/Private/FAssetCookedPayloadCodec.cpp`
- [X] T036 Implement strict canonical manifest JSON parsing, writing, locator validation, and semantic generation hashing behind `FAssetCookContractCodec` in `Source/Asset/Private/FAssetCookManifestCodec.h` and `Source/Asset/Private/FAssetCookManifestCodec.cpp`
- [X] T037 [P] Implement deterministic image, uncompressed texture, and KTX2 codecs/loaders including semantic-aware profile projection, target selection, and explicit fallback in `Source/Asset/Private/FImageTextureCookedCodec.h` and `Source/Asset/Private/FImageTextureCookedCodec.cpp`
- [X] T038 [P] Implement deterministic material, material-instance, shader-source, shader-payload, and shader-program codecs/loaders including backend/profile-tagged selection and explicit fallback in `Source/Asset/Private/FMaterialShaderCookedCodec.h` and `Source/Asset/Private/FMaterialShaderCookedCodec.cpp`
- [X] T039 [P] Implement deterministic static-mesh/static-model codecs/loaders and their minimal relevant-profile projection in `Source/Asset/Private/FStaticModelCookedCodec.h` and `Source/Asset/Private/FStaticModelCookedCodec.cpp`
- [X] T040 Register versioned cooked codecs through public Asset extension contracts and reject producers that omit strict producer-settings validation or complete relevant-profile declarations in `Source/Asset/Private/AssetModule.cpp`
- [X] T041 Prove every codec preserves AssetId, type, version, dependencies, payload semantics, target choice/fallback evidence, and byte-identical repeated writing in `Tests/AssetCookerEquivalenceTests.cpp`
- [X] T042 Add parser and writer bounds for assets, dependencies, sources, locators, diagnostics, strings, and payload bytes in `Source/Asset/Private/FAssetCookManifestCodec.cpp` and `Source/Asset/Private/FAssetCookedPayloadCodec.cpp`
- [X] T043 Register cooked-contract sources and focused `asset-cooker-codec` suites in `Source/Asset/SConscript`, `Tests/AssetTests.cpp`, and `Tests/SConscript`
- [X] T044 Run the complete M0-M2 contract gate and record profile, key, envelope, manifest, target-sensitive codec, equivalence, and filesystem evidence in `Validation/025/reports/foundation-contracts.txt`
- [X] T045 Extend and run architecture verification for Asset-to-graphics, runtime-to-Tools, Tool-to-Asset-private includes, native handle, and private JSON leakage with unit coverage and recorded evidence in `Tests/verify_architecture.py`, `Tests/test_verify_architecture.py`, and `Validation/025/reports/foundation-architecture.txt`

**Checkpoint**: Runtime-consumable cooked contracts and transactional Core
primitives are stable before offline orchestration begins.

---

## Phase 3: User Story 1 - Produce Deterministic Cooked Assets (Priority: P1) MVP

**Goal**: Discover a selected source closure, cook every in-scope asset family,
and produce byte-identical payloads plus a canonical generation manifest.

**Independent Test**: Cook the representative corpus twice from empty output
and cache roots with one worker and eight workers; compare payload bytes,
manifest bytes, normalized reports, and source-import/cooked-load semantics.

### Tests For User Story 1

- [X] T046 [P] [US1] Add failing source discovery, normalized locator, alias deduplication, empty cook-all, collision, explicit-root, cook-all, and unreachable-asset tests in `Tests/AssetCookerSourceCatalogTests.cpp` and `Tests/AssetCookerSourceCatalogTests.h`
- [X] T047 [P] [US1] Add failing role/type/version, missing dependency, cycle path, graph limit, and deterministic topological-order tests in `Tests/AssetCookerGraphTests.cpp` and `Tests/AssetCookerGraphTests.h`
- [X] T048 [P] [US1] Add failing worker-count, ready-node, multi-output atomicity, plan-index commit, and bounded scheduling tests in `Tests/AssetCookerSchedulerTests.cpp` and `Tests/AssetCookerSchedulerTests.h`
- [X] T049 [P] [US1] Add failing input pinning, complete source manifest, re-resolution, source-change, and no-retry tests in `Tests/AssetCookerInputSnapshotTests.cpp` and `Tests/AssetCookerInputSnapshotTests.h`
- [X] T050 [P] [US1] Add failing clean-cook, every-family equivalence, repeated-byte determinism, and 1-worker/8-worker comparison tests in `Tests/AssetCookerDeterminismTests.cpp` and `Tests/AssetCookerDeterminismTests.h`
- [X] T051 [P] [US1] Populate licensed Feature 021-024 representative source assets, expected typed roots, and complete provenance/hash records in `Tests/Fixtures/AssetCooker/Representative/` and `Validation/025/fixture-manifest.json`

### Implementation For User Story 1

- [X] T052 [US1] Define the cook/plan-only request selection, worker bounds, canonically non-overlapping source/output/cache roots, report-path non-aliasing, mode-specific lease fields, target profile, limits, and exact incremental reuse-eligibility policy in `Tools/AssetCooker/Public/AssetCooker/FAssetCookRequest.h`
- [X] T053 [P] [US1] Define stable result categories and non-partial caller-visible result ownership in `Tools/AssetCooker/Public/AssetCooker/FAssetCookResult.h`
- [X] T054 [P] [US1] Define normalized per-asset decisions, reuse eligibility/exclusion reasons, aggregate counts, artifact digests, and non-deterministic telemetry separation in `Tools/AssetCooker/Public/AssetCooker/FAssetCookReport.h`
- [X] T055 [US1] Define the reusable offline runner entry point without CLI or runtime-manager policy in `Tools/AssetCooker/Public/AssetCooker/FAssetCookRunner.h`
- [X] T056 [US1] Implement bounded stable filesystem discovery and built-in Feature 021-024 source adapters in `Tools/AssetCooker/Private/FAssetSourceCatalog.h` and `Tools/AssetCooker/Private/FAssetSourceCatalog.cpp`
- [X] T057 [US1] Resolve all source bytes through Asset resolver/importer contracts and reject path, Unicode, case, and typed-identity collisions in `Tools/AssetCooker/Private/FAssetSourceCatalog.cpp`
- [X] T058 [US1] Implement mutually exclusive explicit-root dependency closure and explicit cook-all selection in `Tools/AssetCooker/Private/FAssetCookGraph.h` and `Tools/AssetCooker/Private/FAssetCookGraph.cpp`
- [X] T059 [US1] Validate dependency roles, types, versions, missing nodes, cycles, limits, and deterministic topological plan indices in `Tools/AssetCooker/Private/FAssetCookGraph.cpp`
- [X] T060 [US1] Implement immutable resolved-byte and source-version pinning in `Tools/AssetCooker/Private/FCookInputSnapshot.h` and `Tools/AssetCooker/Private/FCookInputSnapshot.cpp`
- [X] T061 [US1] Re-resolve each node's consumed locator set before committing its processing result and support complete pre-publication snapshot revalidation, returning stable `SourceChanged` without retry in `Tools/AssetCooker/Private/FCookInputSnapshot.cpp`
- [X] T062 [US1] Implement the bounded 1-32 worker ready-graph scheduler with plan-index result commits in `Tools/AssetCooker/Private/FAssetCookScheduler.h` and `Tools/AssetCooker/Private/FAssetCookScheduler.cpp`
- [X] T063 [US1] Preserve atomic multi-output source imports and deterministic failure propagation in `Tools/AssetCooker/Private/FAssetCookScheduler.cpp`
- [X] T064 [US1] Load and validate exactly one target profile through the public Asset codec facade, then integrate discovery, selection, graph planning, snapshots, scheduling, codec dispatch, and canonical manifest assembly in `Tools/AssetCooker/Private/FAssetCookRunner.cpp`
- [X] T065 [US1] Assemble the clean-cook generation image only in request-local scratch outside the publication root, with digest-addressed deduplicated `.sgasset` payloads and final relative locators, in `Tools/AssetCooker/Private/FAssetCookRunner.cpp`
- [X] T066 [US1] Register all Tool sources and link only Asset plus Core dependencies in `Tools/AssetCooker/SConscript`
- [X] T067 [US1] Register focused `asset-cooker-clean`, graph, scheduler, snapshot, and determinism suites in `Tests/Main.cpp` and `Tests/SConscript`
- [X] T068 [US1] Add twenty-run clean-cook and 1-worker/8-worker normalized artifact comparison to `Tests/AssetCookerDeterminismTests.cpp`
- [X] T069 [US1] Verify every representative source payload, including profile-sensitive shader/texture selection and authorized fallback, against its cooked-load normalized model in `Tests/AssetCookerEquivalenceTests.cpp`
- [X] T070 [US1] Reject all missing, cyclic, conflicting, unreachable, and source-changed cases without publishing caller-visible results in `Tests/AssetCookerGraphTests.cpp` and `Tests/AssetCookerInputSnapshotTests.cpp`
- [X] T071 [US1] Run the complete US1 independent gate and record discovered/reachable/cooked counts, supported/unsupported target choices, fallbacks, artifact digests, equivalence, and determinism in `Validation/025/reports/us1-clean-deterministic-cook.txt`

**Checkpoint**: A clean selected cook is deterministic, complete, and
semantically equivalent without relying on a DDC or current-generation pointer.

---

## Phase 4: User Story 2 - Reuse Valid Derived Data Incrementally (Priority: P1)

**Goal**: Reuse validated immutable local derived entries, rebuild precisely
invalidated nodes, and converge to the same generation as a clean cook.

**Independent Test**: Populate a DDC, repeat unchanged for 100% eligible reuse,
apply the complete mutation matrix, corrupt at least 15 cache entries, issue
eight concurrent same-key requests, and compare the final output with clean cook.

### Tests For User Story 2

- [X] T072 [P] [US2] Add failing immutable entry install, validated hit, miss, stale metadata, interrupted write, and strict-cache tests in `Tests/AssetCookerDerivedDataTests.cpp` and `Tests/AssetCookerDerivedDataTests.h`
- [X] T073 [P] [US2] Add failing source, dependency, importer, cooker, schema, profile BuildPolicy producer-settings schema/value, profile-capability, and unrelated-branch invalidation tests plus an addition/edit/removal/rename sequence-to-clean convergence test in `Tests/AssetCookerIncrementalTests.cpp` and `Tests/AssetCookerIncrementalTests.h`
- [X] T074 [P] [US2] Add failing eight-writer same-key, quarantine race, winner re-query, and clean-convergence tests in `Tests/AssetCookerConcurrencyTests.cpp` and `Tests/AssetCookerConcurrencyTests.h`
- [X] T075 [P] [US2] Add at least 15 deterministic cache truncation, substitution, key mismatch, metadata mutation, and staging-remnant fixtures plus complete records in `Tests/Fixtures/AssetCooker/CorruptCache/` and `Validation/025/fixture-manifest.json`

### Implementation For User Story 2

- [X] T076 [US2] Define immutable entry metadata, validation result, cache decision, quarantine evidence, and storage paths in `Tools/AssetCooker/Private/FDerivedDataStore.h`
- [X] T077 [US2] Implement full-key sharded lookup and strict validation of `Entry.json` plus `Payload.sgasset` using only the public Asset codec facade in `Tools/AssetCooker/Private/FDerivedDataStore.cpp`
- [X] T078 [US2] Cook outside the key lease, then acquire a short-lived native lease and re-query the winning entry in `Tools/AssetCooker/Private/FDerivedDataStore.cpp`
- [X] T079 [US2] Install absent immutable entries through same-root staging, durable writes, atomic move, and never-overwrite-valid semantics in `Tools/AssetCooker/Private/FDerivedDataStore.cpp`
- [X] T080 [US2] Quarantine invalid entries under stable failure-evidence digests for ordinary cook while making strict validation fail without rebuilding in `Tools/AssetCooker/Private/FDerivedDataStore.cpp`
- [X] T081 [US2] Compute complete per-node derived evidence from pinned sources, dependencies, importer/cooker revisions, codecs, projected schema-versioned BuildPolicy producer settings, and target capabilities in `Tools/AssetCooker/Private/FAssetCookRunner.cpp`
- [X] T082 [US2] Integrate validated hit, miss, invalidation, rebuild, removal, and unrelated-branch reuse decisions into scheduling in `Tools/AssetCooker/Private/FAssetCookScheduler.cpp`
- [X] T083 [US2] Exclude unreachable and removed assets from the next manifest while retaining reconstructible immutable DDC entries in `Tools/AssetCooker/Private/FAssetCookRunner.cpp`
- [X] T084 [US2] Emit stable per-asset eligibility, exclusion, hit, miss, invalidation, quarantine, and rebuild reasons plus aggregate cache counts in `Tools/AssetCooker/Private/FAssetCookRunner.cpp`
- [X] T085 [US2] Register focused `asset-cooker-ddc`, incremental, and concurrency suites in `Tests/Main.cpp` and `Tests/SConscript`
- [X] T086 [US2] Prove an unchanged incremental cook reports the explicit eligible/ineligible denominator, reuses 100% of eligible entries, and emits the same normalized generation as clean cook in `Tests/AssetCookerIncrementalTests.cpp`
- [X] T087 [US2] Prove the complete mutation matrix invalidates exactly affected dependency closures, preserves unrelated branches, and makes an addition/edit/removal/rename sequence converge artifact-for-artifact with a clean final-state cook in `Tests/AssetCookerIncrementalTests.cpp`
- [X] T088 [US2] Prove ordinary corruption quarantine/rebuild and strict-cache no-mutation failure over at least 15 cache cases in `Tests/AssetCookerDerivedDataTests.cpp`
- [X] T089 [US2] Prove eight concurrent same-key requests converge without overwrite, partial entry, or nondeterministic result in `Tests/AssetCookerConcurrencyTests.cpp`
- [X] T090 [US2] Run the complete US2 independent gate and record reuse, invalidation, quarantine, concurrency, and convergence evidence in `Validation/025/reports/us2-derived-data.txt`

**Checkpoint**: Local derived data is optional, validated before trust, safe
under contention, and incapable of changing clean-cook semantics.

---

## Phase 5: User Story 3 - Publish And Validate Outputs Atomically (Priority: P1)

**Goal**: Publish only complete immutable generations through one atomic current
pointer and validate them without source files or DDC access.

**Independent Test**: Inject every defined filesystem failure, race two writer
processes, crash a lease holder, and corrupt at least 30 published artifacts;
the prior generation remains current until one fully validated commit succeeds.

### Tests For User Story 3

- [X] T091 [P] [US3] Add failing pre-commit stage, payload, manifest, validation, generation move, pointer write/replace, and cleanup injection tests plus post-commit audit-warning and commit-boundary interruption tests in `Tests/AssetCookerPublicationTests.cpp` and `Tests/AssetCookerPublicationTests.h`
- [X] T092 [P] [US3] Add failing current-pointer, generation, payload, manifest, locator, source independence, and unexpected-file validation tests in `Tests/AssetCookerPublishedValidationTests.cpp` and `Tests/AssetCookerPublishedValidationTests.h`
- [X] T093 [P] [US3] Add failing two-process wait/timeout, equivalent-generation no-op, crash release, reader race, and wrong-filesystem tests in `Tests/AssetCookerPublicationConcurrencyTests.cpp` and `Tests/AssetCookerPublicationConcurrencyTests.h`
- [X] T094 [P] [US3] Add a subprocess publication writer/holder probe with stable exit categories in `Tests/Helpers/AssetCookerPublicationProbe.cpp`
- [X] T095 [P] [US3] Add at least 30 deterministic published current-pointer, path, dependency, schema, size, digest, target, missing, truncation, substitution, and unexpected-file corruptions plus complete records in `Tests/Fixtures/AssetCooker/CorruptPublished/` and `Validation/025/fixture-manifest.json`

### Implementation For User Story 3

- [X] T096 [US3] Define request-local generation image, immutable generation, current pointer, publication request, bounded lease, pre-commit failure, committed success, and post-commit audit contracts in `Tools/AssetCooker/Private/FCookedGenerationPublisher.h`
- [X] T097 [US3] Define standalone validation request, strictness, limits, corruption categories, and normalized evidence in `Tools/AssetCooker/Private/FPublishedGenerationValidator.h`
- [X] T098 [US3] Build and validate a self-contained generation image in request-local scratch outside the output root with deduplicated digest-addressed payloads and canonical `Manifest.json` in `Tools/AssetCooker/Private/FCookedGenerationPublisher.cpp`
- [X] T099 [US3] Validate request-local, output-staged, and installed generations through the same standalone validator using only the public Asset codec facade in `Tools/AssetCooker/Private/FPublishedGenerationValidator.cpp`
- [X] T100 [US3] Acquire the target-root publication lease with configurable default 30-second bounded wait before any output-root staging creation or cleanup in `Tools/AssetCooker/Private/FCookedGenerationPublisher.cpp`
- [X] T101 [US3] While holding the lease, create output-root staging from the request-local image, validate it, re-verify the complete input snapshot, and fail `SourceChanged` without retry before installation in `Tools/AssetCooker/Private/FCookedGenerationPublisher.cpp`
- [X] T102 [US3] Install absent immutable generations, accept byte-equivalent existing generations, durably write `Current.next`, atomically replace `Current.json` as the sole commit point, and return committed success after replacement in `Tools/AssetCooker/Private/FCookedGenerationPublisher.cpp`
- [X] T103 [US3] Preserve all successful generations and perform best-effort staging cleanup without exposing failed work in `Tools/AssetCooker/Private/FCookedGenerationPublisher.cpp`
- [X] T104 [US3] Implement source/DDC-independent validation through the public Asset codec facade for pointer, schema, profile, manifest, locators, files, envelopes, digests, dependencies, and unexpected files in `Tools/AssetCooker/Private/FPublishedGenerationValidator.cpp`
- [X] T105 [US3] Add test-only deterministic injection for every pre-commit filesystem boundary, commit-boundary interruption, and post-commit audit warning without exposing hooks in production CLI contracts in `Tools/AssetCooker/Private/FCookedGenerationPublisher.cpp`
- [X] T106 [US3] Integrate validated generation publication and no-partial-result failure semantics into `Tools/AssetCooker/Private/FAssetCookRunner.cpp`
- [X] T107 [US3] Register publication, validation, subprocess, and concurrency focused suites in `Tests/Main.cpp` and `Tests/SConscript`
- [X] T108 [US3] Prove every pre-commit injected failure preserves the previous current generation, successful replacement remains committed success despite audit warnings, and commit-boundary interruption reveals only a complete old or new pointer in `Tests/AssetCookerPublicationTests.cpp`
- [X] T109 [US3] Prove two overlapping writers have deterministic wait/success or timeout outcomes and process crash releases ownership in `Tests/AssetCookerPublicationConcurrencyTests.cpp`
- [X] T110 [US3] Prove source/DDC-independent standalone validation detects 100% of the separate published-generation corruption corpus containing at least 30 cases in `Tests/AssetCookerPublishedValidationTests.cpp`
- [X] T111 [US3] Run the complete US3 independent gate and record failure matrix, previous-generation survival, concurrency, and standalone validation in `Validation/025/reports/us3-atomic-publication.txt`

**Checkpoint**: `Current.json` is the only visibility boundary, and every
visible generation validates independently of authoring inputs and caches.

---

## Phase 6: User Story 4 - Define Explicit Target Profiles (Priority: P2)

**Goal**: Make platform-sensitive cook decisions explicit, reproducible, and
precisely scoped to the fields each payload producer consumes.

**Independent Test**: Cook with Windows, macOS, Linux, renamed-equivalent, and
fallback fixture profiles; identical effective configurations share identity,
while each relevant mutation invalidates only affected payload families.

### Tests For User Story 4

- [X] T112 [P] [US4] Add failing Windows/macOS/Linux capability, producer-settings presence/schema/value, texture format, shader payload, fallback, unsupported, and effective-identity tests in `Tests/AssetCookerTargetProfileTests.cpp` and `Tests/AssetCookerTargetProfileTests.h`
- [X] T113 [P] [US4] Add failing per-family relevant/irrelevant profile mutation and precise DDC invalidation tests in `Tests/AssetCookerProfileInvalidationTests.cpp` and `Tests/AssetCookerProfileInvalidationTests.h`
- [X] T114 [P] [US4] Add versioned checked-in Vulkan profiles with explicit effective fields and fixture-manifest records in `Config/AssetCooker/Profiles/Windows-Vulkan.json`, `Config/AssetCooker/Profiles/Mac-Vulkan.json`, `Config/AssetCooker/Profiles/Linux-Vulkan.json`, and `Validation/025/fixture-manifest.json`

### Implementation For User Story 4

- [X] T115 [US4] Extend the foundational backend/profile-tagged shader selection with cross-profile compatibility validation and normalized decision evidence in `Source/Asset/Private/FMaterialShaderCookedCodec.cpp`
- [X] T116 [US4] Extend the foundational semantic-aware texture selection with cross-profile capability validation and normalized fallback evidence in `Source/Asset/Private/FImageTextureCookedCodec.cpp`
- [X] T117 [US4] Verify the foundational static mesh/model projection remains identical across profiles that differ only in irrelevant graphics capabilities in `Source/Asset/Private/FStaticModelCookedCodec.cpp`
- [X] T118 [US4] Validate each producer declares consumed effective-profile fields and emits complete projection evidence in `Source/Asset/Private/FAssetDerivedKeyBuilder.cpp`
- [X] T119 [US4] Compare differently named and mutated profile files through the public Asset codec facade while preserving display-name exclusion from effective identity in `Tools/AssetCooker/Private/FAssetCookRunner.cpp`
- [X] T120 [US4] Report selected target decisions, fallbacks, projected fields, and unsupported requirements per AssetId in `Tools/AssetCooker/Private/FAssetCookRunner.cpp`
- [X] T121 [US4] Register target-profile and invalidation suites in `Tests/Main.cpp` and `Tests/SConscript`
- [X] T122 [US4] Prove differently named profiles with identical normalized effective configuration produce identical keys, payloads, manifests, and reports in `Tests/AssetCookerTargetProfileTests.cpp`
- [X] T123 [US4] Prove every relevant and irrelevant field mutation has the exact expected payload-family invalidation set in `Tests/AssetCookerProfileInvalidationTests.cpp`
- [X] T124 [US4] Run the complete US4 independent gate and record target selection, fallback, profile equivalence, and precise invalidation in `Validation/025/reports/us4-target-profiles.txt`

**Checkpoint**: Target profiles are explicit semantic inputs rather than host
probes, and producer projections prevent accidental whole-graph invalidation.

---

## Phase 7: User Story 5 - Inspect, Diagnose, And Reproduce A Cook (Priority: P3)

**Goal**: Provide stable CLI commands, exit categories, normalized reports, and
clean-machine workflows that explain and reproduce every cook decision.

**Independent Test**: Exercise `cook`, `plan`, `validate`, `validate-cache`, and
`inspect` across success and failure cases; verify plan-only mutation freedom,
stable exits, deterministic report digests, and published validation after DDC
and source removal.

### Tests For User Story 5

- [X] T125 [P] [US5] Add failing command grammar, required/mutually-exclusive option, canonical root-overlap/report-alias rejection, worker/timeout bound, stdout/stderr, and stable exit-code tests in `Tests/AssetCookerCliTests.cpp` and `Tests/AssetCookerCliTests.h`
- [X] T126 [P] [US5] Add failing canonical report schema, complete hit/miss/invalidate/quarantine/cook/rebuild/fallback/ineligible/reuse/stage/validate/publish/fail action vocabulary, target-profile diagnostic subject, reuse eligibility denominator/exclusions, aggregate count, provenance, deterministic digest, and telemetry-exclusion tests in `Tests/AssetCookerReportTests.cpp` and `Tests/AssetCookerReportTests.h`
- [X] T127 [P] [US5] Add failing plan DDC/publication/source no-mutation except its explicit report output, inspect evidence, strict validation, and source/DDC removal workflow tests in `Tests/AssetCookerWorkflowTests.cpp` and `Tests/AssetCookerWorkflowTests.h`
- [X] T128 [US5] Add golden argument, report, diagnostic, and exit-category fixtures plus complete provenance/hash records in `Tests/Fixtures/AssetCooker/Cli/`, `Tests/Fixtures/AssetCooker/Reports/`, and `Validation/025/fixture-manifest.json`

### Implementation For User Story 5

- [X] T129 [US5] Implement strict `cook`, `plan`, `validate`, `validate-cache`, and `inspect` command parsing in `Tools/AssetCooker/Private/FAssetCookCli.h` and `Tools/AssetCooker/Private/FAssetCookCli.cpp`
- [X] T130 [US5] Implement one canonical normalized report codec including reuse eligibility/exclusion counts and reasons conforming to `report.schema.json` in `Tools/AssetCooker/Private/FAssetCookReportCodec.h` and `Tools/AssetCooker/Private/FAssetCookReportCodec.cpp`
- [X] T131 [US5] Route all commands through the reusable runner, DDC validator, or published validator without arbitrary shell execution in `Tools/AssetCooker/Private/FAssetCookCli.cpp`
- [X] T132 [US5] Implement mutation-free plan decisions and complete inspectable key/profile/source/dependency/reuse-eligibility/cache/publication evidence in `Tools/AssetCooker/Private/FAssetCookRunner.cpp`
- [X] T133 [US5] Separate deterministic report content from wall-clock and peak-RSS telemetry and exclude telemetry from report digests in `Tools/AssetCooker/Private/FAssetCookReportCodec.cpp`
- [X] T134 [US5] Implement stable process exit mapping and concise stdout/stderr summaries in `Tools/AssetCooker/Private/Main.cpp`
- [X] T135 [US5] Register the executable, focused CLI/report/workflow suites, and subprocess invocations in `Tools/AssetCooker/SConscript`, `Tests/Main.cpp`, and `Tests/SConscript`
- [X] T136 [US5] Verify plan-only leaves source, DDC, and publication roots byte-for-byte unchanged apart from its explicitly requested report output and explains all prospective hit/miss/invalidation/publication decisions in `Tests/AssetCookerWorkflowTests.cpp`
- [X] T137 [US5] Verify standalone published validation succeeds after removing source and DDC roots in `Tests/AssetCookerWorkflowTests.cpp`
- [X] T138 [US5] Verify repeated equivalent commands produce byte-identical normalized report artifacts and stable exit categories in `Tests/AssetCookerReportTests.cpp` and `Tests/AssetCookerCliTests.cpp`
- [X] T139 [US5] Implement the documented clean, incremental, plan, corruption, inspect, and standalone validation command sequence in `specs/025-asset-cooker-derived-data/quickstart.md`
- [X] T140 [US5] Run the complete US5 independent gate and record command, report, diagnostics, plan purity, and clean-machine evidence in `Validation/025/reports/us5-cli-reproduction.txt`

**Checkpoint**: Operators can reproduce and explain a cook using stable commands
and artifacts without knowledge of internal storage layout.

---

## Phase 8: Polish And Cross-Cutting Closeout

**Purpose**: Satisfy scale, architecture, regression, sanitizer, CI, evidence,
and roadmap completion gates after all user stories pass locally.

- [X] T141 [P] Implement a deterministic 1,000-asset/5,000-edge corpus generator with unit coverage and complete provenance/hash records in `Tests/Fixtures/AssetCooker/generate_scale_corpus.py`, `Tests/test_generate_asset_cooker_scale_corpus.py`, and `Validation/025/fixture-manifest.json`
- [X] T142 Finalize and verify fixture-manifest provenance, license, hash, schema, profile, expected result, and complete corpus coverage with unit tests in `Validation/025/fixture-manifest.json`, `Tests/verify_asset_cooker_fixtures.py`, and `Tests/test_verify_asset_cooker_fixtures.py`
- [X] T143 Add opt-in synthetic plan/cached/validation/clean, representative clean, and peak-RSS benchmark modes that hard-fail the documented Apple M4 Pro Release thresholds and expose a separate CI 4x time profile in `Tests/AssetCookerBenchmark.cpp` and `Tests/AssetCookerBenchmark.h`
- [X] T144 Register the benchmark target without adding it to default correctness suites in `Tests/Main.cpp` and `Tests/SConscript`
- [X] T145 Run twenty repetitions of plan-only, clean, incremental, validation, and 1-worker/8-worker modes and record normalized artifact equality in `Validation/025/reports/determinism-20-runs.txt`
- [X] T146 Run the eight same-key and two same-root-process concurrency matrix including timeout and crash recovery and record evidence in `Validation/025/reports/concurrency.txt`
- [X] T147 Run and require the Apple M4 Pro Release reference scale gate to meet synthetic plan <=2s, cached <=10s, validate <=10s, synthetic clean <=60s, representative clean <=60s, and synthetic peak RSS <1 GiB, then record toolchain, corpus, counts, bytes, and digests in `Validation/025/reports/performance-m4-pro.txt`
- [X] T148 Extend repository contract checks for schemas, profiles, fixtures, ignored outputs, source registration, and runtime-consumable public Asset boundaries in `Tests/verify_asset_cooker_contracts.py`
- [X] T149 Add Feature 025 Windows/macOS/Linux Debug and strict Release, Linux ASan/UBSan/TSan, deterministic, subprocess, architecture, schema, and artifact-upload jobs in `.github/workflows/feature-025-asset-cooker.yml`
- [X] T150 Add clean-checkout cross-platform orchestration that performs and compares two clean cooks per host, unchanged incremental and standalone validation, separate 15-case DDC and 30-case published corruption, concurrency, benchmarks, and normalized reports with unit coverage; require CI plan/cached/validate/synthetic-clean/representative-clean smoke times to pass the documented 4x ceilings in `.github/scripts/run_asset_cooker_validation.py` and `.github/scripts/test_run_asset_cooker_validation.py`
- [X] T151 Run focused Feature 025 and complete Features 020-024 plus engine Debug and strict Release regressions and record commands/results in `Validation/025/reports/regressions.txt`
- [ ] T152 Run Linux ASan, UBSan, and TSan gates over focused cooker, filesystem lease, DDC, and publication suites and archive normalized results through `.github/workflows/feature-025-asset-cooker.yml`
- [ ] T153 Run Windows, macOS, and Linux CI, require every Feature 025 job to pass, and record workflow/run/artifact identities in `Validation/025/reports/cross-platform-ci.txt`
- [X] T154 Verify zero runtime-to-Tools, Asset-to-RHI/Renderer/Application/Backend, native-handle, private JSON, and untracked-output violations and record the final scan in `Validation/025/reports/architecture.txt`
- [X] T155 Create the Feature 025 system-design summary consistent with `doc/SYSTEM_DESIGN.MD` and prior feature pages in `doc/025-asset-cooker-derived-data.html`
- [ ] T156 Run `git diff --check`, contract/fixture verifiers, all focused suites, full Debug/strict Release regressions, sanitizers, hard-gated M4/CI benchmarks, CI, and every command in `specs/025-asset-cooker-derived-data/quickstart.md`; require every documented threshold and corpus count to pass before finalizing `Validation/025/README.md`
- [ ] T157 After T156 passes, update Feature 025 status, actual dependency evidence, exclusions, and Feature 026 handoff in `doc/roadmap.md` and `AGENTS.md`

---

## Dependencies And Execution Order

### Phase Dependencies

- **Phase 1 - Setup**: Starts immediately. T003 and T004 may run in parallel;
  T005 depends on their conventions.
- **Phase 2 - Foundations**: Depends on Phase 1. Core transactions T007-T017
  complete first. Profile/key T018-T027 and cooked contracts T028-T044 may then
  proceed as separate tracks; T045 joins them and blocks every story.
- **Phase 3 - US1**: Depends on T045 and establishes the authoritative clean
  cook, graph, snapshots, scheduler, payloads, and manifest.
- **Phase 4 - US2**: Depends on US1 because DDC decisions wrap the same graph
  nodes and must converge to the clean generation.
- **Phase 5 - US3**: Depends on US1 and US2 so publication validates complete
  clean or cached generations through one path. Request-local generation images
  precede the lease; output-root staging begins only after T100 acquires it.
- **Phase 6 - US4**: Initial target-sensitive producer behavior is already
  complete in Foundations and US1. Full multi-profile and precise-invalidation
  acceptance depends on US2 but not US3, so it may overlap publication work.
- **Phase 7 - US5**: Depends on completed US3 and US4 because the CLI exposes
  publication, validation, and full target-profile decisions.
- **Phase 8 - Closeout**: Depends on all five user stories.

### User Story Dependency Graph

```text
Setup
  -> Foundations
      -> US1 Deterministic Clean Cook
          -> US2 Incremental DDC
              -> US3 Atomic Publication ---------+
              -> US4 Explicit Target Profiles ---+-> US5 CLI And Reproduction
                                                      -> Cross-Platform Closeout
```

### Requirement Ownership

| Story/Foundation | Primary Requirements |
|---|---|
| Foundations | FR-010 to FR-022, FR-031 to FR-035, FR-044, FR-054, FR-057, FR-058 |
| US1 | FR-001 to FR-013, FR-030, FR-034, FR-049, FR-051; SC-001, SC-002, SC-007, SC-008 |
| US2 | FR-017 to FR-030, FR-047 to FR-050; SC-002 to SC-004, SC-009 |
| US3 | FR-031 to FR-045; SC-005, SC-006, SC-009 |
| US4 | FR-002, FR-014 to FR-017, FR-046 to FR-048; SC-003, SC-015 |
| US5 | FR-030, FR-041 to FR-048, FR-052, FR-055, FR-056; SC-011, SC-012 |
| Shared cross-cutting | FR-044 to FR-058; SC-001 to SC-015 |

### Within Each User Story

1. Write the listed tests and confirm they fail for the intended missing
   behavior.
2. Implement public/value contracts before private orchestration.
3. Implement leaf validation and storage services before runner integration.
4. Register focused suites only after all referenced test functions compile.
5. Run and record the independent gate before starting a dependent story.

---

## Parallel Execution Examples

### Foundation Tracks

```text
Blocking Core track: T007-T017
Then in parallel:
  Track A: T018-T027 target profiles and derived keys
  Track B: T028-T044 cooked payloads, manifests, and codecs
Join: T045
```

### User Story 1

```text
Parallel failing-test/fixture wave: T046, T047, T048, T049, T050, T051
Then: T052-T055 contracts -> T056-T063 services -> T064-T070 integration
Gate: T071
```

### User Story 2

```text
Parallel failing-test/fixture wave: T072, T073, T074, T075
Then: T076-T080 DDC -> T081-T084 runner integration -> T085-T089 acceptance
Gate: T090
```

### User Story 3

```text
Parallel failing-test/probe/fixture wave: T091, T092, T093, T094, T095
Then: T096-T105 publication/validation -> T106-T110 integration/acceptance
Gate: T111
```

### User Stories 4 And 5

```text
US4 after US2: T112-T114 tests/profiles -> T115-T123 validation/acceptance -> T124
US5: T125-T128 tests/fixtures -> T129-T138 implementation/acceptance -> T139-T140
```

---

## Implementation Strategy

### Technical MVP

1. Complete T001-T006 setup.
2. Complete T007-T045 foundational contracts.
3. Complete T046-T071 for US1 deterministic clean cook.
4. Stop and validate two cooks from independently empty output/DDC roots plus
   source/cooked equivalence.

This MVP already provides a reusable offline canonical cook path and immutable
output contract, without claiming incremental caching or atomic publication.

### Incremental Delivery

1. **Foundation ready**: Core transactions, profiles, keys, payloads, and
   manifests pass.
2. **US1**: Deterministic clean cook passes independently.
3. **US2**: Validated local DDC converges to clean output.
4. **US3**: Immutable generations become atomically visible and independently
   validatable.
5. **US4**: Checked-in target profiles prove precise platform-sensitive
   invalidation and fallback.
6. **US5**: CLI, reports, inspection, and clean-machine reproduction close the
   operator workflow.
7. **Closeout**: Scale, sanitizers, architecture, CI, evidence, docs, and
   roadmap status complete Feature 025.

### Git And CI Strategy

- Commit M0 Core, M1 profile/key, and M2 payload/manifest foundations as
  separate cohesive conventional commits before the T045 join.
- Commit each user-story checkpoint with `feat`, `fix`, `test`, or `docs`
  prefixes; keep generated local DDC and cooked outputs untracked.
- Push and run CI after T045, T071, T090, T111, T124, T140, and final closeout.
  Use focused local gates between those boundaries to control Actions usage.
- Never amend published history after CI evidence references a commit.

## Notes

- `[P]` never permits concurrent edits to the same file.
- Runtime targets must never link `Tools/AssetCooker`; later runtime loading
  consumes only Asset-owned contracts and codecs.
- A semantic oracle cannot replace byte-level cooked-load equivalence,
  publication failure injection, or standalone validation.
- Feature 025 excludes runtime asset management, async handles, packaging,
  remote DDC, cache eviction/GC, streaming, GPU residency, and editor database
  behavior.
- Successful generation pruning is deliberately deferred; Feature 025 retains
  immutable successful generations.
