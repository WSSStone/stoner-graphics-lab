# Implementation Plan: Window & Input System

**Branch**: `016-window-input-system` | **Date**: 2026-07-03 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/016-window-input-system/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/plan-template.md` for the execution workflow.

## Summary

Implement the Application layer's first window, input, and application-loop planning surface: a single primary window lifecycle, deterministic frame-based keyboard/mouse state, resize/close/focus events, presentation-paused behavior for minimized or zero drawable size windows, and a minimal loop skeleton that can run without real rendering. The design uses an Application-level public API, a required deterministic headless driver for automated validation, and a GLFW-first real-window adapter when display access and the dependency are available. It does not implement multi-window support, gamepad/controller input, text input composition, debug UI, renderer presentation, swapchain recreation, or triangle rendering.

## Technical Context

**Language/Version**: C++20 with traditional header/source separation; no C++20 Modules  
**Primary Dependencies**: Existing Core types/math/logging/platform abstractions; existing Renderer dependency boundary for future presentation handoff; SCons 4.10.1; GLFW-first real-window adapter behind Application implementation boundaries when available; deterministic headless/null window driver required for tests  
**Storage**: Process-local in-memory window configuration, lifecycle state, event queues, input frame snapshots, loop state, diagnostics, and debug dump strings only; no persistent database, input recording, or preference storage  
**Testing**: Existing SCons test target and platform-specific `StonerTest` executable; add focused Application window/input tests that run deterministically in headless mode; add optional real-window smoke tests that are skipped or report unavailable when display access is absent; add or update a GitHub Actions CI matrix for Windows, macOS, and Linux headless build/test validation  
**Target Platform**: Cross-platform desktop development targets: Windows, macOS, Linux; headless validation must run without a visible display, while real-window smoke validation runs when the environment provides display access  
**Project Type**: C++ graphics engine Application-layer library feature  
**Performance Goals**: Representative application loop with deterministic window/input events runs at least 300 frames or until close request with stable state transitions; input polling and snapshot update for a typical keyboard/mouse event batch completes well under one frame budget; repeated headless scenarios produce byte-stable diagnostics and dumps across at least 20 runs  
**Constraints**: Must not expose GLFW, native OS window handles, Vulkan/Metal/DX/OpenGL objects, swapchain objects, or renderer presentation details through Application public contracts; v1 scope is one primary window and physical keyboard/mouse state only; text character input, keyboard layout translation, gamepad/controller input, multi-window behavior, debug UI, real rendering output, and swapchain recreation are out of scope  
**Scale/Scope**: Foundation scope covers primary window creation/destruction, lifecycle states, resize/minimize/close/focus events, physical key and mouse-button vocabulary, pressed/held/released semantics, pointer position/delta, focus-loss held-state clearing, presentation-paused loop state, deterministic headless validation, optional real-window smoke validation, diagnostics, and public Application contracts

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] **Spec-Driven Development**: The feature has an active specification with four recorded clarifications in `specs/016-window-input-system/spec.md`.
- [x] **Decoupled Architecture**: Application public contracts expose window/input/loop state only and do not call or expose graphics API, RHI, swapchain, or renderer presentation objects.
- [x] **Design Pattern Discipline**: Window lifecycle, platform driver behavior, input state derivation, event queues, loop decisions, diagnostics, and optional real-window integration are separate responsibilities.
- [x] **Multi-API Support**: The feature provides backend-neutral window state and resize/paused presentation signals that Vulkan now and future Metal/DX/OpenGL paths can consume later without public API changes.
- [x] **Advanced Graphics Readiness**: Stable loop and presentation-paused states provide the event and resize foundation needed by later triangle, deferred, meshlet, ray tracing, and GI phases.
- [x] **Naming Conventions**: Planned public names follow UE5-style names such as `FWindow`, `FWindowDesc`, `FInputManager`, `FInputState`, `EKey`, `EMouseButton`, and `FApplicationLoop`.
- [x] **Cross-Platform Compatibility**: Planned code is standard C++20 at the public contract level, isolates platform/GLFW-specific code behind implementation boundaries, and keeps headless tests independent of display availability.
- [x] **Automated Cross-Platform Validation**: The plan includes a GitHub Actions CI matrix for Windows, macOS, and Linux deterministic headless SCons build/test validation, with real-window smoke behavior optional or unavailable-safe.

## Project Structure

### Documentation (this feature)

```text
specs/016-window-input-system/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── window-input-contract.md
└── tasks.md
```

### Source Code (repository root)

```text
Source/Application/
├── Public/Application/
│   ├── ApplicationMinimal.h
│   ├── EKey.h
│   ├── EMouseButton.h
│   ├── FApplicationDiagnostics.h
│   ├── FApplicationLoop.h
│   ├── FInputEvent.h
│   ├── FInputManager.h
│   ├── FInputState.h
│   ├── FWindow.h
│   ├── FWindowDesc.h
│   └── FWindowEvent.h
├── Private/
│   ├── ApplicationModule.cpp
│   ├── FApplicationDiagnostics.cpp
│   ├── FApplicationLoop.cpp
│   ├── FHeadlessWindowDriver.cpp
│   ├── FInputManager.cpp
│   ├── FInputState.cpp
│   ├── FWindow.cpp
│   ├── FWindowEvent.cpp
│   └── FGlfwWindowDriver.cpp
└── SConscript

Tests/
├── ApplicationWindowInputTests.h
├── ApplicationWindowInputTests.cpp
├── Main.cpp
└── SConscript

.github/
└── workflows/
    └── ci.yml              # Cross-platform headless build/test matrix

ThirdParty/
└── GLFW/                 # Optional/provisioned dependency location if the implementation vendors GLFW for real-window smoke validation
```

**Structure Decision**: Add window/input contracts to the existing Application layer because the feature sits above Core platform abstractions and below later demo/runtime workflows. Public headers expose Application concepts only. Private drivers isolate deterministic headless behavior from the optional GLFW-backed real-window path. Tests live in the existing single test executable beside Core, RHI, Renderer, and Vulkan tests.

## Phase 0: Research

Completed in [research.md](./research.md). The main decisions are:

- Use Application-level public window/input/loop contracts with private driver strategy implementations.
- Require deterministic headless validation for CI and no-display environments.
- Use a GLFW-first real-window adapter for display-backed smoke validation when available.
- Represent keyboard input as physical key/button state only; text input and layout translation are deferred.
- Clear all held keyboard and mouse-button state immediately on focus loss.
- Treat minimized or zero drawable size windows as presentation-paused while event polling and updates continue.
- Keep resize/close/focus events deterministic and coalesce final resize size per frame.
- Keep diagnostics and debug dumps byte-stable for regression tests.

## Phase 1: Design & Contracts

Completed outputs:

- [data-model.md](./data-model.md): entities, fields, validation rules, and state transitions.
- [contracts/window-input-contract.md](./contracts/window-input-contract.md): public Application-layer behavioral contract.
- [quickstart.md](./quickstart.md): expected development and verification flow.

## Post-Design Constitution Check

- [x] **Spec-Driven Development**: Plan, research, data model, contract, and quickstart align with the active spec and clarifications.
- [x] **Decoupled Architecture**: Contracts expose Application window/input behavior without graphics API, RHI, swapchain, or renderer presentation dependencies.
- [x] **Design Pattern Discipline**: Design artifacts keep window driver strategy, lifecycle state, input derivation, event queueing, loop decisions, diagnostics, and inspection independently testable.
- [x] **Multi-API Support**: Window state and presentation-paused signals remain backend-neutral and suitable for later Vulkan, Metal, DX, OpenGL, GLES, and WebGL integration layers.
- [x] **Advanced Graphics Readiness**: Resize, close, focus, and no-drawable loop semantics create the stable runtime foundation needed by future advanced rendering phases.
- [x] **Naming Conventions**: Planned public names follow project naming conventions.
- [x] **Cross-Platform Compatibility**: Platform-specific or third-party window code is isolated behind private adapters, and headless validation remains display-independent.
- [x] **Automated Cross-Platform Validation**: Tasks require `.github/workflows/ci.yml` or equivalent validation coverage for `ubuntu-latest`, `macos-latest`, and `windows-latest`, plus quickstart documentation of expected matrix behavior.

## Complexity Tracking

No constitution violations or complexity exceptions are required.
