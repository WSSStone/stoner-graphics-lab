# Implementation Plan: Forward Rendering Pipeline

**Branch**: `015-forward-rendering-pipeline` | **Date**: 2026-07-03 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/015-forward-rendering-pipeline/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/plan-template.md` for the execution workflow.

## Summary

Implement the Renderer layer's first forward rendering pipeline planning surface: deterministic forward frame preparation, render graph-compatible pass/resource declarations, view and light validation, reusable mesh draw descriptions, full PBR-style material input validation, configurable point light culling with deterministic influence ordering, sky/background participation, transparent draw ordering, ambient-only no-light fallback diagnostics, and stable debug dumps. The design remains headless and backend-agnostic: it does not require real GPU execution, swapchain/window presentation, shadow mapping, post-processing, deferred rendering, or Application-layer scene ownership.

## Technical Context

**Language/Version**: C++20 with traditional header/source separation; no C++20 Modules  
**Primary Dependencies**: Existing Core types/containers/math/logging; existing Renderer render graph and material/shader public contracts; existing RHI public resource/result conventions only for abstract compatibility summaries; SCons 4.10.1  
**Storage**: Process-local in-memory forward frame inputs, prepared frame plans, view records, light records, mesh draw descriptions, render graph-compatible declaration summaries, diagnostics, and debug dump strings only  
**Testing**: Existing SCons test target and `Build/Mac/Debug/Tests/StonerTest`; add focused Renderer forward pipeline tests that run headlessly without a visible window, swapchain presentation, or physical GPU requirement  
**Target Platform**: Cross-platform desktop development targets: Windows, macOS, Linux; headless test execution must not require a visible window or GPU-backed presentation  
**Project Type**: C++ graphics engine library feature in the Renderer layer  
**Performance Goals**: Representative forward frame with one view, one directional light, more than the configured point light limit, at least four opaque draws, at least two transparent draws, full PBR-style material inputs, sky/background data, render graph-compatible declarations, diagnostics, and debug dump preparation completes in under 1 second; repeated preparation and dumps are deterministic across at least 20 runs  
**Constraints**: Must not include Vulkan/Metal/DX/OpenGL headers or backend concepts in Renderer public contracts; must not require real GPU execution, presentation to a window/swapchain, shadow mapping, post-processing, deferred rendering, application scene ownership, runtime shader compilation, local shader file scanning/loading, or backend-specific resource handles  
**Scale/Scope**: Foundation scope covers frame planning, pass/resource declaration summaries, depth/opaque/sky/transparent pass ordering, configurable default-4 point light selection, directional light validation, full PBR-style material input validation, reusable draw descriptions, transparent sorting, no-light ambient fallback, deterministic diagnostics, and text dumps

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] **Spec-Driven Development**: The feature has an active specification with five recorded clarifications in `specs/015-forward-rendering-pipeline/spec.md`.
- [x] **Decoupled Architecture**: Renderer forward planning remains above RHI abstractions and does not depend on any backend graphics API or presentation surface.
- [x] **Design Pattern Discipline**: Frame orchestration, view validation, light selection, material binding, draw preparation, graph declaration, diagnostics, and dumping are separate responsibilities.
- [x] **Multi-API Support**: The feature emits Renderer-level frame and render graph-compatible declarations that can be consumed by Vulkan now and future Metal/DX/OpenGL backends later.
- [x] **Advanced Graphics Readiness**: Pass ordering, material input records, draw descriptions, and extension slots leave room for deferred rendering, shadow mapping, meshlets, ray tracing, and GI.
- [x] **Naming Conventions**: Planned public names follow UE5-style names such as `FForwardRenderer`, `FForwardFramePlan`, `FMeshDrawCommand`, and `FForwardLightSet`.
- [x] **Cross-Platform Compatibility**: Planned code is standard C++20, headless-testable, and avoids platform-specific assumptions.

## Project Structure

### Documentation (this feature)

```text
specs/015-forward-rendering-pipeline/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── forward-rendering-contract.md
└── tasks.md
```

### Source Code (repository root)

```text
Source/Renderer/
├── Public/Renderer/
│   ├── RendererMinimal.h
│   ├── FForwardRenderer.h
│   ├── FForwardFramePlan.h
│   ├── FForwardViewData.h
│   ├── FForwardLightData.h
│   ├── FMeshDrawCommand.h
│   ├── FForwardRenderGraphDeclaration.h
│   └── FForwardDiagnostics.h
├── Private/
│   ├── FForwardRenderer.cpp
│   ├── FForwardFramePlan.cpp
│   ├── FForwardViewData.cpp
│   ├── FForwardLightData.cpp
│   ├── FMeshDrawCommand.cpp
│   ├── FForwardRenderGraphDeclaration.cpp
│   └── FForwardDiagnostics.cpp
└── SConscript

Tests/
├── RendererForwardPipelineTests.h
├── RendererForwardPipelineTests.cpp
├── Main.cpp
└── SConscript
```

**Structure Decision**: Add forward rendering planning contracts to the existing Renderer layer because this feature composes render graph and material decisions above raw RHI objects. Tests live in the existing single test executable beside Renderer render graph and material/shader tests.

## Phase 0: Research

Completed in [research.md](./research.md). The main decisions are:

- Keep the first forward renderer as deterministic frame planning plus render graph-compatible declarations, not real GPU execution or presentation.
- Model forward pass order as depth, opaque, sky/background, and transparent declaration stages.
- Use full PBR-style material input validation while deferring shader compilation and file loading.
- Use configurable point light selection with default limit 4 and deterministic influence ordering.
- Sort transparent draws by camera-space depth descending, stable material id, then stable object id.
- Allow no-light scenes with a constant ambient-only fallback diagnostic.
- Keep draw commands reusable and deterministic, but not tied to backend command buffers.
- Keep diagnostics and debug dumps byte-stable for regression tests.

## Phase 1: Design & Contracts

Completed outputs:

- [data-model.md](./data-model.md): entities, fields, validation rules, and state transitions.
- [contracts/forward-rendering-contract.md](./contracts/forward-rendering-contract.md): public Renderer-layer behavioral contract.
- [quickstart.md](./quickstart.md): expected development and verification flow.

## Post-Design Constitution Check

- [x] **Spec-Driven Development**: Plan, research, data model, contract, and quickstart align with the active spec and clarifications.
- [x] **Decoupled Architecture**: Contracts expose Renderer-layer planning concepts and render graph-compatible declarations without backend or presentation details.
- [x] **Design Pattern Discipline**: Design artifacts keep frame orchestration, view/light validation, material/draw preparation, graph declaration, diagnostics, and inspection independently testable.
- [x] **Multi-API Support**: The contract is backend-agnostic and suitable for current Vulkan plus future Metal/DX/OpenGL RHI implementations.
- [x] **Advanced Graphics Readiness**: Full PBR-style input slots, pass declarations, draw descriptions, and extension slots leave room for later shadow, deferred, meshlet, ray tracing, and GI work.
- [x] **Naming Conventions**: Planned public names follow project naming conventions.
- [x] **Cross-Platform Compatibility**: No platform-specific dependency is introduced; validation remains headless-testable.

## Complexity Tracking

No constitution violations or complexity exceptions are required.
