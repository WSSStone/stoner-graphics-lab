# B05-S11: Graphics, Compute Pipeline, And Cache Fix

## Repair Target

This packet repairs:

- `CR001-B05-F010`: pipeline wrappers bypass device authority and capability
  checks;
- `CR001-B05-F011`: pipeline cache can reuse non-equivalent or stale pipelines;
- `CR001-B05-F012`: pipeline factories neither create native objects nor
  publish atomically.

The repair is one Vulkan pipeline ownership and cache-key migration. It does not
introduce Asset code, persistent pipeline disk caches, mesh shaders, ray tracing,
or command-buffer native execution.

## Device Authority And Binding

`FVulkanGraphicsPipeline` and `FVulkanComputePipeline` now have private
constructors, are created only by `FVulkanDevice`, retain their creating device
owner, and expose dependency validity checks. Command buffers validate that a
bound pipeline belongs to the command buffer's device and that retained shader
and layout dependencies are still valid before recording a compatible binding.

Graphics factory validation now rejects attachment formats absent from the
selected device capabilities.

## Cache Equivalence

Pipeline cache keys are built from a collision-safe serialized description that
includes runtime mode, shader bytecode, shader identity, entry point, interface
metadata, canonical layout declarations, render target state, and fixed-function
pipeline state. Cache hits are ignored when the retained shader or layout
dependencies have been invalidated.

## Native Pipeline Ownership

When `EnableNativeShaderRuntime` succeeds and the request uses native shader
modules from the same native context, the Vulkan RHI factories now create and
retain real graphics and compute pipeline bundles through
`FVulkanNativeContext`. Explicit wrapper invalidation and device shutdown release
the owned native pipeline resources.

Factory publication is ordered so native pipeline creation, wrapper allocation,
device tracking, and cache insertion either all publish or return a failure with
partial native state destroyed.

## Tests And Evidence

Maintained tests cover:

- private wrapper construction at compile time;
- foreign-device graphics and compute pipeline binding rejection;
- unsupported graphics attachment format rejection;
- complete shader-interface cache key distinction;
- delimiter-ambiguous shader identity and entry point distinction;
- stale dependency cache rejection;
- real native graphics and compute pipeline ownership;
- explicit native pipeline cleanup on invalidation and shutdown.

Fresh local evidence:

- `strict-debug`: passed at `2026-07-27T05:19:16+00:00`.
- Scoped maintained test binary with optional deferred native skipped:
  `STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1 Build/Mac/Debug/Tests/StonerTest`,
  exit code `0`.

The full `tests` gate still contains an unrelated optional Feature 019 deferred
native readback fluctuation that predates this packet. It remains for B08
characterization and must not be used to claim B05 verification.

## Finding State

- `CR001-B05-F010`: Fixed at `a62e0f1`, pending B05-S12 verification.
- `CR001-B05-F011`: Fixed at `a62e0f1`, pending B05-S12 verification.
- `CR001-B05-F012`: Fixed at `a62e0f1`, pending B05-S12 verification.

B05-S12 must independently verify parent/current behavior and gate evidence
before transitioning these findings to Verified.

