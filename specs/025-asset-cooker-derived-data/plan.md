# Implementation Plan: Asset Cooker, Manifest & Derived Data

**Branch**: `025-asset-cooker-derived-data` | **Date**: 2026-08-14 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/025-asset-cooker-derived-data/spec.md`

## Summary

Feature 025 adds a standalone, offline `StonerAssetCooker` that turns the
source-backed Features 021-024 Asset graph into deterministic target-ready
payloads, a canonical manifest, and one atomically published immutable
generation. The tool supports explicit root Assets plus dependency closure and
an explicit cook-all mode, clean and incremental execution, plan-only
inspection, strict cache validation, and standalone published-output
validation.

Reusable target-profile, derived-key, cooked-envelope, manifest, and payload
loader contracts live in Asset so Feature 026 can consume them without
depending on Tools. Source discovery, graph scheduling, filesystem DDC policy,
staging, publication, reporting, and CLI parsing live under
`Tools/AssetCooker`. Core receives only the portable filesystem operations that
cannot be expressed safely by the current four-function abstraction: bounded
enumeration, canonical path containment, same-volume atomic move/replace,
durable writes, safe recursive removal, and an RAII cross-process file lease.

The DDC is a disposable local directory cache keyed by a complete
domain-separated SHA-256 evidence stream. Cooked payloads use a versioned
little-endian `.sgasset` envelope with deterministic type-specific bodies.
Profiles, DDC metadata, manifests, pointers, and reports reuse the project's
strict typed canonical JSON profile. Publication stages a self-contained
generation in the destination filesystem, validates it, re-verifies all source
versions, installs the immutable generation, and atomically replaces only
`Current.json` while holding the clarified bounded-wait cross-process lease.

## Technical Context

**Language/Version**: C++20 with traditional public/private header and source separation; no C++20 Modules; Python 3 standard-library validation scripts
**Primary Dependencies**: Existing Core and Asset contracts from Features 003-006 and 020-024; existing Asset-private yyjson 0.12.0 canonical JSON path; existing SHA-256 `FAssetDigest`; SCons 4.10.1; descriptor-owned POSIX `flock`/`rename` behind Core on macOS/Linux; durable Win32 file handles plus `ReplaceFileW`/`MoveFileExW` transaction primitives behind Core on Windows; no new third-party runtime dependency
**Storage**: Local source roots; local immutable directory-entry DDC; local self-contained cooked generation directories and atomic `Current.json`; checked-in target profiles, fixtures, schemas, and normalized validation evidence; no database, archive/package, remote cache, network service, or runtime cache
**Testing**: Existing `StonerTest` suite registry plus focused asset-cooker profile/manifest/envelope/key/graph/DDC/snapshot/lease/publication/determinism/concurrency/performance suites; subprocess CLI probes; Python schema, fixture, layout, and architecture verifiers; Debug and strict Release on Windows/macOS/Linux; Linux ASan/UBSan and TSan; clean-machine, incremental, corruption, failure-injection, and twenty-repeat validation
**Target Platform**: Cooker host support on Windows x64, macOS Apple Silicon, and Linux x64; explicit target profiles initially cover the repository's Vulkan Windows/Linux/macOS delivery evidence and leave registered Metal/DX12/OpenGL/GLES profile extensions for later backend features
**Project Type**: Layered C++ graphics engine with a new standalone offline CLI tool and reusable Asset delivery contracts
**Performance Goals**: On Apple M4 Pro Release, plan a 1,000-asset/5,000-edge graph in <=2 seconds, complete a fully cached incremental cook in <=10 seconds, validate it in <=10 seconds, clean-cook that synthetic graph in <=60 seconds, and separately clean-cook the representative Feature 021-024 corpus in <=60 seconds; these are hard reference gates, while CI uses separate hard 4x time ceilings for smoke regression only
**Constraints**: `Tools/AssetCooker -> Asset + Core`; `Asset -> Core`; no runtime-to-Tools edge; immutable AssetId distinct from derived/version hashes; host-independent normalized output; bounded inputs, workers, arithmetic, JSON, paths, graph, diagnostics, and aggregate payload bytes; no source lock; no automatic source-change retry; no in-place publication; no automatic successful-generation pruning in Feature 025
**Scale/Scope**: Default maxima of 100,000 discovered sources/assets, 1,000,000 dependency edges, depth 256, 1 GiB per source/payload, 8 GiB aggregate active payload bytes, 256 MiB manifest, 4,096 diagnostics, and 1-32 workers; validation corpus includes every Feature 021-024 payload family, at least 1,000 synthetic assets/5,000 edges, at least 15 DDC corruption cases, at least 30 separate published-generation corruption cases, and concurrent same-key/same-root requests

## Constitution Check

*GATE: Passed before research and re-checked after Phase 1 design.*

- [x] **Spec-Driven Development**: The clarified specification contains 58
  functional requirements, 15 measurable success criteria, five recorded
  policy decisions, bounded exclusions, and no unresolved clarification marker.
- [x] **Decoupled Architecture**: Reusable cooked data, manifest, profile, and
  loading contracts remain `Asset -> Core`. Offline coordination is isolated in
  `Tools/AssetCooker -> Asset + Core`. Runtime modules never include or link
  Tools, and no graphics layer participates in cooking.
- [x] **Design Pattern Discipline**: Profile parsing, discovery, graph planning,
  scheduling, codec dispatch, DDC storage, snapshot verification, publication,
  validation, and reporting have separate interfaces. The CLI is a composition
  root, not a god-class.
- [x] **Multi-API Support**: Target profiles describe graphics backend and
  shader/texture payload choices as Asset vocabulary. No RHI or native API type
  enters Asset or Tools; later backends register their payload producers.
- [x] **Advanced Graphics Readiness**: Domain-separated extension contracts can
  add meshlet, ray-tracing, SDF, and backend-shader derived data without changing
  AssetId, manifest fundamentals, or DDC authority.
- [x] **Naming Conventions**: Engine and tool C++ contracts follow project
  PascalCase and `F`/`E`/`I`/`T` prefixes. Serialized fields and CLI terms have
  canonical lower-camel/kebab spellings defined in contracts.
- [x] **Cross-Platform Compatibility**: Platform locks and atomic replacement
  are isolated in Core. Paths are UTF-8 logical contracts converted at Core
  boundaries. Tool behavior and normalized evidence are shared across all three
  hosts.
- [x] **Automated Cross-Platform Validation**: Windows, macOS, and Linux build,
  clean/incremental cook, CLI, Unicode/path, and publication tests are required;
  Linux ASan/UBSan/TSan and retained normalized artifacts cover memory and
  concurrency risks.

### Post-Design Re-check

Phase 1 introduces no constitutional exception. Core additions are generic
platform primitives, Asset remains graphics-independent, and Tools is a terminal
offline dependency node. The design deliberately excludes runtime requests,
packaging, streaming, and GPU residency instead of hiding them in the cooker.

## Project Structure

### Documentation (this feature)

```text
specs/025-asset-cooker-derived-data/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── asset-cooker-cli.md
│   ├── cooked-payload.md
│   ├── derived-data-layout.md
│   ├── manifest.schema.json
│   ├── publication-protocol.md
│   ├── report.schema.json
│   └── target-profile.schema.json
└── tasks.md
```

### Source Code (repository root)

```text
Source/
├── Core/
│   ├── Public/Core/
│   │   ├── FPlatformFileLease.h
│   │   └── FPlatformFileSystem.h
│   └── Private/
│       ├── FPlatformFileLease.cpp
│       ├── FPlatformFileSystem.cpp
│       ├── FPlatformFileSystemPosix.cpp
│       └── FPlatformFileSystemWindows.cpp
└── Asset/
    ├── Public/Asset/
    │   ├── FAssetCookManifest.h
    │   ├── FAssetCookContractCodec.h
    │   ├── FAssetCookedPayload.h
    │   ├── FAssetDerivedKey.h
    │   ├── FPublishedGenerationValidator.h
    │   ├── FAssetTargetProfile.h
    │   ├── IAssetCooker.h
    │   └── IAssetLoader.h
    └── Private/
        ├── FAssetCookManifestCodec.*
        ├── FAssetCookedPayloadCodec.*
        ├── FAssetDerivedKeyBuilder.*
        ├── FPublishedGenerationValidator.cpp
        ├── FAssetTargetProfileCodec.*
        ├── FImageTextureCookedCodec.*
        ├── FMaterialShaderCookedCodec.*
        └── FStaticModelCookedCodec.*

Tools/AssetCooker/
├── Public/AssetCooker/
│   ├── FAssetCookRequest.h
│   ├── FAssetCookReport.h
│   ├── FAssetCookResult.h
│   └── FAssetCookRunner.h
├── Private/
│   ├── FAssetCookCli.*
│   ├── FAssetCookGraph.*
│   ├── FAssetCookScheduler.*
│   ├── FAssetSourceCatalog.*
│   ├── FCookInputSnapshot.*
│   ├── FDerivedDataStore.*
│   ├── FCookedGenerationPublisher.*
│   ├── FPublishedGenerationValidator.h (compatibility alias)
│   └── Main.cpp
└── SConscript

Config/AssetCooker/Profiles/
├── Linux-Vulkan.json
├── Mac-Vulkan.json
└── Windows-Vulkan.json

Tests/
├── AssetCookerCodecTests.*
├── AssetCookerGraphTests.*
├── AssetCookerDerivedDataTests.*
├── AssetCookerPublicationTests.*
├── AssetCookerDeterminismTests.*
├── AssetCookerConcurrencyTests.*
├── AssetCookerBenchmark.*
├── Fixtures/AssetCooker/
└── verify_asset_cooker_contracts.py

Validation/025/
├── README.md
├── fixture-manifest.json
├── reports/
└── artifacts/                  # only small normalized/golden evidence tracked

.github/workflows/
└── feature-025-asset-cooker.yml
```

**Structure Decision**: Extend Core only for reusable platform primitives,
extend Asset for runtime-consumable cooked contracts/codecs, and introduce the
first standalone `Tools/AssetCooker` composition root for offline policy.
`FAssetCookContractCodec` is the public typed parse/write/validate facade;
yyjson and type-specific codec implementations remain Asset-private, and Tools
MUST NOT include Asset-private headers. The tool exposes a small public runner
surface so unit/integration tests can drive the same path as the CLI; no runtime
layer links that library. Default local outputs live under ignored
`Saved/DerivedDataCache` and `Saved/Cooked`, while only source profiles,
fixtures, schemas, and normalized validation evidence are tracked.

## Design And Delivery Order

### M0 - Core Filesystem Transaction Primitives

1. Add result-bearing, UTF-8-safe Core APIs for bounded recursive enumeration,
   regular-file queries, canonical containment, same-volume move, atomic file
   replacement, durable write, and safe recursive removal.
2. Add move-only `FPlatformFileLease` with non-inheritable native ownership,
   monotonic bounded acquisition, owner metadata, explicit timeout, and crash
   release semantics using POSIX record locks and Win32 sharing handles.
3. Prove same-directory replacement, cross-volume rejection, symlink/junction
   containment, Unicode/long path, permission failure, existing destination,
   process crash, and two-process timeout behavior on supported hosts.
4. Keep legacy `FPlatformFileSystem` callers source-compatible while routing new
   detailed operations through result categories and diagnostics.

### M1 - Target Profile And Deterministic Evidence

1. Add typed `FAssetTargetProfile` v1 and strict canonical parser/writer.
2. Compute effective-profile digest from normalized semantic configuration,
   excluding display name, and expose per-cooker projection evidence.
   `BuildPolicy` carries unique sorted schema-versioned producer-settings
   records; registered cookers validate and project exactly their own record.
3. Replace the ambiguous `FAssetCookRequest::TargetProfile` string with the
   typed profile/effective digest contract and migrate Feature 022 KTX2 cooking
   plus all tests without changing existing portable texture semantics.
4. Make shader, image/texture/KTX2, mesh, and model producers declare and apply
   their profile projections, target selection, and explicit fallback policy in
   this foundation so the first clean cook supports all Feature 021-024 types.
5. Add `FAssetDerivedKey` and a domain-separated, tagged, length-delimited key
   builder covering every byte-affecting source, dependency, importer, cooker,
   schema, projected schema-versioned profile `BuildPolicy` producer settings,
   and relevant target field.
6. Validate equivalent renamed profiles, irrelevant-field reuse, relevant-field
   invalidation, malformed profiles, ambiguous evidence boundaries, and golden
   digest stability on all hosts.

### M2 - Cooked Payload And Manifest Contracts

1. Define and validate the `SGCOOK01` little-endian envelope with canonical
   AssetId, codec/schema revisions, bounded body, body digest, and whole-envelope
   digest.
2. Implement registered deterministic cooker/loader codecs for all in-scope
   Feature 021-024 image, texture/KTX2, material, material-instance, shader
   source/payload/program, static-mesh, and static-model payloads.
3. Define typed `FAssetCookManifest` records, dependency/source evidence,
   selection evidence, canonical ordering, semantic generation hash, and final
   locator writing.
4. Expose profile, envelope, and manifest parse/write/validate operations only
   through the Asset-public `FAssetCookContractCodec` facade; keep yyjson and
   concrete implementations private.
5. Validate source-import versus cooked-load semantic equivalence, truncated and
   substituted envelopes, unknown codec/schema, limit overflow, every payload
   family, and byte-identical repeated writing.

### M3 - Source Catalog, Graph, Snapshot, And Scheduler

1. Add bounded stable source enumeration and built-in source adapters for the
   Feature 021-024 repository formats through public Asset resolver/importer
   contracts.
2. Build a request-local typed source catalog and reject path/Unicode/case or
   AssetId collisions before root selection.
3. Support mutually exclusive explicit-root closure and explicit cook-all root
   selection; build a normalized DAG with role/type/version checks, cycle paths,
   graph/depth/diagnostic limits, and deterministic topological indices.
4. Pin all resolved bytes and source-version records in `FCookInputSnapshot`;
   re-resolve each node's consumed locator set before committing its processing
   result, then re-resolve and hash the complete snapshot again before
   publication, returning `SourceChanged` without retry on mismatch.
5. Execute ready graph nodes through a 1-32 worker pool while committing results
   and diagnostics by plan index, not completion order. Compare 1-worker and
   8-worker output over twenty runs.

### M4 - Local Derived Data Store

1. Implement immutable DDC directory entries under full derived keys with
   canonical `Entry.json`, `Payload.sgasset`, same-root staging, validation, and
   final installation under a short-lived per-key native lease.
2. Validate every hit before reuse. On ordinary cook, quarantine invalid entries
   with stable failure evidence and rebuild; strict cache validation reports and
   fails without rebuilding.
3. Resolve concurrent same-key writers by cooking outside the key lease, then
   serializing final re-query/install and validating the winning immutable
   entry; no writer may overwrite an existing valid key.
4. Add clean hit/miss, unchanged reuse, complete invalidation matrix, stale
   metadata, corruption corpus, quarantine races, interrupted writes, and
   convergence-to-clean tests.
5. Do not add remote cache, cache eviction, automatic cache GC, or editor
   database behavior.

### M5 - Immutable Generation Publication

1. Build a self-contained generation image in request-local scratch outside the
   publication root; deduplicate digest-addressed `.sgasset` envelopes and
   prepare canonical manifest semantics without mutating output state.
2. Acquire the target-root publication lease with configurable bounded wait,
   create output-root staging, copy/finalize the generation image, validate it,
   and re-verify the complete input snapshot while the lease remains held.
3. Move the absent immutable generation into `Generations`, durably write
   `Current.next`, and atomically replace `Current.json` as the sole commit
   point. An equivalent already-installed generation is a successful no-op.
4. Treat a successful atomic replacement as committed success. Post-commit
   re-read is an audit diagnostic and cannot reclassify the cook as failed;
   simulated interruption at the commit boundary may reveal either complete
   pointer but never partial JSON or mixed content.
5. Keep all successful generations in Feature 025; clean failed staging
   best-effort under lease and never expose it through the current pointer.
6. Add failure injection at every filesystem boundary, previous-generation
   survival, two-process wait/timeout, process-crash lease release, validator
   race, wrong-filesystem rejection, and standalone corruption tests.

### M6 - CLI, Reports, And Clean-Machine Workflow

1. Build `StonerAssetCooker` with `cook`, `plan`, `validate`, `validate-cache`,
   and `inspect` commands, strict mutual-exclusion/required-option rules, stable
   process exit categories, and no arbitrary shell execution.
2. Emit one canonical normalized report for every command. Wall-clock/RSS
   telemetry is clearly non-deterministic and excluded from report digests.
3. Add checked-in Vulkan target profiles and a representative source corpus
   spanning every Feature 021-024 payload family plus synthetic scale/corruption
   fixtures with provenance and licenses.
4. Document the clean, incremental, plan-only, corruption, and standalone
   validation workflow in `quickstart.md`; add ignored `Saved/` defaults.
5. Prove a clean checkout can build the tool, cook, validate, inspect, remove the
   DDC, and still validate the published generation.

### M7 - Cross-Platform Evidence And Closeout

1. Add or update the Feature 025 workflow for Windows/macOS/Linux Debug and
   strict Release, Linux ASan/UBSan/TSan, deterministic repeats, subprocess
   lease/publication probes, and uploaded normalized reports.
2. Run the Apple M4 Pro Release reference benchmark as a hard gate for the
   documented plan/incremental/validation/clean and 1 GiB synthetic RSS budgets;
   record toolchain, corpus, times, peak RSS, hit/miss counts, bytes, and artifact
   digests without making host telemetry deterministic evidence. CI uses separate
   hard 4x time ceilings for smoke regression.
3. Run architecture verification for runtime-to-Tools, Asset-to-graphics, private
   JSON/native-type leakage, source/output tracking, and schema/fixture coverage.
4. Re-run all Features 020-024 and complete engine regressions, then update
   Feature 025 validation evidence, system-design HTML, roadmap status, and
   `AGENTS.md` only after every gate passes.

## Validation Gates

| Gate | Required Evidence |
|---|---|
| Contract | Profile, key, envelope, manifest, DDC metadata, report, and CLI golden fixtures parse/rewrite deterministically |
| Equivalence | Every valid Feature 021-024 source payload equals its cooked-load normalized model |
| Incremental | Complete mutation matrix invalidates exactly affected nodes; final incremental output equals clean output |
| Corruption | At least 15 DDC corruption cases rebuild/fail by mode, and a separate corpus of at least 30 published-generation corruptions is detected by standalone validation |
| Atomicity | Every injected pre-commit failure preserves current generation; no mixed generation is observable |
| Concurrency | Eight same-key requests and two same-root processes produce deterministic outcomes with no partial data |
| Determinism | Twenty runs and 1-worker/8-worker modes produce identical normalized artifacts |
| Scale | 1,000 assets/5,000 edges hard-fail when Apple M4 Pro Release plan/incremental/validation/clean or 1 GiB RSS budgets are exceeded; CI smoke jobs hard-fail at the documented 4x time ceilings |
| Architecture | Zero runtime-to-Tools, Asset-to-graphics, native-type, or private-import boundary violations |
| Cross-platform | Windows/macOS/Linux Debug and strict Release, Linux ASan/UBSan/TSan, and all regressions pass |

## Complexity Tracking

No constitutional violation requires an exception. Feature size is managed by
milestone gates and explicit ownership boundaries rather than by collapsing the
tool, cache, codecs, graph, and publication protocol into one component.
