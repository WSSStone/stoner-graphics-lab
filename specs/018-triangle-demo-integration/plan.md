# Implementation Plan: Triangle Demo Integration Milestone

**Branch**: `018-triangle-demo-integration` | **Date**: 2026-07-20 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `specs/018-triangle-demo-integration/spec.md`

## Summary

Deliver the first visible end-to-end engine milestone as a standalone `StonerDemo` executable. The demo creates a real GLFW window, initializes the native Vulkan runtime, uploads one colored triangle, prepares a forward frame and render graph, translates that plan into real RHI commands, and acquires, submits, and presents until interactive exit or a configured validation frame budget completes. Existing deterministic Application/RHI/Vulkan models remain available as an explicit test mode; native visible mode must reject fallback execution. Windows and macOS require retained real-window screenshot/log evidence, while Linux CI runs both deterministic headless tests and a no-window native Vulkan integration through Mesa Lavapipe.

## Technical Context

**Language/Version**: C++20 with traditional header/source separation; no C++20 Modules
**Primary Dependencies**: Existing Core, Application, Renderer, RHI, and Vulkan backend contracts; SCons 4.10.1; GLFW 3.4-compatible desktop window integration; Vulkan 1.3-compatible headers/loader; MoltenVK portability path on macOS; Mesa Lavapipe software Vulkan path on Linux CI; offline GLSL-to-SPIR-V compiler and SPIR-V validator when available
**Storage**: Repository-owned shader source and checked-in SPIR-V payloads; process-local runtime/frame/diagnostic state; local screenshot and log evidence under `Validation/018/`; no database or asset catalog
**Testing**: Existing `StonerTest` executable plus deterministic demo contract tests, native no-window Vulkan integration, bounded endurance validation, SCons build checks, and GitHub Actions Windows/macOS/Linux matrix
**Target Platform**: Visible Windows and macOS desktop presentation; Linux build, deterministic headless execution, and no-window native Vulkan execution through a CPU software device; Android excluded
**Project Type**: Cross-platform C++ graphics-engine libraries plus one standalone desktop demo executable
**Performance Goals**: First successful presentation within 5,000 milliseconds of process startup; two frames in flight by default; first successful presentation of each replacement generation within 2,000 milliseconds of observing a restored non-zero drawable extent; bounded validation defaults of 4,096 frames for automated headless/software-Vulkan jobs and 10,000 frames for Windows/macOS real-window smoke, all configurable
**Constraints**: Visible mode cannot accept deterministic fallback as success; Application/Renderer cannot call Vulkan directly; native handles remain backend-private except for the opaque Core platform-window bridge; zero drawable size pauses presentation but not event polling; shutdown leaves zero demo-owned live resources; post-warm-up resident-memory growth must remain within configured mode-specific limits
**Scale/Scope**: One window, one hardcoded three-vertex RGB triangle, one forward color pass, one swapchain or offscreen color target, default two frames in flight, no scene/ECS geometry, depth, textures, lighting, runtime shader compilation, Android, or advanced rendering

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] **Spec-Driven Development**: The active specification records five clarification decisions and testable completion evidence in `specs/018-triangle-demo-integration/spec.md`.
- [x] **Decoupled Architecture**: Raw Vulkan and GLFW calls remain in backend/private Application adapters. The demo composition root coordinates public Application, Renderer, and RHI contracts without exposing graphics API types upward.
- [x] **Design Pattern Discipline**: Window driver and runtime mode use Strategy boundaries. `FStonerDemoApplication` is a lifecycle Composite over independently owned Presentation State, Triangle Resource Set, Frame Context collection, diagnostics, and validation monitor children, with deterministic reverse-order teardown rather than one demo god-class.
- [x] **Multi-API Support**: New execution and presentation contracts are expressed at RHI level; Vulkan is the first implementation, while later Metal/DX12 backends can implement the same contracts.
- [x] **Advanced Graphics Readiness**: Imported presentation targets, interleaved render-graph transitions, explicit frame contexts, and RHI command execution remain usable by deferred, meshlet, ray tracing, and GI phases.
- [x] **Naming Conventions**: Planned public and private types use UE5-style names such as `FStonerDemoApplication`, `FForwardFrameExecutor`, `FRHIPresentationSurfaceDesc`, `FFrameContext`, and `EDemoRunMode`.
- [x] **Cross-Platform Compatibility**: Dependency detection, runtime availability, MoltenVK portability enumeration, Windows loader linking, and Linux software Vulkan are isolated by build/runtime capability checks.
- [x] **Automated Cross-Platform Validation**: The plan updates the existing CI matrix for demo build and deterministic tests on all three platforms and adds a Linux Lavapipe no-window native-backend job; Windows/macOS visible evidence follows the clarified manual validation contract.

**Pre-Design Gate Result**: PASS. No constitution violation requires an exception.

## Project Structure

### Documentation (this feature)

```text
specs/018-triangle-demo-integration/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── triangle-demo-runtime-contract.md
│   └── triangle-demo-validation-contract.md
└── tasks.md
```

### Source Code (repository root)

```text
Demo/StonerDemo/
├── SConscript
├── Private/
│   ├── Main.cpp
│   ├── FStonerDemoApplication.h
│   ├── FStonerDemoApplication.cpp
│   ├── FDemoConfiguration.h
│   ├── FDemoDiagnostics.h
│   └── FDemoValidationMonitor.h
└── Shaders/
    ├── Triangle.vert
    ├── Triangle.frag
    ├── Triangle.vert.spv
    └── Triangle.frag.spv

Source/Core/
├── Public/Core/FPlatformMemory.h
└── Private/FPlatformMemory.cpp

Source/Application/
├── Public/Application/FWindow.h
└── Private/
    ├── FWindow.cpp
    ├── FWindowDriver.h
    └── FGlfwWindowDriver.cpp

Source/RHI/Public/RHI/
├── ERHIRuntimeMode.h
├── FRHIPresentationSurfaceDesc.h
├── FRHISwapchainDesc.h
├── IRHIPresentationSurface.h
├── IRHIDevice.h
├── IRHISwapchain.h
└── IRHICommandBuffer.h

Source/Renderer/
├── Public/Renderer/FForwardFrameExecutor.h
└── Private/FForwardFrameExecutor.cpp

Source/Backend/Vulkan/
├── Public/VulkanRHI/
│   ├── FVulkanNativeContext.h
│   ├── FVulkanInstance.h
│   ├── FVulkanDevice.h
│   ├── FVulkanSurface.h
│   ├── FVulkanSwapchain.h
│   ├── FVulkanCommandBuffer.h
│   └── FVulkanRuntimeSnapshot.h
└── Private/
    └── corresponding native Vulkan implementations

Tests/
├── TriangleDemoIntegrationTests.h
├── TriangleDemoIntegrationTests.cpp
├── VulkanNativeIntegrationTests.h
├── VulkanNativeIntegrationTests.cpp
└── Main.cpp

Validation/018/
├── Windows/
│   ├── triangle.png
│   └── triangle.log
└── macOS/
    ├── triangle.png
    └── triangle.log

site_scons/
└── GraphicsDependencyDetect.py

.github/workflows/
└── ci.yml
```

**Structure Decision**: Keep reusable window behavior in Application, backend-neutral execution translation in Renderer/RHI, native object ownership in `Backend/Vulkan`, and demo-only orchestration/configuration in a new `Demo/StonerDemo` target. Its SConscript builds the private runtime sources as a static library shared by `StonerDemo` and `StonerTest`, with `Main.cpp` linked only into the executable. The demo is the composition root and lifecycle Composite: Presentation State, Triangle Resource Set, and the Frame Context collection remain independently testable child owners, while the root coordinates their reverse-order shutdown. The executable may link all layers, but reusable Application and Renderer public headers remain free of Vulkan/GLFW types. Real and deterministic runtime modes share public contracts but never share success labels.

## Phase 0: Research

Completed in [research.md](./research.md). Key decisions:

- Preserve deterministic behavior as an explicit runtime strategy and add an explicit native strategy; visible mode requires native execution.
- Replace the GLFW placeholder with a private real driver and expose only an opaque Core platform-window bridge.
- Add native Vulkan ownership incrementally behind existing backend classes instead of introducing raw Vulkan into the demo or Renderer.
- Extend RHI presentation, swapchain-image, upload, vertex-binding, and command execution contracts only where the triangle path requires them.
- Add `FForwardFrameExecutor` to translate prepared forward/render-graph declarations into RHI commands with transitions interleaved per pass.
- Use two frames in flight by default, per-frame command/acquire/fence state, and render-finished synchronization keyed by swapchain image.
- Use simple wait-idle swapchain recreation for this milestone, preserving correctness while deferring advanced retirement queues.
- Keep GLSL source and checked-in SPIR-V; rebuild/validate payloads when tools exist and reject invalid payloads at runtime.
- Run Linux native integration with Mesa Lavapipe and no presentation surface.
- Use tiered configurable endurance budgets with resource counters plus post-warm-up resident-memory deltas.
- Require manual Windows/macOS screenshot and matching log evidence; automated golden-image comparison remains deferred.

## Phase 1: Design & Contracts

Completed outputs:

- [data-model.md](./data-model.md): demo configuration, runtime state machine, presentation state, frame contexts, resource set, diagnostics, memory samples, and validation report.
- [contracts/triangle-demo-runtime-contract.md](./contracts/triangle-demo-runtime-contract.md): executable CLI, runtime modes, frame order, RHI/Renderer handoff, resize, error, and shutdown behavior.
- [contracts/triangle-demo-validation-contract.md](./contracts/triangle-demo-validation-contract.md): platform matrix, endurance defaults, memory/resource gates, native-vs-fallback proof, and retained evidence format.
- [quickstart.md](./quickstart.md): deterministic tests, Linux Lavapipe validation, Windows/macOS real-window smoke, shader verification, and evidence workflow.

## Post-Design Constitution Check

- [x] **Spec-Driven Development**: Every planned contract and validation path maps back to FR-001 through FR-021 and SC-001 through SC-009.
- [x] **Decoupled Architecture**: The executable composes public layer contracts; GLFW remains Application-private and Vulkan handles remain backend-private.
- [x] **Design Pattern Discipline**: Driver/runtime Strategy boundaries and the Demo Application lifecycle Composite assign separate ownership and tests to RHI execution, presentation recreation, triangle resources, frame contexts, diagnostic aggregation, and validation monitoring.
- [x] **Multi-API Support**: RHI presentation and execution contracts do not encode Vulkan handles, extension names, queue-family indices, or SPIR-V in backend-neutral types.
- [x] **Advanced Graphics Readiness**: Render-graph imported resources and transition callbacks are generalized beyond one triangle while demo-only data remains outside reusable Renderer contracts.
- [x] **Naming Conventions**: All proposed names and file locations follow project conventions.
- [x] **Cross-Platform Compatibility**: The design specifies capability detection and controlled unavailable results rather than unconditional platform assumptions.
- [x] **Automated Cross-Platform Validation**: All three platforms build/run deterministic coverage; Linux additionally executes native Vulkan through Lavapipe; required Windows/macOS presentation evidence is explicit and retained.

**Post-Design Gate Result**: PASS. Design introduces no constitution exception.

## Complexity Tracking

No constitution violations or complexity exceptions are required. The cross-layer scope is inherent to the roadmap's first integration milestone; responsibilities remain within existing ownership boundaries and the demo executable acts only as composition root.
