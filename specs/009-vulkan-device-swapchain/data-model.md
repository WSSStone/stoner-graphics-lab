# Data Model: Vulkan Device & Swapchain Backend

**Feature**: 009-vulkan-device-swapchain  
**Date**: 2026-06-30

## Overview

This feature introduces Vulkan backend runtime entities that implement existing RHI device, queue, synchronization, and swapchain contracts. The data model captures ownership, selection, lifecycle, and failure states without expanding into resources, command buffers, shaders, descriptors, or pipelines.

## Shared State Concepts

### Backend Availability

**Values**:
- Available
- DeterministicFallback
- UnsupportedRuntime
- MissingRequiredCapability
- FailedInitialization

**Rules**:
- Available means native runtime ownership has actually been established.
- DeterministicFallback requires explicit caller opt-in and is never equivalent to Available.
- Unsupported runtime is a valid test outcome on machines without Vulkan or presentation support.
- Failed partial initialization must leave no usable backend device.

### Backend Object Lifecycle

```text
Uninitialized -> Ready -> Shutdown
Uninitialized -> Failed
Ready -> Unavailable
Ready -> ResizeRequired
```

**Rules**:
- Ready objects can be queried and used according to their RHI contract.
- Shutdown objects reject new creation or new work.
- Failed objects are never usable.
- Swapchains can enter ResizeRequired or Unavailable and be recreated when valid inputs return.

### Presentation Ownership

**Rules**:
- One device-owned presentation token identifies the lifetime and provenance
  of every surface created by that device.
- Surface value copies share one validity record; invalidating any copy,
  shutting down the owner, or destroying the owner invalidates them all.
- A surface-backed swapchain retains its associated surface and cannot become
  Ready, expose images, acquire, present, or recreate while that surface is
  invalid.
- Imported deterministic swapchain images belong to one generation. Successful
  recreation invalidates the prior image generation before exposing the next.
- Deterministic surface/image state is contract coverage only and is never
  native-execution evidence.

## Entities

### 1. Backend Instance

**Purpose**: Represents initialized backend runtime state needed before device selection.

**Fields / Properties**:
- Requested runtime mode: RealRuntime or DeterministicFallback.
- Runtime availability.
- Requested validation state.
- Enabled validation state.
- Required and optional runtime capability availability.
- Diagnostics for missing optional validation or missing required support.

**Relationships**:
- Owns physical adapter discovery.
- Precedes backend device creation.

**Validation Rules**:
- RealRuntime is the default request and cannot become Ready through synthetic adapter data.
- DeterministicFallback is test-only, explicit, and observable in availability and diagnostics.
- Required runtime support must be available before adapter selection can produce a usable device.
- Missing validation support is diagnostic, not fatal.

### 2. Physical Adapter Candidate

**Purpose**: Represents one discoverable graphics adapter before selection.

**Fields / Properties**:
- Owned, non-empty adapter identity for diagnostics.
- Device type classification.
- Required capability gate result.
- Queue support summary.
- Presentation support summary.
- Concrete supported-format set.
- Deterministic suitability score.
- Rejection reason when unsuitable.

**Relationships**:
- Produced by backend instance discovery.
- One selected candidate becomes the backend device source.

**Validation Rules**:
- Empty adapter identity fails the required capability gate before ordering.
- Candidate and selected identity/rejection values never borrow caller storage.
- Candidates that fail required capability gates cannot be selected.
- Compatible candidates are sorted deterministically by score, with discrete GPU preference and queue/presentation/format support considered.

### 3. Backend Device

**Purpose**: Implements the existing RHI device contract for this backend phase.

**Fields / Properties**:
- Device lifecycle state.
- Selected adapter identity.
- RHI-visible capabilities.
- Owned queue objects.
- Owned synchronization objects.
- Owned swapchains.
- Diagnostics snapshot.

**Relationships**:
- Created from one selected physical adapter candidate.
- Creates backend queues, fences, semaphores, surfaces, and swapchains.
- Rejects out-of-scope resource/pipeline factories with explicit unsupported status.

**Validation Rules**:
- Device is Ready only after all required initialization steps complete.
- Public format capabilities and format-gated factories consume the selected adapter's same concrete format set.
- Device shutdown invalidates or releases owned backend objects.
- Creation requests after shutdown return invalid-state.

### 4. Backend Queue

**Purpose**: Implements RHI command queue behavior for supported queue types.

**Fields / Properties**:
- Queue type.
- Compatibility capabilities.
- Submitted command count.
- Idle state.

**Relationships**:
- Owned by backend device.
- Associated with selected adapter queue support.

**Validation Rules**:
- Unsupported queue types cannot be created.
- Wait-idle succeeds for a valid queue.
- Submission rejects missing or non-executable command buffers until command recording exists.
- Queue cannot accept work after device shutdown.

### 5. Presentation Surface

**Purpose**: Represents a backend presentation target derived from the Core platform window wrapper.

**Fields / Properties**:
- Source Core platform window validity.
- Platform presentation availability.
- Surface lifecycle state.
- Shared device-owner provenance token.
- Shared validity record across compatibility-value copies and RHI object
  references.
- Diagnostics for unsupported or invalid handles.

**Relationships**:
- Created by backend device using a valid Core platform window wrapper.
- Required for swapchain creation.
- Invalidated by explicit surface invalidation, owner shutdown, or owner
  destruction.

**Validation Rules**:
- Missing, null, invalid, or unsupported window handles are rejected.
- Stale and foreign-device surfaces cannot create a swapchain.
- Failed output-parameter creation clears the destination surface.
- Presentation validation can be skipped when no valid wrapper exists, but headless device validation remains mandatory.

### 6. Swapchain

**Purpose**: Implements the existing RHI swapchain contract for presentation.

**Fields / Properties**:
- Frame count.
- Current frame index.
- State: Ready, Acquired, ResizeRequired, Unavailable.
- Compatible format and presentation mode summary.
- Associated presentation surface.
- Imported image wrappers for the current generation.
- Maximum supported frame count retained from the creating device.

**Relationships**:
- Created by backend device for a valid presentation surface.
- Uses backend synchronization and presentation queue capabilities.

**Validation Rules**:
- Swapchain creation requires mutually compatible device, surface, frame count, format, and presentation settings.
- Acquire twice before present returns invalid-state.
- Present without acquire or with a stale frame returns invalid-state.
- Semaphore-aware acquire and present preserve frame and synchronization state
  on every preflight failure.
- Invalidating the associated surface makes image access unavailable and
  prevents recreation against that surface.
- Successful recreation invalidates old imported images and increments the
  generation.
- ResizeRequired and Unavailable are explicit recoverable states.

### 7. Fence

**Purpose**: Implements RHI fence completion observation.

**Fields / Properties**:
- Fence state.
- Initial signaled option.
- Timeout/not-ready observation.

**Relationships**:
- Created by backend device.
- Used by future queue submission and frame pacing.

**Validation Rules**:
- Creation after shutdown returns invalid-state.
- Wait must return success, not-ready, timeout, or invalid-state explicitly.

### 8. Semaphore

**Purpose**: Implements RHI semaphore ordering for backend operations.

**Fields / Properties**:
- Semaphore state.
- Signal/consume readiness.

**Relationships**:
- Created by backend device.
- Used by future queue submission and presentation synchronization.

**Validation Rules**:
- Creation after shutdown returns invalid-state.
- Invalid signal/consume transitions return explicit status.

### 9. Validation Diagnostics

**Purpose**: Captures optional development validation status and backend selection reasoning.

**Fields / Properties**:
- Validation requested.
- Validation enabled.
- Validation unavailable reason.
- Selected adapter reason.
- Unsupported runtime reason.
- Presentation skip reason.

**Relationships**:
- Queryable from backend instance or backend device.
- Used by tests and developer-facing logs.

**Validation Rules**:
- Diagnostics must not replace explicit operation result codes.
- Missing optional validation cannot turn successful device initialization into failure.
