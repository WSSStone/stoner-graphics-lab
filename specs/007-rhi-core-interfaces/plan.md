# Implementation Plan: RHI Core Interfaces

**Branch**: `007-rhi-core-interfaces` | **Date**: 2026-06-25 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/007-rhi-core-interfaces/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/plan-template.md` for the execution workflow.

## Summary

Define the first real RHI layer contracts for device lifecycle, capabilities, command buffers, queues, synchronization primitives, headless/mock swapchain behavior, result/status reporting, and format identities. The implementation approach is interface-first and mock-test-driven: public RHI headers expose stable contracts, while deterministic mock implementations in tests validate lifecycle matrices, negative paths, and renderer-facing smoke flow without depending on a concrete graphics backend.

## Technical Context

**Language/Version**: C++20 with traditional header/source separation  
**Primary Dependencies**: Existing Core layer types/math/logging/platform abstractions; SCons 4.10.1; C++ standard library for non-graphics utilities  
**Storage**: N/A  
**Testing**: Existing `Tests/StonerTest` executable built by SCons; new RHI mock tests integrated into the same test runner  
**Target Platform**: Windows, macOS, Linux  
**Project Type**: Layered C++ graphics engine library  
**Performance Goals**: RHI core mock lifecycle and smoke tests complete within the existing test executable's normal local feedback loop; command and state validation remains deterministic and allocation-light for unit-test scale usage  
**Constraints**: No concrete graphics API calls; no Renderer/Application/Backend dependencies in public RHI core headers; no real GPU, native window, or platform surface required for tests; command recording remains symbolic; resource/pipeline creation and validation deferred to the next RHI feature  
**Scale/Scope**: One RHI core interface slice covering device, capabilities, queues, command buffers, fences, semaphores, headless/mock swapchain, result/status values, format identities, and mock-based contract tests

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] **Spec-Driven Development**: Specification exists at `specs/007-rhi-core-interfaces/spec.md` with clarifications recorded before planning.
- [x] **Decoupled Architecture**: Feature defines the RHI layer and explicitly avoids Application, Renderer, Backend, or graphics API dependencies.
- [x] **Design Pattern Discipline**: Responsibilities are split across device, queue, command buffer, synchronization, swapchain, and value/status contracts; no catch-all object is planned.
- [x] **Multi-API Support**: Contracts describe portable RHI behavior suitable for Vulkan, DX12, Metal, OpenGL, GLES, WebGL, and mock implementations.
- [x] **Advanced Graphics Readiness**: Symbolic command and capability contracts leave room for future compute, ray tracing, meshlet, and GI extensions.
- [x] **Naming Conventions**: Planned public names follow UE5-style prefixes: `I*` interfaces, `F*` structs/value objects, `E*` enums.
- [x] **Cross-Platform Compatibility**: Planning requires SCons build and `StonerTest` verification on supported platforms; no platform-specific public dependency is introduced.

## Project Structure

### Documentation (this feature)

```text
specs/007-rhi-core-interfaces/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── rhi-core-api.md
├── checklists/
│   └── requirements.md
└── tasks.md
```

### Source Code (repository root)

```text
Source/RHI/
├── Public/RHI/
│   ├── RHIMinimal.h
│   ├── ERHIFormat.h
│   ├── ERHIQueueType.h
│   ├── ERHIResult.h
│   ├── FRHIDeviceCapabilities.h
│   ├── IRHICommandBuffer.h
│   ├── IRHICommandQueue.h
│   ├── IRHIDevice.h
│   ├── IRHIFence.h
│   ├── IRHISemaphore.h
│   └── IRHISwapchain.h
├── Private/
│   └── RHIModule.cpp
└── SConscript

Tests/
├── RHICoreTests.h
├── RHICoreTests.cpp
├── Main.cpp
└── SConscript
```

**Structure Decision**: Add focused public RHI headers under `Source/RHI/Public/RHI/`, keep production implementation minimal because contracts are abstract, and place deterministic mock implementations inside `Tests/RHICoreTests.cpp` so the feature validates public behavior without adding fake backend code to shipping layers.

## Complexity Tracking

No constitution violations identified.

## Phase 0: Research

Research completed in [research.md](./research.md). All planning uncertainties were resolved without `NEEDS CLARIFICATION` markers.

## Phase 1: Design & Contracts

Design artifacts:

- [data-model.md](./data-model.md)
- [contracts/rhi-core-api.md](./contracts/rhi-core-api.md)
- [quickstart.md](./quickstart.md)

## Post-Design Constitution Check

- [x] **Spec-Driven Development**: Plan, research, data model, contract, and quickstart are generated before implementation.
- [x] **Decoupled Architecture**: Public contracts remain in RHI and depend only on Core; tests use mocks rather than backend APIs.
- [x] **Design Pattern Discipline**: Interface boundaries keep lifecycle, submission, synchronization, and presentation concerns separate.
- [x] **Multi-API Support**: No contract assumes Vulkan/DX/Metal/OpenGL semantics beyond portable RHI concepts.
- [x] **Advanced Graphics Readiness**: Capability/result/queue concepts are extensible for future compute and advanced rendering work.
- [x] **Naming Conventions**: Contract names preserve UE5-style prefixes.
- [x] **Cross-Platform Compatibility**: Quickstart requires SCons build and `StonerTest` verification through the existing cross-platform build path.
