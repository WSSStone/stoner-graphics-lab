# Implementation Plan: Asset Core, Identity & Registry

**Branch**: `020-asset-core` | **Date**: 2026-07-28 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/020-asset-core/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See
`.specify/templates/plan-template.md` for the execution workflow.

## Summary

Add the first `Source/Asset` runtime layer as a Core-only, CPU-side foundation.
The feature defines canonical typed `FAssetId` values with a fixed ASCII asset
type grammar, SHA-256-tagged version
digests, metadata and dependency records, typed soft references, an in-memory
concurrent-read registry, deterministic resolver/importer dispatch, loader and
cooker extension contracts, scoped registrations with execution leases,
diagnostics, and a public `FAssetInspection` formatting boundary. It also adds first-class
`StonerTest --suite asset` selection to close `CR001-B09-F003`.

Logical path and subresource text is validated and normalized to Unicode NFC
through a Core `FUnicode` wrapper backed by pinned, privately vendored
`utf8proc`. Registry writes
are serialized and staged before commit; readers observe complete pre-batch or
post-batch state. Missing required targets remain explicit `Unresolved` edges,
while known required cycles reject the batch. This phase uses synthetic
extensions and payloads only and does not implement concrete formats,
asynchronous asset management, persistence, Tools, RHI objects, or graphics
execution.

## Technical Context

**Language/Version**: C++20 with traditional header/source separation; no C++20 Modules
**Primary Dependencies**: Existing Core types, containers, strings, logging, ownership, and platform conventions; new Core `FUnicode` NFC wrapper; C++ standard library concurrency and value facilities (`<array>`, `<atomic>`, `<mutex>`, `<optional>`, `<shared_mutex>`, `<span>`); Core-private vendored `utf8proc 2.11.3` for Unicode 17 normalization; SCons 4.10.1
**Storage**: Process-local in-memory identities, metadata, dependency indexes, extension registrations, execution leases, diagnostics, and synthetic CPU payloads only; no persistent registry, database, manifest, cooked cache, or asset catalog
**Testing**: Existing SCons-built `StonerTest`; add `--list-suites` and `--suite asset` selection, a test-only in-process fake-suite callback for failure propagation, focused Asset tests, NIST SHA-256 vectors, forced internal lookup-hasher collisions, Unicode NFC corpus, deterministic repeated-run checks, concurrent-reader/serialized-writer stress, full regression, Linux ASan/UBSan, and Windows/macOS/Linux CI
**Target Platform**: Windows with MSVC, macOS with Apple Clang, and Linux with GCC/Clang; all Feature 020 validation is headless and graphics-runtime independent
**Project Type**: C++ graphics-engine static library layer with public engine subsystem contracts
**Performance Goals**: Exact identity lookup and indexed type/source lookup target average constant-time behavior; dependency queries are proportional to returned edges; deterministic dumps may sort in `O(n log n)`; an opt-in standalone `StonerAssetBenchmark` records 10,000 metadata records and 50,000 edges without running in `StonerTest` or CI gates
**Constraints**: Asset production code depends only on Core; Core privately wraps the Unicode implementation and no third-party type leaks publicly; asset types follow `[A-Za-z][A-Za-z0-9_.-]*` and remain case-sensitive; paths/subresources use NFC and case-sensitive identity semantics identically across platforms; registry readers must never observe partial batches; new extension selection stops at unregistration while acquired leases remain valid; importer selection considers at most 64 eligible candidates after hint filtering and reads at most 64 KiB per candidate; canonical identity text is limited to 1,024 UTF-8 bytes after NFC with 255-byte type, segment, and subresource limits
**Scale/Scope**: 12 primary domain entities, 30 functional requirements, 12 success criteria, one Asset static library, one focused test suite, three documented public contracts, and one CI-focused suite invocation; concrete payload formats and runtime async management remain in Features 021-026

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] **Spec-Driven Development**: The active specification records four user stories, 30 functional requirements, 12 measurable outcomes, and five accepted clarification decisions.
- [x] **Decoupled Architecture**: `Source/Asset` depends only on Core. Core owns the private `utf8proc` integration behind `FUnicode`, and no third-party type leaks through public headers. Asset does not include RHI, Renderer, Application, Backend, Tools, editor, or graphics API contracts.
- [x] **Design Pattern Discipline**: Identity normalization, digest representation, registry state, dependency validation, resolver dispatch, importer dispatch, extension lifetime, diagnostics, and inspection remain focused responsibilities. Resolver/importer/loader/cooker strategies are separate interfaces rather than one manager.
- [x] **Multi-API Support**: All public Asset contracts are CPU-side and graphics-API-neutral. Later Vulkan, Metal, DX12, OpenGL, GLES, and WebGL consumers receive assets through Renderer/RHI adapters outside this layer.
- [x] **Advanced Graphics Readiness**: Typed stable identity, versioned metadata, dependencies, and cooker/load contracts support later texture, meshlet, ray-tracing, and GI derived assets without creating GPU ownership here.
- [x] **Naming Conventions**: Planned names follow UE5-style conventions, including `FAssetId`, `FAssetVersion`, `FAssetRegistry`, `IAssetResolver`, `IAssetImporter`, `IAssetLoader`, `IAssetCooker`, and `TSoftAssetRef<T>`.
- [x] **Cross-Platform Compatibility**: Canonicalization uses one pinned Unicode implementation and platform-independent path grammar. Concurrency uses portable C++20 synchronization. Tests run headlessly on Windows, macOS, and Linux.
- [x] **Automated Cross-Platform Validation**: The existing CI matrix will explicitly run `StonerTest --suite asset` in Debug while full Debug, Release strict, and Linux sanitizer gates continue to cover regressions.

**PRE-DESIGN GATE RESULT**: PASS - no constitution violations.

## Project Structure

### Documentation (this feature)

```text
specs/020-asset-core/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── asset-core-api.md
│   ├── core-unicode-api.md
│   └── test-runner-cli.md
└── tasks.md
```

### Source Code (repository root)

```text
Source/Asset/
├── Public/Asset/
│   ├── AssetMinimal.h
│   ├── EAssetResult.h
│   ├── FAssetDependency.h
│   ├── FAssetDiagnostics.h
│   ├── FAssetDigest.h
│   ├── FAssetExtensionRegistry.h
│   ├── FAssetId.h
│   ├── FAssetInspection.h
│   ├── FAssetMetadata.h
│   ├── FAssetParticipant.h
│   ├── FAssetPayload.h
│   ├── FAssetRegistry.h
│   ├── FAssetSource.h
│   ├── FAssetVersion.h
│   ├── IAssetCooker.h
│   ├── IAssetImporter.h
│   ├── IAssetLoader.h
│   ├── IAssetResolver.h
│   └── TSoftAssetRef.h
├── Private/
│   ├── AssetModule.cpp
│   ├── FAssetDependencyGraph.cpp
│   ├── FAssetDiagnostics.cpp
│   ├── FAssetDigest.cpp
│   ├── FAssetDispatch.cpp
│   ├── FAssetExtensionRegistry.cpp
│   ├── FAssetId.cpp
│   ├── FAssetInspection.cpp
│   └── FAssetRegistry.cpp
└── SConscript

Source/Core/
├── Public/Core/
│   └── FUnicode.h
├── Private/
│   └── FUnicode.cpp
└── SConscript

ThirdParty/utf8proc/
├── LICENSE.md
├── VERSION
├── utf8proc.c
├── utf8proc.h
└── utf8proc_data.c

Tests/
├── AssetCoreTests.h
├── AssetCoreTests.cpp
├── Main.cpp
├── TestSuiteRegistry.h
├── TestSuiteRegistry.cpp
├── TestSuiteRegistryTests.h
├── TestSuiteRegistryTests.cpp
├── verify_asset_layer.py
├── AssetRegistryBenchmark.cpp
└── SConscript

.github/workflows/
└── ci.yml

site_scons/
└── LayerBuilder.py
```

**Structure Decision**: Add Asset as a peer engine layer immediately after Core
in `SConstruct`, with `BuildLayer` enforcing `Asset -> Core`. Add a focused
Core `FUnicode` API and privately compile pinned `utf8proc` C sources into Core
with isolated third-party warnings; neither Core nor Asset public headers expose
`utf8proc` symbols. Tests link Asset between higher-level engine libraries and
Core, and remain in the existing single executable with a general suite
registry rather than an Asset-specific test binary.

## Phase 0: Research

Completed in [research.md](./research.md). Principal decisions:

- Normalize valid UTF-8 path and subresource inputs through Core `FUnicode`,
  backed by vendored `utf8proc 2.11.3`, preserving case and rejecting
  invalid/path-unsafe syntax.
- Represent version evidence as algorithm-tagged 256-bit SHA-256 digests;
  equality compares algorithm, availability, length, and every digest byte.
  Collision tests force internal lookup hashers to collide; equal digest bytes
  remain equal revision evidence.
- Keep identity hashing as an internal lookup optimization with full-value
  equality and canonical identity ordering as the public deterministic order.
- Use a mutable registry protected by `std::shared_mutex`; validate and stage
  batches before exclusive commit, return copied query snapshots, and never
  expose references into mutable indexes.
- Accept absent required targets as `Unresolved`; reject a batch when its
  resolved required-edge graph would contain a cycle.
- Select a resolver only when one eligible candidate has unique highest
  priority; equal leaders produce `AmbiguousResolver`.
- Restrict importers by declared format hints where available, then probe at
  most 64 eligible candidates and 64 KiB per candidate. More than 64 eligible
  candidates returns `CapacityExceeded` before any probe; a unique highest
  confidence wins and ties fail.
- Separate registration tokens from acquired execution leases so unregistration
  removes future eligibility without cancelling or invalidating in-flight work.
- Model imported/loaded CPU data behind an Asset-owned `FAssetPayload` base so
  later typed payloads need no RHI or type-erased raw ownership.
- Resolve `CR001-B09-F003` through a reusable test suite registry supporting
  `--list-suites`, repeated `--suite <name>`, and no-argument all-suite behavior.
  A test-only in-process fake callback validates selected-suite failure
  propagation without adding a production CLI injection mechanism.

## Phase 1: Design & Contracts

Completed outputs:

- [data-model.md](./data-model.md): Entity fields, relationships, validation
  rules, indexes, and state transitions.
- [contracts/asset-core-api.md](./contracts/asset-core-api.md): Public Asset
  value, registry, extension, dispatch, diagnostics, and inspection contract.
- [contracts/core-unicode-api.md](./contracts/core-unicode-api.md): Minimal Core
  UTF-8 validation and NFC normalization contract with no third-party leakage.
- [contracts/test-runner-cli.md](./contracts/test-runner-cli.md): Focused test
  selection and backward-compatible all-suite command contract.
- [quickstart.md](./quickstart.md): Expected build, focused validation,
  regression, sanitizer, and CI workflow.

## Post-Design Constitution Check

- [x] **Spec-Driven Development**: Plan, research, data model, contracts, and quickstart trace to the clarified Feature 020 specification.
- [x] **Decoupled Architecture**: The designed Asset boundary imports Core only. Core privately owns the pinned, licensed, warning-isolated Unicode source behind `FUnicode`; no engine-layer or graphics dependency is introduced.
- [x] **Design Pattern Discipline**: Registry, dependency graph, dispatch, lifetime, payload, diagnostics, and inspection responsibilities are independently testable; extension behavior uses narrow strategy interfaces.
- [x] **Multi-API Support**: No backend type, shader representation, texture format, or graphics handle appears in Asset contracts.
- [x] **Advanced Graphics Readiness**: Stable typed IDs and derived-version inputs can identify future meshlet, BLAS, SDF, surface-cache, and streamed chunks without changing identity.
- [x] **Naming Conventions**: Public design and contract names use the project naming convention.
- [x] **Cross-Platform Compatibility**: The design fixes Unicode version, canonical grammar, ordering, digest bytes, concurrency behavior, and diagnostics independently from host filesystem and compiler.
- [x] **Automated Cross-Platform Validation**: The design adds focused Asset execution to all three Debug CI jobs and retains full suite, Release strict, and Linux sanitizer coverage.

**POST-DESIGN GATE RESULT**: PASS - no regressions or exceptions.

## Complexity Tracking

No constitution violations or complexity exceptions are required.
