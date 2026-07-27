# Contract: RHI Resource & Pipeline API

**Feature**: 008-rhi-resource-pipeline  
**Date**: 2026-06-26

This contract describes the public RHI resource and pipeline behavior expected by consumers, mock implementations, and future backend implementations. Names below describe intended public concepts; exact signatures are finalized during implementation while preserving these behaviors.

## Shared Result and Lifecycle Contract

### Result handling

Resource and pipeline operations use the existing RHI explicit result/status convention.

Required behavior:

- Creation success returns a success status and a usable object.
- Invalid descriptions return explicit failure or invalid-state status.
- Unsupported formats, usage flags, shader stages, descriptor types, or attachment combinations return unsupported status where applicable.
- Recoverable validation failures do not require exceptions, logs, or debug-only assertions.

### Lifecycle

Required states:

- `Valid`
- `Invalidated`

Contract rules:

- Public resource/pipeline-family objects expose lifecycle state.
- Valid objects can be queried and used in compatible operations.
- Invalidated objects remain safe to query but cannot be used for descriptor updates, pipeline creation, render pass compatibility, or framebuffer creation.
- Device shutdown or explicit mock invalidation may transition created objects to Invalidated.

## Resource Description Contract

### Buffer resources

Required public concepts:

- `FRHIBufferDesc`
- `ERHIBufferUsage` or equivalent composable usage flag type
- `IRHIBuffer`

Required behavior:

- Describes size in bytes.
- Describes composable buffer usage intent, including vertex, index, uniform, storage, copy source, copy destination, and future-compatible categories.
- Exposes original description and lifecycle state.
- Rejects zero-size descriptions.
- Rejects explicitly incompatible usage combinations.
- Rejects Invalidated buffers in descriptor updates or compatibility checks.

### Texture resources

Required public concepts:

- `FRHITextureDesc`
- `ERHITextureDimension`
- `ERHITextureUsage` or equivalent composable usage flag type
- `IRHITexture`

Required behavior:

- Describes dimension type, width, height, depth, mip levels, array layers, sample count, format, and usage intent.
- Supports 1D, 2D, 3D, cube, and array texture descriptions.
- Exposes one exact byte width for every uncompressed RHI format; `Unknown`
  reports zero so allocation and upload validators share one format contract.
- Exposes original description and lifecycle state.
- Rejects zero dimensions required by the texture type.
- Rejects non-square cube face descriptions.
- Rejects invalid mip/layer counts.
- Rejects unsupported format/usage combinations.
- Rejects Invalidated textures in descriptor updates and framebuffer attachments.

### Samplers

Required public concepts:

- `FRHISamplerDesc`
- sampler filter and address mode enums
- `IRHISampler`

Required behavior:

- Describes filtering, address behavior, and comparison behavior.
- Exposes original description and lifecycle state.
- Rejects unsupported sampler mode combinations.
- Rejects Invalidated samplers in descriptor updates.

## Shader Module Contract

Required public concepts:

- `FRHIShaderModuleDesc`
- `ERHIShaderStage`
- `IRHIShaderModule`

Required behavior:

- Describes declared shader stage.
- Describes entry point identity.
- Describes opaque payload identity.
- May describe debug identity.
- Exposes original description and lifecycle state.
- Does not validate bytecode or require a shader compiler.
- Supports vertex, fragment/pixel, and compute stages in this phase.
- Future stages such as ray tracing and mesh shader stages are either represented as unsupported or explicitly rejected until their roadmap phases.

Negative-path requirements:

- Missing stage returns explicit failure.
- Missing entry point identity returns explicit failure.
- Missing payload identity returns explicit failure.
- Using a shader module in the wrong pipeline type returns explicit failure.

## Pipeline Layout and Descriptor Contract

### Descriptor bindings

Required public concepts:

- `ERHIDescriptorType`
- `FRHIDescriptorBinding`
- descriptor stage visibility flags

Required behavior:

- Describes set index.
- Describes binding slot.
- Describes descriptor type.
- Describes array count.
- Describes shader stage visibility.
- Requires unique binding slots within each set.
- Requires array count greater than zero.
- Requires at least one shader stage visibility bit.

### Pipeline layouts

Required public concepts:

- `FRHIPipelineLayoutDesc`
- `IRHIPipelineLayout`

Required behavior:

- Describes one or more descriptor set layouts.
- Uses set index + binding slot addressing.
- Exposes binding lookup by set index and binding slot for tests and consumers.
- Exposes lifecycle state.
- Rejects duplicate bindings within the same set.
- Rejects Invalidated layouts for descriptor set or pipeline creation.

### Descriptor sets

Required public concepts:

- `IRHIDescriptorSet`
- descriptor write/update descriptions

Required behavior:

- Is created for a specific pipeline layout and set index.
- Binds buffers, textures, samplers, and combined texture-sampler resources.
- Validates descriptor type compatibility.
- Validates array index bounds.
- Rejects missing bindings.
- Rejects Invalidated resources and Invalidated descriptor sets.
- Exposes enough query behavior for mock tests to verify bound resource category and binding count.

## Pipeline Contract

### Graphics pipelines

Required public concepts:

- `FRHIGraphicsPipelineDesc`
- graphics state value objects for vertex input, topology, rasterization, blending, depth-stencil, and render target compatibility
- `IRHIGraphicsPipeline`

Required behavior:

- References shader modules.
- References a pipeline layout.
- Describes vertex input behavior.
- Describes primitive topology.
- Describes rasterization behavior.
- Describes blending behavior.
- Describes depth-stencil behavior.
- Describes compatible render target or render pass information.
- Exposes original description and lifecycle state.

Negative-path requirements:

- Missing required shader stages return explicit failure.
- Duplicate incompatible stages return explicit failure.
- Missing or Invalidated pipeline layout returns explicit failure.
- Invalidated shader module returns explicit failure.
- Unsupported attachment format or incompatible render target state returns explicit failure.

### Compute pipelines

Required public concepts:

- `FRHIComputePipelineDesc`
- `IRHIComputePipeline`

Required behavior:

- References exactly one compute shader module.
- References one pipeline layout.
- Exposes original description and lifecycle state.

Negative-path requirements:

- Missing compute shader returns explicit failure.
- Non-compute shader stage returns explicit failure.
- Multiple compute stages return explicit failure.
- Missing or Invalidated pipeline layout returns explicit failure.

## Render Pass and Framebuffer Contract

### Render passes

Required public concepts:

- `FRHIRenderPassDesc`
- attachment description value objects
- attachment load/store behavior enums
- `IRHIRenderPass`

Required behavior:

- Describes a single-subpass render target flow.
- Describes color and optional depth-stencil attachment roles.
- Describes attachment formats and sample counts.
- Describes load and store behavior.
- Exposes original description and lifecycle state.
- Rejects empty render pass descriptions with no usable attachment roles.
- Rejects unsupported formats.
- Does not model full multi-subpass dependencies in this phase.

### Framebuffers

Required public concepts:

- `FRHIFramebufferDesc`
- `IRHIFramebuffer`

Required behavior:

- References a compatible render pass.
- References concrete texture attachments.
- Exposes width, height, attachment count, and lifecycle state.
- Validates attachment count against render pass expectations.
- Validates attachment formats against render pass descriptions.
- Validates dimensions and sample counts.
- Rejects Invalidated textures or Invalidated render passes.

## Device Factory Extension Contract

The existing RHI device contract is extended to create or reject:

- Buffers.
- Textures.
- Samplers.
- Shader modules.
- Pipeline layouts.
- Descriptor sets.
- Graphics pipelines.
- Compute pipelines.
- Render passes.
- Framebuffers.

Required behavior:

- Device remains authoritative owner/factory.
- Creation after device shutdown returns invalid-state status.
- Unsupported format, usage, stage, descriptor, or attachment requests return explicit status.
- Created objects follow the shared lifecycle contract.

## Aggregate Include Contract

### `RHIMinimal.h`

Required behavior:

- Includes the public RHI resource and pipeline contracts introduced by this feature.
- Remains usable by downstream tests and layers without including Backend, Renderer, Application, platform windowing, or graphics API headers.

## Mock Test Contract

Mock tests must validate:

- Valid and invalid buffer descriptions.
- Valid and invalid texture descriptions.
- Valid and invalid sampler descriptions.
- Shader module payload identity, entry point, stage, and negative paths.
- Multi-set pipeline layouts and binding lookup.
- Descriptor set writes, descriptor type compatibility, array bounds, and Invalidated resource rejection.
- Graphics pipeline creation and invalid stage/layout/render target combinations.
- Compute pipeline creation and non-compute rejection.
- Single-subpass render pass creation and invalid attachment descriptions.
- Framebuffer compatibility against render pass and texture attachments.
- Device-owned creation and shutdown-state rejection.
- Aggregate include isolation through `RHIMinimal.h`.
