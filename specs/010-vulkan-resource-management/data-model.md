# Data Model: Vulkan Resource Management

**Feature**: 010-vulkan-resource-management  
**Date**: 2026-06-30

## Overview

This feature adds Vulkan backend resource entities that satisfy existing RHI resource and descriptor contracts while keeping allocation, fallback, descriptor pool, and upload staging details inside the Vulkan backend. The model tracks lifecycle, ownership, diagnostics, and deterministic failure conditions.

## Shared State Concepts

### Allocation Mode

**Values**:
- RealRuntime
- DeterministicFallback
- Failed

**Rules**:
- RealRuntime is used when backend runtime support is available and the resource path can be created.
- DeterministicFallback is valid when runtime support is unavailable but contract validation can still create a backend-owned resource representation.
- Failed records must not produce usable resources.

### Resource Lifecycle

```text
Uninitialized -> Valid -> Invalidated
Uninitialized -> Failed
Valid -> Released
```

**Rules**:
- Valid resources can be queried and used by descriptor and upload validation.
- Invalidated resources remain queryable but cannot be used for new descriptor updates or upload requests.
- Released resources are owned cleanup results after device shutdown or explicit invalidation.
- Failed resources never become usable.

### Upload Lifecycle

```text
Pending -> ConsumedByFutureCommandPhase
Pending -> Invalidated
```

**Rules**:
- This phase only creates Pending upload records.
- No upload record may claim that GPU execution occurred.
- Invalidated destination resources invalidate future use of the upload record.

## Entities

### 1. Backend Buffer

**Purpose**: Represents a backend-owned buffer satisfying the existing RHI buffer contract.

**Fields / Properties**:
- Buffer description.
- Size in bytes.
- Usage flags.
- Resource lifecycle state.
- Allocation record.
- Allocation diagnostics.

**Relationships**:
- Created by backend device.
- Owns or references one resource allocation record.
- Can be referenced by descriptor sets and upload requests.

**Validation Rules**:
- Description must be valid and supported.
- Creation after device shutdown returns invalid-state.
- Invalidated buffers cannot be used for descriptor updates or uploads.

### 2. Backend Texture

**Purpose**: Represents a backend-owned texture satisfying the existing RHI texture contract.

**Fields / Properties**:
- Texture description.
- Dimension, extent, mip levels, array layers, sample count, format, usage.
- Resource lifecycle state.
- Allocation record.
- Allocation diagnostics.

**Relationships**:
- Created by backend device.
- Owns or references one resource allocation record.
- Can be referenced by descriptor sets and texture upload requests.

**Validation Rules**:
- Description must be valid and supported by backend capabilities.
- Unsupported format, dimensions, usage combinations, mip levels, array layers, or sample counts return explicit failure.
- Invalidated textures cannot be used for descriptor updates or uploads.

### 3. Backend Sampler

**Purpose**: Represents texture sampling state satisfying the existing RHI sampler contract.

**Fields / Properties**:
- Sampler description.
- Resource lifecycle state.
- Unsupported mode diagnostics.

**Relationships**:
- Created by backend device.
- Can be referenced by descriptor sets directly or as part of combined texture-sampler bindings.

**Validation Rules**:
- Supported sampler descriptions create valid sampler objects.
- Unsupported mode combinations return unsupported.
- Creation after shutdown returns invalid-state.

### 4. Resource Allocation

**Purpose**: Captures ownership, mode, size, limits, and failure state for backend memory associated with buffers and textures.

**Fields / Properties**:
- Allocation mode: RealRuntime, DeterministicFallback, Failed.
- Requested size or estimated allocation footprint.
- Resource kind.
- Configured budget limit at time of allocation.
- Allocation-count limit at time of allocation.
- Failure or fallback reason.
- Released flag.

**Relationships**:
- Owned by one buffer or texture.
- Created by backend memory allocator.
- Reported by diagnostics and tests.

**Validation Rules**:
- Allocation-count or budget limits can force deterministic failure.
- Failed allocations cannot be attached to usable resources.
- Release is idempotent for cleanup paths.

### 5. Backend Memory Allocator

**Purpose**: Coordinates real or fallback resource allocation and deterministic test-limited failure behavior.

**Fields / Properties**:
- Current allocation count.
- Current allocated bytes estimate.
- Optional configured resource budget.
- Optional configured allocation-count limit.
- Runtime availability and fallback mode.
- Last allocation diagnostics.

**Relationships**:
- Owned by backend device.
- Produces resource allocation records for buffers and textures.

**Validation Rules**:
- Allocation requests after shutdown return invalid-state.
- Requests exceeding configured limits fail deterministically.
- Partial creation failure must release any temporary allocation record.

### 6. Descriptor Pool

**Purpose**: Represents configurable fixed descriptor set capacity.

**Fields / Properties**:
- Maximum descriptor set count.
- Current allocated descriptor set count.
- Exhaustion state.
- Lifecycle state.
- Move-only active reservation count; reservations are not publicly forgeable.

**Relationships**:
- Owned by backend device.
- Allocates backend descriptor sets.

**Validation Rules**:
- Allocating beyond capacity returns explicit failure.
- Exhaustion must not invalidate existing descriptor sets.
- Shutdown invalidates the pool and its descriptor sets.
- Every successful set owns exactly one reservation. Move transfers authority;
  invalidation, destruction, and failed factory bookkeeping release it once.

### 7. Descriptor Set

**Purpose**: Represents a backend descriptor set satisfying the existing descriptor set contract.

**Fields / Properties**:
- Set index.
- Pipeline layout reference.
- Bound resource records.
- Lifecycle state.
- Owning descriptor pool.

**Relationships**:
- Allocated from descriptor pool.
- References buffers, textures, samplers, or combined texture-sampler pairs.
- Depends on a pipeline layout contract.

**Validation Rules**:
- Set index must exist in the layout.
- Missing binding, wrong descriptor type, invalid array index, missing resource, or invalid resource lifecycle returns explicit failure.
- Existing bindings remain queryable after referenced resources invalidate and must report invalid resource lifecycle.

### 8. Bound Resource Record

**Purpose**: Captures a descriptor binding outcome.

**Fields / Properties**:
- Binding slot.
- Array index.
- Resource kind.
- Referenced resource lifecycle validity.
- Optional texture and sampler references for combined bindings.

**Relationships**:
- Owned by descriptor set.
- References backend buffer, texture, sampler, or combined texture-sampler.

**Validation Rules**:
- Querying a retained record must report the resource kind even when the referenced resource is invalidated.
- Updating a binding with an invalidated resource fails without changing unrelated records.

### 9. Upload Request

**Purpose**: Represents a pending CPU-visible upload for future command execution.

**Fields / Properties**:
- Upload kind: Buffer or Texture.
- CPU-visible staging data.
- Destination resource reference.
- Destination byte range or texture region.
- Pending lifecycle state.
- Rejection diagnostics.

**Relationships**:
- Created by backend device or upload staging service.
- References one destination buffer or texture.
- Consumed by a future command recording/submission phase.

**Validation Rules**:
- Source data must be present.
- Buffer upload ranges must fit within destination bounds, declare
  copy-destination usage, and exactly match the staged source byte count.
- Texture upload regions must fit the selected mip extent and array layer.
  Their exact source footprint is checked as width times height times depth
  times the shared RHI format byte width.
- Multisampled textures and resources without copy-destination usage are
  unsupported transfer paths in this staging contract.
- Invalidated destinations reject upload requests.
- Creating an upload request does not imply GPU execution.

### CR-001 Descriptor And Upload Ownership Amendment (2026-07-26)

- Descriptor pool reservations, descriptor sets, samplers, and upload records
  are created only through their owning validation boundary.
- Reservation state performs no secondary allocation after pool creation and
  is move-only. Pool object/control-block, set wrapper/control-block, and device
  tracking failures preserve a zero-net reservation count.
- Upload request wrapper, control-block, staging storage, and device tracking
  failures map to `Unavailable`; no partially usable request escapes.
- Exact format byte widths live in the RHI format contract and are shared by
  full texture allocation and subresource upload validation.

### 10. Resource Diagnostics

**Purpose**: Provides queryable reasons for fallback, unsupported descriptions, allocation failure, descriptor failure, and upload rejection.

**Fields / Properties**:
- Allocation mode.
- Fallback reason.
- Unsupported format or usage reason.
- Allocation failure reason.
- Descriptor update rejection reason.
- Descriptor pool exhaustion reason.
- Upload rejection reason.

**Relationships**:
- Updated by allocator, resource factories, descriptor pool/set, and upload staging.
- Queried by tests and future developer-facing diagnostics.

**Validation Rules**:
- Diagnostics explain outcomes but do not replace explicit result codes.
- Missing runtime can be diagnostic when deterministic fallback allocation succeeds.

## CR-001 Allocation Model Amendment (2026-07-26)

### Resource Allocation Ownership Ticket

- The ticket is move-only and contains kind, mode, failure, checked byte size,
  limits observed at creation, static diagnostic reason, allocator identity,
  allocator epoch, allocation ID, and released state.
- Moving transfers the only release authority and leaves the source inert.
- Release succeeds only when allocator identity and epoch match and the ticket
  remains live. Foreign, stale, moved-from, and repeated release attempts do not
  alter allocator counters.
- Ticket creation performs no secondary heap allocation, preserving the
  allocator's non-throwing failure contract.

### Checked Texture Footprint

- Each mip contributes `width * height * depth * arrayLayers * formatBytes *
  sampleCount`, with every multiplication and total addition checked before
  mutation.
- Format width is exact for every currently supported RHI format. Overflow or
  an unknown width produces an explicit allocation failure and a zero estimate.

### Fallback Buffer Mirror

- Logical buffer size and resident CPU mirror size are independent. The mirror
  grows only through the end of the largest successful upload.
- Unrepresentable or unavailable mirror growth returns `Unavailable`; existing
  mirrored bytes and resource lifecycle remain unchanged.
