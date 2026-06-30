# Quickstart: Vulkan Resource Management

**Feature**: 010-vulkan-resource-management  
**Date**: 2026-06-30

This quickstart describes the expected verification flow after implementation tasks are generated and completed.

## Prerequisites

- Work from the repository root.
- Use the active feature directory `specs/010-vulkan-resource-management/`.
- Existing Core, RHI resource/descriptor contracts, and Vulkan backend device/swapchain implementation are available.
- A Vulkan SDK or compatible runtime may be present. If absent, resource tests must use deterministic fallback allocation with diagnostics rather than crash or silently skip contract validation.
- Command recording and queue execution of uploads are not part of this feature.

## Build

```bash
conda run -n godot scons
```

Expected result:

- Build exits with code 0.
- Vulkan resource source compiles with real runtime/SKD support when configured or fallback allocation support when unavailable.
- Existing Core, RHI, Renderer, Application, and test targets remain buildable.

## Run Full Tests

```bash
Build/Mac/Debug/Tests/StonerTest
```

Expected result:

- Existing Core, RHI core, RHI resource/pipeline, and Vulkan device/swapchain tests continue to pass.
- Vulkan resource tests pass in explicit modes:
  - Real runtime mode: buffers and textures report real allocation diagnostics when supported.
  - Fallback mode: buffers and textures report deterministic fallback allocation diagnostics when runtime support is unavailable.
  - Allocation-failure mode: configured resource budget or allocation-count limits trigger explicit failures and cleanup validation.
  - Descriptor mode: fixed-capacity descriptor pools allocate valid sets and report exhaustion.
  - Upload staging mode: upload records preserve CPU-visible staging data and destination ranges without command execution.

## Runtime Capability Checks

Check test output or diagnostic assertions for:

- Resource allocation mode: real runtime or deterministic fallback.
- Allocation failure reason for configured budget/count limits.
- Unsupported buffer, texture, sampler, descriptor, and upload rejection reasons.
- Descriptor pool capacity and exhaustion state.
- Retained descriptor binding records after resource invalidation.
- Pending upload request state and destination range/region metadata.

## Contract Checklist

Before closing the feature:

- Buffer creation succeeds for at least one valid description and rejects invalid descriptions.
- Texture creation succeeds for at least one valid description and rejects invalid dimensions, formats, usage, mip, array, or sample count cases.
- Sampler creation succeeds for at least one valid description and rejects unsupported mode combinations.
- Real-or-fallback allocation diagnostics are queryable.
- Allocation failure is deterministically triggered by configured budget or allocation-count limits.
- Device shutdown invalidates owned buffers, textures, samplers, descriptor pools, descriptor sets, and upload records.
- Descriptor set allocation validates set index and pool capacity.
- Descriptor updates validate binding type, array index, resource lifecycle, and retained invalidated bindings.
- Upload staging records CPU-visible data and destination ranges without claiming execution.
- No Renderer/Application-facing contract depends on Vulkan-specific allocation details.
