# Contract: Vulkan Resource Management

**Feature**: 010-vulkan-resource-management  
**Date**: 2026-06-30

This contract describes observable behavior for the Vulkan backend resource management slice. Exact signatures are finalized during implementation while preserving existing RHI device, resource, descriptor, lifecycle, and result contracts.

## Resource Creation Contract

Required behavior:

- A caller can create backend buffers from valid buffer descriptions when the backend device is active.
- A caller can create backend textures from valid texture descriptions when the backend device is active.
- A caller can create backend samplers from valid sampler descriptions when the backend device is active.
- Created resources preserve their RHI descriptions and lifecycle state.
- Created buffers and textures expose allocation mode diagnostics: real runtime or deterministic fallback.
- Unsupported runtime may still create deterministic fallback resources when the resource contract can be validated.

Negative-path requirements:

- Invalid buffer, texture, or sampler descriptions return explicit invalid-state or unsupported results.
- Resource creation after device shutdown returns invalid-state.
- Unsupported formats, usage combinations, dimensions, mip levels, array layers, sample counts, and sampler modes return explicit failures.
- Failed or partial creation paths leave no usable partial resource.

## Allocation Contract

Required behavior:

- Buffer and texture allocation produces an allocation record for successful resources.
- Allocation records identify real-runtime, fallback, failed, and released states.
- Tests can configure resource budget or allocation-count limits to trigger deterministic failure.
- Device shutdown releases or invalidates owned resource allocations.

Negative-path requirements:

- Allocation failure returns explicit failed or unavailable result without exposing a usable resource.
- Budget and allocation-count exhaustion must be deterministic and repeatable.
- Partial creation cleanup must release any temporary allocation ownership.
- Repeated cleanup must be safe.

## Descriptor Pool Contract

Required behavior:

- Descriptor pools expose configurable fixed capacity.
- Descriptor sets allocate only from available pool capacity.
- Existing descriptor sets remain valid when the pool later reaches capacity.

Negative-path requirements:

- Allocating a descriptor set after capacity is exhausted returns explicit failure.
- Allocating a descriptor set for a missing layout set returns invalid-state.
- Descriptor pool invalidation or device shutdown prevents new allocations.

## Descriptor Set Contract

Required behavior:

- Descriptor sets expose set index, pipeline layout reference, bound resource kind, bound resource count, and lifecycle state.
- Descriptor updates support buffer, texture, sampler, and combined texture-sampler resources according to declared layout bindings.
- Bound resource records remain queryable after the referenced resource is invalidated.
- Retained bound resource records report when the referenced resource is no longer valid.

Negative-path requirements:

- Missing bindings, wrong descriptor types, invalid array indices, missing resources, invalidated resources, invalidated layouts, and post-shutdown updates return explicit failure.
- Failed updates do not change unrelated binding records.
- Invalidated descriptor sets reject further updates.

## Upload Staging Contract

Required behavior:

- Buffer upload requests validate source data and destination byte ranges.
- Texture upload requests validate source data, destination region, and format compatibility expectations.
- Valid upload requests record CPU-visible staging data, destination resource, destination range or region, and pending execution state.
- Upload staging does not claim GPU execution or queue submission.

Negative-path requirements:

- Missing source data, out-of-bounds ranges, incompatible texture regions, invalidated destination resources, and unsupported transfer paths return explicit failure.
- Failed upload requests do not modify destination resource lifecycle or pretend resource contents changed.
- Upload records become non-usable when their destination resource is invalidated.

## Device Integration Contract

Required behavior:

- Existing Vulkan backend device resource factories for buffers, textures, samplers, and descriptor sets are replaced with resource-management behavior in this phase.
- Existing unsupported behavior remains for command buffers, shader modules, pipeline layouts where not required for descriptor validation, graphics pipelines, compute pipelines, render passes, framebuffers, command execution, and render graph behavior.
- Device shutdown invalidates queues, synchronization, swapchains, buffers, textures, samplers, descriptor pools, descriptor sets, and pending upload requests.

Negative-path requirements:

- Creation requests after shutdown return invalid-state.
- Unsupported out-of-scope factory paths remain explicit unsupported results rather than fake usable objects.
- Existing Vulkan device/swapchain behavior remains passing.

## Test Contract

Required coverage:

- Buffer creation success and invalid/unsupported buffer descriptions.
- Texture creation success and invalid/unsupported texture descriptions.
- Sampler creation success and unsupported sampler mode combinations.
- Real-runtime or fallback allocation diagnostics.
- Allocation failure through configured budget and allocation-count limits.
- Partial creation cleanup and device shutdown invalidation.
- Descriptor set allocation success, missing set rejection, and pool exhaustion.
- Descriptor updates for buffer, texture, sampler, and combined texture-sampler resources.
- Descriptor update failures for wrong type, missing binding, invalid array index, invalidated resource, and post-shutdown state.
- Retained descriptor binding query after referenced resource invalidation.
- Buffer upload staging success and out-of-bounds/missing-data/invalidated-destination failures.
- Texture upload staging success and invalid-region/incompatible-format/invalidated-destination failures.
- Existing Core, RHI, and Vulkan backend tests remain passing.

## CR-001 Hardening Addendum (2026-07-26)

- Allocation accounting and texture footprint arithmetic are checked before
  state mutation. Overflow returns an explicit unavailable allocation result.
- Allocation records are move-only, allocator- and epoch-bound release tickets.
  Cross-allocator, stale, moved-from, and repeated release attempts fail without
  changing counters.
- Buffer and texture implementation wrappers are device-factory-only. Factory
  bookkeeping failure rolls ownership back before returning.
- Texture footprint includes exact format width, every mip extent, depth, array
  layers, and sample count.
- Host-visible fallback upload storage grows only to the uploaded range and maps
  storage allocation failure to `Unavailable` without throwing through `Upload`.
- Shutdown after successful extreme-size fallback allocation reports zero live
  allocations and zero allocated bytes.
