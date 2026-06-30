# Implementation Plan: Vulkan Command Recording & Submission

**Branch**: `011-vulkan-commands-submission` | **Date**: 2026-06-30 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `specs/011-vulkan-commands-submission/spec.md`

## Summary

Implement the Vulkan backend command recording and submission slice through the existing RHI command, queue, synchronization, render pass, framebuffer, resource, and upload-staging contracts. The plan adds backend command pools and command buffers, command recording diagnostics, compatible queue submission, deterministic fallback submission state, minimal single-subpass backend render pass/framebuffer validation, declarative barrier/layout intent validation, and upload scheduling from existing pending upload records. Shader compilation, pipeline creation, full pipeline binding validation, full resource state tracking, render graph scheduling, multi-threaded command recording, and visible frame rendering remain out of scope.

## Technical Context

**Language/Version**: C++20 with traditional header/source separation; no C++20 Modules  
**Primary Dependencies**: Existing Core types and containers; existing RHI command/queue/sync/resource/render pass/framebuffer contracts; existing Vulkan backend device, queue, sync, resource, diagnostics, and SCons SDK detection; Vulkan SDK or platform Vulkan loader/headers where available  
**Storage**: Process-local backend state only; command records, submission records, diagnostics, render pass/framebuffer objects, and upload scheduling records live in memory  
**Testing**: Project SCons build plus `Tests/StonerTest`; deterministic tests in `Tests/VulkanBackendTests.cpp` with existing RHI mock coverage in `Tests/RHICoreTests.cpp` preserved  
**Target Platform**: Desktop development targets supported by the project: Windows, macOS, Linux; unsupported runtime capability paths report explicit results  
**Project Type**: C++ graphics engine library/backend module  
**Performance Goals**: Verification flow can allocate, record, submit, observe completion, wait idle, and reset a command buffer in under 30 seconds; command lifecycle validation rejects 100% of covered invalid transitions  
**Constraints**: Preserve Renderer/Application-facing RHI abstraction; no backend-specific details in public RHI headers; real runtime path uses real queue submission when available; unavailable runtime may use deterministic fallback submission diagnostics; fallback completion defaults to immediate completion with test-configurable not-ready/timeout injection; draw/dispatch are placeholder commands with missing-pipeline diagnostics; barriers/layout transitions are declarative intent with basic validation only  
**Scale/Scope**: One Vulkan backend command submission slice covering command pools, command buffers, queue submission behavior, completion observation, upload scheduling, minimal single-subpass render pass/framebuffer support, diagnostics, shutdown invalidation, and regression tests

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] **Spec-Driven Development**: A clarified feature specification exists at `specs/011-vulkan-commands-submission/spec.md`.
- [x] **Decoupled Architecture**: Renderer/Application behavior remains expressed through RHI contracts; Vulkan-specific command and runtime details stay in the backend.
- [x] **Design Pattern Discipline**: Command pool ownership, command buffer lifecycle, queue submission, diagnostics, render pass/framebuffer validation, and upload scheduling are planned as separate responsibilities.
- [x] **Multi-API Support**: The feature implements one Vulkan backend slice while preserving backend-neutral RHI command and resource contracts for future DX12/Metal/GL implementations.
- [x] **Advanced Graphics Readiness**: Command categories, barriers, render pass scope, and upload scheduling leave room for future pipeline, render graph, ray tracing, meshlet, and GI phases.
- [x] **Naming Conventions**: Planned public project-facing names follow UE5-style prefixes such as `F`, `I`, and `E`.
- [x] **Cross-Platform Compatibility**: Platform/runtime differences are isolated behind backend implementation and explicit unsupported/fallback diagnostics.

## Project Structure

### Documentation (this feature)

```text
specs/011-vulkan-commands-submission/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── vulkan-command-submission-contract.md
└── tasks.md
```

### Source Code (repository root)

```text
Source/
├── RHI/Public/RHI/
│   ├── IRHICommandBuffer.h
│   ├── IRHICommandQueue.h
│   ├── IRHIFence.h
│   ├── IRHISemaphore.h
│   ├── IRHIRenderPass.h
│   ├── IRHIFramebuffer.h
│   └── RHIMinimal.h
├── Backend/Vulkan/Public/VulkanRHI/
│   ├── FVulkanCommandPool.h
│   ├── FVulkanCommandBuffer.h
│   ├── FVulkanCommandSubmission.h
│   ├── FVulkanRenderPass.h
│   ├── FVulkanFramebuffer.h
│   ├── FVulkanQueue.h
│   ├── FVulkanDevice.h
│   ├── FVulkanDiagnostics.h
│   └── VulkanDevice.h
└── Backend/Vulkan/Private/
    ├── FVulkanCommandPool.cpp
    ├── FVulkanCommandBuffer.cpp
    ├── FVulkanCommandSubmission.cpp
    ├── FVulkanRenderPass.cpp
    ├── FVulkanFramebuffer.cpp
    ├── FVulkanQueue.cpp
    ├── FVulkanDevice.cpp
    └── FVulkanDiagnostics.cpp

Tests/
├── RHICoreTests.cpp
└── VulkanBackendTests.cpp
```

**Structure Decision**: Extend the existing single C++ engine repository. Public RHI headers remain backend-neutral; Vulkan backend headers expose Vulkan-specific diagnostics and helper objects only under `Source/Backend/Vulkan/Public/VulkanRHI`; tests stay in the existing project test executable.

## Complexity Tracking

No constitution violations are required. The feature is scoped to one backend slice and explicitly defers shader compilation, real pipeline binding, full resource state tracking, render graph scheduling, multi-threaded command recording, and visible frame rendering.

## Phase 0: Research

Research decisions are captured in [research.md](./research.md). All planning unknowns were resolved by the clarified spec and research decisions.

## Phase 1: Design & Contracts

- Data model: [data-model.md](./data-model.md)
- Observable contract: [contracts/vulkan-command-submission-contract.md](./contracts/vulkan-command-submission-contract.md)
- Verification quickstart: [quickstart.md](./quickstart.md)
- Agent context: `AGENTS.md` updated to point at this plan

## Post-Design Constitution Check

- [x] **Spec-Driven Development**: The plan, research, data model, contract, and quickstart derive from the clarified spec.
- [x] **Decoupled Architecture**: Design artifacts keep Vulkan details behind backend classes and preserve RHI-facing behavior.
- [x] **Design Pattern Discipline**: Data model separates command pools, command buffers, recorded commands, submission batches, render pass/framebuffer objects, diagnostics, and upload scheduling records.
- [x] **Multi-API Support**: RHI contracts remain generic enough for later backend implementations.
- [x] **Advanced Graphics Readiness**: Deferred items are explicit, and command records carry enough intent for future pipelines and render graph integration.
- [x] **Naming Conventions**: Planned entities and files follow current project naming style.
- [x] **Cross-Platform Compatibility**: Runtime availability and unsupported paths are explicit and testable.
