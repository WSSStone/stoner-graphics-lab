# B05-S10 Inspection Evidence

## Source State

- Inspection source: `b60df60`
- Production files changed: none
- Maintained tests changed: none
- Findings opened and accepted: F010, F011, F012

## Requirement Matrix

| Requirement cluster | Current evidence | Result |
|---|---|---|
| Device-owned compatible creation and binding | Public wrappers, no owner field, lifecycle-only command binding, no adapter format check | F010 |
| Deterministic equivalent reuse without stale state | Incomplete ambiguous shader key and hit check limited to wrapper lifecycle | F011 |
| Real runtime ownership and explicit failure results | Description-only wrappers, separate native authorities, uncaught multi-stage publication | F012 |

## Static Evidence

- Pipeline constructors are public and repository search finds no
  construction-authority guard.
- `BindGraphicsPipeline` and `BindComputePipeline` accept interface objects
  based only on recording context and pipeline lifecycle.
- `CreateGraphicsPipeline` does not call `Capabilities.SupportsFormat` for its
  color or depth/stencil attachments.
- `AppendShaderKey` omits `InterfaceMetadata` and uses unescaped delimiters.
- `FindGraphics` and `FindCompute` do not validate retained shader/layout
  lifecycle.
- `FVulkanGraphicsPipeline` and `FVulkanComputePipeline` own no native token or
  context; all `vkCreateGraphicsPipelines` call sites are outside the RHI
  factories, and no RHI compute pipeline creates a `VkPipeline`.
- Pipeline factory key/copy/wrapper/tracking/cache allocations have no catches
  or rollback.

## Maintained Coverage Gap

Existing tests cover ordinary fallback creation, selected validation failures,
basic cache variants, binding, creation limits, and shutdown. Searches found no
maintained assertion for:

- direct or foreign pipeline binding;
- adapter-unsupported pipeline attachment formats;
- interface metadata or delimiter cache collisions;
- replacement after cached dependency invalidation;
- factory/cache allocation rollback;
- native RHI graphics or compute pipeline creation and destruction.

## Boundaries

- The inspection remained within one responsibility domain and eight primary
  production files.
- Authority and RHI description files were supporting contract context.
- The B05-S09 gate records apply to this unchanged production source state.
- No debugger, custom executable, fault trigger, memory-check tool, remote CI,
  or network operation was used.
