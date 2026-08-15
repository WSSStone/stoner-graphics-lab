# Implementation Plan: Runtime Asset Manager

**Branch**: `026-runtime-asset-manager` | **Date**: 2026-08-15 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/026-runtime-asset-manager/spec.md`

## Summary

Feature 026 adds an Asset-layer runtime manager that loads the same immutable
typed payload contracts from development sources or one validated cooked
generation. A bounded manager-owned worker pool executes source/cooked loading
strategies and a dependency scheduler coalesces equivalent work while preserving
per-caller cancellation. Generation-safe request handles expose polling;
callbacks enter a deterministic completion queue and run only when consumers
explicitly pump it. Typed handles retain payload ownership independently of the
manager, while cache ownership disappears immediately after all three retention
classes reach zero.

Cooked startup reads `Current.json`, acquires a generation-scoped shared native
lease under an explicit writable coordination root, revalidates and binds the
selected immutable manifest/layout without modifying the potentially read-only
publication root, then loads
and validates individual `.sgasset` payloads on demand. Development loads pin
source-version evidence and fail without retry if it changes. The design extends
Core's existing exclusive file lease with compatible shared/exclusive modes,
splits published-generation validation into full and index/layout policies, and
introduces no dependency on Tools, Renderer, RHI, or a graphics API.

## Technical Context

**Language/Version**: C++20 with traditional public/private headers and sources; no C++20 Modules; Python 3 standard-library validation scripts  
**Primary Dependencies**: Existing Core ownership, filesystem, Unicode, diagnostics, and platform lease contracts; Asset identity/metadata/extension dispatch from 020; immutable payload types and validators from 021-024; target profile, manifest, envelope codec, and published-generation validation from 025; C++ standard library concurrency (`std::thread`, `std::mutex`, `std::condition_variable`, atomics); SCons 4.10.1; no new third-party dependency  
**Storage**: Immutable development source leases and one local potentially read-only published generation; explicit pre-provisioned writable local lease-coordination root; process-local request/operation/cache/diagnostic state; generation-scoped OS reader lease; no database, DDC ownership, package archive, persistent runtime cache, or network storage  
**Testing**: Existing `StonerTest` suite registry; focused manager state/coalescing/dependency/cache/source/cooked/lease/completion/shutdown/concurrency/stress suites; subprocess generation-lease probes; Python architecture/contract/evidence checks; Windows/macOS/Linux Debug and strict Release; Linux ASan/UBSan and TSan  
**Target Platform**: Windows x64, macOS Apple Silicon, Linux x64; headless and filesystem-only validation  
**Project Type**: Layered C++ graphics engine library with Asset runtime services  
**Performance Goals**: On Apple M4 Pro Release, accept and coalesce eight equivalent requests in <=5 ms excluding physical load; load a 1,000-Asset/5,000-edge synthetic graph from a pre-bound cooked manifest index with an empty payload cache in <=2 s while retaining the resulting root handles through measurement; pump 10,000 pre-reserved no-op completions in <=50 ms; complete 10,000 load/share/release cycles in <=30 s; CI smoke ceilings are 4x local reference limits  
**Constraints**: `Asset -> Core`; bounded workers, requests, graph depth/edges, payload and aggregate bytes, diagnostics, completion reservations, and extension deadlines; callback requests reserve delivery capacity at admission; runtime-compatible extensions receive cooperative cancellation/deadline context; no user callbacks under manager locks or on workers; no blocking public wait; no source-change retry; no grace cache; no runtime generation mutation/pruning; native lease details remain in Core  
**Scale/Scope**: Defaults of 100,000 known Assets, 1,000,000 dependency edges, depth 256, 1 GiB per payload, 8 GiB aggregate retained payload bytes, 4,096 diagnostics, 65,536 accepted requests, 65,536 queued completions, and 1-32 workers; representative validation covers all 021-024 families

## Constitution Check

*GATE: Passed before research and re-checked after Phase 1 design.*

- [x] **Spec-Driven Development**: The clarified spec defines 46 FRs, 13 SCs,
  five independently testable stories, four recorded policy decisions, and no
  unresolved marker.
- [x] **Decoupled Architecture**: Runtime manager, strategies, payloads, and
  generation binding stay in `Asset -> Core`; Tools, Renderer, RHI, Application,
  Backend, and graphics APIs do not enter public or private Asset dependencies.
- [x] **Design Pattern Discipline**: Loading strategy, dependency scheduler,
  operation table, cache, completion queue, generation binding, and inspection
  are separate responsibilities composed by `FAssetManager`.
- [x] **Multi-API Support**: The manager consumes target-profile evidence as
  Asset vocabulary and has no backend-specific payload selection logic.
- [x] **Advanced Graphics Readiness**: Typed complete-payload handles and shared
  dependency operations can later support meshlet/SDF/RT Assets; chunking,
  priority, budgets, and GPU residency remain explicitly deferred.
- [x] **Naming Conventions**: Public C++ uses PascalCase and `F`/`E`/`I`/`T`
  prefixes; diagnostic tokens use stable lower-case dotted spelling.
- [x] **Cross-Platform Compatibility**: Shared/exclusive ownership is isolated
  in Core using POSIX `flock` and Win32 byte-range locking; all manager behavior
  above it is shared C++.
- [x] **Automated Cross-Platform Validation**: A Feature 026 workflow covers
  Windows/macOS/Linux Debug and strict Release plus Linux ASan/UBSan/TSan, with
  focused headless runtime-manager and cross-process lease tests.

### Post-Design Re-check

Phase 1 introduces no constitutional exception. The only Core change is a
generic backward-compatible shared/exclusive file-lease primitive. Asset public
contracts contain no platform handles, scheduler implementation, Tools types,
or graphics vocabulary. On-demand cooked payload validation retains the strict
025 contract while avoiding eager decoding of every payload at startup.

## Project Structure

### Documentation (this feature)

```text
specs/026-runtime-asset-manager/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── generation-reader-lease.md
│   ├── loading-strategies.md
│   ├── request-lifecycle.md
│   └── runtime-manager-api.md
└── tasks.md
```

### Source Code (repository root)

```text
Source/
├── Core/
│   ├── Public/Core/FPlatformFileLease.h
│   └── Private/FPlatformFileLease.cpp
└── Asset/
    ├── Public/Asset/
    │   ├── FAssetManager.h
    │   ├── FAssetManagerConfig.h
    │   ├── FAssetManagerInspection.h
    │   ├── FAssetRequestHandle.h
    │   ├── FAssetRuntimeExecutionContext.h
    │   ├── FGenerationReaderLease.h
    │   └── TAssetHandle.h
    └── Private/
        ├── FAssetManager.cpp
        ├── FAssetDependencyScheduler.*
        ├── FAssetLoadOperationTable.*
        ├── FAssetRuntimeCache.*
        ├── FAssetCompletionQueue.*
        ├── FDevelopmentAssetLoadingStrategy.*
        ├── FCookedAssetLoadingStrategy.*
        └── FGenerationReaderLease.cpp

Tests/
├── AssetManagerContractTests.*
├── AssetManagerDependencyTests.*
├── AssetManagerCookedTests.*
├── AssetManagerConcurrencyTests.*
├── AssetManagerLifetimeTests.*
├── AssetManagerStressTests.*
├── Helpers/GenerationReaderLeaseProbe.cpp
└── verify_runtime_asset_manager.py

.github/
├── scripts/run_runtime_asset_manager_validation.py
└── workflows/feature-026-runtime-asset-manager.yml

Validation/026/
├── README.md
├── reports/
└── CI/
```

**Structure Decision**: Extend the existing layered engine and monolithic test
runner. Public engine contracts remain in `Source/Asset/Public`; orchestration
details remain in `Private`. Generic native ownership belongs in Core. No new
runtime module, executable, or Tools dependency is introduced.

## Implementation Phases

1. **M0 Contracts and Core ownership**: Finalize public state/result/config/
   handle contracts; extend platform lease modes; add generation index/layout
   validation policy, writable coordination namespaces, completion reservation,
   cooperative extension control, and bound-generation primitives while
   preserving 025 defaults.
2. **M1 Deterministic request kernel**: Implement generation-safe slots,
   operation coalescing keys, completion pump, bounded worker executor, cache
   retention accounting, and shutdown without physical loaders.
3. **M2 Development strategy**: Integrate resolver/importer/loader dispatch,
   atomic multi-output publication, dependency closure, pinned source evidence,
   mutation rejection, and family-equivalence fixtures.
4. **M3 Cooked strategy**: Consume the foundational bound generation, validate
   per-request envelope/record/dependencies/target/extensions, and prove zero
   source fallback and read-only publication-root behavior.
5. **M4 Concurrency and lifetime hardening**: Cancellation matrices, shared
   dependencies, stale handles, immediate unload, callback reentrancy, shutdown,
   process-exit lease recovery, sanitizers, and bounded stress.
6. **M5 Integration and closeout**: Three-platform workflow, normalized
   reports, performance reference, architecture checks, docs, and evidence.

## Complexity Tracking

No constitution violations require justification.
