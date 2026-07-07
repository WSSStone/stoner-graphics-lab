# Implementation Plan: Scene Graph & ECS Foundation

**Branch**: `017-scene-graph-ecs` | **Date**: 2026-07-07 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/017-scene-graph-ecs/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/plan-template.md` for the execution workflow.

## Summary

Implement the Application layer's first scene organization foundation: a single-world Entity-Component-System surface with generation-safe entity handles, single-instance transform/mesh/light/camera components, deterministic parent-child hierarchy operations, world transform propagation, recursive subtree destruction, explicit component update/replace semantics, renderer-facing scene summaries, stable diagnostics, and deterministic debug/inspection output. The design keeps entity/component storage authoritative and flat, treats hierarchy and render summaries as explicit ordering views, and leaves physics, animation, scripting, serialization, editor UI, spatial acceleration structures, full archetype queries, and live graphics resource ownership out of scope.

## Technical Context

**Language/Version**: C++20 with traditional header/source separation; no C++20 Modules  
**Primary Dependencies**: Existing Core types, containers, math, strings, and logging/diagnostic conventions; existing Application window/input public boundary; existing Renderer forward/material public contracts only as abstract future handoff vocabulary; SCons 4.10.1  
**Storage**: Process-local in-memory world state, entity slot records, generation/version counters, component records, parent-child relationships, transform propagation caches, render collection summaries, diagnostics, and debug dump strings only; no persistent database, asset catalog, scene serialization, or live graphics resource ownership  
**Testing**: Existing SCons test target and platform-specific `StonerTest` executable; add focused Application scene/ECS tests for entity lifecycle, stale handles, component add/update/replace/remove, hierarchy transforms, recursive destruction, reparent preserve modes, render collection ordering, diagnostics, and deterministic dump stability; keep or update GitHub Actions/equivalent Windows/macOS/Linux build/test validation  
**Target Platform**: Cross-platform desktop development targets: Windows, macOS, Linux; validation must run headlessly without display access, GPU presentation, local assets, or external services  
**Project Type**: C++ graphics engine Application-layer library feature  
**Performance Goals**: Representative world with at least 100 entities can create, update, destroy, and validate handles without crashes; representative scene with at least 10 mesh entities, 4 lights, and 2 cameras produces stable render summaries across 20 repeated runs; hierarchy propagation and render collection for the representative scene complete well under one frame budget in debug validation  
**Constraints**: Public Application scene contracts must not expose Vulkan/Metal/DX/OpenGL types, RHI resource handles, Renderer-owned frame plan objects, live GPU resources, asset loaders, editor UI, serialization formats, physics, animation, scripting, multi-world scheduling, or archetype optimization details; hierarchy operation order and render collection order must be explicit ordering keys independent of internal storage layout  
**Scale/Scope**: Foundation scope covers one world, generation-safe entity handles, recursive entity destruction, single-instance transform/mesh/light/camera components, duplicate-add rejection, explicit update/replace operations, parent-child hierarchy, cycle rejection, default world-transform-preserving reparent with local-preserve option, topological transform/subtree order, per-category render collection by entity identity, diagnostics, and public Application contracts

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] **Spec-Driven Development**: The feature has an active specification with six recorded clarification decisions in `specs/017-scene-graph-ecs/spec.md`.
- [x] **Decoupled Architecture**: Application scene contracts own entity/component/hierarchy data only and do not call or expose graphics API, RHI, backend, or Renderer execution objects.
- [x] **Design Pattern Discipline**: World ownership, entity identity, component stores, hierarchy operations, transform propagation, render collection, diagnostics, and inspection can remain separate responsibilities.
- [x] **Multi-API Support**: Renderer-facing scene summaries use abstract mesh/light/camera data and world transforms that future Vulkan, Metal, DX, OpenGL, GLES, and WebGL paths can consume through Renderer/RHI layers.
- [x] **Advanced Graphics Readiness**: Stable entity identity, transforms, mesh/light/camera summaries, and deterministic ordering support future deferred, meshlet, ray tracing, and GI scene ingestion without backend coupling.
- [x] **Naming Conventions**: Planned public names follow UE5-style names such as `FWorld`, `FEntity`, `FTransformComponent`, `FMeshComponent`, `FLightComponent`, `FCameraComponent`, `FSceneRenderSummary`, and `FRenderSystem`.
- [x] **Cross-Platform Compatibility**: Planned code is standard C++20 at the public contract level and uses only existing Core/Application abstractions with no platform-specific runtime dependencies.
- [x] **Automated Cross-Platform Validation**: The plan keeps deterministic headless SCons test coverage in the existing Windows, macOS, and Linux CI/equivalent validation path; no display or GPU presentation is required.

## Project Structure

### Documentation (this feature)

```text
specs/017-scene-graph-ecs/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── scene-graph-ecs-contract.md
└── tasks.md
```

### Source Code (repository root)

```text
Source/Application/
├── Public/Application/
│   ├── ApplicationMinimal.h
│   ├── ESceneComponentType.h
│   ├── ESceneLightType.h
│   ├── ESceneProjectionType.h
│   ├── ESceneResult.h
│   ├── FCameraComponent.h
│   ├── FEntity.h
│   ├── FEntityHierarchy.h
│   ├── FLightComponent.h
│   ├── FMeshComponent.h
│   ├── FRenderSystem.h
│   ├── FSceneDiagnostics.h
│   ├── FSceneRenderSummary.h
│   ├── FTransformComponent.h
│   └── FWorld.h
├── Private/
│   ├── ESceneResult.cpp
│   ├── FCameraComponent.cpp
│   ├── FEntity.cpp
│   ├── FEntityHierarchy.cpp
│   ├── FLightComponent.cpp
│   ├── FMeshComponent.cpp
│   ├── FRenderSystem.cpp
│   ├── FSceneDiagnostics.cpp
│   ├── FSceneRenderSummary.cpp
│   ├── FTransformComponent.cpp
│   └── FWorld.cpp
└── SConscript

Tests/
├── ApplicationSceneEcsTests.h
├── ApplicationSceneEcsTests.cpp
├── Main.cpp
└── SConscript

.github/
└── workflows/
    └── ci.yml              # Existing cross-platform headless build/test matrix
```

**Structure Decision**: Add scene graph and ECS contracts to the existing Application layer because this feature owns runtime scene organization above Core math and below Renderer frame preparation. Public headers expose Application concepts only; Renderer-facing output is an abstract scene summary, not a Renderer frame plan or RHI resource binding. Tests live in the existing single test executable beside Application window/input, Core, RHI, Renderer, and Vulkan tests.

## Phase 0: Research

Completed in [research.md](./research.md). The main decisions are:

- Use flat authoritative entity/component storage with hierarchy as relationship data and ordered traversal views.
- Use generation/version-validated entity handles and allow destroyed slots to be reused safely.
- Treat transform/subtree ordering as topological parent-before-child order with roots in creation order and siblings in insertion order.
- Treat render collection ordering as per-category ascending entity identity, with optional sort keys allowed and entity identity as final tie-breaker.
- Destroying an entity recursively destroys descendants and invalidates their handles.
- Duplicate component add is rejected; explicit update/replace operations are required for mutation.
- Reparenting preserves world transform by default and exposes a local-preserve option.
- Keep spatial acceleration structures such as octrees/BVH/grids out of v1 and leave room for future derived indexes.
- Keep diagnostics and debug dumps byte-stable for regression tests.

## Phase 1: Design & Contracts

Completed outputs:

- [data-model.md](./data-model.md): entities, fields, validation rules, ordering keys, and state transitions.
- [contracts/scene-graph-ecs-contract.md](./contracts/scene-graph-ecs-contract.md): public Application-layer behavioral contract.
- [quickstart.md](./quickstart.md): expected development and verification flow.

## Post-Design Constitution Check

- [x] **Spec-Driven Development**: Plan, research, data model, contract, and quickstart align with the active spec and clarifications.
- [x] **Decoupled Architecture**: Contracts expose Application world/entity/component behavior without graphics API, RHI, backend, or Renderer execution dependencies.
- [x] **Design Pattern Discipline**: Design artifacts keep entity identity, component storage, hierarchy operations, transform propagation, render collection, diagnostics, and inspection independently testable.
- [x] **Multi-API Support**: Scene summaries stay backend-neutral and suitable for later Vulkan, Metal, DX, OpenGL, GLES, and WebGL integration through Renderer/RHI layers.
- [x] **Advanced Graphics Readiness**: Mesh/light/camera summaries, deterministic transform propagation, and stable identity ordering leave room for deferred rendering, meshlets, ray tracing, GI, and future spatial indexes.
- [x] **Naming Conventions**: Planned public names follow project naming conventions.
- [x] **Cross-Platform Compatibility**: The feature is headless, process-local, and standard C++20 at the public contract level.
- [x] **Automated Cross-Platform Validation**: Tasks must keep `.github/workflows/ci.yml` or equivalent validation coverage for `ubuntu-latest`, `macos-latest`, and `windows-latest` using deterministic headless build/test execution.

## Complexity Tracking

No constitution violations or complexity exceptions are required.
