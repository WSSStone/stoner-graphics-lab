# Implementation Plan: RHI Resource & Pipeline Interfaces

**Branch**: `008-rhi-resource-pipeline` | **Date**: 2026-06-26 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/008-rhi-resource-pipeline/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/plan-template.md` for the execution workflow.

## Summary

Define the second RHI interface slice: resource descriptions and contracts for buffers, textures, samplers, shader modules, multi-set pipeline layouts, descriptor sets, graphics/compute pipelines, single-subpass render passes, and framebuffers. The implementation approach remains contract-first and mock-test-driven: production code adds focused public RHI headers and aggregate includes, while deterministic mock implementations in tests validate creation descriptors, lifecycle invalidation, usage compatibility, binding compatibility, pipeline validation, and framebuffer/render-pass compatibility without requiring a real graphics backend.

## Technical Context

**Language/Version**: C++20 with traditional header/source separation  
**Primary Dependencies**: Existing Core layer types and containers; existing RHI core contracts (`ERHIResult`, `ERHIFormat`, `ERHIQueueType`, `IRHIDevice`, lifecycle/result conventions); SCons 4.10.1; C++ standard library for non-graphics utilities  
**Storage**: N/A  
**Testing**: Existing `Tests/StonerTest` executable built by SCons; extend RHI mock tests with resource/pipeline lifecycle and negative-path validation  
**Target Platform**: Windows, macOS, Linux  
**Project Type**: Layered C++ graphics engine library  
**Performance Goals**: Resource and pipeline contract mock tests complete within the existing local test feedback loop; descriptor/pipeline validation remains deterministic and allocation-light for unit-test scale usage  
**Constraints**: No concrete graphics API calls; no Renderer/Application/Backend dependencies in public RHI resource headers; no real GPU, shader compiler, native window, swapchain image, platform surface, or backend object required for tests; shader modules carry opaque payload identity only; resource usage flags are composable but invalid incompatible combinations are rejected; render pass contracts are single-subpass only in this phase  
**Scale/Scope**: One RHI resource/pipeline interface slice covering buffers, textures, samplers, shader modules, pipeline layouts, descriptor sets, graphics pipelines, compute pipelines, render passes, framebuffers, lifecycle invalidation, aggregate includes, and mock-based contract tests

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] **Spec-Driven Development**: Specification exists at `specs/008-rhi-resource-pipeline/spec.md` with clarification decisions recorded before planning.
- [x] **Decoupled Architecture**: Feature extends the RHI layer and explicitly avoids Application, Renderer, Backend, platform-window, or graphics API dependencies in public contracts.
- [x] **Design Pattern Discipline**: Responsibilities are split across resource, sampler, shader, layout, descriptor, pipeline, render pass, and framebuffer contracts; no catch-all rendering object is planned.
- [x] **Multi-API Support**: Contracts describe portable behavior suitable for Vulkan, DX12, Metal, OpenGL/GLES/WebGL, and mock implementations.
- [x] **Advanced Graphics Readiness**: Usage flags, descriptor sets, shader stages, and pipeline layout concepts leave room for future ray tracing, meshlet, compute-heavy, and GI phases without implementing those pipelines now.
- [x] **Naming Conventions**: Planned public names follow UE5-style prefixes: `I*` interfaces, `F*` structs/value objects, `E*` enums, and flag-style enum names.
- [x] **Cross-Platform Compatibility**: Planning requires SCons build and `StonerTest` verification through the existing cross-platform build path; no platform-specific public dependency is introduced.

## Project Structure

### Documentation (this feature)

```text
specs/008-rhi-resource-pipeline/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── rhi-resource-pipeline-api.md
├── checklists/
│   └── requirements.md
└── tasks.md
```

### Source Code (repository root)

```text
Source/RHI/
├── Public/RHI/
│   ├── RHIMinimal.h
│   ├── ERHIResourceUsage.h
│   ├── ERHITextureDimension.h
│   ├── ERHISamplerMode.h
│   ├── ERHIShaderStage.h
│   ├── ERHIDescriptorType.h
│   ├── ERHIPipelineState.h
│   ├── FRHIBufferDesc.h
│   ├── FRHITextureDesc.h
│   ├── FRHISamplerDesc.h
│   ├── FRHIShaderModuleDesc.h
│   ├── FRHIDescriptorBinding.h
│   ├── FRHIPipelineLayoutDesc.h
│   ├── FRHIGraphicsPipelineDesc.h
│   ├── FRHIComputePipelineDesc.h
│   ├── FRHIRenderPassDesc.h
│   ├── FRHIFramebufferDesc.h
│   ├── IRHIBuffer.h
│   ├── IRHITexture.h
│   ├── IRHISampler.h
│   ├── IRHIShaderModule.h
│   ├── IRHIPipelineLayout.h
│   ├── IRHIDescriptorSet.h
│   ├── IRHIGraphicsPipeline.h
│   ├── IRHIComputePipeline.h
│   ├── IRHIRenderPass.h
│   └── IRHIFramebuffer.h
├── Private/
│   └── RHIModule.cpp
└── SConscript

Tests/
├── RHICoreTests.h
├── RHICoreTests.cpp
├── Main.cpp
└── SConscript
```

**Structure Decision**: Add focused public RHI resource/pipeline headers under `Source/RHI/Public/RHI/`, update `RHIMinimal.h` to aggregate them, and keep deterministic mock implementations inside `Tests/RHICoreTests.cpp`. Production RHI resource objects remain abstract interfaces in this phase; fake backend code does not enter shipping layers.

## Complexity Tracking

No constitution violations identified.

## Phase 0: Research

Research completed in [research.md](./research.md). Planning decisions resolve the clarified areas:

- Multi-set pipeline layout model with set index + binding slot addressing.
- Explicit `Valid` / `Invalidated` lifecycle state for resource and pipeline-family objects.
- Opaque shader payload identity without bytecode validation.
- Composable usage flags with explicit invalid incompatible combinations.
- Single-subpass render pass model with attachment roles and load/store behavior.
- Mock-only validation strategy that preserves RHI/backend separation.

## Phase 1: Design & Contracts

Design artifacts:

- [data-model.md](./data-model.md)
- [contracts/rhi-resource-pipeline-api.md](./contracts/rhi-resource-pipeline-api.md)
- [quickstart.md](./quickstart.md)

## Post-Design Constitution Check

- [x] **Spec-Driven Development**: Plan, research, data model, contract, and quickstart are generated before implementation.
- [x] **Decoupled Architecture**: Public contracts remain in RHI and depend only on Core/RHI core; tests use mocks rather than backend APIs.
- [x] **Design Pattern Discipline**: Interface boundaries keep resources, bindings, pipelines, render passes, and framebuffers separate.
- [x] **Multi-API Support**: No contract assumes Vulkan/DX/Metal/OpenGL details beyond portable RHI concepts.
- [x] **Advanced Graphics Readiness**: Descriptor sets, usage flags, shader stages, and pipeline layout leave future room for ray tracing, meshlets, compute-heavy workflows, and GI.
- [x] **Naming Conventions**: Contract names preserve UE5-style prefixes.
- [x] **Cross-Platform Compatibility**: Quickstart requires SCons build and `StonerTest` verification through the existing cross-platform build path.
