# Implementation Plan: Vulkan Resource Management

**Branch**: `010-vulkan-resource-management` | **Date**: 2026-06-30 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/010-vulkan-resource-management/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/plan-template.md` for the execution workflow.

## Summary

Implement Vulkan backend resource management through the existing RHI resource and descriptor contracts: backend-backed buffers, textures, samplers, allocation ownership, fixed-capacity descriptor pools and descriptor sets, deterministic real-or-fallback allocation diagnostics, test-controlled allocation failure, and CPU-visible upload staging records. Command recording, queue execution of uploads, shader compilation, pipelines, render passes, framebuffers, and render graph scheduling remain out of scope.

## Technical Context

**Language/Version**: C++20 with traditional header/source separation  
**Primary Dependencies**: Existing Core types and RHI resource/descriptor contracts; existing Vulkan backend device, diagnostics, lifecycle, and SCons SDK detection; Vulkan SDK/runtime where available with deterministic fallback allocation when unavailable  
**Storage**: N/A; resource allocation records and upload staging data are process-local runtime state only  
**Testing**: Existing `Tests/StonerTest` executable built by SCons; extend Vulkan backend tests for resource creation, descriptor binding, allocation limits, upload staging, lifecycle invalidation, and fallback diagnostics  
**Target Platform**: Windows, macOS, and Linux desktop development environments; unsupported runtime remains explicit while deterministic fallback allocation allows contract validation  
**Project Type**: Layered C++ graphics engine backend library  
**Performance Goals**: Valid buffer, texture, sampler creation plus descriptor and upload staging smoke flow completes in under 30 seconds; invalid descriptions and configured allocation failures return explicit results deterministically; no stale usable resources after shutdown in lifecycle tests  
**Constraints**: Preserve Renderer/Application-facing RHI abstraction; no command buffer recording, queue execution of uploads, shader compilation, real pipeline creation, render passes, framebuffers, or render graph scheduling; descriptor pools have configurable fixed capacity; descriptor sets retain binding records when resources later invalidate; upload staging records CPU-visible data and destination ranges only  
**Scale/Scope**: One Vulkan backend resource slice covering buffers, textures, samplers, resource allocation records, fixed-capacity descriptor pools, descriptor sets, bound resource records, upload requests, diagnostics, tests, and clean shutdown/failure recovery

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] **Spec-Driven Development**: Spec exists at `specs/010-vulkan-resource-management/spec.md` and clarifications are recorded before planning.
- [x] **Decoupled Architecture**: Renderer/Application behavior remains through RHI contracts; Vulkan allocation and descriptor details remain in Backend/Vulkan.
- [x] **Design Pattern Discipline**: Resource objects, allocator, descriptor pool, descriptor set, upload staging, and diagnostics are planned as separate responsibilities.
- [x] **Multi-API Support**: The plan implements the Vulkan backend without making RHI resource contracts Vulkan-specific.
- [x] **Advanced Graphics Readiness**: Resource descriptions, descriptor behavior, retained invalidated bindings, and upload staging leave space for render graph, bindless-like models, RT, meshlets, and GI.
- [x] **Naming Conventions**: Planned project-facing types follow UE5-style `F*`, `E*`, `I*`, and `T*` naming conventions.
- [x] **Cross-Platform Compatibility**: Real runtime support is optional for validation; unsupported and fallback modes keep the feature buildable and testable across desktop platforms.

## Project Structure

### Documentation (this feature)

```text
specs/010-vulkan-resource-management/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── vulkan-resource-management-contract.md
├── checklists/
│   └── requirements.md
└── tasks.md
```

### Source Code (repository root)

```text
Source/Backend/Vulkan/
├── Public/VulkanRHI/
│   ├── VulkanDevice.h
│   ├── FVulkanDevice.h
│   ├── FVulkanDiagnostics.h
│   ├── FVulkanBuffer.h
│   ├── FVulkanTexture.h
│   ├── FVulkanSampler.h
│   ├── FVulkanResourceAllocation.h
│   ├── FVulkanMemoryAllocator.h
│   ├── FVulkanDescriptorPool.h
│   ├── FVulkanDescriptorSet.h
│   └── FVulkanUploadStaging.h
├── Private/
│   ├── FVulkanDevice.cpp
│   ├── FVulkanDiagnostics.cpp
│   ├── FVulkanBuffer.cpp
│   ├── FVulkanTexture.cpp
│   ├── FVulkanSampler.cpp
│   ├── FVulkanResourceAllocation.cpp
│   ├── FVulkanMemoryAllocator.cpp
│   ├── FVulkanDescriptorPool.cpp
│   ├── FVulkanDescriptorSet.cpp
│   └── FVulkanUploadStaging.cpp
└── SConscript

Tests/
├── VulkanBackendTests.h
├── VulkanBackendTests.cpp
├── Main.cpp
└── SConscript
```

**Structure Decision**: Keep all Vulkan resource ownership, allocation mode, descriptor pool capacity, fallback behavior, and upload staging detail inside `Source/Backend/Vulkan/`. Public RHI headers remain unchanged unless implementation discovers a true contract gap; tests exercise behavior through existing RHI resource/descriptor interfaces plus backend diagnostic queries.

## Complexity Tracking

No constitution violations identified.

## Phase 0: Research

Research completed in [research.md](./research.md). Planning decisions resolve:

- Real runtime resource allocation when available, deterministic fallback allocation with diagnostics when unavailable.
- Lightweight internal allocation ownership model with test-controlled budget/allocation-count limits.
- Fixed-capacity descriptor pools with explicit exhaustion results.
- Descriptor sets retain binding records when referenced resources invalidate.
- CPU-visible upload staging records data and destination ranges without command execution.
- Resource factories replace unsupported placeholders from the previous Vulkan device phase while keeping shader, pipeline, render pass, framebuffer, and command behavior out of scope.

## Phase 1: Design & Contracts

Design artifacts:

- [data-model.md](./data-model.md)
- [contracts/vulkan-resource-management-contract.md](./contracts/vulkan-resource-management-contract.md)
- [quickstart.md](./quickstart.md)

## Post-Design Constitution Check

- [x] **Spec-Driven Development**: Plan, research, data model, contract, and quickstart are generated before implementation.
- [x] **Decoupled Architecture**: Public interaction stays on RHI resource/device contracts; Vulkan allocation and descriptor internals remain in Backend/Vulkan.
- [x] **Design Pattern Discipline**: Design separates resource wrappers, allocation records, allocator policy, descriptor pool, descriptor set, upload staging, and diagnostics.
- [x] **Multi-API Support**: No RHI contract change forces other backends to adopt Vulkan-specific memory or descriptor details.
- [x] **Advanced Graphics Readiness**: Descriptor binding records, lifecycle invalidation, staging records, and allocation diagnostics support later render graph/material/resource evolution.
- [x] **Naming Conventions**: Planned source artifacts follow project naming conventions.
- [x] **Cross-Platform Compatibility**: Quickstart and tests define supported runtime, unsupported runtime, and deterministic fallback allocation paths across desktop platforms.

## CR-001 Design Amendment (2026-07-26)

The reviewed allocation boundary uses a no-allocation, move-only ownership
ticket instead of a copyable record. The allocator binds each ticket to its
object identity and reset epoch, performs checked counter and texture-footprint
arithmetic, and mutates counters only after all gates pass. `FVulkanDevice` is
the sole constructor of buffer and texture wrappers and rolls ownership back if
wrapper creation or device tracking fails. Fallback host buffers keep sparse CPU
mirrors and translate mirror growth failures to explicit RHI results.
