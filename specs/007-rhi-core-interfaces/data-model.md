# Data Model: RHI Core Interfaces

**Feature**: 007-rhi-core-interfaces  
**Date**: 2026-06-25

## Entity: Rendering Device

**Purpose**: Authoritative lifecycle owner and factory for RHI core objects.

**Fields / Properties**:
- Lifecycle state: uninitialized, active, inactive/shutdown.
- Device capabilities snapshot.
- Supported queue types.
- Object creation status for queues, command buffers, fences, semaphores, and headless/mock swapchains.

**Relationships**:
- Owns command queues, command buffers, fences, semaphores, and swapchains.
- Exposes device capabilities.

**Validation Rules**:
- Creation requests after shutdown return explicit invalid-state status.
- Unsupported queue or swapchain requests return explicit unsupported status.
- Capability queries remain safe before initialization and after shutdown.

## Entity: Device Capabilities

**Purpose**: Portable summary of queue support, limits, and feature flags used by consumers to choose rendering paths.

**Fields / Properties**:
- Supported queue types: graphics, compute, transfer, present.
- Feature flags: present support, compute support, transfer support, synchronization support.
- Limits: maximum in-flight frames, maximum command buffers per queue, maximum queues per type.
- Supported format set for core/presentation usage.

**Relationships**:
- Belongs to a rendering device.
- References format identities and queue classifications.

**Validation Rules**:
- Queue support must be internally consistent with object creation results.
- Limits must be deterministic and queryable in mock tests.

## Entity: Result / Status

**Purpose**: Explicit outcome for recoverable operations.

**Values**:
- Success.
- Invalid state.
- Unsupported.
- Timeout.
- Not ready.
- Resize required.
- Unavailable.
- Failed.

**Validation Rules**:
- Recoverable operation failures must return a status rather than crash.
- Tests must assert expected status for negative paths.

## Entity: Queue Type

**Purpose**: Portable classification of execution lanes.

**Values**:
- Graphics.
- Compute.
- Transfer.
- Present.

**Validation Rules**:
- Queue creation and submission must reject unsupported queue types.
- Queue compatibility must be checked before accepting command buffers.

## Entity: Command Buffer

**Purpose**: Unit of symbolic rendering or compute work.

**Lifecycle States**:
- Idle.
- Recording.
- Completed.
- Submitted.
- Resettable.

**Fields / Properties**:
- Current lifecycle state.
- Compatible queue type.
- Ordered symbolic command list.
- Completion/submission status.

**Relationships**:
- Created by a rendering device.
- Submitted to a compatible command queue.

**Validation Rules**:
- Begin is valid only from idle or resettable states.
- End is valid only while recording.
- Submit is valid only after completion.
- Recording after completion, ending without begin, double begin, and submitting while recording return invalid-state status.
- Symbolic commands do not validate concrete resources, pipeline objects, descriptors, or shaders.

## Entity: Symbolic Command

**Purpose**: Minimal command payload for lifecycle and ordering tests.

**Types**:
- Draw.
- Dispatch.
- Barrier.

**Validation Rules**:
- Commands can be appended only while the command buffer is recording.
- Commands preserve insertion order for test verification.
- Commands must not require concrete resource or pipeline references in this feature.

## Entity: Command Queue

**Purpose**: Execution lane that accepts completed compatible command buffers.

**Fields / Properties**:
- Queue type.
- Submitted command sequence.
- Idle/completion status.

**Relationships**:
- Created by a rendering device.
- Accepts command buffers compatible with its queue type.
- May observe fences and semaphores during submission.

**Validation Rules**:
- Submission of incomplete, recording, invalid, or incompatible command buffers returns explicit status.
- Wait idle reports completion deterministically in mock tests.

## Entity: Fence

**Purpose**: Observable completion primitive.

**Lifecycle States**:
- Unsignaled.
- Signaled.
- Waited.
- Reset.

**Validation Rules**:
- Wait on signaled fence succeeds.
- Wait on unsignaled fence may return not-ready or timeout status.
- Reset returns fence to unsignaled state.

## Entity: Semaphore

**Purpose**: Queue ordering primitive for submitted work.

**Lifecycle States**:
- Unsignaled.
- Signaled.
- Consumed.

**Validation Rules**:
- Queue submissions can reference semaphores without exposing backend handles.
- Invalid or unsupported semaphore use returns explicit status.

## Entity: Swapchain

**Purpose**: Headless/mockable presentation contract for frame acquisition and presentation.

**Fields / Properties**:
- Frame count.
- Current frame index.
- Presentation state.
- Resize-required flag.

**Relationships**:
- Created by a rendering device.
- Used by renderer-facing smoke flow.

**Validation Rules**:
- Acquire returns a frame index or explicit status.
- Present accepts a valid acquired frame or returns invalid-state status.
- Simulated resize/unavailable states return resize-required or unavailable status.
- No native window, platform surface, or backend surface binding is required.

## Entity: Format

**Purpose**: Portable identity for color, depth, stencil, and common data formats needed by capabilities and swapchain behavior.

**Validation Rules**:
- Format identities must be backend-neutral.
- Capability queries must state which relevant formats are supported.
