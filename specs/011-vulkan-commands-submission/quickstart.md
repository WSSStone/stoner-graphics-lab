# Quickstart: Vulkan Command Recording & Submission

**Feature**: 011-vulkan-commands-submission  
**Date**: 2026-06-30

## Prerequisites

- Use the existing project environment with SCons 4.10.1.
- If using the provided conda environment, activate or run commands through `conda run -n godot`.
- Vulkan SDK or platform loader support is optional for deterministic fallback validation but required to exercise real runtime submission.

## Build

```bash
conda run -n godot scons
```

## Run Tests

```bash
Build/Mac/Debug/Tests/StonerTest
```

Adjust the executable path for other configured platforms or build variants.

## Expected Verification Flow

1. Create or initialize a Vulkan backend device.
2. Request a supported graphics queue.
3. Allocate a command buffer compatible with the graphics queue.
4. Begin recording.
5. Create a minimal single-subpass render pass and compatible framebuffer.
6. Begin a render pass scope.
7. Record placeholder draw and indexed draw commands and verify missing-pipeline diagnostics.
8. End the render pass scope.
9. Record declarative barrier/layout intent and transfer commands with valid resources.
10. End recording and verify command summaries preserve ordering.
11. Submit the command buffer to the compatible queue.
12. Observe real submission when runtime execution is available, or deterministic fallback submission diagnostics when unavailable.
13. Wait for completion or queue idle.
14. Reset the command buffer for reuse.
15. Shut down the device and verify command-related objects reject new work.

## Negative Verification Matrix

- Allocate command buffer after device shutdown.
- Allocate command buffer for unsupported queue type.
- Begin twice, End before Begin, record after End, Reset while Submitted.
- Record graphics commands outside render pass scope.
- Begin nested render pass scope or end a missing scope.
- Create framebuffer with mismatched attachment count, format, dimensions, sample count, mip level, array layer, or invalidated texture.
- Record transfer or barrier commands with invalidated, incompatible, missing, or out-of-bounds resources.
- Submit a missing, still-recording, never-recorded, reset, already submitted, incompatible, or invalidated command buffer.
- Inject fallback not-ready and timeout completion outcomes.
- Schedule missing, already scheduled, non-pending, or invalidated-destination upload requests.

## Out of Scope Checks

- Do not require shader compilation.
- Do not require real graphics or compute pipeline creation.
- Do not require full pipeline binding validation.
- Do not require full resource state tracking across command buffers.
- Do not require render graph scheduling.
- Do not require multi-threaded command recording.
- Do not require visible frame rendering.
