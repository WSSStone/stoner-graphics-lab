# Contract: RHI Core API

**Feature**: 007-rhi-core-interfaces  
**Date**: 2026-06-25

This contract describes the public RHI core behavior expected by consumers, mock implementations, and future backend implementations. Names below describe intended public concepts; exact signatures are finalized during implementation while preserving these behaviors.

## Result and Status Contract

### `ERHIResult`

Required outcome categories:

- `Success`
- `InvalidState`
- `Unsupported`
- `Timeout`
- `NotReady`
- `ResizeRequired`
- `Unavailable`
- `Failed`

Contract rules:

- Recoverable operations return explicit status.
- Invalid lifecycle transitions return `InvalidState`.
- Unsupported queue, capability, or format requests return `Unsupported`.
- Swapchain resize simulation returns `ResizeRequired`.
- Tests must not depend on exceptions, logs, or debug-only assertions for recoverable outcomes.

## Capability Contract

### `ERHIQueueType`

Required queue classifications:

- Graphics
- Compute
- Transfer
- Present

### `ERHIFormat`

Required format categories:

- Unknown/Undefined.
- Common color formats.
- Common depth formats.
- Common stencil or depth-stencil formats.

### `FRHIDeviceCapabilities`

Required behavior:

- Reports supported queue types.
- Reports present/compute/transfer/synchronization feature support.
- Reports deterministic limits needed by tests.
- Reports supported core formats.

## Device Contract

### `IRHIDevice`

Required behavior:

- Exposes lifecycle state.
- Exposes immutable or safely queryable capabilities snapshot.
- Creates or rejects command queues by queue type.
- Creates or rejects command buffers for compatible queue usage.
- Creates or rejects fences, semaphores, and headless/mock swapchains.
- Owns lifetime of RHI core objects it creates.
- Reports explicit status for creation failures, unsupported capabilities, and invalid shutdown-state use.

Negative-path requirements:

- Creation after shutdown returns `InvalidState`.
- Unsupported queue type returns `Unsupported`.
- Querying capabilities before or after active use remains safe.

## Command Buffer Contract

### `IRHICommandBuffer`

Required lifecycle:

```text
Idle -> Recording -> Completed -> Submitted -> Resettable -> Recording
```

Required behavior:

- Begin recording.
- End recording.
- Reset when allowed.
- Append symbolic draw command while recording.
- Append symbolic dispatch command while recording.
- Append symbolic barrier command while recording.
- Expose lifecycle state for tests.
- Preserve symbolic command order for mock validation.

Negative-path requirements:

- Double begin returns `InvalidState`.
- End without begin returns `InvalidState`.
- Record after end returns `InvalidState`.
- Submit while recording returns `InvalidState`.
- Commands must not require concrete resources, pipeline state objects, descriptor sets, or shader modules in this phase.

## Command Queue Contract

### `IRHICommandQueue`

Required behavior:

- Exposes queue type.
- Accepts completed compatible command buffers.
- Rejects incomplete or incompatible command buffers.
- Supports wait-idle style completion observation.
- Records submission order in mock tests.
- Supports optional fence/semaphore references through backend-neutral contracts.

Negative-path requirements:

- Submit incomplete command buffer returns `InvalidState`.
- Submit incompatible queue work returns `Unsupported` or `InvalidState`, depending on whether the queue type is unsupported or the command buffer state is invalid.
- Wait idle remains deterministic in mock tests.

## Synchronization Contract

### `IRHIFence`

Required behavior:

- Observe signaled/unsignaled state.
- Wait with deterministic result.
- Reset to unsignaled.
- Transition to signaled when associated mock work completes.

Negative-path requirements:

- Waiting on an unsignaled fence without completion returns `NotReady` or `Timeout`.
- Resetting an invalid or destroyed fence returns explicit status.

### `IRHISemaphore`

Required behavior:

- Represents queue ordering dependency.
- Can be signaled or consumed by mock queue submission.
- Does not expose backend handles.

Negative-path requirements:

- Invalid or unsupported semaphore usage returns explicit status.

## Swapchain Contract

### `IRHISwapchain`

Required behavior:

- Headless/mockable only in this phase.
- Acquire next frame.
- Present acquired frame.
- Report resize-required, unavailable, invalid-state, and success outcomes.
- Expose frame count/current frame behavior for tests.

Negative-path requirements:

- Present without acquisition returns `InvalidState`.
- Acquire or present during simulated resize returns `ResizeRequired`.
- No native window, platform surface, or graphics backend surface is required.

## Aggregate Include Contract

### `RHIMinimal.h`

Required behavior:

- Includes the public RHI core contracts introduced by this feature.
- Remains usable by downstream tests and layers without including Backend, Renderer, Application, platform windowing, or graphics API headers.

## Mock Test Contract

Mock tests must validate:

- Device lifecycle and capability discovery.
- Device-owned object creation and rejection paths.
- Command buffer lifecycle matrix.
- Symbolic command ordering.
- Queue submission and wait-idle behavior.
- Fence/semaphore state transitions and negative paths.
- Headless swapchain acquire/present/resize-required behavior.
- Public RHI headers compile through `RHIMinimal.h`.
