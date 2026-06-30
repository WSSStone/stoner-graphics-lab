# Contract: Vulkan Device & Swapchain Backend

**Feature**: 009-vulkan-device-swapchain  
**Date**: 2026-06-30

This contract describes the expected behavior of the Vulkan backend device and swapchain slice. Exact function signatures are finalized during implementation while preserving these observable behaviors and keeping Renderer/Application-facing interactions on RHI/Core abstractions.

## Backend Initialization Contract

Required behavior:

- A caller can request backend initialization without a presentation surface.
- Supported environments return a success result and an active RHI device implementation.
- Unsupported environments return explicit unsupported or failed status within the success criteria window.
- Partial initialization failures leave no usable backend instance, device, queue, surface, sync object, or swapchain.
- Diagnostics expose whether optional development validation was enabled, unavailable, or disabled.

Negative-path requirements:

- Missing required runtime support returns unsupported or failed status.
- Missing compatible adapter returns unsupported status.
- Missing required queue capability returns unsupported status.
- Missing optional validation support does not fail initialization.

## Adapter Selection Contract

Required behavior:

- Enumerated adapter candidates record required capability gate results.
- Candidates that fail required gates are rejected before scoring.
- Compatible candidates receive deterministic scores.
- Selection prefers discrete GPUs, stronger queue support, presentation support, and required formats.
- The selected adapter identity and reason are available for diagnostics.

Negative-path requirements:

- No candidate passing gates returns unsupported.
- Multi-adapter selection is deterministic across repeated runs with the same candidate set.

## Device Contract

Required behavior:

- Device exposes RHI-visible capabilities for queue support, presentation support, synchronization support, frame count limits, and supported formats.
- Device creates supported queues, fences, semaphores, and swapchains.
- Device can shut down cleanly.
- Device rejects new creation requests after shutdown with invalid-state.
- Out-of-scope resource, descriptor, shader, pipeline, command buffer recording, and upload behavior returns explicit unsupported status when required by inherited RHI interfaces.

Negative-path requirements:

- Creation after shutdown returns invalid-state.
- Unsupported factory paths return unsupported rather than fake usable objects.
- Shutdown is idempotent or safely rejects invalid repeated transitions.

## Queue Contract

Required behavior:

- Queue exposes its RHI queue type.
- Queue exposes submitted command count.
- Queue supports wait-idle.
- Queue rejects missing or non-executable command buffer submission explicitly until command recording exists.

Negative-path requirements:

- Unsupported queue type creation returns unsupported.
- Queue requests after shutdown return invalid-state.
- Submit with missing/non-executable command buffer returns invalid-state or unsupported consistently.

## Synchronization Contract

Required behavior:

- Fence creation supports an initial signaled option.
- Fence wait returns success, not-ready, timeout, or invalid-state according to state.
- Fence reset/signal behavior follows the existing RHI synchronization contract.
- Semaphore signal, consume, and reset behavior follows the existing RHI synchronization contract.

Negative-path requirements:

- Fence and semaphore creation after shutdown returns invalid-state.
- Invalid signal/consume transitions return explicit status.

## Surface Contract

Required behavior:

- Surface creation uses the existing Core platform window wrapper as canonical input.
- Valid supported native handles can produce presentation surfaces.
- Missing, null, invalid, or unsupported native handles are rejected explicitly.
- Headless backend device validation does not require a surface.

Negative-path requirements:

- Invalid Core platform window wrapper returns invalid-state.
- Unsupported platform presentation bridge returns unsupported.
- Presentation tests may skip only when no valid wrapper or presentation bridge is available.

## Swapchain Contract

Required behavior:

- Swapchain creation requires an active device, valid presentation surface, compatible frame count, compatible format, compatible presentation mode, and presentation-capable queue support.
- Swapchain exposes frame count and current frame index.
- Acquire-next-frame transitions ready swapchain state to acquired.
- Present transitions acquired state back to ready and advances the frame index.
- Resize-required and unavailable states are explicit and recoverable through recreation when valid inputs return.

Negative-path requirements:

- Acquire twice before present returns invalid-state.
- Present without acquire returns invalid-state.
- Presenting a stale or wrong frame returns invalid-state.
- Missing or invalid surface returns invalid-state or unsupported.
- Creation after device shutdown returns invalid-state.

## Test Contract

Required coverage:

- Supported headless backend initialization, capability query, and shutdown.
- Unsupported runtime or no compatible adapter.
- Deterministic adapter selection with at least one rejected candidate and one selected candidate.
- Queue creation success and unsupported queue rejection.
- Queue wait-idle success and non-executable submit rejection.
- Fence and semaphore success and invalid-state paths.
- Swapchain creation/acquire/present/recreate when a valid Core platform window wrapper is available.
- Presentation skip path when no valid wrapper is available.
- Clean repeated create/destroy cycles.
- Existing Core, RHI core, and RHI resource/pipeline tests remain passing.
