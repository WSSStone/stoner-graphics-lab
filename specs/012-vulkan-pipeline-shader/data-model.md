# Data Model: Vulkan Pipeline & Shader

**Feature**: 012-vulkan-pipeline-shader  
**Date**: 2026-06-30

## Shader Module

Represents precompiled shader bytecode for one shader stage.

Fields:

- `Stage`: declared shader stage; supported initial values are vertex, fragment, and compute.
- `EntryPoint`: non-empty entry point name.
- `PayloadIdentity`: deterministic identity for the bytecode payload.
- `Bytecode`: byte payload or structural bytecode summary used for validation.
- `InterfaceMetadata`: declared resource binding and small constant-data requirements.
- `ValidationMode`: real-runtime validation or fallback structural validation.
- `RuntimeMode`: real-runtime object or deterministic fallback object.
- `LifecycleState`: valid or invalidated.
- `Diagnostics`: latest creation, validation, unsupported, or invalidation reason.

Validation rules:

- Stage must be supported for this phase.
- Entry point and payload identity must be non-empty.
- Bytecode must pass lightweight structural checks in fallback mode.
- Interface metadata must be internally valid and compatible with the declared shader stage.
- Creation after device shutdown returns invalid-state.

State transitions:

- `Created -> Valid`
- `Valid -> Invalidated`
- `CreationRejected -> no usable object`

## Shader Interface Metadata

Represents shader-declared resource interface requirements before reflection exists.

Fields:

- `Bindings`: set index, binding slot, descriptor type, array count, and visible shader stages.
- `ConstantRanges`: optional small constant-data ranges with offset, size, and visible stages.
- `DebugName`: optional human-readable identifier for diagnostics.

Validation rules:

- Duplicate set/binding pairs are rejected.
- Descriptor array counts must be non-zero.
- Binding stage visibility must include the shader module's declared stage when required.
- Constant ranges must have non-zero size and must not overlap for incompatible stage usage.

Relationships:

- Referenced by `Shader Module`.
- Validated against `Pipeline Layout` during graphics and compute pipeline creation.

## Pipeline Layout

Represents the resource binding contract for graphics and compute pipelines.

Fields:

- `Bindings`: descriptor layout entries grouped by set index.
- `ConstantRanges`: small constant-data ranges accepted by the layout.
- `SetCount`: number of declared descriptor sets.
- `LifecycleState`: valid or invalidated.

Validation rules:

- Descriptor bindings must be valid and non-duplicated.
- Constant ranges must match shader interface metadata requirements.
- Invalidated layouts cannot create pipelines or accept descriptor sets.

Relationships:

- Referenced by `Graphics Pipeline`, `Compute Pipeline`, `Descriptor Set`, and command binding state.

## Graphics Pipeline

Represents triangle-ready drawable pipeline state.

Fields:

- `ShaderModules`: valid vertex and fragment shader modules.
- `PipelineLayout`: valid compatible layout.
- `RenderTargetCompatibility`: color formats, optional depth/stencil format, and sample count.
- `VertexInput`: stride and attributes.
- `PrimitiveTopology`: triangle-ready topology, with invalid values rejected.
- `RasterizerState`: culling, front face, and depth clamp behavior.
- `DepthStencilState`: depth test/write/compare behavior.
- `BlendState`: color blend enable and blend factors.
- `MultisampleState`: supported sample-count compatibility.
- `DynamicStateRequirements`: viewport and scissor requirements.
- `RuntimeMode`: real-runtime object or deterministic fallback object.
- `ReuseRecord`: newly created, reused, rejected, invalidated, or unavailable.
- `LifecycleState`: valid or invalidated.
- `Diagnostics`: latest creation, compatibility, reuse, or invalidation reason.

Validation rules:

- Required vertex and fragment stages must be present exactly once.
- Shader interface metadata must match the pipeline layout.
- Vertex input formats, stride, topology, raster/depth/blend/multisample/dynamic state, and render target compatibility must be valid.
- Invalidated dependencies reject creation and binding.

State transitions:

- `CreationRequested -> Valid`
- `CreationRequested -> Reused`
- `CreationRejected -> no usable object`
- `Valid/Reused -> Invalidated`

## Compute Pipeline

Represents dispatch pipeline state.

Fields:

- `ShaderModule`: valid compute shader module.
- `PipelineLayout`: valid compatible layout.
- `RuntimeMode`: real-runtime object or deterministic fallback object.
- `ReuseRecord`: newly created, reused, rejected, invalidated, or unavailable.
- `LifecycleState`: valid or invalidated.
- `Diagnostics`: latest creation, compatibility, reuse, or invalidation reason.

Validation rules:

- Exactly one compute shader module is required.
- Non-compute shader modules are rejected.
- Shader interface metadata must match the pipeline layout.
- Invalidated dependencies reject creation and binding.

## Pipeline Cache Record

Represents process-local reuse state for successful pipeline descriptions.

Fields:

- `StableKey`: deterministic identity derived from shader identities, layout summary, compatibility state, and relevant pipeline state.
- `PipelineKind`: graphics or compute.
- `RuntimeMode`: real-runtime or deterministic fallback.
- `ReuseState`: created, reused, rejected, invalidated, or unavailable.
- `Generation`: process-local invalidation counter.

Validation rules:

- Only successful pipeline creation requests enter reusable state.
- Failed, unsupported, and invalidated requests are never reused as successful entries.
- Device shutdown invalidates cache records.
- Persistent disk load/save/versioning is out of scope.

## Pipeline Binding State

Represents bound pipeline state on a command buffer.

Fields:

- `GraphicsPipeline`: currently bound graphics pipeline, if any.
- `ComputePipeline`: currently bound compute pipeline, if any.
- `ActiveRenderPassCompatibility`: current render pass/framebuffer compatibility summary.
- `BindingStatus`: missing, compatible, incompatible, wrong-kind, invalidated, or invalid-state.
- `Diagnostics`: latest binding or draw/dispatch validation reason.

Validation rules:

- Graphics binding requires a recording graphics command buffer and compatible active render pass scope.
- Compute binding requires a recording compute-compatible command buffer.
- Draw and indexed draw require a compatible valid graphics pipeline to remove missing-pipeline diagnostics.
- Dispatch requires a compatible valid compute pipeline to remove missing-pipeline diagnostics.
- Failed bindings do not mutate unrelated recorded commands.

## Pipeline Diagnostics

Represents deterministic user/test-facing reasons for creation, compatibility, binding, reuse, fallback, and lifecycle outcomes.

Fields:

- `ShaderReason`
- `PipelineLayoutReason`
- `GraphicsPipelineReason`
- `ComputePipelineReason`
- `PipelineCacheReason`
- `PipelineBindingReason`
- `RuntimeModeReason`

Validation rules:

- Runtime fallback diagnostics must explicitly state that no real runtime execution occurred.
- Diagnostics must distinguish missing pipeline, incompatible pipeline, wrong pipeline kind, invalidated dependency, configured failure, and post-shutdown invalid-state.
