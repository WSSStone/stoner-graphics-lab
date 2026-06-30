# Research: Vulkan Device & Swapchain Backend

**Feature**: 009-vulkan-device-swapchain  
**Date**: 2026-06-30

## Decision: Treat headless device initialization as the MVP path

**Rationale**: The project already has RHI device contracts and a Core platform window wrapper, but not a full Application windowing lifecycle. Headless device initialization validates the first real backend without requiring a native window in every environment.

**Alternatives considered**:
- Require swapchain validation everywhere: rejected because it would make the first backend milestone depend on platform-window availability and CI display state.
- Defer device initialization until Application windowing exists: rejected because later Vulkan resource and command phases need a real backend device first.

## Decision: Gate presentation validation on a valid Core platform window wrapper

**Rationale**: The existing Core abstraction owns the opaque native window handle concept. Reusing it avoids a parallel surface descriptor and keeps Application/Backend integration aligned with the constitution.

**Alternatives considered**:
- Define a backend-specific presentation descriptor: rejected because it duplicates Core platform abstraction and risks leaking backend assumptions.
- Skip all swapchain work: rejected because roadmap Phase 008 explicitly includes swapchain creation/recreation.

## Decision: Use deterministic adapter selection with capability gates and scoring

**Rationale**: Required capability gates prevent unsuitable adapters from being chosen. Scoring gives deterministic behavior in multi-adapter machines while favoring discrete GPUs, stronger queue support, presentation support, and required formats.

**Alternatives considered**:
- First compatible adapter wins: rejected because adapter order can vary across systems and drivers.
- Caller selects adapter: rejected for this phase because no public selection UX or configuration exists yet.
- Always prefer presentation-attached adapter: rejected because headless MVP must remain valid and presentation may be unavailable.

## Decision: Validation layers are optional diagnostics

**Rationale**: Development validation is valuable, but missing validation support should not prevent backend initialization on machines or CI environments with only runtime support. The result should remain visible as diagnostics.

**Alternatives considered**:
- Make validation mandatory in debug: rejected because it would make supported runtime environments fail for a development-only aid.
- Defer validation entirely: rejected because debug diagnostics are an important backend integration signal.

## Decision: Queue submit rejects non-executable command buffers until command recording exists

**Rationale**: This phase must implement queue creation and wait-idle while explicitly avoiding command buffer allocation and recording. Returning explicit rejection preserves the RHI queue contract without inventing placeholder command execution.

**Alternatives considered**:
- Implement real command submission now: rejected because command buffer recording is roadmap Phase 010.
- Defer queue behavior entirely: rejected because queues are part of device readiness and capability validation.

## Decision: Keep buffer/resource/pipeline APIs out of Vulkan backend scope

**Rationale**: RHI resource and pipeline contracts exist, but real Vulkan resource allocation, shader modules, descriptors, and pipelines belong to later backend phases. Device methods for unsupported out-of-scope factories should return explicit unsupported status rather than fake usable objects.

**Alternatives considered**:
- Stub every RHI resource object with fake Vulkan wrappers: rejected because it would blur roadmap phase boundaries.
- Remove those factories from RHI temporarily: rejected because RHI resource contracts are already established.

## Decision: SCons should detect Vulkan SDK availability and allow explicit unsupported paths

**Rationale**: The project must compile across Windows, macOS, and Linux. Planning should allow build-time detection or configuration for Vulkan headers/libraries and test-time unsupported results when runtime or presentation support is unavailable.

**Alternatives considered**:
- Hard-require Vulkan SDK for all builds: rejected because it would break non-Vulkan development paths and unsupported CI environments.
- Vendor a fixed SDK immediately: rejected because dependency sourcing belongs in implementation tasks and may vary by platform.

## Decision: Keep backend diagnostics separate from RHI result codes

**Rationale**: RHI result codes communicate operation outcomes. Diagnostics such as validation availability, selected adapter reason, and runtime absence explain why outcomes occurred without overloading result enums.

**Alternatives considered**:
- Add many new RHI result values: rejected because that would leak backend-specific detail into cross-API contracts.
- Use only logs: rejected because tests need queryable diagnostics.
