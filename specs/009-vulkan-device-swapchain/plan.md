# Implementation Plan: Vulkan Device & Swapchain Backend

**Branch**: `009-vulkan-backend-device` | **Date**: 2026-06-30 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/009-vulkan-device-swapchain/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/plan-template.md` for the execution workflow.

## Summary

Implement the first real graphics backend slice for Vulkan: backend runtime initialization, deterministic physical adapter selection, logical device ownership, queue exposure, synchronization objects, Core platform-window-backed surface creation, and swapchain lifecycle through existing RHI contracts. The MVP path is headless device initialization and shutdown; presentation validation is required only when a valid Core platform window wrapper is available. The plan keeps command buffer recording, resources, descriptors, shaders, and pipelines out of scope while making queue submit behavior explicitly reject missing or non-executable command buffers until later phases.

## Technical Context

**Language/Version**: C++20 with traditional header/source separation  
**Primary Dependencies**: Existing Core and RHI public contracts; Vulkan SDK or platform Vulkan loader/headers where available; platform presentation bridge guarded by backend implementation boundaries; SCons 4.10.1  
**Storage**: N/A  
**Testing**: Existing `Tests/StonerTest` executable built by SCons; add Vulkan backend tests that can report unsupported when the runtime is unavailable and exercise presentation only when a valid Core platform window wrapper is available  
**Target Platform**: Windows, macOS, Linux desktop development environments where Vulkan or compatible presentation bridge is available; unsupported status is valid when the required runtime is absent  
**Project Type**: Layered C++ graphics engine backend library  
**Performance Goals**: Supported headless backend initialization, capability query, and shutdown complete in under 30 seconds; unsupported backend initialization reports explicit unsupported/failed status in under 10 seconds; acquire/present smoke flow remains deterministic when presentation is available  
**Constraints**: Maintain Renderer/Application-facing RHI abstraction; no buffer/texture allocation, shader compilation, descriptor updates, pipeline creation, real command recording, or resource uploads; validation support is optional diagnostic state; command queue submission rejects missing or non-executable command buffers explicitly; Core platform window wrapper is the only presentation input  
**Scale/Scope**: One Vulkan backend device/swapchain slice covering instance/runtime state, physical adapter candidates, logical device, queues, surface, swapchain, fence, semaphore, diagnostics, SCons SDK detection, integration tests, and clean shutdown/failure recovery

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] **Spec-Driven Development**: Spec exists at `specs/009-vulkan-device-swapchain/spec.md` and clarification decisions are recorded before planning.
- [x] **Decoupled Architecture**: Renderer/Application-facing behavior remains through RHI contracts; Vulkan details stay in Backend/Vulkan.
- [x] **Design Pattern Discipline**: Instance, adapter selection, device, queues, surface, swapchain, and synchronization have separate responsibilities.
- [x] **Multi-API Support**: The feature implements the Vulkan backend without changing RHI contracts in a Vulkan-specific direction.
- [x] **Advanced Graphics Readiness**: Capability reporting and queue/device ownership leave room for future resources, pipelines, render graph, ray tracing, meshlets, and GI.
- [x] **Naming Conventions**: Planned project-facing types follow UE5-style `F*`, `E*`, `I*`, and `T*` conventions.
- [x] **Cross-Platform Compatibility**: Planning includes Windows, macOS, and Linux runtime detection with explicit unsupported results when a platform lacks Vulkan/presentation support.

## Project Structure

### Documentation (this feature)

```text
specs/009-vulkan-device-swapchain/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── vulkan-device-swapchain-contract.md
├── checklists/
│   └── requirements.md
└── tasks.md
```

### Source Code (repository root)

```text
Source/Backend/Vulkan/
├── Public/VulkanRHI/
│   ├── VulkanDevice.h
│   ├── FVulkanInstance.h
│   ├── FVulkanPhysicalDevice.h
│   ├── FVulkanDevice.h
│   ├── FVulkanQueue.h
│   ├── FVulkanSurface.h
│   ├── FVulkanSwapchain.h
│   ├── FVulkanFence.h
│   ├── FVulkanSemaphore.h
│   └── FVulkanDiagnostics.h
├── Private/
│   ├── VulkanDevice.cpp
│   ├── FVulkanInstance.cpp
│   ├── FVulkanPhysicalDevice.cpp
│   ├── FVulkanDevice.cpp
│   ├── FVulkanQueue.cpp
│   ├── FVulkanSurface.cpp
│   ├── FVulkanSwapchain.cpp
│   ├── FVulkanFence.cpp
│   ├── FVulkanSemaphore.cpp
│   └── FVulkanDiagnostics.cpp
└── SConscript

Tests/
├── VulkanBackendTests.h
├── VulkanBackendTests.cpp
├── Main.cpp
└── SConscript
```

**Structure Decision**: Keep all Vulkan-specific ownership and runtime types inside `Source/Backend/Vulkan/`. Expose only backend-facing creation helpers and RHI implementations through `VulkanRHI/`; downstream Renderer/Application code continues to see RHI contracts. Tests live in the existing `Tests/StonerTest` executable and may skip presentation checks when no valid Core platform window wrapper is available.

## Complexity Tracking

No constitution violations identified.

## Phase 0: Research

Research completed in [research.md](./research.md). Planning decisions resolve:

- Optional validation diagnostics rather than validation-required initialization failure.
- Headless device MVP with presentation validation gated by Core platform window availability.
- Deterministic adapter selection through capability gates and scoring.
- Queue wait-idle support with explicit submission rejection until command recording exists.
- Surface creation from the existing Core platform window wrapper.
- SCons Vulkan SDK detection and explicit unsupported behavior when the SDK/runtime is absent.

## Phase 1: Design & Contracts

Design artifacts:

- [data-model.md](./data-model.md)
- [contracts/vulkan-device-swapchain-contract.md](./contracts/vulkan-device-swapchain-contract.md)
- [quickstart.md](./quickstart.md)

## Post-Design Constitution Check

- [x] **Spec-Driven Development**: Plan, research, data model, contract, and quickstart are generated before implementation.
- [x] **Decoupled Architecture**: Public interaction remains through RHI and Core platform abstractions; Vulkan runtime details remain in Backend/Vulkan.
- [x] **Design Pattern Discipline**: Data model splits runtime instance, adapter candidate, device, queues, surface, swapchain, sync, and diagnostics.
- [x] **Multi-API Support**: No RHI contract changes require other backends to mimic Vulkan-only details.
- [x] **Advanced Graphics Readiness**: Device capabilities and queue model leave space for later resource, command, pipeline, render graph, RT, meshlet, and GI phases.
- [x] **Naming Conventions**: Planned source artifacts follow project naming conventions.
- [x] **Cross-Platform Compatibility**: Quickstart and tests define supported, unsupported, headless, and presentation-available paths across desktop platforms.
