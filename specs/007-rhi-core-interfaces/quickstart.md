# Quickstart: RHI Core Interfaces

**Feature**: 007-rhi-core-interfaces  
**Date**: 2026-06-25

## What This Feature Produces

This feature adds the engine's first stable RHI core contracts:

- Rendering device lifecycle and capability discovery.
- Queue type and format identities.
- Explicit result/status values for recoverable outcomes.
- Command buffer lifecycle and symbolic command recording.
- Command queue submission and idle observation.
- Fence and semaphore synchronization contracts.
- Headless/mock swapchain acquire, present, and resize-required behavior.
- Mock-based RHI core verification in the existing test executable.

## Prerequisites

- Core types, math, logging, and platform abstraction are complete.
- Current branch is `007-rhi-core-interfaces`.
- SCons 4.10.1 or newer is available.
- A supported C++20 compiler is available.

## Expected Public Include Usage

Focused headers:

```cpp
#include "RHI/IRHIDevice.h"
#include "RHI/IRHICommandBuffer.h"
#include "RHI/IRHICommandQueue.h"
#include "RHI/IRHIFence.h"
#include "RHI/IRHISemaphore.h"
#include "RHI/IRHISwapchain.h"
```

Aggregate header:

```cpp
#include "RHI/RHIMinimal.h"
```

Expected behavior:

- Public RHI headers compile without Backend, Renderer, Application, platform windowing, or graphics API headers.
- Consumers can use the contracts with mock implementations.
- Recoverable operation outcomes are reported through explicit result/status values.

## Build

From the repository root:

```bash
scons
```

If using the local conda environment that provides SCons:

```bash
conda run -n godot scons
```

Expected result:

- Core, RHI, Backend/Vulkan placeholder, Renderer, Application, and Tests build successfully.
- `Build/<Platform>/Debug/Tests/StonerTest` is generated.

## Run Verification

macOS Debug:

```bash
Build/Mac/Debug/Tests/StonerTest
```

Windows Debug:

```bash
Build\Win64\Debug\Tests\StonerTest.exe
```

Linux Debug:

```bash
Build/Linux/Debug/Tests/StonerTest
```

Expected result:

- Process exits with code `0`.
- Existing Core and platform tests still pass.
- RHI core tests report no failures.

## Manual Validation Checklist

1. Verify `RHI/RHIMinimal.h` exposes all new RHI core contracts.
2. Verify public RHI headers include Core only and do not include Backend, Renderer, Application, platform windowing, or graphics API headers.
3. Verify device capabilities report graphics, compute, transfer, and present queue support through deterministic mock data.
4. Verify device-owned creation succeeds for supported queues and fails clearly for unsupported or shutdown states.
5. Verify command buffer lifecycle matrix covers valid and invalid transitions.
6. Verify symbolic draw, dispatch, and barrier commands preserve ordering without concrete resource or pipeline validation.
7. Verify queue submission rejects incomplete, recording, or incompatible command buffers.
8. Verify fence wait, signal, and reset states are deterministic.
9. Verify semaphore ordering behavior is backend-neutral.
10. Verify headless swapchain acquire, present, unavailable, invalid-state, and resize-required statuses are covered.

## Cross-Platform Verification

Run the build and verification flow on:

- Windows with MSVC.
- macOS with Apple Clang or Clang.
- Linux with GCC or Clang.

The feature is complete only when public RHI behavior is consistent across all supported platforms.

## Troubleshooting

- If RHI includes fail, confirm headers live under `Source/RHI/Public/RHI/` and are included as `RHI/<Header>.h`.
- If downstream layers fail, confirm public RHI headers depend only on Core and standard language facilities.
- If command tests fail, verify state transitions return explicit status values instead of relying on assertions.
- If swapchain tests require a real window or graphics API, the test has escaped this phase's scope.
- If resource or pipeline validation appears in this phase, move it to the next RHI resource/pipeline feature.
