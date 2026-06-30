# Data Model: Vulkan Command Recording & Submission

**Feature**: 011-vulkan-commands-submission  
**Date**: 2026-06-30

## Overview

This feature adds Vulkan backend command entities that satisfy existing RHI command, queue, synchronization, render pass, framebuffer, resource, and upload-staging contracts. The model tracks command lifecycle, recording summaries, render pass scope, submission diagnostics, fallback completion, upload scheduling, and shutdown invalidation.

## Shared State Concepts

### Command Buffer Lifecycle

```text
Idle -> Recording -> Executable -> Submitted -> Resettable -> Idle
Idle -> Invalidated
Recording -> Invalidated
Executable -> Invalidated
Submitted -> Invalidated
```

**Rules**:
- Idle command buffers can begin recording.
- Recording command buffers can accept valid commands and can end to become executable.
- Executable command buffers can be submitted to a compatible queue.
- Submitted command buffers cannot be reset until completion observation or wait-idle makes them resettable.
- Invalidated command buffers remain queryable but reject new work.

### Submission Mode

**Values**:
- RealRuntime
- DeterministicFallback
- Failed

**Rules**:
- RealRuntime is used when queue submission can be performed through the supported runtime path.
- DeterministicFallback is used when runtime execution is unavailable but command submission state can be validated.
- Failed submissions must not increment successful submission observability.

### Completion Observation State

**Values**:
- Pending
- Completed
- NotReady
- Timeout
- Invalidated
- Failed

**Rules**:
- Real runtime completion follows available synchronization behavior.
- Deterministic fallback submission completes immediately by default.
- Tests can inject not-ready or timeout outcomes to validate wait behavior.

## Entities

### 1. Command Pool

**Purpose**: Represents queue-compatible ownership for command buffer allocation, reset, recycling, and shutdown invalidation.

**Fields / Properties**:
- Compatible queue type.
- Maximum command buffer capacity.
- Allocated command buffer count.
- Lifecycle state.
- Runtime/fallback allocation diagnostics.

**Relationships**:
- Owned by backend device.
- Produces command buffers for one compatible queue type.

**Validation Rules**:
- Allocation after device shutdown returns invalid-state.
- Unsupported queue type or exhausted capacity returns explicit failure.
- Pool invalidation invalidates all owned command buffers.

### 2. Command Buffer

**Purpose**: Represents a recordable unit of backend work.

**Fields / Properties**:
- Compatible queue type.
- Lifecycle state.
- Recorded command summaries.
- Active render pass scope, if any.
- Submission mode and last submission diagnostics.
- Missing-pipeline diagnostic flag for placeholder draw/dispatch commands.

**Relationships**:
- Allocated by a command pool.
- Submitted by a compatible command queue.
- References recorded commands, render pass scope, resources, and upload scheduling records.

**Validation Rules**:
- Begin is valid only from Idle or Resettable state.
- End is valid only from Recording state and fails if render pass scope remains active.
- Reset is rejected while Recording or Submitted.
- Recording after End or after invalidation returns invalid-state.
- Queue mismatch returns unsupported during submission.

### 3. Recorded Command

**Purpose**: Captures command intent and ordering for deterministic validation.

**Fields / Properties**:
- Command kind: Draw, DrawIndexed, Dispatch, BufferCopy, TextureCopy, Barrier, LayoutTransition, BeginRenderPass, EndRenderPass, UploadSchedule.
- Ordering index.
- Referenced resource identities or weak references where applicable.
- Range, region, or dispatch/draw dimensions.
- Validation diagnostic reason.

**Relationships**:
- Owned by one command buffer.
- May reference buffers, textures, render pass/framebuffer objects, and upload records.

**Validation Rules**:
- Commands are accepted only during Recording state.
- Graphics commands require graphics-capable queue and active render pass scope.
- Compute commands require compute-capable queue.
- Transfer commands require transfer-compatible queue and valid source/destination ranges or regions.
- Failed command validation must not append a recorded command.

### 4. Declarative Barrier/Layout Intent

**Purpose**: Represents resource barrier and texture layout transition intent without full cross-command-buffer state tracking.

**Fields / Properties**:
- Resource reference.
- Before state or layout.
- After state or layout.
- Resource usage expectation.
- Diagnostic reason for rejection.

**Relationships**:
- Stored as a recorded command.
- References a backend buffer or texture.

**Validation Rules**:
- Resource must be valid and support the requested usage.
- Before and after values must be internally consistent for the command kind.
- Full per-resource state tracking across command buffers is out of scope.

### 5. Backend Render Pass

**Purpose**: Represents a minimal single-subpass backend render pass compatible with the existing RHI render pass description.

**Fields / Properties**:
- Render pass description.
- Attachment descriptions.
- Lifecycle state.
- Validation diagnostics.

**Relationships**:
- Created by backend device.
- Referenced by backend framebuffers and render pass scopes.

**Validation Rules**:
- At least one usable attachment is required.
- Attachment format, role, load/store behavior, and sample count must be supported by the backend capability model.
- Invalidated render passes cannot begin a render pass scope or create framebuffers.

### 6. Backend Framebuffer

**Purpose**: Represents a minimal backend framebuffer with attachments compatible with a backend render pass.

**Fields / Properties**:
- Framebuffer description.
- Width and height.
- Attachment references.
- Lifecycle state.
- Validation diagnostics.

**Relationships**:
- References one backend render pass.
- References texture attachments.
- Used by command buffers to begin render pass scope.

**Validation Rules**:
- Attachment count, formats, dimensions, sample count, mip level, array layer, and lifecycle must match render pass expectations.
- Invalidated textures, render passes, or framebuffers cannot be used for render pass scope.

### 7. Render Pass Scope

**Purpose**: Represents an active graphics recording region inside a command buffer.

**Fields / Properties**:
- Render pass reference.
- Framebuffer reference.
- Begin command index.
- Active flag.

**Relationships**:
- Owned by a command buffer while recording.
- Requires backend render pass and framebuffer objects.

**Validation Rules**:
- BeginRenderPass requires Recording state, graphics-capable queue, valid render pass, and compatible framebuffer.
- Nested render pass scopes are rejected.
- EndRenderPass requires an active scope.
- End() rejects command buffers with an unclosed scope.

### 8. Submission Batch

**Purpose**: Represents one accepted queue submission.

**Fields / Properties**:
- Queue type.
- Submitted command buffer references.
- Submission mode.
- Submission result.
- Completion observation state.
- Wait semaphore references.
- Signal semaphore references.
- Optional fence reference.
- Diagnostic reason.

**Relationships**:
- Created by command queue submission.
- References command buffers and synchronization objects.

**Validation Rules**:
- Command buffers must be executable and compatible with the queue.
- Wait semaphores must be consumable.
- Signal semaphores and fence must be signalable.
- Failed validation must not increment successful submission count.

### 9. Completion Observation

**Purpose**: Represents wait-idle, fence wait, or submission completion behavior.

**Fields / Properties**:
- Completion state.
- Timeout request, if any.
- Injected fallback outcome, if configured.
- Diagnostic reason.

**Relationships**:
- Associated with a submission batch, queue, command buffer, and optional fence.

**Validation Rules**:
- Fallback submissions complete immediately unless a test injection requests not-ready or timeout.
- Completion makes submitted command buffers resettable when work is considered done.
- Waiting after invalidation returns invalid-state.

### 10. Upload Scheduling Record

**Purpose**: Represents a pending upload request recorded into command work.

**Fields / Properties**:
- Upload kind: buffer or texture.
- Original staging data reference.
- Destination range or region.
- Scheduling lifecycle: Pending, Scheduled, Invalidated.
- Command buffer reference.
- Diagnostic reason.

**Relationships**:
- References one existing `FVulkanUploadRequest`.
- Stored as a recorded command when scheduling succeeds.

**Validation Rules**:
- Only Pending upload requests can be scheduled.
- Destination resource must remain valid and compatible.
- Scheduling does not claim execution before submission completion.
- Repeated scheduling of the same request is rejected unless explicitly reset by future behavior.

### 11. Command Diagnostics

**Purpose**: Captures deterministic reasons for command allocation, recording, submission, completion, render pass, barrier, and upload scheduling outcomes.

**Fields / Properties**:
- Allocation rejection reason.
- Recording rejection reason.
- Missing pipeline binding reason.
- Render pass/framebuffer rejection reason.
- Barrier/layout rejection reason.
- Submission mode and rejection reason.
- Completion fallback/injection reason.
- Upload scheduling rejection reason.

**Relationships**:
- Reported by command pools, command buffers, queues, render passes, framebuffers, upload scheduling, and device diagnostics.

**Validation Rules**:
- Diagnostics must distinguish unsupported runtime fallback from real execution.
- Diagnostics must remain queryable after invalidation where objects remain queryable.
