# Implementation Plan: Deferred Rendering Pipeline

**Branch**: `019-deferred-rendering-pipeline` | **Date**: 2026-07-23 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `specs/019-deferred-rendering-pipeline/spec.md`

## Summary

Deliver deferred rendering as a sibling strategy to the existing forward renderer. The feature prepares a deterministic multi-pass frame plan, declares surface-data, directional-light, point-volume, spot-volume, composition, and optional forward-transparent work through the render graph, and executes it through backend-neutral RHI bindings. The initial surface layout uses three color targets plus depth to retain base color, normalized world-space normal, metallic, roughness, emissive, and ambient occlusion; lighting reconstructs world-space position with the inverse view-projection. Windows, macOS, and Linux run deterministic coverage; Linux Lavapipe additionally executes the real Vulkan path offscreen and validates intermediate/final readback probes using the clarified semantic tolerances. A four-tier forward/deferred comparison produces a reproducible baseline without imposing a speedup gate.

## Technical Context

**Language/Version**: C++20 with traditional header/source separation; no C++20 Modules
**Primary Dependencies**: Existing Core math/types/logging; Renderer material, forward, render-graph, and scene-identity contracts; RHI textures, buffers, descriptors, pipelines, render passes, command buffers, queues, fences, runtime-mode contracts, and explicit two-/three-component float vertex formats; existing Vulkan native offscreen context; SCons 4.10.1; Vulkan 1.3-compatible headers/loader; Mesa Lavapipe for Linux native CI; offline GLSL-to-SPIR-V compiler and validator when available
**Storage**: Repository-owned deferred GLSL sources and checked-in SPIR-V payloads; process-local frame plans, surface layouts, graph declarations, RHI/native resources, readback probes, diagnostics, and comparison reports; CI report artifacts; no database, scene serialization, runtime shader cache, or asset catalog
**Testing**: Existing `StonerTest` executable plus deferred planner/graph/executor tests, deterministic mock-RHI command tests, Vulkan native offscreen readback tests, SCons build validation, and GitHub Actions Windows/macOS/Linux matrix with Linux Lavapipe native execution
**Target Platform**: Deterministic build/test behavior on Windows, macOS, and Linux; required native Vulkan offscreen execution and pixel readback on Linux Lavapipe; Windows/macOS native execution optional and no visible-window requirement; Android excluded
**Project Type**: Cross-platform C++ graphics-engine libraries with reusable Renderer, RHI, and Vulkan backend layers
**Performance Goals**: Prepare and compare equivalent workloads at 0, 16, 64, and 256 local-light tiers; collect at least 100 measured frames after warm-up per tier; report median/p95 timings and observed crossover; maintain one surface-data geometry sequence independent of accepted light count; no deferred-faster-than-forward pass gate
**Constraints**: Forward remains the default unchanged strategy; Renderer cannot call Vulkan; all passes flow through the render graph; no fixed local-light cap; surface targets share extent/sample count; single-sample rendering only; standard-Z uses far clear `1.0`/`LessEqual` and reversed-Z uses far clear `0.0`/`GreaterEqual`; native validation uses at least 18 semantic probes per depth convention, including the six required point/spot local-light edge probes, and the specified per-semantic tolerances; transparent blending remains forward-transparent; shutdown leaves zero deferred frame-owned resources
**Scale/Scope**: One active view and output; representative validation workload of at least 100 opaque draws, 1 directional light, 64 point lights, and 16 spot lights; three color surface targets plus depth; directional fullscreen draws; reusable sphere/cone local-light volumes; tiled/clustered lighting, shadows, SSAO, SSR, temporal effects, anti-aliasing, decals, editor tooling, new backends, and visible demo integration excluded

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] **Spec-Driven Development**: `spec.md` defines 25 testable functional requirements, 10 measurable outcomes, and five recorded clarification decisions before implementation planning.
- [x] **Decoupled Architecture**: `FDeferredRenderer` and `FDeferredFrameExecutor` consume Renderer/RHI contracts only. Native image, descriptor, pipeline, command, and readback ownership stays inside the Vulkan backend.
- [x] **Design Pattern Discipline**: Forward and deferred are sibling Strategies. Deferred validation, surface planning, light planning, graph declaration, execution, diagnostics, and comparison reporting remain separate responsibilities; frame resources form a lifecycle Composite owned by the execution session.
- [x] **Multi-API Support**: New descriptor binding, index binding, clear-value, and texture-to-buffer readback operations are backend-neutral RHI capabilities reusable by later Metal/DX12 implementations.
- [x] **Advanced Graphics Readiness**: Explicit surface semantics, render-graph resources, bounded light records, and separable composition leave extension points for clustered lighting, SSAO/SSR, ray tracing, meshlets, and GI.
- [x] **Naming Conventions**: Planned public types use UE5-style PascalCase names with `F`/`E` prefixes and remain in existing module namespaces.
- [x] **Cross-Platform Compatibility**: Shared source and deterministic tests compile/run on Windows, macOS, and Linux; native availability is capability-gated and platform code remains backend-private.
- [x] **Automated Cross-Platform Validation**: The existing GitHub Actions matrix remains required on all three platforms, with an added Linux Lavapipe native deferred readback gate and uploaded normalized reports.

**Pre-Design Gate Result**: PASS. No constitution violation or exception is required.

## Project Structure

### Documentation (this feature)

```text
specs/019-deferred-rendering-pipeline/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── deferred-renderer-contract.md
│   ├── deferred-execution-contract.md
│   └── deferred-validation-contract.md
├── checklists/
│   └── requirements.md
└── tasks.md
```

### Source Code (repository root)

```text
Source/Renderer/
├── SConscript
├── Public/Renderer/
│   ├── FDeferredDiagnostics.h
│   ├── FDeferredSurfaceData.h
│   ├── FDeferredLightData.h
│   ├── FDeferredLightVolume.h
│   ├── FDeferredFramePlan.h
│   ├── FDeferredRenderer.h
│   ├── FDeferredRenderGraphDeclaration.h
│   ├── FDeferredFrameExecutor.h
│   └── FRendererComparisonReport.h
├── Private/
│   └── matching implementations
└── Shaders/Deferred/
    ├── Surface.vert
    ├── Surface.frag
    ├── Fullscreen.vert
    ├── DirectionalLight.frag
    ├── PointLight.vert
    ├── PointLight.frag
    ├── SpotLight.vert
    ├── SpotLight.frag
    ├── Composition.frag
    └── checked-in matching .spv payloads

Source/RHI/Public/RHI/
├── ERHIFormat.h
├── ERHIIndexType.h
├── FRHITextureBufferCopyRegion.h
├── IRHICommandBuffer.h
└── RHIMinimal.h

Source/Backend/Vulkan/
├── Public/VulkanRHI/
│   ├── FVulkanCommandBuffer.h
│   └── FVulkanNativeContext.h
└── Private/
    ├── FVulkanCommandBuffer.cpp
    ├── FVulkanNativeContext.cpp
    ├── FVulkanNativeOffscreenSession.h
    └── FVulkanNativeOffscreenSession.cpp

Tests/
├── SConscript
├── RHICoreTests.cpp
├── VulkanBackendTests.cpp
├── RendererForwardPipelineTests.cpp
├── DeferredRenderingTests.h
├── DeferredRenderingTests.cpp
├── DeferredNativeIntegrationTests.h
├── DeferredNativeIntegrationTests.cpp
├── RendererComparisonTests.h
├── RendererComparisonTests.cpp
└── Main.cpp

.github/scripts/
├── run_deferred_validation.py
└── test_run_deferred_validation.py

.github/workflows/
└── ci.yml

Validation/019/
├── README.md
├── completion.md
└── Linux/
    ├── deferred-readback-report.txt
    └── renderer-comparison-report.txt
```

**Structure Decision**: Keep all reusable deferred planning, diagnostics, graph declaration, comparison, and RHI execution translation in `Renderer`; add only generally reusable commands to `RHI`; keep native Vulkan objects and readback mapping in `Backend/Vulkan`; keep fixed reference scenes and assertion logic in `Tests` and CI scripts. The Renderer shader assets are source-controlled beside their owning module. `FDeferredRenderer` is the Strategy entry point, while surface/light/composition builders and `FDeferredFrameExecutor` remain independently testable collaborators. The native offscreen session is a lifecycle Composite over images, buffers, descriptors, pipelines, pass/framebuffer objects, command/fence state, and staging readback, with reverse-order cleanup.

## Phase 0: Research

Completed in [research.md](./research.md). Key decisions:

- Keep forward and deferred as sibling Renderer strategies; forward remains the default.
- Use three color surface targets plus `D32_Float` depth, storing direct normalized world-space normals and every clarified material semantic.
- Reconstruct world-space position from depth and the inverse view-projection rather than storing position.
- Derive far clear, depth comparison, projection, reconstruction, and probe decode from one standard-Z or reversed-Z convention identity.
- Use fullscreen directional lighting and indexed reusable sphere/cone volumes for point/spot lights, including explicit camera-inside variants, radian cone fields, and type-then-entity ordering without an influence key.
- Add backend-neutral descriptor-set binding, index-buffer binding, explicit render-pass clear values, and texture-to-buffer readback commands.
- Fix one canonical deferred shader interface before shader implementation: explicit set/binding tables, exact 304-byte frame/view, 176-byte draw/material, and 64-byte light-record layouts, `R32G32_Float`/`R32G32B32_Float` vertex formats, and surface/fullscreen/volume vertex layouts.
- Execute compiled graph passes through `FDeferredFrameExecutor`, with all bindings validated before recording.
- Generalize the existing native Vulkan offscreen context enough to expose real RHI wrappers without creating a second backend or adding Renderer dependencies to Vulkan.
- Keep deferred GLSL and checked-in SPIR-V under the Renderer module.
- Validate at least 18 named intermediate/final sample probes per depth convention, including the six required point/spot local-light edge probes, using semantic tolerances.
- Produce a four-tier, fingerprinted forward/deferred timing baseline with no speedup completion gate.
- Preserve three-platform deterministic CI and require native Vulkan readback only on Linux Lavapipe.

## Phase 1: Design & Contracts

Completed outputs:

- [data-model.md](./data-model.md): frame configuration, surface layout, view/draw/light records, frame plan, graph declaration, execution bindings/results, probe validation, comparison reports, diagnostics, and lifecycle transitions.
- [contracts/deferred-renderer-contract.md](./contracts/deferred-renderer-contract.md): public strategy inputs, material compatibility, pass order, local-light policy, transparent handoff, and deterministic reporting.
- [contracts/deferred-execution-contract.md](./contracts/deferred-execution-contract.md): RHI command extensions, concrete surface layout, binding groups, graph execution order, native readback, failure ownership, and cleanup.
- [contracts/deferred-validation-contract.md](./contracts/deferred-validation-contract.md): cross-platform matrix, semantic probe thresholds, comparison tiers, normalized artifacts, and completion gates.
- [quickstart.md](./quickstart.md): local deterministic build/tests, Linux Lavapipe native readback, report inspection, shader verification, and CI commands.

## Post-Design Constitution Check

- [x] **Spec-Driven Development**: Every design entity and contract maps to FR-001 through FR-025 and SC-001 through SC-010; all five clarification decisions are represented.
- [x] **Decoupled Architecture**: Renderer contracts contain no Vulkan handles, ICD names, descriptor APIs, or native layouts. Backend tests supply RHI wrappers to the Renderer executor.
- [x] **Design Pattern Discipline**: Strategy selection and lifecycle Composite ownership are explicit; surface, light, graph, execution, diagnostics, validation, and comparison responsibilities remain separate.
- [x] **Multi-API Support**: New RHI commands describe intent and stable validation only. Vulkan is the first native implementation, while deterministic mocks prove backend independence.
- [x] **Advanced Graphics Readiness**: Semantic surface data and graph resources can be consumed later by clustered lighting, screen-space passes, ray tracing, and GI without changing scene ownership.
- [x] **Naming Conventions**: Proposed names, enums, records, and paths follow established project conventions.
- [x] **Cross-Platform Compatibility**: No platform branch changes frame semantics; unsupported native execution is distinct from deterministic success and is not required on Windows/macOS.
- [x] **Automated Cross-Platform Validation**: The design specifies all three deterministic jobs, Linux Lavapipe native execution, pass/fail thresholds, and uploaded artifacts with no temporary coverage gap.

**Post-Design Gate Result**: PASS. Phase 1 introduces no constitution exception.

## Complexity Tracking

No constitution violations require justification. The RHI command additions are the minimum general capabilities needed to bind deferred inputs, draw indexed light volumes, and validate native pixels. Native Vulkan work extends the existing offscreen session instead of creating a parallel backend or exposing backend types upward.
