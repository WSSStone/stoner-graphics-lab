# Quickstart: Vulkan Pipeline & Shader

**Feature**: 012-vulkan-pipeline-shader  
**Date**: 2026-06-30

## Prerequisites

- Use the project root: `/Users/wangshi/Documents/UGit/stoner-graphics-lab`
- Use the existing `godot` conda environment for build/test commands when available.
- Read the current plan before implementation: `specs/012-vulkan-pipeline-shader/plan.md`

## Expected Developer Flow

1. Create an active Vulkan backend device.
2. Create shader modules for vertex, fragment, and compute stages using structurally valid precompiled bytecode payloads and explicit shader interface metadata.
3. Create a pipeline layout whose descriptor bindings and small constant-data ranges match the shader interface metadata.
4. Create a triangle-ready graphics pipeline with compatible render target, vertex input, primitive topology, rasterization, depth/stencil, blend, multisample, and dynamic viewport/scissor requirements.
5. Create a compute pipeline with a compatible compute shader module and pipeline layout.
6. Allocate command buffers and verify graphics/compute binding behavior:
   - Draw and indexed draw without a graphics pipeline keep missing-pipeline diagnostics.
   - Draw and indexed draw with a compatible bound graphics pipeline report compatible binding diagnostics.
   - Dispatch without a compute pipeline keeps missing-pipeline diagnostics.
   - Dispatch with a compatible bound compute pipeline reports compatible binding diagnostics.
7. Repeat equivalent pipeline creation requests in the same process and confirm deterministic reuse diagnostics.
8. Shut down the device and confirm shader modules, layouts, pipelines, binding state, and reuse records reject further use.

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
- Explicit shader interface metadata validation against pipeline layouts.
- Real-runtime and deterministic fallback diagnostics.
- Triangle-ready graphics pipeline creation and negative paths.
- Compute pipeline creation and negative paths.
- Process-local cache/reuse behavior.
- Command buffer graphics/compute pipeline binding behavior.
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
