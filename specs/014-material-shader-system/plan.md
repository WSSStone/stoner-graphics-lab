# Implementation Plan: Material & Shader System

**Branch**: `014-material-shader-system` | **Date**: 2026-07-01 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/014-material-shader-system/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/plan-template.md` for the execution workflow.

## Summary

Implement the Renderer layer's material and shader management foundation: reusable material definitions, instance inheritance with cycle detection, deterministic shader permutation selection from explicitly registered precompiled shader records, typed material parameter sets, abstract Renderer-level resource references, render graph resource requirement summaries, validation diagnostics, invalidation/reset behavior, and deterministic human-readable inspection dumps. The design stays backend-agnostic, avoids runtime shader compilation and file scanning/loading, and prepares the next forward rendering phase to bind material data through existing render graph and RHI contracts.

## Technical Context

**Language/Version**: C++20 with traditional header/source separation; no C++20 Modules  
**Primary Dependencies**: Existing Core types/containers/math/logging; existing Renderer render graph public contracts; existing RHI public shader/resource/descriptor/pipeline result conventions for compatibility checks; SCons 4.10.1  
**Storage**: Process-local in-memory material definitions, material instances, parameter sets, shader records, shader permutations, resource reference identifiers, diagnostics, and inspection dump strings only  
**Testing**: Existing SCons test target and `Build/Mac/Debug/Tests/StonerTest`; add focused Renderer material/shader tests with headless/mock resource references and no physical graphics device requirement  
**Target Platform**: Cross-platform desktop development targets: Windows, macOS, Linux; headless test execution must not require a visible window or GPU-backed presentation  
**Project Type**: C++ graphics engine library feature in the Renderer layer  
**Performance Goals**: Representative material library with at least 5 parent materials, 10 instances, 4 parameter types, and 3 shader permutations can be validated, inspected, and resource-summarized in under 60 seconds through the project verification flow; repeated permutation resolution and text dumps are deterministic across at least 20 runs  
**Constraints**: Must not include Vulkan/Metal/DX/OpenGL headers or backend concepts in Renderer public contracts; must not implement visual material editing, runtime shader compilation, local shader file scanning/loading, PBR-specific material models, concrete forward rendering passes, scene graph integration, visible presentation, or live resource ownership in material parameters  
**Scale/Scope**: Foundation scope covers reusable materials, instance chains, inheritance cycle rejection, shader record registration, per-shader allowed permutation flags, variant selection, parameter validation, abstract texture/resource references, render graph resource requirement summaries, deterministic diagnostics, invalidation/reset behavior, and text dumps

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] **Spec-Driven Development**: The feature has an active specification with five recorded clarifications in `specs/014-material-shader-system/spec.md`.
- [x] **Decoupled Architecture**: Renderer material contracts remain above RHI abstractions and do not depend on any backend graphics API.
- [x] **Design Pattern Discipline**: Material, instance, parameter, shader library, permutation, diagnostics, and resource-requirement responsibilities remain separate.
- [x] **Multi-API Support**: The feature uses Renderer-level material/shader concepts and abstract resource references suitable for all RHI backends.
- [x] **Advanced Graphics Readiness**: Material domains, permutation flags, abstract resources, and deterministic shader selection leave room for forward, deferred, meshlet, ray tracing, and GI workflows.
- [x] **Naming Conventions**: Planned types follow UE5-style names such as `FMaterial`, `FMaterialInstance`, `FShaderLibrary`, and `EMaterialBlendMode`.
- [x] **Cross-Platform Compatibility**: Planned code is standard C++20, headless-testable, and avoids platform-specific assumptions.

## Project Structure

### Documentation (this feature)

```text
specs/014-material-shader-system/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── material-shader-contract.md
└── tasks.md
```

### Source Code (repository root)

```text
Source/Renderer/
├── Public/Renderer/
│   ├── RendererMinimal.h
│   ├── FMaterial.h
│   ├── FMaterialInstance.h
│   ├── FMaterialParameterSet.h
│   ├── FMaterialResourceRequirement.h
│   ├── FMaterialShaderBinding.h
│   ├── FShaderLibrary.h
│   ├── FShaderPermutation.h
│   └── FMaterialDiagnostics.h
├── Private/
│   ├── FMaterial.cpp
│   ├── FMaterialInstance.cpp
│   ├── FMaterialParameterSet.cpp
│   ├── FMaterialResourceRequirement.cpp
│   ├── FMaterialShaderBinding.cpp
│   ├── FShaderLibrary.cpp
│   ├── FShaderPermutation.cpp
│   └── FMaterialDiagnostics.cpp
└── SConscript

Tests/
├── RendererMaterialShaderTests.h
├── RendererMaterialShaderTests.cpp
├── Main.cpp
└── SConscript
```

**Structure Decision**: Add material/shader foundation types to the existing Renderer layer because the feature describes render-surface policy above raw RHI objects. Tests live in the repository's existing single test executable beside Core/RHI/Vulkan/Renderer render graph tests.

## Phase 0: Research

Completed in [research.md](./research.md). The main decisions are:

- Keep material and shader contracts backend-agnostic and Renderer-owned.
- Use explicit in-memory registration for precompiled shader records.
- Store material texture/resource parameters as abstract Renderer-level references.
- Support material instance chains with deterministic cycle detection.
- Validate permutation flags against each target shader record before variant selection.
- Provide deterministic human-readable inspection dumps as the primary debug artifact.
- Defer visual editing, runtime compilation, shader file loading, PBR models, and live resource binding to later phases.

## Phase 1: Design & Contracts

Completed outputs:

- [data-model.md](./data-model.md): entities, fields, validation rules, and state transitions.
- [contracts/material-shader-contract.md](./contracts/material-shader-contract.md): public Renderer-layer behavioral contract.
- [quickstart.md](./quickstart.md): expected development and verification flow.

## Post-Design Constitution Check

- [x] **Spec-Driven Development**: Plan, research, data model, contracts, and quickstart align with the active spec and clarifications.
- [x] **Decoupled Architecture**: Contracts expose Renderer-layer concepts and abstract RHI-compatible resource needs without backend details.
- [x] **Design Pattern Discipline**: Design artifacts keep material definition, instance resolution, shader lookup, parameter validation, diagnostics, and inspection independently testable.
- [x] **Multi-API Support**: The contract is backend-agnostic and suitable for current Vulkan plus future Metal/DX/OpenGL RHI implementations.
- [x] **Advanced Graphics Readiness**: Material domains, blend modes, permutation flags, and resource requirement summaries leave room for later forward, deferred, meshlet, ray tracing, and GI work.
- [x] **Naming Conventions**: Planned public names follow project naming conventions.
- [x] **Cross-Platform Compatibility**: No platform-specific dependency is introduced; validation remains headless-testable.

## Complexity Tracking

No constitution violations or complexity exceptions are required.
