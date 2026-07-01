# Implementation Plan: Render Graph Foundation

**Branch**: `013-render-graph-foundation` | **Date**: 2026-07-01 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/013-render-graph-foundation/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/plan-template.md` for the execution workflow.

## Summary

Implement the Renderer layer's render graph foundation: a deterministic DAG-based declaration, validation, compilation, execution, and text-debugging system for graphics/compute/copy-like work. The design keeps the public renderer API backend-agnostic, resolves transient resources during execution through RHI-facing resource creation, validates caller-supplied imported resources, emits compiled transitions during execution, stops immediately on pass failure, and reports aliasing eligibility without reusing backing storage in this phase.

## Technical Context

**Language/Version**: C++20 with traditional header/source separation; no C++20 Modules  
**Primary Dependencies**: Existing Core types/containers/logging; existing RHI public contracts for resources, command buffers, barriers, queues, and result/lifecycle states; SCons 4.10.1  
**Storage**: Process-local in-memory graph declarations, compiled schedules, diagnostics, transient resource records, imported resource bindings, and debug dump strings only  
**Testing**: Existing SCons test target and `Build/Mac/Debug/Tests/StonerTest`; add focused Renderer render graph tests with mock RHI command/resource behavior  
**Target Platform**: Cross-platform desktop development targets: Windows, macOS, Linux; headless test execution must not require a physical graphics device  
**Project Type**: C++ graphics engine library feature in the Renderer layer  
**Performance Goals**: Representative five-pass graph can be declared, compiled, inspected, and mock-executed in under 60 seconds through the project verification flow; repeated compilation of the same graph produces deterministic metadata across at least 20 runs  
**Constraints**: Must not include Vulkan/Metal/DX/OpenGL headers or concepts in Renderer public contracts; must not implement materials, concrete forward/deferred passes, scene graph integration, visible presentation, async compute overlap, multi-threaded compilation, or real backing-storage alias reuse  
**Scale/Scope**: Foundation scope covers at least five passes, four virtual resources, one imported resource, one exported output, graphics and compute pass kinds, transition planning/execution, culling, failure handling, reset/invalidation, and deterministic text debug output

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] **Spec-Driven Development**: The feature has an active specification with five recorded clarifications in `specs/013-render-graph-foundation/spec.md`.
- [x] **Decoupled Architecture**: Renderer code depends on Core and RHI contracts only; no backend graphics API dependency is introduced.
- [x] **Design Pattern Discipline**: Builder, graph, compiler, resource planner, transition planner, executor, and debug dump responsibilities remain separate.
- [x] **Multi-API Support**: The graph works through RHI-facing resource and command abstractions, so Vulkan/Metal/DX/OpenGL backends can implement the same behavior.
- [x] **Advanced Graphics Readiness**: The model supports graphics/compute passes, imported/transient resources, explicit outputs, side-effect passes, and future meshlet/ray tracing/GI workflows.
- [x] **Naming Conventions**: Planned types follow UE5-style names such as `FRenderGraph`, `FRenderGraphBuilder`, `ERenderGraphPassType`, and `FRenderGraphResourceDesc`.
- [x] **Cross-Platform Compatibility**: Planned code is standard C++20, headless-testable, and avoids OS/toolchain-specific assumptions.

## Project Structure

### Documentation (this feature)

```text
specs/013-render-graph-foundation/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── render-graph-contract.md
└── tasks.md
```

### Source Code (repository root)

```text
Source/Renderer/
├── Public/Renderer/
│   ├── RendererMinimal.h
│   ├── FRenderGraph.h
│   ├── FRenderGraphBuilder.h
│   ├── FRenderGraphCompiler.h
│   ├── FRenderGraphExecutor.h
│   ├── FRenderGraphResource.h
│   ├── FRenderGraphPass.h
│   └── FRenderGraphDiagnostics.h
├── Private/
│   ├── FRenderGraph.cpp
│   ├── FRenderGraphBuilder.cpp
│   ├── FRenderGraphCompiler.cpp
│   ├── FRenderGraphExecutor.cpp
│   ├── FRenderGraphResource.cpp
│   ├── FRenderGraphPass.cpp
│   └── FRenderGraphDiagnostics.cpp
└── SConscript

Tests/
├── RendererRenderGraphTests.h
├── RendererRenderGraphTests.cpp
├── Main.cpp
└── SConscript
```

**Structure Decision**: Add render graph foundation types to the existing Renderer layer because the feature is higher-level scheduling/orchestration over RHI contracts. Tests live in the repository's existing single test executable beside Core/RHI/Vulkan tests.

## Phase 0: Research

Completed in [research.md](./research.md). The main decisions are:

- Keep Render Graph public contracts backend-agnostic and expressed through Renderer/Core/RHI terminology.
- Use deterministic topological scheduling with stable insertion-order tie breaking.
- Resolve transient resources during execution, validate caller-supplied imported resources, and fail fast if resource resolution fails.
- Compile inspectable transition plans and emit those planned transitions during execution.
- Report aliasing eligibility only in this phase; actual backing-storage reuse is deferred.
- Stop execution immediately on the first pass failure.
- Keep debug output deterministic text for tests and developer inspection.

## Phase 1: Design & Contracts

Completed outputs:

- [data-model.md](./data-model.md): entities, fields, validation rules, and state transitions.
- [contracts/render-graph-contract.md](./contracts/render-graph-contract.md): public Renderer-layer behavioral contract.
- [quickstart.md](./quickstart.md): expected development and verification flow.

## Post-Design Constitution Check

- [x] **Spec-Driven Development**: Plan, research, data model, contracts, and quickstart align with the active spec and clarifications.
- [x] **Decoupled Architecture**: Contracts expose Renderer-layer concepts and reference RHI abstractions without backend details.
- [x] **Design Pattern Discipline**: Design artifacts keep declaration, compilation, planning, execution, diagnostics, and debug output independently testable.
- [x] **Multi-API Support**: The contract is backend-agnostic and suitable for current Vulkan plus future Metal/DX/OpenGL RHI implementations.
- [x] **Advanced Graphics Readiness**: Pass/resource/access model leaves room for later forward, deferred, material, meshlet, ray tracing, and GI work.
- [x] **Naming Conventions**: Planned public names follow project naming conventions.
- [x] **Cross-Platform Compatibility**: No platform-specific dependency is introduced; validation remains headless-testable.

## Complexity Tracking

No constitution violations or complexity exceptions are required.
