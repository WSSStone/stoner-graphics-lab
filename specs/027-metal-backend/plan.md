# Implementation Plan: Native Metal Backend

**Branch**: `027-metal-backend` | **Date**: 2026-08-18 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/027-metal-backend/spec.md`

## Summary

Implement a native macOS Metal realization of every applicable public Feature
007/008 RHI contract while preserving Vulkan/MoltenVK and keeping Renderer,
Application, and Asset free of Metal API ownership. Private Objective-C++
backend components own Metal devices, resources, commands, synchronization,
pipelines, and an attached `CAMetalLayer`; backend-neutral RHI corrections
generalize shader payload bytes and capability limits without weakening the
existing Vulkan path.

The authoritative GLSL/SPIR-V Asset chain remains unchanged. A Tools-only,
pinned SPIRV-Cross stage deterministically derives normalized MSL on all three
development hosts. macOS alone invokes the Apple offline compiler without a
shell to produce target-qualified `metallib` payloads for strict cooked
generations. CI builds and cooks on both Apple Silicon and Intel macOS runners.
The required full native gates run on the physical M4 Pro self-hosted arm64
runner and GitHub-hosted `macos-26-intel` x86_64 runner; the latter counts only
with a real Metal device and GPU readback. Visible lifecycle acceptance remains
a physical-M4 gate, while a physical Intel compatibility run is optional.

## Technical Context

**Language/Version**: C++20 with traditional public/private headers and sources; Objective-C++20 with ARC in Metal-private `.mm` units; Python 3 standard-library validation scripts; no C++20 Modules
**Primary Dependencies**: Existing Core/RHI/Renderer/Application/Asset and AssetCooker contracts; Apple Metal, QuartzCore, Cocoa, and Foundation frameworks on macOS; GLFW 3.4 native Cocoa bridge; pinned private SPIRV-Cross `a0fba56c34a6700f1724bf9b751da5b488a3775c`; SCons 4.10.1
**Storage**: Existing immutable DDC entries and published cooked generations containing versioned Metal shader payloads and derivation/compiler evidence; process-local native Metal state and validation artifacts under `Validation/027/`; no database, runtime shader cache, or Asset-owned GPU state
**Testing**: Existing C++ test runner; backend contract, shader-cook, native offscreen, presentation, failure-injection, lifecycle, and cross-backend probes; Python validators; Windows/macOS/Linux Debug and strict Release; Linux shared-code sanitizers; `macos-26` arm64 and `macos-26-intel` x86_64 build/cook jobs; physical M4 Pro arm64 plus GPU-qualified GitHub-hosted Intel full native evidence
**Target Platform**: macOS 12.0+ desktop on Apple Silicon and Intel Metal-capable Macs, MSL 2.4 baseline with runtime capability gating; Windows and Linux build shared contracts and run deterministic MSL derivation without Apple frameworks
**Project Type**: Layered C++ graphics engine, desktop demo, and offline Asset cooker CLI
**Performance Goals**: At least 3,000 visible frames and 20 lifecycle cycles; 20 byte-stable shader derivations/final cooks under an identical architecture/toolchain tuple; 10,000 bounded resource/pipeline/command lifecycle iterations with a 1,000-iteration warm-up and final-versus-initial post-warm-up median RSS growth no greater than `max(16 MiB, 5%)`
**Constraints**: No runtime MSL compilation; no shell invocation; no Metal/Objective-C types in public RHI, Asset, Renderer, or Application headers; no silent backend fallback; no semantic oracle counted as native evidence; Metal units and Apple linkage excluded on non-macOS hosts
**Scale/Scope**: All applicable current Feature 007/008 device, resource, descriptor, graphics/compute pipeline, render-pass/framebuffer, command, queue, synchronization, upload/readback, surface, and swapchain contracts; triangle and deferred integrations; two Mac CPU architectures; one versioned shader-binding policy

## Constitution Check

*GATE: Passed before Phase 0 research and re-checked after Phase 1 design.*

- [x] **Spec-Driven Development**: The clarified specification defines 45
  functional requirements, ten measurable outcomes, ownership boundaries, and
  explicit exclusions.
- [x] **Decoupled Architecture**: Metal implementation remains `Backend -> RHI
  + Core`; Renderer selects RHI payloads, Application owns its window/view, and
  Asset owns immutable CPU bytes and evidence only.
- [x] **Design Pattern Discipline**: Device selection, capabilities, resources,
  binding translation, commands, synchronization, pipelines, presentation,
  shader cooking, and validation are separate components coordinated through a
  small factory and shared device-owner state.
- [x] **Multi-API Support**: RHI corrections are backend-neutral and must migrate
  Vulkan plus mocks in the same milestone; backend selection remains explicit
  and both Metal and Vulkan/MoltenVK run common integration workloads.
- [x] **Advanced Graphics Readiness**: Capability and binding contracts retain
  extensible limits for later meshlets, ray tracing, GI, and residency without
  implementing or promising those Metal features in 027.
- [x] **Naming Conventions**: New engine-facing contracts use PascalCase and the
  established Unreal-style type prefixes.
- [x] **Cross-Platform Compatibility**: Apple frameworks and Objective-C++ are
  macOS-private; Windows/Linux compile public selection and RHI changes and run
  the same deterministic SPIR-V-to-MSL derivation.
- [x] **Automated Cross-Platform Validation**: The plan adds Windows/macOS/Linux
  Debug and strict Release gates, shared-code sanitizer gates, and current
  `macos-26` arm64 plus `macos-26-intel` x86_64 build/cook jobs. Required full
  native jobs use the physical M4 Pro self-hosted arm64 runner and GitHub-hosted
  `macos-26-intel`; both pass only after a real Metal-device probe and native
  GPU readback. Hosted `unavailable` results cannot satisfy either gate.

### Post-Design Re-check

Phase 1 introduces no constitutional exception. SPIRV-Cross is private to the
offline Tools composition and never becomes an Asset or runtime dependency.
Objective-C++ and Apple frameworks remain under `Source/Backend/Metal/Private`.
The shader-payload and capability changes are backend-neutral RHI corrections,
with mandatory Vulkan and mock migrations. Presentation preserves Application
window ownership and gives Backend ownership only of its attached layer.

## Project Structure

### Documentation (this feature)

```text
specs/027-metal-backend/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── backend-neutral-rhi.md
│   ├── metal-device-resource-command.md
│   ├── metal-presentation.md
│   ├── metal-shader-cook.md
│   ├── rhi-operation-matrix.md
│   ├── validation-evidence.md
│   ├── metal-shader-evidence.schema.json
│   └── metal-validation-report.schema.json
└── tasks.md
```

### Source Code (repository root)

```text
Source/
├── Core/
│   ├── Public/Core/FPlatformProcess.h
│   └── Private/FPlatformProcess.cpp
├── RHI/Public/RHI/
│   ├── ERHIShaderPayloadFormat.h
│   ├── FRHINativeBindingMap.h
│   ├── FRHIDeviceCapabilities.h
│   └── FRHIShaderModuleDesc.h
├── Asset/Public/Asset/
│   └── FShaderNativeBindingEvidence.h
├── Renderer/Private/
│   └── FShaderAssetConversion.cpp
└── Backend/Metal/
    ├── Public/MetalRHI/
    │   ├── FMetalBackendConfig.h
    │   ├── FMetalBackendDiagnostics.h
    │   ├── FMetalBackendInspection.h
    │   └── FMetalDeviceFactory.h
    ├── Private/
    │   ├── FMetalAdapter.*
    │   ├── FMetalBindingMapValidator.*
    │   ├── FMetalCapabilities.*
    │   ├── FMetalCommandBuffer.mm
    │   ├── FMetalDevice.mm
    │   ├── FMetalDeviceOwnerState.*
    │   ├── FMetalFormat.*
    │   ├── FMetalPipeline.mm
    │   ├── FMetalPresentation.mm
    │   ├── FMetalQueue.mm
    │   ├── FMetalResource.mm
    │   ├── FMetalShaderLibrary.mm
    │   └── FMetalSynchronization.mm
    └── SConscript

Tools/AssetCooker/Private/
├── FMetalLibraryCompiler.*
├── FMetalShaderCooker.*
└── FSpirvCrossMslDeriver.*

ThirdParty/spirv-cross/
├── LICENSE
├── UPSTREAM.md
├── spirv_cross.cpp
├── spirv_cross_parsed_ir.cpp
├── spirv_parser.cpp
├── spirv_cfg.cpp
├── spirv_glsl.cpp
└── spirv_msl.cpp

Config/AssetCooker/Profiles/
├── Mac-Metal-Arm64.json
└── Mac-Metal-X86_64.json

Demo/
└── StonerDemo/Private/
    ├── FDemoBackendFactory.*
    ├── FDemoConfiguration.*
    └── FStonerDemoApplication.*

Tests/
├── MetalBackendContractTests.*
├── MetalShaderCookerTests.*
├── MetalNativeIntegrationTests.*
├── MetalPresentationTests.*
├── MetalFailureInjectionTests.*
├── verify_metal_backend.py
└── test_verify_metal_backend.py

.github/
├── scripts/run_metal_validation.py
├── workflows/feature-027-metal-backend.yml
└── workflows/feature-027-metal-hardware.yml

Validation/027/
├── README.md
├── reports/
├── captures/
└── CI/
```

**Structure Decision**: Extend the existing layered engine and monolithic test
runner. Public Metal headers expose only API-free configuration, diagnostics,
inspection, and factory contracts. All native objects live in private
Objective-C++ units. Offline shader transformation stays in AssetCooker and
vendors only the minimal SPIRV-Cross core/MSL source set. Reusable process
launching belongs in Core because it is backend-neutral and needed by Tools;
it accepts an executable plus argv, never a shell command.

## Implementation Phases

1. **M0 Backend-neutral prerequisites**: Generalize RHI shader payload storage
   from SPIR-V words to typed bytes, add inspectable binding/dispatch/resource
   limits, freeze the public RHI operation matrix, migrate Vulkan and mocks, add
   safe process execution, enforce `MACOSX_DEPLOYMENT_TARGET=12.0`, teach SCons
   and architecture scans about private `.mm` units, and preserve regressions.
2. **M1 Deterministic Metal shader derivation**: Vendor and verify pinned
   SPIRV-Cross, define binding-map policy v1, derive canonical LF UTF-8 MSL from
   validated SPIR-V on all hosts, and persist canonical map entries/digest in
   Asset evidence for Renderer-to-RHI transfer.
3. **M2a Native finalizer prerequisites**: Add macOS-only `metal`/`metallib`
   invocation plus explicit Metal-library payload, profile, and evidence codec
   primitives. Phase 2 completes M0, M1, and M2a only.
4. **M3 Device, capabilities, and resources**: Implement deterministic adapter
   selection, shared device-owner state, full capability mapping, buffers,
   textures, samplers, staging/readback, storage-mode coherency, lifecycle
   validation, and resource failure injection.
5. **M4 Bindings and pipelines**: Validate and consume the Asset-authored,
   Renderer-transferred RHI binding map, implement descriptor/pipeline layouts,
   native library validation,
   graphics/compute pipelines, render passes/framebuffers, reuse keys, and
   bounded diagnostics.
6. **M5 Commands, queues, and synchronization**: Implement all applicable RHI
   recording commands, encoder transitions, copies/barriers, queue submission,
   events/fences/semaphores, CPU waits, in-flight retention, readback, shutdown,
   and native conformance/failure probes.
7. **M6a Presentation**: Attach Backend-owned `CAMetalLayer` to the borrowed
   GLFW Cocoa view and implement drawable lifecycle, resize/pause/recovery, and
   detach-before-window-destroy behavior.
8. **M2b Production Metal shader cooking**: Compose the cooker, DDC inputs,
   strict publication/loading, canonical arm64/x86_64 profiles, and Renderer
   conversion of Asset binding evidence into immutable RHI metadata. This is
   User Story 3 and completes M2 after the native pipeline consumer exists.
9. **M6b Shared demos**: Refactor demo composition to explicit backend-neutral
   selection and run triangle plus deferred through Metal without Renderer forks.
10. **M7 Native equivalence and hardening**: Add real GPU readback, Metal/Vulkan
   semantic and image comparisons, 3,000-frame/20-cycle visible validation,
   10,000-iteration lifecycle stress, deterministic diagnostics, ownership
   inspection, and cross-backend failure injection.
11. **M8 CI, evidence, and closeout**: Run the ten-job hosted build/cook matrix
   plus the physical M4 Pro arm64 and GitHub-hosted Intel x86_64 full native
   jobs.
   Archive tiered evidence and digests, rerun affected Vulkan/Asset regressions,
   update system documentation, and retain manual evidence only for visible
   desktop lifecycle acceptance.

## Complexity Tracking

No constitution violations require justification.
