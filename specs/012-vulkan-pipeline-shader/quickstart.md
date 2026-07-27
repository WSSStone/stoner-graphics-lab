# Quickstart: Vulkan Pipeline & Shader

**Feature**: 012-vulkan-pipeline-shader  
**Date**: 2026-06-30
**CR-001 Amendment**: 2026-07-27

## Prerequisites

- Use the project root: `/Users/wangshi/Documents/UGit/stoner-graphics-lab`
- Use the existing `godot` conda environment for build/test commands when available.
- Read the current plan before implementation: `specs/012-vulkan-pipeline-shader/plan.md`

## Expected Developer Flow

1. Create an active Vulkan backend device.
2. Optionally call `FVulkanDevice::EnableNativeShaderRuntime` when native
   shader and pipeline validation is required. Success enables one
   device-owned native object context for `VkShaderModule`, graphics pipeline,
   and compute pipeline ownership. Only wrappers with retained native tokens
   report `RealRuntime`; unrelated resources remain deterministic. Unsupported
   or unavailable environments remain explicit.
3. Create shader modules for vertex, fragment, and compute stages using
   structurally valid precompiled bytecode payloads whose SPIR-V execution
   model and entry-point name match the declared stage and entry point, plus
   explicit shader interface metadata.
4. Create a pipeline layout whose descriptor bindings and small constant-data ranges match the shader interface metadata.
5. Create a triangle-ready graphics pipeline with compatible render target, vertex input, primitive topology, rasterization, depth/stencil, blend, multisample, and dynamic viewport/scissor requirements.
6. Create a compute pipeline with a compatible compute shader module and pipeline layout.
7. Allocate command buffers and verify graphics/compute binding behavior:
   - Draw and indexed draw without a graphics pipeline keep missing-pipeline diagnostics.
   - Draw and indexed draw with a compatible bound graphics pipeline report compatible binding diagnostics.
   - Dispatch without a compute pipeline keeps missing-pipeline diagnostics.
   - Dispatch with a compatible bound compute pipeline reports compatible binding diagnostics.
8. Repeat equivalent pipeline creation requests in the same process and
   confirm deterministic reuse diagnostics; invalidate a retained shader or
   layout and confirm a corrected request does not reuse the stale pipeline.
9. Confirm a command buffer rejects pipelines created by another device.
10. Shut down the device and confirm native shader/pipeline handles, shader
    modules, layouts, pipelines, binding state, and reuse records are released
    or reject further use.

## Validation Commands

```bash
conda run -n godot scons
Build/Mac/Debug/Tests/StonerTest
```

## Verification Result

Validated on 2026-06-30 in the `godot` conda environment.

- `conda run -n godot scons`: PASS, completed in approximately 1.7 seconds on the final verification run.
- `Build/Mac/Debug/Tests/StonerTest`: PASS, completed in approximately 0.4 seconds on the final verification run.
- RHI abstraction boundary check: PASS, no Renderer/Application references were found in `Source/RHI/Public/RHI` or `Source/Backend/Vulkan/Public/VulkanRHI`.
- Out-of-scope check: PASS, no source shader compilation, automatic reflection, persistent disk cache, material system, render graph scheduling, mesh shader, ray tracing, or visible triangle demo behavior was introduced in `Source` or `Tests`.

## Required Verification Coverage

- Shader module success and rejection paths.
- Complete SPIR-V header/instruction bounds plus declared execution-model and
  entry-point matching.
- Device-owned construction, cross-device dependency rejection, and
  failure-atomic shader/layout factories.
- Explicit shader interface metadata validation against pipeline layouts.
- Real-runtime native shader, graphics pipeline, and compute pipeline ownership
  with deterministic fallback diagnostics.
- Triangle-ready graphics pipeline creation and negative paths.
- Compute pipeline creation and negative paths.
- Collision-safe process-local cache/reuse behavior and stale-dependency
  rejection.
- Command buffer graphics/compute pipeline binding behavior.
- Cross-device pipeline binding rejection.
- Draw/indexed draw/dispatch diagnostics after binding.
- Device shutdown invalidation.
- Existing regression tests remain passing.

## Out of Scope Checks

Do not implement or require these in this phase:

- Runtime shader compilation from source languages.
- Automatic shader reflection.
- Persistent disk pipeline cache.
- Material system integration.
- Render graph scheduling.
- Mesh shader or ray tracing pipeline creation.
- Full triangle demo application and visible swapchain presentation.
