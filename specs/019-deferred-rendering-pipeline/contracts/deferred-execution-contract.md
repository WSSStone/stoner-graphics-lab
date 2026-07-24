# Contract: Deferred RHI Execution

## Purpose

Define how a valid deferred frame and compiled render graph become backend-neutral RHI commands, how the Linux Vulkan implementation supplies real offscreen resources, how pixels are copied back, and how failures and cleanup are owned.

## Required RHI Command Extensions

`IRHICommandBuffer` gains backend-neutral operations equivalent to:

```text
BindDescriptorSet(bindPoint, pipelineLayout, setIndex, descriptorSet)
BindIndexBuffer(buffer, offsetBytes, indexType)
BeginRenderPass(renderPass, framebuffer, clearValues)
RecordTextureToBufferCopy(sourceTexture, destinationBuffer, region)
```

Associated symbolic command types and stable validation records are added. Existing implementations and test mocks must implement or explicitly return `Unsupported`; deferred execution treats `Unsupported` as failure when the command is required.

Validation rules:

- descriptor set layout/set index matches the bound pipeline layout;
- all bound resources are valid and compatible with declared shader visibility/type;
- index buffer has index usage, valid offset/alignment, and sufficient range;
- clear-value count/order matches render-pass attachments;
- texture has copy-source usage, buffer has copy-destination/host-visible access, and copy region lies within both resources;
- required layout transitions occur outside an active render pass and match declared usage.

## Concrete Initial Surface Layout

| Ordered attachment | Format | Required usage |
|--------------------|--------|----------------|
| Base color + AO | `R8G8B8A8_UNorm` | color attachment, sampled, copy source |
| World normal + roughness | `R16G16B16A16_Float` | color attachment, sampled, copy source |
| Emissive + metallic | `R16G16B16A16_Float` | color attachment, sampled, copy source |
| Depth | `D32_Float` | depth attachment, sampled, copy source |

Lighting accumulation uses `R16G16B16A16_Float` with color-attachment and sampled usage. Final validation output uses `R8G8B8A8_UNorm` with color-attachment and copy-source usage. Every target is 2D, one mip, one layer, sample count one, and shares the active extent.

## Canonical Shader Interface

The RHI adds `R32G32_Float` and `R32G32B32_Float` vertex formats. Deferred pipelines use these fixed descriptor bindings:

| Set | Binding | Descriptor type | Visibility | Meaning |
|-----|---------|-----------------|------------|---------|
| 0 | 0 | Uniform buffer | Vertex + Fragment | Frame/view record |
| 1 | 0 | Uniform buffer | Vertex + Fragment | Per-draw/material record |
| 2 | 0 | Combined texture sampler | Fragment | Base color + AO |
| 2 | 1 | Combined texture sampler | Fragment | World normal + roughness |
| 2 | 2 | Combined texture sampler | Fragment | Emissive + metallic |
| 2 | 3 | Combined texture sampler | Fragment | Depth |
| 2 | 4 | Combined texture sampler | Fragment | Lighting accumulation |
| 3 | 0 | Storage buffer | Vertex + Fragment | Ordered light records |

Pipeline set usage:

- Surface: sets 0 and 1.
- Directional, point, and spot lighting: sets 0, 2, and 3.
- Composition: sets 0 and 2.

Mirrored C++/GLSL buffer records use column-major `float32` matrices and 16-byte `float32 vec4` slots at these exact byte offsets:

| Record | Byte offset | Field and representation |
|--------|-------------|--------------------------|
| Frame/view | 0 | `View`, `mat4`, 64 bytes |
| Frame/view | 64 | `Projection`, `mat4`, 64 bytes |
| Frame/view | 128 | `InverseViewProjection`, `mat4`, 64 bytes |
| Frame/view | 192 | `ViewProjection`, `mat4`, 64 bytes |
| Frame/view | 256 | `CameraPosition`, world-space `vec4` (`xyz`, reserved `w`) |
| Frame/view | 272 | `OutputExtent`, `vec4` (`width`, `height`, inverse width, inverse height) |
| Frame/view | 288 | `DepthConvention`, `vec4` (near, far, reversed-Z flag, reserved) |
| Per-draw/material | 0 | `Model`, `mat4`, 64 bytes |
| Per-draw/material | 64 | `WorldNormalFromModel`, affine `mat4` embedding `transpose(inverse(mat3(Model)))`, 64 bytes |
| Per-draw/material | 128 | `BaseColorAO`, `vec4` (base-color RGB, AO) |
| Per-draw/material | 144 | `EmissiveMetallic`, `vec4` (emissive RGB, metallic) |
| Per-draw/material | 160 | `RoughnessAlphaCutoffFlags`, `vec4` (roughness, alpha cutoff, flags-as-float, reserved) |
| Light | 0 | `PositionRange`, world-space `vec4` (position XYZ, range) |
| Light | 16 | `DirectionOuterCos`, world-space `vec4` (direction XYZ, cosine outer angle) |
| Light | 32 | `ColorIntensity`, `vec4` (linear color RGB, intensity) |
| Light | 48 | `InnerCosTypeVolumeMode`, `vec4` (cosine inner angle, type code, volume-mode code, reserved) |

The exact record sizes are 304 bytes for frame/view, 176 bytes for per-draw/material, and 64 bytes per light. Integer-like flags/type/mode values use exactly representable non-negative `float32` codes defined by the deferred public vocabulary; reserved values are zero.

`WorldNormalFromModel` has zero translation, bottom-right `1`, and an upper-left inverse-transpose normal matrix. Non-finite or non-invertible model transforms fail before command recording; surface shaders normalize the transformed normal before writing the GBuffer.

Depth state derives from `DepthConvention`: standard-Z uses far clear `1.0` with `ERHICompareOp::LessEqual`, while reversed-Z uses far clear `0.0` with `ERHICompareOp::GreaterEqual`. Projection, `InverseViewProjection`, clear value, pipeline comparison, and readback decode must agree or binding validation fails before recording.

Vertex/index layouts:

| Geometry | Attributes | Stride | Index type |
|----------|------------|--------|------------|
| Surface | location 0 `R32G32B32_Float` position at byte 0; location 1 `R32G32B32_Float` normal at byte 12 | 24 | caller-declared compatible type |
| Fullscreen | location 0 `R32G32_Float` position at byte 0 | 8 | none |
| Sphere/cone | location 0 `R32G32B32_Float` position at byte 0 | 12 | `UInt16` |

Pipeline creation fails when a descriptor, mirrored record size/offset, visibility, attribute, stride, or index type differs from this schema.

## Binding Contract

Before recording, `FDeferredFrameExecutor` validates all required bindings as one transaction:

- command buffer is resettable/idle and graphics-compatible;
- textures match layout formats, extent, usages, and lifecycle;
- render pass attachment order matches framebuffers and pipeline compatibility;
- shader/pipeline interfaces match descriptor set layouts;
- surface geometry and reusable sphere/cone volume buffers are valid;
- local-light inside/outside pipeline variants exist for every accepted classification;
- final output and readback resources exist when requested;
- optional transparent bindings are complete when transparent work exists.

Any missing/incompatible binding returns `InvalidBinding` before `Begin()` and records no commands.

## Command Recording Order

For each non-culled compiled graph pass:

1. Emit required barriers/layout transitions for that pass.
2. Begin its matching render pass with explicit clear/load values.
3. Bind the stage pipeline and descriptor sets.
4. Bind stage geometry:
   - surface draw geometry for `SurfaceData`;
   - fullscreen triangle for directional/composition;
   - sphere vertex/index buffers for point lights;
   - cone vertex/index buffers for spot lights.
5. Set viewport/scissor from the active extent or bounded light region.
6. Record deterministic draws in frame-plan order.
7. End the render pass.

After composition, optional forward-transparent execution loads the final output and preserves existing transparent ordering. Validation readback transitions requested targets to copy-source, copies named regions to staging buffers, and transitions resources only when later use requires it.

The executor records only; queue submit/fence wait and staging mapping remain with the execution session.

## Lighting Blend and Volume Contract

- Lighting accumulation starts at zero.
- Every directional and local-light draw uses additive RGB accumulation.
- Directional draws cover the active viewport.
- Point/spot draws use the accepted volume classification to select cull/depth state.
- CPU view rejection may omit an entire local light; per-fragment volume and range/cone tests reject pixels outside exact influence.
- Directional, point, and spot arrays execute in that type order and use ascending stable entity identity within type; spot cone values are radians before shader-side cosine conversion.
- Surface depth and inverse-view-projection reconstructed world-space position use the one declared convention.
- Emissive is not multiplied by light accumulation; ambient occlusion modulates the declared ambient/indirect contribution, not direct emissive output.

## Linux Native Offscreen Session

The Vulkan native offscreen session:

1. Initializes a real Vulkan instance/device/graphics queue and proves real runtime mode.
2. Creates all images, views, memory, buffers, descriptors, shader modules, pipelines, render passes, framebuffers, command pool/buffer, and completion fence required by the fixed validation frame.
3. Wraps those objects behind RHI interfaces and normalized runtime summaries.
4. Supplies the wrappers to `FDeferredFrameExecutor` without depending on Renderer types inside the backend.
5. Submits the recorded command buffer and waits for completion with a bounded timeout.
6. Maps host-visible readback buffers only after completion.
7. Decodes named probes and releases all objects in reverse dependency order.

The job sets `VK_DRIVER_FILES` to the resolved Lavapipe ICD and fails if selected runtime proof is fallback, no native device exists, or the selected adapter is not a software Vulkan device.

## Shader Contract

- Surface, fullscreen, directional, point, spot, and composition GLSL sources are repository-owned.
- Matching checked-in SPIR-V payloads are required.
- Tool-enabled builds regenerate or verify payloads; tool-unavailable builds consume checked-in payloads.
- Runtime validates SPIR-V magic/alignment, stage, `main` entry point, declared descriptor interface, vertex inputs, and render-target compatibility.
- Native shader/pipeline failure cannot be replaced by deterministic success in the Linux native gate.

## Failure Ownership

- The first non-success validation, graph, record, submit, wait, copy, map, decode, or probe result owns the execution result.
- No stage after the failing dependency claims success.
- Cleanup diagnostics may be appended but cannot overwrite the primary result.
- Deterministic diagnostics name stage, stable subject, result category, and reason without native codes or addresses in public reports.

## Cleanup Contract

Cleanup is idempotent after it starts:

1. Stop accepting new execution.
2. Wait for submitted native work when submission occurred.
3. Unmap and release readback/staging resources.
4. Release descriptor sets/pools and uniform/light buffers.
5. Release volume/geometry buffers.
6. Release framebuffers and image views.
7. Release render passes and pipelines/layouts/shaders.
8. Release images and memory.
9. Release command/fence/queue state.
10. Release device and instance ownership.

Partial initialization follows the same dependency order. Final normalized live counts for deferred frame-owned objects must all be zero.
