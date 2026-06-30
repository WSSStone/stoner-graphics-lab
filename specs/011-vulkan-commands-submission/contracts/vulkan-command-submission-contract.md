# Contract: Vulkan Command Recording & Submission

**Feature**: 011-vulkan-commands-submission  
**Date**: 2026-06-30

This contract describes observable behavior for the Vulkan backend command recording and submission slice. Exact signatures are finalized during implementation while preserving existing RHI command, queue, synchronization, render pass, framebuffer, resource, lifecycle, and result contracts.

## Command Pool and Buffer Contract

Required behavior:

- A caller can create command buffers for supported queue types when the backend device is active.
- Command buffers expose compatible queue type, lifecycle state, and recorded command count.
- Begin, End, Reset, and recycling behavior follow explicit lifecycle transitions.
- Device shutdown invalidates owned command pools and command buffers.

Negative-path requirements:

- Unsupported queue types, exhausted command buffer capacity, and post-shutdown allocation return explicit failure.
- Begin while already recording, End before Begin, recording after End, Reset while Recording, Reset while Submitted, and use-after-shutdown return invalid-state.
- Failed lifecycle operations do not mutate unrelated recorded commands.

## Command Recording Contract

Required behavior:

- Recording accepts supported command categories only while the command buffer is Recording.
- Draw and indexed draw can be recorded as placeholder commands with missing-pipeline diagnostics.
- Compute dispatch can be recorded as a placeholder command with missing-pipeline diagnostics.
- Transfer commands validate resource lifecycle, ranges, regions, and queue capability.
- Barrier/layout commands record declarative intent and validate resource lifecycle, usage compatibility, and basic before/after state consistency.
- Recorded command summaries preserve command category and ordering for tests.

Negative-path requirements:

- Graphics commands outside an active render pass scope return invalid-state.
- Compute commands on non-compute-compatible queues return unsupported.
- Transfer or barrier commands with missing, invalidated, incompatible, or out-of-bounds resources return explicit failure.
- Full pipeline execution and full resource state tracking are not required in this phase.

## Render Pass and Framebuffer Contract

Required behavior:

- The backend can create minimal single-subpass render pass objects from valid existing RHI render pass descriptions.
- The backend can create minimal framebuffer objects from valid compatible texture attachments.
- BeginRenderPass establishes an active graphics scope for a command buffer.
- EndRenderPass closes the active graphics scope.

Negative-path requirements:

- Empty or unsupported render pass descriptions return explicit failure.
- Framebuffer creation rejects missing render pass, mismatched attachment count, mismatched format, invalid dimensions, unsupported sample count, invalid mip/array layer, or invalidated texture attachments.
- BeginRenderPass rejects invalidated render passes, invalidated framebuffers, incompatible framebuffers, non-recording command buffers, non-graphics queues, and nested scopes.
- EndRenderPass rejects missing active scope.
- Multi-subpass behavior and full pipeline compatibility are out of scope.

## Queue Submission Contract

Required behavior:

- A compatible queue accepts ended/executable command buffers.
- Real runtime execution is used when supported runtime submission is available.
- Deterministic fallback submission state is allowed when runtime execution is unavailable and diagnostics clearly report that no real GPU execution occurred.
- Successful submission updates queue observability and command buffer submission state.
- Wait semaphores are consumed, signal semaphores are signaled, and optional fences are signaled or associated according to available synchronization behavior.

Negative-path requirements:

- Missing, still-recording, never-recorded, reset, already consumed, incompatible, or invalidated command buffers are rejected.
- Failed submissions do not increment successful submission count.
- Submission after queue or device invalidation returns invalid-state.

## Completion Observation Contract

Required behavior:

- Queue wait-idle and completion observation report completed, not-ready, timeout, invalid-state, or failed outcomes explicitly.
- Deterministic fallback submissions complete immediately by default.
- Tests can configure not-ready or timeout outcomes for fallback completion waits.
- Completed submissions make submitted command buffers resettable.

Negative-path requirements:

- Waiting after queue, device, command buffer, or synchronization invalidation returns invalid-state.
- Timeout or not-ready injection must not be confused with real runtime GPU scheduling.
- Reset while work is still considered pending is rejected.

## Upload Scheduling Contract

Required behavior:

- Existing pending buffer upload requests can be scheduled into compatible command buffers.
- Existing pending texture upload requests can be scheduled into compatible command buffers.
- Upload scheduling records destination ranges or regions and retains staging-data association.
- Upload scheduling does not claim execution before submission completion.

Negative-path requirements:

- Missing upload records, non-pending uploads, already scheduled uploads, invalidated destination resources, invalid ranges or regions, and unavailable queue capabilities return explicit failure.
- Failed upload scheduling does not modify unrelated command records or destination resources.

## Device Integration Contract

Required behavior:

- Existing Vulkan device factories for command buffers, render passes, and framebuffers move from unsupported placeholders to backend command behavior.
- Existing queue, fence, semaphore, resource, descriptor, and upload staging behavior remains compatible with prior tests.
- Device shutdown invalidates queues, command pools, command buffers, pending submissions, render passes, framebuffers, and upload scheduling records.

Negative-path requirements:

- Creation or command operations after shutdown return invalid-state.
- Out-of-scope shader compilation, pipeline creation, full pipeline binding validation, render graph scheduling, multi-threaded command recording, and visible frame rendering remain explicit unsupported or absent behavior.

## Test Contract

Required coverage:

- Command buffer allocation success and unsupported/capacity/shutdown failures.
- Command lifecycle success and invalid transitions.
- Draw, indexed draw, dispatch, missing-pipeline diagnostics, transfer, barrier/layout, BeginRenderPass, EndRenderPass, and command ordering validation.
- Minimal render pass and framebuffer creation success and compatibility failures.
- Queue submission success, incompatible queue rejection, invalid command buffer rejection, submitted-count behavior, wait-idle behavior, and command buffer reset after completion.
- Deterministic fallback submission diagnostics and immediate completion.
- Injected not-ready and timeout completion outcomes.
- Upload scheduling success and invalid/missing/already-scheduled/invalidated-destination failures.
- Device shutdown invalidates command-related objects.
- Existing Core, RHI, Vulkan device/swapchain, and Vulkan resource management tests remain passing.
