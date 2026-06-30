# Implementation Plan: Vulkan Pipeline & Shader

**Branch**: `012-vulkan-pipeline-shader` | **Date**: 2026-06-30 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/012-vulkan-pipeline-shader/spec.md`

## Summary

Implement the Vulkan backend shader and pipeline slice needed to turn command recording's placeholder draw/dispatch diagnostics into validated bound-pipeline behavior. The plan extends existing RHI shader/pipeline descriptions with explicit shader interface metadata, lightweight structural bytecode validation, triangle-ready graphics state, runtime/fallback diagnostics, and process-local pipeline reuse, then adds Vulkan shader module, graphics pipeline, and compute pipeline objects wired into device factories and command buffer binding validation.

## Technical Context

**Language/Version**: C++20 with traditional header/source separation; no C++20 Modules  
**Primary Dependencies**: Existing Core types/containers, RHI resource/descriptor/pipeline contracts, Vulkan backend device/resource/command/diagnostics layers, Vulkan SDK or platform Vulkan loader/headers where available, SCons 4.10.1  
**Storage**: Process-local backend state only; no persistent pipeline disk cache, database, or asset catalog  
**Testing**: Project test executable through SCons build flow; focused RHI core mock tests and Vulkan backend integration tests  
**Target Platform**: Desktop development platforms supported by the project: Windows, macOS, Linux, with unsupported runtime paths reported explicitly  
**Project Type**: Cross-platform C++ graphics engine backend/library feature  
**Performance Goals**: The project verification flow can create valid real-runtime or deterministic fallback shader modules, one graphics pipeline, one compute pipeline, bind them, and record draw/dispatch validation in under 60 seconds  
**Constraints**: Preserve RHI abstraction, support deterministic fallback when runtime is unavailable, keep source shader compilation/reflection/material/render graph/visible triangle application out of scope, keep pipeline reuse process-local only  
**Scale/Scope**: One backend feature slice covering shader modules, explicit shader interface metadata, pipeline layouts, triangle-ready graphics pipelines, compute pipelines, command binding validation, diagnostics, lifecycle invalidation, process-local reuse, and regression tests

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [X] **Spec-Driven Development**: 012 spec and clarification log exist before implementation planning.
- [X] **Decoupled Architecture**: Renderer/Application behavior remains behind RHI shader, layout, pipeline, command, resource, and synchronization contracts; Vulkan details stay inside Backend/Vulkan.
- [X] **Design Pattern Discipline**: Shader module validation, layout compatibility, pipeline creation, command binding, cache/reuse, and diagnostics remain separate responsibilities.
- [X] **Multi-API Support**: RHI description changes stay backend-neutral and leave DX12/Metal/GL backends able to implement the same contracts later.
- [X] **Advanced Graphics Readiness**: Explicit interface metadata, pipeline compatibility summaries, and binding state leave room for material permutations, meshlet, ray tracing, and GI pipelines without implementing them now.
- [X] **Naming Conventions**: New C++ concepts use UE5-style `F` structs/classes, `E` enums, and `I` interfaces.
- [X] **Cross-Platform Compatibility**: Runtime availability is explicit; fallback validation avoids hard dependency on a single platform's Vulkan runtime.

## Project Structure

### Documentation (this feature)

```text
specs/012-vulkan-pipeline-shader/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── vulkan-pipeline-shader-contract.md
└── tasks.md
```

### Source Code (repository root)

```text
Source/RHI/Public/RHI/
├── FRHIShaderModuleDesc.h
├── FRHIGraphicsPipelineDesc.h
├── FRHIComputePipelineDesc.h
├── FRHIPipelineLayoutDesc.h
├── IRHIShaderModule.h
├── IRHIGraphicsPipeline.h
├── IRHIComputePipeline.h
├── IRHICommandBuffer.h
└── RHIMinimal.h

Source/Backend/Vulkan/Public/VulkanRHI/
├── FVulkanShaderModule.h
├── FVulkanGraphicsPipeline.h
├── FVulkanComputePipeline.h
├── FVulkanPipelineCache.h
├── FVulkanDevice.h
├── FVulkanCommandBuffer.h
├── FVulkanDiagnostics.h
└── VulkanDevice.h

Source/Backend/Vulkan/Private/
├── FVulkanShaderModule.cpp
├── FVulkanGraphicsPipeline.cpp
├── FVulkanComputePipeline.cpp
├── FVulkanPipelineCache.cpp
├── FVulkanDevice.cpp
├── FVulkanCommandBuffer.cpp
└── FVulkanDiagnostics.cpp

Tests/
├── RHICoreTests.cpp
└── VulkanBackendTests.cpp
```

**Structure Decision**: Extend existing RHI headers in place and add Vulkan backend classes beside current resource/command classes. Tests remain in the existing monolithic C++ test executable to preserve the project's current verification shape.

## Complexity Tracking

No constitution violations are required. The feature stays within the existing RHI/Backend layering and expands only the contracts needed for the roadmap's Vulkan Pipeline & Shader phase.

## Phase 0: Research Summary

See [research.md](./research.md).

Resolved decisions:

- Explicit shader interface metadata is supplied at shader module creation and validated against pipeline layouts.
- Real runtime objects are preferred when available; deterministic fallback objects are allowed with explicit diagnostics.
- Graphics pipeline scope is triangle-ready with fixed-function validation for vertex input, topology, rasterization, depth/stencil, blend, multisample, and dynamic viewport/scissor requirements.
- Fallback shader bytecode validation is lightweight and structural; complete semantic validation is delegated to real runtime creation.
- Pipeline reuse is process-local only; persistent disk cache behavior is deferred.

## Phase 1: Design Summary

See [data-model.md](./data-model.md) and [contracts/vulkan-pipeline-shader-contract.md](./contracts/vulkan-pipeline-shader-contract.md).

Design outputs:

- RHI data contracts for shader interface metadata, shader bytecode validation mode, graphics dynamic state requirements, runtime mode, and pipeline reuse diagnostics.
- Vulkan backend objects for shader modules, graphics pipelines, compute pipelines, and process-local pipeline cache records.
- Command buffer binding state so draw/indexed draw/dispatch validation distinguishes missing, compatible, incompatible, wrong-kind, and invalidated pipelines.
- Device shutdown invalidation across shader modules, pipeline layouts, graphics pipelines, compute pipelines, and pipeline cache records.

## Post-Design Constitution Check

- [X] **Spec-Driven Development**: plan, research, data model, contract, and quickstart are generated before implementation tasks.
- [X] **Decoupled Architecture**: public RHI changes remain backend-neutral; Vulkan object details are not exposed to Renderer/Application.
- [X] **Design Pattern Discipline**: no new god-class is introduced; cache, validation, lifecycle, diagnostics, and command binding are separable.
- [X] **Multi-API Support**: pipeline descriptions and interface metadata can be implemented by later Metal/DX12/GL backends.
- [X] **Advanced Graphics Readiness**: out-of-scope advanced pipeline types are explicitly deferred but not blocked by the data model.
- [X] **Naming Conventions**: planned names follow existing project conventions.
- [X] **Cross-Platform Compatibility**: fallback diagnostics and SDK-guarded behavior preserve build/run behavior across supported desktop platforms.
