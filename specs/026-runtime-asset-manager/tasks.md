# Tasks: Runtime Asset Manager

**Input**: Design documents from specs/026-runtime-asset-manager/
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Contract, equivalence, mutation, coalescing, cancellation, dependency,
lifetime, process lease, shutdown, determinism, stress, sanitizer, and
cross-platform tests are mandatory and precede related implementation.

## Format: [ID] [P?] [Story] Description

- **[P]**: Different files and no dependency on another incomplete task in the wave.
- **[Story]**: Maps to a user story from spec.md.
- Every task names exact files or directories.

---

## Phase 1: Setup And Validation Scaffolding

**Purpose**: Establish suites, fixtures, and evidence ownership.

- [X] T001 Register Feature 026 test sources and helper executables in Tests/SConscript
- [X] T002 Register asset-manager suite selection and benchmark arguments in Tests/Main.cpp, Tests/AssetTests.cpp, and Tests/AssetTests.h
- [X] T003 [P] Create equivalence, dependency, mutation, malformed-generation, and scale fixture rules in Tests/Fixtures/RuntimeAssetManager/README.md
- [X] T004 [P] Establish normalized evidence and CI artifact conventions and ignore local Feature 026 work roots in Validation/026/README.md, Validation/026/reports/README.md, Validation/026/CI/README.md, and .gitignore
- [X] T005 [P] Add the contract/architecture verifier skeleton and failing unit tests in Tests/verify_runtime_asset_manager.py and Tests/test_verify_runtime_asset_manager.py
- [X] T006 Add failing bounded worker admission/ordering/stop/join and request-slot generation/terminal-commit/stale-handle kernel tests in Tests/AssetManagerKernelTests.cpp and Tests/AssetManagerKernelTests.h

**Checkpoint**: Every future artifact has an auditable location.

---

## Phase 2: Foundational Contracts And Runtime Kernel

**Purpose**: Complete ownership, validation, public vocabulary, and bounded
execution primitives required by every story.

### Shared/Exclusive Platform Ownership

- [X] T007 Extend the subprocess helper with shared/exclusive modes and stable outcomes in Tests/Helpers/PlatformFileLeaseProbe.cpp
- [X] T008 [P] Add failing shared/shared, shared/exclusive, timeout, move, crash-release, different-generation, and legacy-exclusive tests using the helper in Tests/CorePlatformFileLeaseTests.cpp and Tests/CorePlatformFileLeaseTests.h
- [X] T009 Define source-compatible EPlatformFileLeaseMode and Acquire overloads in Source/Core/Public/Core/FPlatformFileLease.h
- [X] T010 Implement process-local shared arbitration, POSIX flock modes, Win32 LockFileEx modes, timeout, and metadata compatibility in Source/Core/Private/FPlatformFileLease.cpp

### Published Generation Index Validation

- [X] T011 [P] Add failing FullPayloads versus IndexAndLayout policy plus exact-generation binding, explicit coordination root, read-only publication root, and rollback tests in Tests/AssetManagerCookedTests.cpp and Tests/AssetManagerCookedTests.h
- [X] T012 Add a source-compatible validation policy and index/layout evidence in Source/Asset/Public/Asset/FPublishedGenerationValidator.h
- [X] T013 Refactor shared index/layout checks and optional payload decoding while preserving Feature 025 defaults in Source/Asset/Private/FPublishedGenerationValidator.cpp

### Public Runtime Contracts

- [X] T014 [P] Define manager mode, finite request/completion/deadline limits, mutually exclusive mode configuration, read-only publication root, explicit writable coordination root, domain-separated canonical publication namespace v1, and validation in Source/Asset/Public/Asset/FAssetManagerConfig.h
- [X] T015 [P] Define generation-safe request identity, states, terminal results, snapshots, callbacks, and completion reservation vocabulary in Source/Asset/Public/Asset/FAssetRequestHandle.h
- [X] T016 [P] Define immutable typed handle traits, checked construction, copy/move ownership, and independent retention in Source/Asset/Public/Asset/TAssetHandle.h
- [X] T017 Define non-blocking Request, Query, GetResult, Cancel, ReleaseRequest, PumpCompletions, and Shutdown in Source/Asset/Public/Asset/FAssetManager.h

### Private Kernel

- [X] T018 [P] Implement a bounded FIFO worker executor with cooperative stop and join in Source/Asset/Private/FAssetWorkerExecutor.h and Source/Asset/Private/FAssetWorkerExecutor.cpp
- [X] T019 [P] Implement manager-safe request slots, generation reuse, terminal commit, and stale rejection in Source/Asset/Private/FAssetRequestTable.h and Source/Asset/Private/FAssetRequestTable.cpp
- [X] T020 [P] Define complete load keys, scratch results, normalized optional-fallback decisions validated by the owning payload contract, operation states, retention classes, source-compatible runtime-capability declaration, cooperative cancellation token, monotonic deadline request context, and the strategy boundary in Source/Asset/Public/Asset/FAssetRuntimeExecutionContext.h, Source/Asset/Public/Asset/FAssetExtensionRegistry.h, Source/Asset/Public/Asset/FAssetImportRequest.h, Source/Asset/Public/Asset/IAssetLoader.h, Source/Asset/Public/Asset/IAssetResolver.h, Source/Asset/Private/FAssetRuntimeTypes.h, and Source/Asset/Private/IAssetLoadingStrategy.h
- [X] T021 Implement canonical publication path normalization, domain-separated namespace digest v1, contained namespace creation, shared reader ownership, pointer-read/lease/exact-index binding, rollback, and read-only publication-root support in Source/Asset/Public/Asset/FGenerationReaderLease.h, Source/Asset/Private/FGenerationReaderLease.cpp, Source/Asset/Private/FBoundCookedGeneration.h, and Source/Asset/Private/FBoundCookedGeneration.cpp

**Checkpoint**: Foundations compile and focused tests preserve Feature 025.

---

## Phase 3: User Story 1 - Load Typed Assets Through One Runtime Contract (Priority: P1) MVP

**Goal**: Load every representative typed Asset and required dependency in
development or strict cooked mode through one API.

**Independent Test**: Compare all Feature 021-024 families in both modes and
verify strict failures and source-mutation rejection.

### Tests For User Story 1

- [X] T022 [US1] Create matching source and valid/corrupt cooked fixtures including required/optional extensions and declared/undeclared optional fallbacks in Tests/Fixtures/RuntimeAssetManager/Equivalence/ and Tests/Fixtures/RuntimeAssetManager/Cooked/
- [X] T023 [P] [US1] Add failing Request/Query/GetResult type, state, invalid-config, malformed-id, and no-partial-result tests using T022 fixtures in Tests/AssetManagerContractTests.cpp and Tests/AssetManagerContractTests.h
- [X] T024 [P] [US1] Add failing development/cooked equivalence, required/optional extension, and zero-source-fallback tests using T022 fixtures in Tests/AssetManagerEquivalenceTests.cpp and Tests/AssetManagerEquivalenceTests.h
- [X] T025 [P] [US1] Add failing required closure plus declared-satisfiable, undeclared, and unsatisfied optional fallback tests with stable inspectable decisions in Tests/AssetManagerDependencyTests.cpp and Tests/AssetManagerDependencyTests.h
- [X] T026 [P] [US1] Add failing mutation at every pre-publication stage, atomic multi-output, no-retry, and reload-after-unload tests in Tests/AssetManagerDevelopmentTests.cpp and Tests/AssetManagerDevelopmentTests.h

### Implementation For User Story 1

- [X] T027 [P] [US1] Implement cooperative cancellation/deadline propagation, development dispatch, atomic outputs, source pinning, and mutation verification in Source/Asset/Private/FDevelopmentAssetLoadingStrategy.h and Source/Asset/Private/FDevelopmentAssetLoadingStrategy.cpp
- [X] T028 [P] [US1] Implement bound-index lookup, required/optional extension registration checks, bounded reads, strict envelope/record checks, and zero fallback in Source/Asset/Private/FCookedAssetLoadingStrategy.h and Source/Asset/Private/FCookedAssetLoadingStrategy.cpp
- [X] T029 [US1] Implement deterministic required closure, declared optional fallback policy, cycle/version/type/limit checks, ready ordering, and inspectable failure/fallback paths in Source/Asset/Private/FAssetDependencyScheduler.h and Source/Asset/Private/FAssetDependencyScheduler.cpp
- [X] T030 [US1] Compose validated configuration, foundational bound generation, mode selection, worker execution, polling, typed publication, and rollback in Source/Asset/Private/FAssetManager.cpp
- [X] T031 [US1] Register runtime-compatible extensions, verify BuildLayer/test automatic source discovery and strict warnings, and register the equivalence gate in Source/Asset/Private/AssetModule.cpp, Source/Core/SConscript, Source/Asset/SConscript, Tests/SConscript, and Tests/AssetTests.cpp

**Checkpoint**: US1 is a complete headless MVP.

---

## Phase 4: User Story 2 - Share Concurrent Work Without Sharing Cancellation (Priority: P1)

**Goal**: Coalesce equal physical work while preserving independent cancellation.

**Independent Test**: Eight overlapping callers share one load; cancellation at
every state cannot damage surviving callers or shared dependencies.

### Tests For User Story 2

- [X] T032 [US2] Add a barrier-controlled runtime-compatible loader/importer with counters, cooperative deadline handling, and bounded non-conforming mode in Tests/Helpers/AssetManagerControlledLoader.h and Tests/Helpers/AssetManagerControlledLoader.cpp
- [X] T033 [P] [US2] Add failing key equivalence, in-flight/cache decision, eight-caller, and shared-dependency tests using T032 in Tests/AssetManagerCoalescingTests.cpp and Tests/AssetManagerCoalescingTests.h
- [X] T034 [P] [US2] Add failing per-state single/all-caller, late, dependency, deadline, and bounded non-conforming cancellation tests using T032 in Tests/AssetManagerCancellationTests.cpp and Tests/AssetManagerCancellationTests.h

### Implementation For User Story 2

- [X] T035 [US2] Implement complete-key operation indexing, caller membership, one-time commit, and reclamation in Source/Asset/Private/FAssetLoadOperationTable.h and Source/Asset/Private/FAssetLoadOperationTable.cpp
- [X] T036 [US2] Add shared-root/dependency retention and root-specific failure paths in Source/Asset/Private/FAssetDependencyScheduler.cpp
- [X] T037 [US2] Integrate independent cancellation, cooperative stop, late-result discard, and per-caller publication in Source/Asset/Private/FAssetManager.cpp
- [X] T038 [US2] Run the 100-repeat cancellation matrix and record evidence in Validation/026/reports/cancellation-matrix.txt

**Checkpoint**: Coalescing does not couple cancellation.

---

## Phase 5: User Story 3 - Retain Safe Typed Handles And Unload Deterministically (Priority: P2)

**Goal**: Preserve immutable handles across lifetimes and remove every
unreferenced cache entry immediately.

**Independent Test**: Copy, move, release, reload, recycle slots, race shutdown,
and retain payloads beyond manager destruction with exact accounting.

### Tests For User Story 3

- [X] T039 [P] [US3] Add failing three-class retention, immediate zero removal, dependency retention, reload, and byte accounting tests in Tests/AssetManagerCacheTests.cpp and Tests/AssetManagerCacheTests.h
- [X] T040 [P] [US3] Add failing handle copy/move/final-release, stale-slot, destruction, and retained-memory tests in Tests/AssetManagerLifetimeTests.cpp and Tests/AssetManagerLifetimeTests.h
- [X] T041 [P] [US3] Add failing shutdown/admission/loading/release races, cooperative deadline, bounded contract-violation diagnostics, and 100-iteration terminal audits in Tests/AssetManagerShutdownTests.cpp and Tests/AssetManagerShutdownTests.h

### Implementation For User Story 3

- [X] T042 [US3] Implement immutable entries, checked retention counts, byte limits, and synchronous zero removal in Source/Asset/Private/FAssetRuntimeCache.h and Source/Asset/Private/FAssetRuntimeCache.cpp
- [X] T043 [US3] Implement manager-independent handle controls and exact external release in Source/Asset/Public/Asset/TAssetHandle.h and Source/Asset/Private/FAssetRuntimeCache.cpp
- [X] T044 [US3] Integrate request/dependency/cache transitions, slot recycling, reload, and stale isolation in Source/Asset/Private/FAssetManager.cpp and Source/Asset/Private/FAssetRequestTable.cpp
- [X] T045 [US3] Implement idempotent shutdown, cooperative token/deadline signaling, queued cancellation, conforming-work join, bounded violation diagnostics, terminal audit, cache release, and destructor-safe discard without detach or forced termination in Source/Asset/Private/FAssetManager.cpp
- [X] T046 [US3] Run 10,000 lifecycle operations and 100 shutdown cycles and record Validation/026/reports/lifetime-stress.txt

**Checkpoint**: Handles safely outlive cache and manager; no grace cache remains.

---

## Phase 6: User Story 4 - Protect The Active Cooked Generation (Priority: P2)

**Goal**: Bind one generation under cross-process shared reader ownership.

**Independent Test**: Multiple processes read, maintenance contends, Current.json
changes, and normal/crash release behaves exactly by generation.

### Tests For User Story 4

- [X] T047 [US4] Add a runtime reader/exclusive maintenance subprocess probe with explicit coordination root in Tests/Helpers/GenerationReaderLeaseProbe.cpp
- [X] T048 [P] [US4] Add failing namespace digest golden/alias/collision evidence, missing/read-only coordination root, pointer replacement after binding, and final release tests using T047 in Tests/AssetManagerGenerationLeaseTests.cpp and Tests/AssetManagerGenerationLeaseTests.h
- [X] T049 [P] [US4] Add failing multiprocess reader, exclusion, crash recovery, generation independence, and publication tests using T047 in Tests/AssetManagerGenerationLeaseProcessTests.cpp and Tests/AssetManagerGenerationLeaseProcessTests.h

### Implementation For User Story 4

- [X] T050 [US4] Harden foundational lease ownership with namespace collision evidence, external exclusive-maintenance observation, and bounded normalized diagnostics in Source/Asset/Private/FGenerationReaderLease.cpp
- [X] T051 [US4] Harden foundational binding for pointer replacement, rollback, read-only publication content, and concurrent maintenance contention in Source/Asset/Private/FBoundCookedGeneration.cpp
- [X] T052 [US4] Integrate final-operation lease release ordering and bounded generation diagnostic capture into Source/Asset/Private/FAssetManager.cpp
- [X] T053 [US4] Verify the existing generation-reader contract and Feature 025 legacy exclusive compatibility without redesigning it in Tests/CorePlatformFileLeaseTests.cpp and Tests/verify_runtime_asset_manager.py

**Checkpoint**: Live generations exclude maintenance without PID heuristics.

---

## Phase 7: User Story 5 - Observe And Stress Runtime Loading (Priority: P3)

**Goal**: Deliver deterministic completion, bounded inspection, and stress evidence.

**Independent Test**: Vary worker order across twenty traces, poll without
dispatch, pump on a selected thread, test reentrancy, compare reports, and scale.

### Tests For User Story 5

- [X] T054 [P] [US5] Add failing admission reservation exhaustion/rollback/release plus poll, affinity, sequence, bounded-prefix, callback-operation, and recursive-pump tests in Tests/AssetManagerCompletionTests.cpp and Tests/AssetManagerCompletionTests.h
- [X] T055 [P] [US5] Add failing bounded ordering, stable evidence, truncation, and native-data redaction tests in Tests/AssetManagerInspectionTests.cpp and Tests/AssetManagerInspectionTests.h
- [X] T056 [P] [US5] Add twenty-repeat determinism, pre-bound-index/empty-payload-cache 1,000-node/5,000-edge scale with retained result handles, lifecycle, and limit stress tests in Tests/AssetManagerStressTests.cpp and Tests/AssetManagerStressTests.h
- [X] T057 [P] [US5] Add opt-in M4 Pro request, pre-bound-index cold-payload graph, pre-reserved pump, and lifecycle benchmarks in Tests/AssetManagerBenchmark.cpp and Tests/AssetManagerBenchmark.h

### Implementation For User Story 5

- [X] T058 [US5] Implement admission-time completion reservation, exhaustion rollback, sequence assignment, bounded queue, exact release, unlocked prefix dispatch, and recursive-pump guard in Source/Asset/Private/FAssetCompletionQueue.h and Source/Asset/Private/FAssetCompletionQueue.cpp
- [X] T059 [US5] Integrate terminal enqueue, poll-only observation, explicit pump, callback-safe operations, and shutdown semantics in Source/Asset/Private/FAssetManager.cpp
- [X] T060 [US5] Define bounded immutable request/operation/cache/generation snapshots in Source/Asset/Public/Asset/FAssetManagerInspection.h
- [X] T061 [US5] Implement deterministic inspection, truncation, counters, and redaction in Source/Asset/Private/FAssetManagerInspection.cpp
- [X] T062 [US5] Record twenty traces and M4 Pro evidence in Validation/026/reports/determinism.txt and Validation/026/reports/performance-m4-pro.txt

**Checkpoint**: Behavior is deterministic and inspectable without native leaks.

---

## Phase 8: Polish And Cross-Cutting Validation

- [X] T063 [P] Complete FR-001-FR-046 and SC-001-SC-013 trace checks in Tests/verify_runtime_asset_manager.py and Tests/test_verify_runtime_asset_manager.py
- [X] T064 [P] Add normalized focused/full-regression CI orchestration, timeout, cleanup, report generation, and runner unit tests in .github/scripts/run_runtime_asset_manager_validation.py and .github/scripts/test_run_runtime_asset_manager_validation.py
- [X] T065 Run focused Debug and strict Release on macOS and store Validation/026/reports/local-release.json
- [X] T066 Run the exact full-regression commands in quickstart section 10 and store the normalized result in Validation/026/reports/regression.txt
- [X] T067 Add path-filtered three-platform Debug/Release and Linux ASan/UBSan/TSan jobs, each uploading one uniquely named normalized artifact even on failure, in .github/workflows/feature-026-runtime-asset-manager.yml
- [ ] T068 Run the exact gh workflow/watch/download commands in quickstart section 11 for all eight jobs and record conclusions/digests in Validation/026/CI/README.md
- [X] T069 Document architecture, lifecycle, usage, exclusions, and evidence in doc/026-runtime-asset-manager.html following doc/SYSTEM_DESIGN.MD
- [ ] T070 Update Feature 026 completion and next Feature 027 status in doc/roadmap.md and AGENTS.md
- [ ] T071 Execute specs/026-runtime-asset-manager/quickstart.md, run git diff --check, and resolve discrepancies in owning files

---

## Dependencies And Execution Order

### Phase Dependencies

- Setup starts immediately; Foundation depends on Setup and blocks all stories.
- US1 supplies the loading MVP.
- US2 follows US1 for solo implementation.
- US3 depends on US1 publication and US2 interest accounting.
- US4 depends on Foundation and the US1 cooked strategy; it can proceed beside US2/US3.
- US5 depends on terminal publication from US1-US3.
- Polish depends on all stories.

### User Story Dependency Graph

```text
Setup -> Foundation -> US1 -> US2 -> US3 -> US5 -> Polish
                         \-> US4 -----------/
```

### Parallel Opportunities

- T003-T005, T008/T011, T014-T016, and T018-T020 are separate files.
- US1 fixture T022 precedes parallel tests T023-T026; then strategies T027/T028 are parallel.
- Test tasks within US2-US5 are parallel before each implementation wave.
- US4 process work can proceed alongside US2/US3 after US1.
- T063 and T064 are parallel closeout tooling.

## Parallel Example: User Story 1

```text
First wave: T022 fixtures
Second wave: T023, T024, T025, T026 failing tests
Third wave: T027, T028
Join: T029, T030, T031
```

## Implementation Strategy

### MVP First

1. Complete T001-T021.
2. Complete US1 T022-T031.
3. Validate source/cooked equivalence in Debug and Release.
4. Do not claim production readiness before US2-US5 and closeout.

### Incremental Solo Order

1. T001-T021: foundation.
2. T022-T031: typed loading MVP.
3. T032-T038: coalescing and cancellation.
4. T039-T046: cache, handles, and shutdown.
5. T047-T053: generation ownership.
6. T054-T062: completion, inspection, and stress.
7. T063-T071: CI, docs, and closeout.

## Notes

- Runtime code must not depend on Tools/AssetCooker.
- Feature 025 full validation and exclusive lease behavior remain compatible.
- Native/process gates cannot be replaced by semantic simulation.
- Streaming, priorities, eviction, GPU residency, hot reload, pruning,
  packaging, DDC cleanup, and network storage remain excluded.
