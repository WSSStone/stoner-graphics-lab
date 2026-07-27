# B05-S10: Graphics, Compute Pipelines, And Cache Inspection

## Scope

This packet inspected one Feature 012 responsibility domain across eight
primary production files:

- `FVulkanGraphicsPipeline.h/.cpp`;
- `FVulkanComputePipeline.h/.cpp`;
- `FVulkanPipelineCache.h/.cpp`;
- the bounded pipeline validation/factory/shutdown sections of
  `FVulkanDevice.cpp`;
- the bounded pipeline binding sections of `FVulkanCommandBuffer.cpp`.

RHI graphics/compute descriptions, Feature 012 authority documents, maintained
RHI/Vulkan tests, and native-context pipeline call sites were read as
supporting context. No production source or maintained test changed.

## Authority And Invariants

Feature 012 FR-006 through FR-010, FR-013, FR-016 through FR-019, SC-001,
SC-004 through SC-008, and the pipeline/cache contracts require:

- an active device to create compatible, queryable graphics and compute
  pipelines and reject unsupported device capabilities;
- command buffers to reject incompatible, invalidated, or foreign pipeline
  state without changing unrelated recording state;
- real runtime objects to represent actual native pipeline ownership, while
  deterministic fallback remains explicit;
- process-local reuse keys to represent complete semantic equivalence;
- invalidated dependencies and failed creation to remain outside reusable
  successful state;
- allocation and publication failures to return explicit results without a
  usable partial object.

The verified B04 runtime rule remains binding: deterministic fallback must not
be relabeled as native availability.

## Findings

### CR001-B05-F010 - S2 Accepted

`FVulkanGraphicsPipeline` and `FVulkanComputePipeline` expose public
constructors and carry no creating-device identity. Their command-buffer bind
paths accept any `IRHIGraphicsPipeline` or `IRHIComputePipeline` with a valid
lifecycle; they do not require the Vulkan concrete type or compare the command
buffer owner.

The device factory validates shader/layout provenance, but graphics pipeline
creation checks only enum-level render-target validity. It does not compare
color or depth/stencil formats with the selected adapter capabilities. A
directly constructed, foreign-device, foreign-backend, or unsupported-format
pipeline can therefore bypass the authoritative creation/binding boundary.

### CR001-B05-F011 - S2 Accepted

The process-local key serializes a shader as stage, unescaped payload identity,
and entry point. It omits explicit shader interface metadata required by the
cache contract, and unrestricted `:` or `|` characters make different
payload/entry pairs capable of producing the same text key.

Cache hits check only the cached pipeline wrapper lifecycle. They do not check
the retained shader modules or layout. After a dependency is invalidated, a
corrected valid request with the same key can return the old valid wrapper
whose retained dependency is stale. Existing tests cover identical requests
and two fixed-function variants, but not semantic collisions or replacement
after dependency invalidation.

### CR001-B05-F012 - S2 Accepted

The RHI pipeline wrappers retain descriptions only; neither owns a native
context/token nor destroys a `VkPipeline`. `FVulkanDevice` rejects full
`RealRuntime` initialization, while native contexts create Vulkan pipelines
through separate non-RHI paths. Real graphics and compute pipeline objects are
therefore unreachable through the documented RHI factories.

Both factories also perform allocating key construction, description copy,
wrapper allocation, device tracking insertion, and cache insertion without
mapping allocation/capacity failure to `ERHIResult`. Cache insertion occurs
after the valid wrapper has already entered device tracking, so an exception
can escape with partially published ownership and inconsistent creation-limit
state.

## Existing Coverage

Maintained tests currently prove:

- valid fallback graphics and compute creation;
- selected fixed-function enum rejection and depth-attachment compatibility;
- shader/layout interface compatibility and provenance at creation;
- identical cache hits and two fixed-function key variants;
- configured creation limit;
- basic binding, invalidated pipeline rejection, and shutdown invalidation.

They do not prove:

- private pipeline construction or command-buffer owner provenance;
- selected-adapter attachment format rejection;
- complete, collision-safe shader/interface cache identity;
- stale dependency rejection on cache hits;
- allocation/cache publication rollback;
- RHI-created native graphics or compute pipeline ownership/destruction.

## Handoff To B05-S11

B05-S11 should repair F010-F012 as one pipeline factory/cache API migration:

1. make device-owned pipeline construction private, retain owner identity, and
   enforce provenance during binding;
2. validate render-target formats against selected-device capabilities;
3. replace ambiguous text keys with complete collision-safe semantic keys and
   reject stale cached dependencies;
4. make wrapper/tracking/cache publication failure-atomic with explicit
   results;
5. establish owner-safe native graphics and compute pipeline objects through
   the RHI factories when native support is enabled, without changing the
   runtime mode of unrelated fallback objects;
6. add maintained regressions for every repaired path.

The exact production source state had already passed strict Debug,
fallback-strict full tests, and strict Release during B05-S09. No additional
build was needed because this inspection changed no production or test source.
No debugger, custom probe, remote CI, or network action was used.
