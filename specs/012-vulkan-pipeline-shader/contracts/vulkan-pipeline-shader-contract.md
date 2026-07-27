# Contract: Vulkan Pipeline & Shader

**Feature**: 012-vulkan-pipeline-shader  
**Date**: 2026-06-30

This contract describes observable behavior for the Vulkan backend pipeline and shader slice. Exact signatures are finalized during implementation while preserving existing RHI shader, layout, pipeline, command, resource, lifecycle, and result contracts.

## Shader Module Contract

Required behavior:

- A caller can create shader modules from structurally valid precompiled bytecode, a supported declared stage, a non-empty entry point, a deterministic payload identity, and explicit shader interface metadata.
- Real runtime shader module creation is used when the backend runtime is available.
- The backend may enable native shader ownership independently of other
  still-deterministic `FVulkanDevice` objects, but only a retained native
  shader handle may report `RealRuntime`; this must not relabel buffers,
  layouts, or pipelines as native.
- Deterministic fallback shader modules are allowed when runtime creation is unavailable and diagnostics clearly report fallback validation.
- Shader modules expose stage, entry point, payload identity, interface summary, validation mode, runtime mode, lifecycle state, and diagnostics.
- Shader modules and layouts retain creating-device provenance and reject
  cross-device pipeline or descriptor composition.

Negative-path requirements:

- Empty bytecode, incomplete SPIR-V headers, malformed instruction bounds,
  missing or wrong-stage SPIR-V entry points, unsupported stages, wrong-stage
  metadata, missing payload identity, invalid interface metadata, foreign
  dependencies, and post-shutdown creation return explicit failure.
- Failed creation does not expose a usable partial shader module.
- Wrapper, native-object, and tracking allocation failures return explicit
  results and release any partial native shader object.
- Invalidated shader modules cannot be used for pipeline creation.

## Shader Interface Metadata Contract

Required behavior:

- Shader interface metadata declares descriptor bindings and small constant-data requirements before reflection exists.
- Pipeline creation validates shader interface metadata against the selected pipeline layout.
- Metadata remains queryable through shader module summaries for tests and future renderer code.

Negative-path requirements:

- Duplicate bindings, zero descriptor counts, incompatible stage visibility, invalid constant ranges, and layout mismatches return explicit failure.
- Automatic shader reflection and runtime shader compilation remain out of scope.

## Pipeline Layout Compatibility Contract

Required behavior:

- Existing pipeline layout creation continues to accept valid descriptor binding declarations.
- Pipeline layouts participate in shader interface compatibility checks for graphics and compute pipeline creation.
- Invalidated pipeline layouts reject descriptor set creation, pipeline creation, and pipeline binding.

Negative-path requirements:

- Duplicate descriptor bindings, invalid descriptor binding fields, invalid constant ranges, missing dependencies, and post-shutdown creation return explicit failure.
- Failed compatibility checks report deterministic diagnostics.

## Graphics Pipeline Contract

Required behavior:

- A caller can create a triangle-ready graphics pipeline from compatible vertex and fragment shader modules, a compatible pipeline layout, render target compatibility, vertex input, primitive topology, rasterization, depth/stencil, blend, multisample, and dynamic viewport/scissor requirements.
- Real runtime pipeline creation is used when runtime support is available.
- Deterministic fallback graphics pipeline objects are allowed when runtime creation is unavailable and diagnostics clearly report that no real runtime execution occurred.
- Graphics pipelines expose lifecycle state, compatibility summary, runtime mode, reuse state, and creation diagnostics.

Negative-path requirements:

- Missing required shader stages, duplicate required stages, wrong shader stages, invalid or incompatible shader interface metadata, missing or invalidated layout, invalid vertex input, invalid topology, invalid fixed-function state, incompatible render target compatibility, configured failure limits, and post-shutdown creation return explicit failure.
- Failed creation does not populate successful process-local reuse entries.
- Advanced subpass combinations, material permutations, mesh shaders, ray tracing pipelines, and visible application-level triangle presentation remain out of scope.

## Compute Pipeline Contract

Required behavior:

- A caller can create a compute pipeline from exactly one valid compute shader module and a compatible pipeline layout.
- Real runtime pipeline creation is used when runtime support is available.
- Deterministic fallback compute pipeline objects are allowed when runtime creation is unavailable and diagnostics clearly report that no real runtime execution occurred.
- Compute pipelines expose lifecycle state, compatibility summary, runtime mode, reuse state, and creation diagnostics.

Negative-path requirements:

- Non-compute shader modules, missing shader modules, multiple compute shader modules where unsupported, incompatible shader interface metadata, invalidated dependencies, unsupported compute capability, configured failure limits, and post-shutdown creation return explicit failure.
- Failed creation does not populate successful process-local reuse entries.

## Process-Local Pipeline Reuse Contract

Required behavior:

- Equivalent successful graphics and compute pipeline creation requests within the same process produce deterministic reuse or cache-hit diagnostics.
- Reuse keys account for shader payload identities, shader interface metadata, pipeline layout compatibility, render target compatibility, and relevant pipeline state.
- Device shutdown invalidates process-local reuse records.

Negative-path requirements:

- Failed, unsupported, and invalidated pipeline creation requests are never reported as reusable successful entries.
- Persistent disk pipeline cache load, save, versioning, and invalidation behavior are out of scope.

## Command Binding Contract

Required behavior:

- A valid graphics pipeline can be bound to a compatible recording graphics command buffer inside compatible render pass scope.
- A valid compute pipeline can be bound to a compatible recording compute-capable command buffer.
- Draw and indexed draw validation reports compatible bound graphics pipeline state instead of missing-pipeline diagnostics.
- Dispatch validation reports compatible bound compute pipeline state instead of missing-pipeline diagnostics.
- Recorded command summaries or diagnostics distinguish missing, compatible, incompatible, wrong-kind, invalidated, and invalid-state pipeline binding outcomes.

Negative-path requirements:

- Binding outside recording state, binding graphics pipelines without compatible render pass scope, binding compute pipelines to incompatible queues, binding the wrong pipeline kind, binding invalidated pipelines, and binding incompatible render target/layout state return explicit failure.
- Failed bindings do not mutate unrelated recorded commands.

## Device Integration Contract

Required behavior:

- Existing Vulkan device shader and pipeline factories move from unsupported placeholders to validated backend behavior.
- Device shutdown invalidates shader modules, pipeline layouts, graphics pipelines, compute pipelines, and process-local cache records.
- Existing resource, descriptor, render pass, framebuffer, command buffer, queue, synchronization, and upload scheduling behavior remains compatible with prior tests.

Negative-path requirements:

- Creation or binding after shutdown returns invalid-state.
- Runtime-unavailable fallback behavior must not be confused with real GPU execution.
- Renderer and Application code must not depend on Vulkan-specific object types.

## Test Contract

Required coverage:

- Shader module success for vertex, fragment, and compute stages.
- Shader module rejection for empty, structurally malformed, unsupported, wrong-stage, metadata-incompatible, invalidated, and post-shutdown inputs.
- Pipeline layout compatibility success and rejection for duplicate bindings, invalid ranges, missing dependencies, and invalidated dependencies.
- Graphics pipeline creation success and rejection for shader stages, layout/interface mismatch, vertex input, topology, rasterization, depth/stencil, blend, multisample, dynamic viewport/scissor, render target compatibility, configured failure, and shutdown.
- Compute pipeline creation success and rejection for valid compute shader, wrong-stage shader, interface/layout mismatch, unsupported queue capability, configured failure, and shutdown.
- Process-local reuse success for equivalent graphics and compute requests, and no reuse for failed or invalidated requests.
- Command binding success and rejection for graphics and compute pipelines.
- Draw/indexed draw/dispatch diagnostics for missing, compatible, incompatible, wrong-kind, and invalidated pipeline binding states.
- Runtime-unavailable fallback diagnostics for shader module, graphics pipeline, compute pipeline, bind, draw, and dispatch paths.
- Existing Core, RHI, Vulkan device/swapchain, Vulkan resource management, and Vulkan command recording/submission tests remain passing.
