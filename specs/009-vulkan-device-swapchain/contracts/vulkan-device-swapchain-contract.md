# Contract: Vulkan Device & Swapchain Backend

**Feature**: 009-vulkan-device-swapchain  
**Date**: 2026-06-30

This contract describes the expected behavior of the Vulkan backend device and swapchain slice. Exact function signatures are finalized during implementation while preserving these observable behaviors and keeping Renderer/Application-facing interactions on RHI/Core abstractions.

## CR-001 Runtime Truthfulness Amendment

- The default `FVulkanDevice` request means real runtime ownership. It returns
  explicit unsupported status until this abstraction owns native Vulkan
  instance/device resources; native execution remains proven by
  `FVulkanNativeContext` in the current architecture.
- Deterministic fallback is an explicit opt-in test mode. It may expose an
  active deterministic device, but its availability and runtime diagnostics
  must never equal those of a real runtime.
- Adapter names, rejection reasons, and selected identity are owned values.
  Empty identities fail the required gate before deterministic ordering.
- Each adapter carries a concrete supported-format set. Public device
  capabilities and format-gated factories use that exact selected set.

## CR-001 Presentation Lifecycle Amendment

- The current RHI-facing creation path uses `IRHIPresentationSurface` and
  `FRHISwapchainDesc`. Backend-specific `FVulkanSurface` methods are
  compatibility adapters, not the authoritative Renderer/Application API.
- A surface carries shared device-owner provenance and validity. Device
  shutdown, explicit invalidation, or owner destruction makes every copy of
  that surface unusable. A different device cannot create a swapchain from it.
- A surface-backed deterministic swapchain owns imported image wrappers with
  the requested extent and color format. Recreation invalidates the old image
  generation before publishing replacements.
- Semaphore-aware acquire and present perform complete preflight before
  mutating frame or semaphore state. A failed operation preserves all inputs
  and observable state.
- Surface loss reports unavailable and blocks image access and recreation
  until the caller supplies a newly valid presentation target. Deterministic
  images and synchronization never prove native Vulkan presentation.
- Failed output factories leave no usable output. Zero frame count, zero
  extent, missing/invalid format, and depth presentation format are
  invalid-state; supported-domain requests beyond device capability are
  unsupported.

## Backend Initialization Contract

Required behavior:

- A caller can request backend initialization without a presentation surface.
- Supported environments return a success result and an active RHI device implementation.
- Unsupported environments return explicit unsupported or failed status within the success criteria window.
- Partial initialization failures leave no usable backend instance, device, queue, surface, sync object, or swapchain.
- Diagnostics expose whether optional development validation was enabled, unavailable, or disabled.
- Diagnostics distinguish real runtime, deterministic fallback, and unsupported real-runtime requests.

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
- Selected identity and rejection data remain stable after caller-owned discovery storage changes or is destroyed.

Negative-path requirements:

- No candidate passing gates returns unsupported.
- Multi-adapter selection is deterministic across repeated runs with the same candidate set.

## Device Contract

Required behavior:

- Device exposes RHI-visible capabilities for queue support, presentation support, synchronization support, frame count limits, and supported formats.
- Device format capabilities and resource acceptance are derived from the same selected-adapter format set.
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
- Surface creation is exposed through `IRHIPresentationSurface`; backend
  compatibility adapters share the same lifecycle state.
- Valid supported native handles can produce presentation surfaces.
- Missing, null, invalid, or unsupported native handles are rejected explicitly.
- Surfaces retain device provenance and become invalid when their owner shuts
  down or when any shared copy is invalidated.
- Headless backend device validation does not require a surface.

Negative-path requirements:

- Invalid Core platform window wrapper returns invalid-state.
- Unsupported platform presentation bridge returns unsupported.
- Presentation tests may skip only when no valid wrapper or presentation bridge is available.

## Swapchain Contract

Required behavior:

- Swapchain creation requires an active device, valid presentation surface, compatible frame count, compatible format, compatible presentation mode, and presentation-capable queue support.
- Swapchain exposes frame count and current frame index.
- Surface-backed swapchains expose one imported RHI texture per image and
  invalidate the prior image generation during recreation or shutdown.
- Acquire-next-frame transitions ready swapchain state to acquired.
- Present transitions acquired state back to ready and advances the frame index.
- Semaphore-aware acquire/present commits synchronization and frame state
  atomically.
- Resize-required and unavailable states are explicit and recoverable through recreation when valid inputs return.

Negative-path requirements:

- Acquire twice before present returns invalid-state.
- Present without acquire returns invalid-state.
- Presenting a stale or wrong frame returns invalid-state.
- Missing, invalid, stale, or foreign-device surface returns invalid-state.
- Surface loss returns unavailable and prevents recreation against the lost
  surface.
- Malformed swapchain descriptions return invalid-state; valid but unsupported
  capability requests return unsupported.
- Creation after device shutdown returns invalid-state.

## Test Contract

Required coverage:

- Supported headless backend initialization, capability query, and shutdown.
- Default real-runtime rejection until `FVulkanDevice` owns native resources, plus explicit deterministic-fallback initialization and diagnostics.
- Owned/null-safe adapter identity and selected-adapter-specific format acceptance/rejection.
- Unsupported runtime or no compatible adapter.
- Deterministic adapter selection with at least one rejected candidate and one selected candidate.
- Queue creation success and unsupported queue rejection.
- Queue wait-idle success and non-executable submit rejection.
- Fence and semaphore success and invalid-state paths.
- Swapchain creation/acquire/present/recreate when a valid Core platform window wrapper is available.
- Presentation skip path when no valid wrapper is available.
- Clean repeated create/destroy cycles.
- Existing Core, RHI core, and RHI resource/pipeline tests remain passing.
