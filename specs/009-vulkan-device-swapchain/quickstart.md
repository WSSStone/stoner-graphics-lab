# Quickstart: Vulkan Device & Swapchain Backend

**Feature**: 009-vulkan-device-swapchain  
**Date**: 2026-06-30

This quickstart describes the expected verification flow after implementation tasks are generated and completed.

## Prerequisites

- Work from the repository root.
- Use the active feature directory `specs/009-vulkan-device-swapchain/`.
- Existing Core platform abstraction and RHI contracts are available.
- A Vulkan SDK or compatible platform Vulkan loader may be present. If absent, backend tests must report explicit unsupported status rather than crash.
- Presentation validation requires a valid Core platform window wrapper; headless device validation does not.

## Build

```bash
conda run -n godot scons
```

Expected result:

- Build exits with code 0.
- Vulkan backend source compiles when the configured platform has usable headers/libraries or a supported stub/unsupported path.
- Existing Core, RHI, Renderer, Application, and test targets remain buildable.

## Run Full Tests

```bash
Build/Mac/Debug/Tests/StonerTest
```

Expected result:

- Existing Core and RHI tests continue to pass.
- Vulkan backend tests pass in one of the explicit modes:
  - Supported runtime: headless backend device creation, capability query, queue creation, synchronization, and shutdown succeed.
  - Unsupported runtime: initialization reports unsupported/failed status without crash.
  - Presentation available: swapchain create/acquire/present/recreate tests pass.
  - Presentation unavailable: presentation tests report an explicit skip while headless device tests still run.

## Runtime Capability Checks

Check the test output for deterministic evidence of:

- Backend runtime availability or explicit unsupported reason.
- Validation diagnostic state: enabled, unavailable, or disabled.
- Adapter selection reason and rejected candidate reason when candidates are present.
- Queue support summary.
- Headless device initialization result.
- Presentation skip or swapchain validation result.

## Contract Checklist

Before closing the feature:

- Headless device initialization is the MVP path and does not require a window.
- Adapter selection uses required capability gates and deterministic scoring.
- Missing optional validation support does not fail device creation.
- Queue wait-idle works for created queues.
- Queue submit rejects missing or non-executable command buffers explicitly.
- Surface creation uses the existing Core platform window wrapper.
- Null, invalid, or unsupported window handles are rejected explicitly.
- Swapchain acquire/present/recreate works when a valid presentation environment exists.
- Unsupported runtime and unsupported presentation paths are recoverable test outcomes.
- Shutdown rejects subsequent creation requests.
- No Renderer/Application-facing contract depends on Vulkan-specific types.
