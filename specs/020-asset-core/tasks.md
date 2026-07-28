# Tasks: Asset Core, Identity & Registry

**Input**: Design documents from `/specs/020-asset-core/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Feature 020 explicitly requires automated contract, deterministic,
concurrency, lifecycle, architecture, and cross-platform tests. Test tasks are
therefore included before their corresponding implementation tasks.

**Organization**: Tasks are grouped by user story so each story produces an
independently testable Asset increment. The shared setup and foundation phases
must complete first because all stories depend on the Asset build boundary,
Unicode normalization, diagnostics vocabulary, and focused test runner.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel because it changes different files and has no
  dependency on another incomplete task in the same phase.
- **[Story]**: Maps the task to a user story in `spec.md`.
- Every task names the exact repository path it creates, modifies, or validates.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Add the pinned Unicode source and establish the Asset build target
without introducing a dependency beyond Core.

- [X] T001 Vendor utf8proc 2.11.3 sources, Unicode data, upstream license, and pinned version metadata in `ThirdParty/utf8proc/utf8proc.c`, `ThirdParty/utf8proc/utf8proc.h`, `ThirdParty/utf8proc/utf8proc_data.c`, `ThirdParty/utf8proc/LICENSE.md`, and `ThirdParty/utf8proc/VERSION`
- [X] T002 [P] Extend layer construction to support explicitly supplied private C sources and isolated third-party warning flags without weakening project-owned strict warnings in `site_scons/LayerBuilder.py`
- [X] T003 Compile the pinned utf8proc C sources privately into Core while keeping only Core public includes visible downstream in `Source/Core/SConscript`
- [X] T004 Create the Asset static-library target with Core as its only engine dependency in `Source/Asset/SConscript` and add its initial translation unit in `Source/Asset/Private/AssetModule.cpp`
- [X] T005 Insert Asset after Core in the root build and link/include it in the test executable in `SConstruct` and `Tests/SConscript`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Resolve the shared test-runner debt and establish common Asset
result, diagnostics, and suite entry points used by every user story.

**CRITICAL**: No user-story phase begins until focused suite selection and the
minimal Asset public vocabulary build successfully.

- [X] T006 Add failing parser, ordering, duplicate-selection, unknown-argument, and in-process fake-`asset` callback failure-propagation tests for the reusable suite table in `Tests/TestSuiteRegistryTests.cpp` and `Tests/TestSuiteRegistryTests.h`
- [X] T007 Implement canonical suite registration, `--list-suites`, repeatable `--suite`, `all`, deterministic execution order, and exit-code aggregation in `Tests/TestSuiteRegistry.cpp` and `Tests/TestSuiteRegistry.h`
- [X] T008 Refactor the executable entry point to register the test-runner contract suite and all existing suites through the suite table while preserving logging child probes and no-argument all-suite behavior in `Tests/Main.cpp`
- [X] T009 Add the stable result/stage/severity vocabulary, including `AccessDenied`, `MalformedSource`, and `TransientFailure`, plus diagnostic value declarations shared by all Asset operations in `Source/Asset/Public/Asset/EAssetResult.h` and `Source/Asset/Public/Asset/FAssetDiagnostics.h`
- [X] T010 Register an initially empty `asset` suite through `RunAssetCoreTests`, expose the Asset aggregate header, and keep all existing suites registered in `Tests/AssetCoreTests.cpp`, `Tests/AssetCoreTests.h`, and `Source/Asset/Public/Asset/AssetMinimal.h`

**Checkpoint**: `StonerTest --list-suites` includes `asset`, malformed selection
returns `2`, selected failures return `1`, and the no-argument command still
runs all pre-existing suites.

---

## Phase 3: User Story 1 - Stable Logical Identity (Priority: P1) MVP

**Goal**: Provide platform-independent typed identities, independent version
evidence, and type-safe unloaded references.

**Independent Test**: Run `StonerTest --suite asset` against at least 100
identity cases, including 20 NFC-equivalent Unicode pairs, SHA-256 vectors,
deliberate lookup collisions, repeated ordering, and soft-reference mismatch
cases without registering or loading an asset.

### Tests for User Story 1

- [X] T011 [US1] Add failing AssetType ASCII grammar, UTF-8/NFC path/subresource, path grammar, length-limit, canonical round-trip, idempotence, case-sensitivity, ordering, and 20-run determinism tests in `Tests/AssetCoreTests.cpp`
- [X] T012 [US1] Add failing NIST SHA-256 vectors, unavailable digest, lowercase parse/format, forced internal lookup-hasher collision, full-value equality, and identity/version independence tests in `Tests/AssetCoreTests.cpp`
- [X] T013 [US1] Add failing empty-reference, compatible typed-reference, and incompatible asset-type rejection tests in `Tests/AssetCoreTests.cpp`

### Implementation for User Story 1

- [X] T014 [US1] Implement thread-safe UTF-8 validation and NFC normalization with failure-output clearing behind the private utf8proc boundary in `Source/Core/Public/Core/FUnicode.h` and `Source/Core/Private/FUnicode.cpp`
- [X] T015 [US1] Implement fallible canonical identity creation, the case-sensitive `[A-Za-z][A-Za-z0-9_.-]*` AssetType grammar, platform-independent path/subresource grammar, full-value equality, canonical ordering, stable diagnostic text, and collision-safe lookup hashing in `Source/Asset/Public/Asset/FAssetId.h` and `Source/Asset/Private/FAssetId.cpp`
- [X] T016 [P] [US1] Implement algorithm-tagged 32-byte SHA-256 digests, unavailable state, lowercase hexadecimal conversion, and private portable hashing in `Source/Asset/Public/Asset/FAssetDigest.h` and `Source/Asset/Private/FAssetDigest.cpp`
- [X] T017 [US1] Implement independent source/content/cook evidence, producer version, target-profile validation, and full version equality in `Source/Asset/Public/Asset/FAssetVersion.h`
- [X] T018 [US1] Implement the minimal CPU payload type contract and compile-time Asset type trait validation for unloaded references in `Source/Asset/Public/Asset/FAssetPayload.h` and `Source/Asset/Public/Asset/TSoftAssetRef.h`
- [X] T019 [US1] Run the focused identity increment through `Build/Mac/Debug/Tests/StonerTest --suite asset`; fix production behavior for every contract mismatch, and adjust fixtures in `Tests/AssetCoreTests.cpp` only after documenting a demonstrable test defect without weakening accepted behavior

**Checkpoint**: Stable identities and typed soft references work without an
Asset registry, resolver, importer, loader, cooker, filesystem, or graphics
runtime.

---

## Phase 4: User Story 2 - Metadata Registry and Dependencies (Priority: P1)

**Goal**: Register, replace, remove, query, and inspect metadata graphs through
atomic process-local batches with concurrent readers.

**Independent Test**: Register at least 50 assets in 10 source groups with 100
dependency edges, exercise conflicts, cycles, target removal/restoration,
queries, and 8-reader/100-writer-batch stress, then verify every observed
snapshot and deterministic registry dump.

### Tests for User Story 2

- [X] T020 [US2] Add failing source scheme/locator canonicalization, participant/producer token grammar, canonical metadata equality across container insertion orders, attribute ordering, source provenance, dependency role/strength, duplicate-edge, unresolved-target, and idempotent-registration tests in `Tests/AssetCoreTests.cpp`
- [X] T021 [US2] Add failing atomic register/replace/remove, conflict rollback, self-cycle, multi-record cycle, direct/reverse lookup, completeness, and resolution-transition tests in `Tests/AssetCoreTests.cpp`
- [X] T022 [US2] Add failing 50-record/100-edge deterministic corpus and 8-reader/100-serialized-batch snapshot-invariant stress tests in `Tests/AssetCoreTests.cpp`

### Implementation for User Story 2

- [X] T023 [US2] Implement lowercase ASCII scheme canonicalization, NFC case-sensitive locator validation, stable equality/order/hash, and storage-independent source descriptors without a concrete provider in `Source/Asset/Public/Asset/FAssetSource.h`
- [X] T024 [US2] Implement stable participant and producer-version token values plus dependency roles, strengths, registry-derived resolution state, metadata provenance, attributes, canonical field equality, and validation in `Source/Asset/Public/Asset/FAssetParticipant.h`, `Source/Asset/Public/Asset/FAssetDependency.h`, `Source/Asset/Public/Asset/FAssetMetadata.h`, and `Source/Asset/Public/Asset/AssetMinimal.h`
- [X] T025 [US2] Implement proposed-graph construction, required-edge cycle detection, unresolved/resolved transitions, and deterministic forward/reverse edge rebuilding in `Source/Asset/Private/FAssetDependencyGraph.cpp`
- [X] T026 [US2] Implement staged atomic register/replace/remove batches, `std::shared_mutex` query snapshots, revision updates, exact/type/source/dependency indexes, rollback, completeness validation, and canonical query ordering in `Source/Asset/Public/Asset/FAssetRegistry.h` and `Source/Asset/Private/FAssetRegistry.cpp`
- [X] T027 [US2] Expose the initial public `FAssetInspection` API and implement owned deterministic formatting for identities, digests, versions, metadata, dependency edges, and registry snapshots alongside registry mutation diagnostics in `Source/Asset/Public/Asset/FAssetInspection.h`, `Source/Asset/Private/FAssetInspection.cpp`, `Source/Asset/Private/FAssetDiagnostics.cpp`, and `Source/Asset/Public/Asset/AssetMinimal.h`
- [X] T028 [US2] Run the focused registry increment through `Build/Mac/Debug/Tests/StonerTest --suite asset`; fix production behavior for every scale or concurrency invariant failure, and adjust corpus data in `Tests/AssetCoreTests.cpp` only after documenting a demonstrable test defect without weakening accepted behavior

**Checkpoint**: Metadata and dependency graphs are usable and inspectable
without loading content or registering an extension.

---

## Phase 5: User Story 3 - Discovery and Transformation Extensions (Priority: P2)

**Goal**: Register resolver, importer, loader, and cooker strategies; select
participants deterministically; discover multiple assets; and preserve
in-flight calls across unregistration.

**Independent Test**: Use only synthetic sources, payloads, and participants to
cover no-match, unique winner, priority/confidence alternatives, ties, bounded
probes, eight-output discovery, processing failures, duplicate registration,
and at least 100 dispatch/unregistration races.

### Tests for User Story 3

- [X] T029 [US3] Add failing resolver eligibility, unique-priority winner, equal-priority ambiguity, stable candidate-order, and explicit `NotFound`/`AccessDenied`/`MalformedSource`/`TransientFailure` mapping tests in `Tests/AssetCoreTests.cpp`
- [X] T030 [US3] Add failing importer hint filtering, 64-candidate pre-probe cap with a 65-candidate zero-callback case, 64 KiB per-candidate probe cap, confidence validation, unique winner, no-match, strongest-tie ambiguity, and registration-order independence tests in `Tests/AssetCoreTests.cpp`
- [X] T031 [US3] Add failing eight-output discovery, duplicate subresource, payload-type mismatch, dependency validation, repeated normalized output, and pre-registry-mutation failure tests in `Tests/AssetCoreTests.cpp`
- [X] T032 [US3] Add failing duplicate-participant, move-only token, future-selection cutoff, retained execution lease, stale-callback prevention, 100-race, and exactly-once destruction tests in `Tests/AssetCoreTests.cpp`
- [X] T033 [US3] Add failing synthetic loader/cooker success, unsupported, invalid-input, dependency-failure, processing-failure, target-profile, and cook-digest contract tests in `Tests/AssetCoreTests.cpp`

### Implementation for User Story 3

- [X] T034 [P] [US3] Implement read-only bounded source leases, resolver capability/request/result contracts, and explicit portable storage-failure category mapping in `Source/Asset/Public/Asset/FAssetSource.h` and `Source/Asset/Public/Asset/IAssetResolver.h`
- [X] T035 [P] [US3] Implement importer capability, 64-candidate and 64 KiB per-candidate probe limits, multi-output discovery, producer-version, and immutable synthetic payload contracts in `Source/Asset/Public/Asset/IAssetImporter.h`
- [X] T036 [P] [US3] Implement typed loader and target-profile cooker request/result contracts with explicit failure categories in `Source/Asset/Public/Asset/IAssetLoader.h` and `Source/Asset/Public/Asset/IAssetCooker.h`
- [X] T037 [US3] Implement per-kind participant registration, duplicate rejection, move-only scoped tokens, active candidate snapshots, shared execution leases, and deferred instance retirement in `Source/Asset/Public/Asset/FAssetExtensionRegistry.h` and `Source/Asset/Private/FAssetExtensionRegistry.cpp`
- [X] T038 [US3] Implement deterministic resolver priority selection, importer hint filtering with pre-probe `CapacityExceeded` enforcement, bounded probe selection, ambiguity diagnostics, lease-before-callback dispatch, and validated multi-output discovery in `Source/Asset/Private/FAssetDispatch.cpp`
- [X] T039 [US3] Run the focused extension increment through `Build/Mac/Debug/Tests/StonerTest --suite asset`; fix production behavior for every dispatch or lifecycle mismatch, and adjust synthetic participants in `Tests/AssetCoreTests.cpp` only after documenting a demonstrable test defect without weakening accepted behavior

**Checkpoint**: Later format features can add participants through stable
contracts without changing identity, registry, or extension lifetime behavior.

---

## Phase 6: User Story 4 - Diagnostics and Safe Evolution (Priority: P3)

**Goal**: Make every Asset decision deterministic and inspectable, protect the
Core-only boundary, and expose focused headless validation on all supported
platforms.

**Independent Test**: Repeat normalized diagnostics and dumps, run focused and
full suites, execute the architecture scanner, and verify Windows/macOS/Linux
Debug jobs require no graphics runtime.

### Tests for User Story 4

- [X] T040 [US4] Add failing normalized diagnostic ordering, first-actionable-error, stable category/stage/subject/participant, empty-state inspection, and native-address/platform-text exclusion tests in `Tests/AssetCoreTests.cpp`
- [X] T041 [US4] Add executable-level child-process coverage only for list-only execution, real Asset-only execution, and invalid usage status; keep fake failure and no-argument selection semantics in the in-process suite-table tests, and leave the real no-argument executable run to T046/T051 in `Tests/TestSuiteRegistryTests.cpp`
- [X] T042 [US4] Add failing architecture checks for forbidden Asset includes, non-Core engine links, public utf8proc leakage, and direct Asset third-party compilation in `Tests/verify_asset_layer.py`

### Implementation for User Story 4

- [X] T043 [US4] Complete stable diagnostic construction, sorting, normalized formatting, and first-actionable selection in `Source/Asset/Private/FAssetDiagnostics.cpp`
- [X] T044 [US4] Extend `FAssetInspection` with canonical registered-extension capability and participant-ambiguity formatting while preserving its existing value and registry snapshot output in `Source/Asset/Public/Asset/FAssetInspection.h` and `Source/Asset/Private/FAssetInspection.cpp`
- [X] T045 [US4] Integrate `Tests/verify_asset_layer.py` as a build dependency and ensure it scans production/build boundaries independently of graphics availability in `Tests/SConscript`
- [X] T046 [US4] Add explicit `StonerTest --suite asset` execution to Windows, macOS, and Linux Debug jobs while retaining no-argument regression, three-platform Release strict, and Linux sanitizer jobs in `.github/workflows/ci.yml`
- [X] T047 [US4] Run the focused diagnostics and architecture increment through `Build/Mac/Debug/Tests/StonerTest --suite asset` and `python3 Tests/verify_asset_layer.py`; fix production or boundary violations, and adjust normalized expectations in `Tests/AssetCoreTests.cpp` only after documenting a demonstrable test defect without weakening accepted behavior

**Checkpoint**: The Asset suite is independently selectable, diagnostics are
stable, and automation enforces `Asset -> Core` without a GPU.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Close quantitative criteria, documentation, regression, and
cross-platform gates without expanding Feature 020 into concrete formats or
runtime asset management.

- [X] T048 Build an opt-in standalone `StonerAssetBenchmark` executable for 10,000-record/50,000-edge registry query, mutation, and deterministic-dump timings, excluding it from `StonerTest`, no-argument regression, and CI gates in `Tests/AssetRegistryBenchmark.cpp` and `Tests/SConscript`
- [X] T049 Audit the public aggregate, include hygiene, ownership semantics, `noexcept` usage, PascalCase naming, and comments for all exported contracts in `Source/Asset/Public/Asset/AssetMinimal.h`
- [X] T050 Run forbidden-dependency and private-third-party leakage scans over `Source/Asset`, `Source/Core/Public`, `Source/Core/Private/FUnicode.cpp`, and `Source/Asset/SConscript`, then encode any missing invariant in `Tests/verify_asset_layer.py`
- [X] T051 Run strict Debug and Release builds plus focused and full regression using `scons config=debug strict=1`, `scons config=release strict=1`, `Build/Mac/Debug/Tests/StonerTest --suite asset`, and `Build/Mac/Debug/Tests/StonerTest`
- [ ] T052 Run Linux ASan/UBSan focused and full suites through the sanitizer commands documented in `specs/020-asset-core/quickstart.md` and preserve the commands in `.github/workflows/ci.yml`
- [X] T053 Update delivered architecture, API, validation, and exclusions documentation from `doc/SYSTEM_DESIGN.MD` into `doc/020-asset-core.html` and reconcile final commands and outcomes in `specs/020-asset-core/quickstart.md`
- [ ] T054 Verify all 30 functional requirements and 12 success criteria against tests and CI, mark completed implementation tasks in `specs/020-asset-core/tasks.md`, and confirm Windows/macOS/Linux gates in `.github/workflows/ci.yml`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: Starts immediately. T001 and T002 can proceed in
  parallel; T003 consumes both. T004 precedes T005.
- **Foundational (Phase 2)**: Depends on Phase 1 and blocks every user story.
  T006 precedes T007; T007 precedes T008; T009 precedes the Asset operations;
  T010 makes the focused suite selectable.
- **US1 (Phase 3)**: Depends only on Foundation and is the MVP.
- **US2 (Phase 4)**: Depends on US1 identity, digest, and version values.
- **US3 (Phase 5)**: Depends on US1 values and US2 metadata/dependency
  validation so discovery can validate a complete output batch before mutation.
- **US4 (Phase 6)**: Depends on US1-US3 behavior so diagnostics and inspection
  can cover all operation stages.
- **Polish (Phase 7)**: Depends on all desired user stories.

### User Story Dependency Graph

```text
Setup -> Foundation -> US1 -> US2 -> US3 -> US4 -> Polish
```

The stories are independently testable at each checkpoint, but their
implementation order is intentionally cumulative because Registry owns
`FAssetId`, dispatch produces `FAssetMetadata`, and complete diagnostics inspect
all earlier operations.

### Within Each User Story

- Add contract and acceptance tests first and confirm they fail for the missing
  behavior.
- Implement public value/contracts before private services that consume them.
- Acquire extension leases before invoking any callback.
- Complete the focused checkpoint before moving to the next story.
- Never satisfy a deterministic or lifecycle gate with filtered output,
  environment-variable suite skips, native addresses, or registration order.

## Parallel Opportunities

- T001 and T002 can run together.
- In US1, T016 can proceed independently after its tests while identity
  normalization is implemented in T014-T015.
- In US3, resolver/source contracts (T034), importer contracts (T035), and
  loader/cooker contracts (T036) can proceed together after their tests exist.
- CI editing in T046 should wait for the local suite and architecture commands
  to exist; it is not marked parallel with implementation.
- Tasks sharing `Tests/AssetCoreTests.cpp`, registry internals, extension
  internals, or build scripts are intentionally sequential and carry no `[P]`.

## Parallel Example: User Story 3

```text
Task T034: Implement source and resolver contracts in
Source/Asset/Public/Asset/FAssetSource.h and IAssetResolver.h

Task T035: Implement importer and discovery contracts in
Source/Asset/Public/Asset/IAssetImporter.h

Task T036: Implement loader and cooker contracts in
Source/Asset/Public/Asset/IAssetLoader.h and IAssetCooker.h
```

After T034-T036 complete, continue sequentially with T037 registration
lifetime, T038 dispatch, and T039 focused verification.

## Implementation Strategy

### MVP First

1. Complete Setup and Foundation.
2. Complete US1 identity, digest, version, and typed-reference behavior.
3. Stop at T019 and validate `StonerTest --suite asset`.
4. Treat this checkpoint as the minimum usable Feature 020 increment; do not
   claim the complete feature until US2-US4 and cross-platform gates pass.

### Incremental Delivery

1. **US1**: Stable logical references and version evidence.
2. **US2**: Atomic metadata/dependency registry.
3. **US3**: Extensible deterministic processing contracts.
4. **US4**: Diagnostics, focused selection, architecture enforcement, and CI.
5. **Polish**: Scale sample, full regression, sanitizer, documentation, and
   requirement closure.

### Commit Strategy

- Commit Setup/Foundation as `build(asset)` and `test(runner)` changes.
- Commit each user-story checkpoint as one or more focused `test(asset)`,
  `feat(asset)`, or `fix(asset)` commits.
- Keep CI/documentation changes in `ci(asset)` or `docs(asset)` commits.
- Do not combine Feature 021 concrete image/texture work with Feature 020.

## Notes

- `[P]` is used only for different files with no incomplete prerequisite in the
  same phase.
- Feature 020 remains process-local, synchronous, CPU-side, and format-neutral.
- No task adds RHI, Renderer, Application, Backend, Tools, editor, native
  graphics, persistent registry, async request, cancellation, hot reload,
  network storage, or residency behavior.
- The native deferred-session finding `CR001-B09-F005` remains outside scope.
