# B05-S12: Graphics, Compute Pipeline, And Cache Verification

## Verification Target

This packet independently verifies the repairs committed at `00751c7`:

- `CR001-B05-F010`: pipeline wrappers bypassed device authority and capability
  checks;
- `CR001-B05-F011`: pipeline cache could reuse non-equivalent or stale
  pipelines;
- `CR001-B05-F012`: pipeline factories neither created native objects nor
  published atomically.

No production source or maintained test implementation changed during this
verification packet.

## Parent Reproduction

The exact implementation parent `00751c7^` retained all three defect shapes:

- `FVulkanGraphicsPipeline` had a public constructor in
  `FVulkanGraphicsPipeline.h`, allowing construction outside `FVulkanDevice`.
- `FVulkanPipelineCache.cpp` had `AppendShaderKey` and cache lookup code without
  `InterfaceMetadata` serialization or `HasValidDependencies` hit checks.
- `FVulkanCommandBuffer.cpp` bound pipeline interfaces without checking pipeline
  device ownership or retained dependency validity.
- The only `vkCreateGraphicsPipelines` call sites in the parent belonged to
  standalone native context/session paths, not RHI pipeline factories; no
  RHI-owned compute pipeline creation path existed.

## Current Verification

Current HEAD closes the public construction path with private graphics/compute
pipeline constructors and `FVulkanDevice` friendship. Wrappers retain owner
provenance, native context, native token, and dependency validity helpers.

Command buffer binding now rejects foreign-device graphics and compute pipelines
and invalidated retained dependencies. Graphics pipeline creation validates
attachment formats through `FVulkanDevice::SupportsGraphicsPipelineDesc`.

The cache key now serializes runtime mode, shader bytecode, shader identity,
entry point, shader interface metadata, canonical layout declarations, render
target state, and fixed-function state using length-delimited records. Cache
hits require live retained shader/layout dependencies.

Native graphics and compute pipeline bundles are created and destroyed through
`FVulkanNativeContext`, and device factory failure paths release native tokens
before returning.

## Local Gate Evidence

Fresh local gates:

- `strict-debug`: passed at `2026-07-27T05:26:54+00:00`.
- `fallback-strict`: passed at `2026-07-27T05:27:34+00:00`, including full
  tests under `graphics=disabled`.
- `strict-release`: passed at `2026-07-27T05:28:00+00:00`.
- `sanitizers`: passed at `2026-07-27T05:29:07+00:00` with
  `STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1`.

The sanitizer run reached the maintained native pipeline checks:

- `Vulkan RHI pipeline factories retain real native graphics and compute pipelines`
- `Vulkan RHI device destroys native shader and pipeline ownership on explicit invalidation and shutdown`

It also covered the cache and binding regressions:

- `Vulkan pipeline cache key includes complete shader interface metadata`
- `Vulkan pipeline cache key length-prefixes shader identities and entry points`
- `Vulkan pipeline cache rejects entries with invalidated retained dependencies`
- `Vulkan graphics pipeline rejects attachment formats absent from device capabilities`
- `Vulkan pipeline and descriptor factories reject foreign shader and layout provenance`

## Known Gate Boundary

The formal `tests` gate was rerun at `2026-07-27T05:30:11+00:00` and failed
only on the pre-existing optional Feature 019 deferred native readback checks:

- `Deferred native validation completes a real Vulkan submission`
- `Mapped attachment probes are finite, unique, and within semantic tolerances`
- `Deferred native validation passes semantic probes and releases frame-owned objects`

The same run reported the B05 Vulkan pipeline/cache native integration checks as
passing. This failure remains assigned to the already-open B08 native validation
boundary, not to the B05 pipeline/cache repair.

## Finding Decisions

- `CR001-B05-F010`: Verified.
- `CR001-B05-F011`: Verified.
- `CR001-B05-F012`: Verified.

No new B05 finding was opened. `B05-S13` is the next packet.

