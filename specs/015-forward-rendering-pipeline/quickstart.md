# Quickstart: Forward Rendering Pipeline

## Goal

Validate that the Forward Rendering Pipeline can prepare deterministic forward frame plans and render graph-compatible pass/resource declarations from view, light, material, draw, and environment inputs without requiring a physical graphics device, visible window, swapchain presentation, or real GPU command execution.

## Expected Development Flow

1. Add Renderer public contracts under `Source/Renderer/Public/Renderer/`.
2. Add private implementation files under `Source/Renderer/Private/`.
3. Export forward rendering headers through `RendererMinimal.h` or direct public includes.
4. Add focused tests in `Tests/RendererForwardPipelineTests.cpp` and wire them into `Tests/Main.cpp`.
5. Reuse existing Renderer material/shader binding and render graph declaration concepts.
6. Keep all forward rendering behavior backend-agnostic; do not include backend-specific or platform-window headers in Renderer forward code.
7. Keep shader records explicitly registered in memory; do not implement runtime shader compilation or shader file loading.

## Representative Forward Frame Scenario

Create a headless forward frame fixture with:

- One valid view with finite camera/view-projection data and positive viewport dimensions.
- One valid abstract color output and optional abstract depth output.
- At least four opaque draw candidates with valid full PBR-style material inputs.
- At least two transparent draw candidates at distinct camera-space depths.
- At least one equal-depth transparent tie case using stable material id and object id ordering.
- One primary directional light.
- More than the default point light limit of 4 point light candidates.
- A configured point light limit override case.
- A simple sky/background input.
- At least one material resource requirement consumed by render graph-compatible declarations.

Expected results:

- Frame preparation succeeds without real GPU execution or window presentation.
- Pass order is depth, opaque, sky/background, transparent where applicable.
- Point lights are sorted by deterministic influence and only the front N lights are accepted.
- Accepted and rejected light counts are reported in diagnostics and dumps.
- Transparent draws sort by camera-space depth descending, then stable material id, then stable object id.
- Opaque and transparent material bindings validate full PBR-style inputs.
- Render graph-compatible pass/resource declarations include output, depth, material, and environment requirements without backend handles.
- Text dumps are byte-identical across 20 repeated preparations of unchanged inputs.

## Negative Scenarios

Cover at minimum:

- Invalid view data.
- Missing or invalid output target.
- Multiple primary directional lights.
- Point light candidates with invalid range, intensity, position, or identity.
- Point light submissions above the configured limit.
- Missing or incomplete PBR surface inputs.
- Material binding incompatible with opaque rendering.
- Material binding incompatible with transparent rendering.
- Transparent equal-depth tie ordering.
- No accepted lights with valid geometry, which must produce a constant ambient-only fallback diagnostic.
- Resource requirements that attempt to expose backend-specific or graph-local handles.

## Verification Commands

```bash
conda run -n godot scons
Build/Mac/Debug/Tests/StonerTest
```

## Verification Results

Recorded on 2026-07-03 from branch `015-forward-rendering-pipeline`:

- `conda run -n godot scons` succeeded. macOS emitted transient `DARWIN_USER_TEMP_DIR` warnings from the toolchain, but the build completed and linked `Build/Mac/Debug/Tests/StonerTest`.
- `Build/Mac/Debug/Tests/StonerTest` succeeded. The new `RendererForwardPipelineTests.cpp` cases passed alongside existing Core, RHI, Renderer, and Vulkan tests.
- Representative forward frame preparation is verified under the SC-001 target by `Representative forward frame preparation completes under one second`.
- 20-run deterministic frame plan and debug dump stability is verified by `Forward frame plan diagnostics ordering and dump are stable across 20 runs`.
- Default point light limit 4, configurable overrides, zero-limit behavior, distance/effectiveness influence sorting, ambient-only fallback, full PBR-style material validation, sky/background participation, transparent depth sorting, and equal-depth tie-breakers are covered in `Tests/RendererForwardPipelineTests.cpp`.
- Cross-platform compatibility evidence: implementation uses C++20 standard library and existing Core/Renderer public contracts only, adds no platform-specific source branches, and builds in the existing SCons layer structure. macOS build/test evidence is recorded above; Windows and Linux compatibility are documented as source-level/SCons-level evidence pending those CI or local runners.

## Boundary Check

```bash
rg -n "\bVulkan\b|\bMetal\b|\bDX12\b|\bDirectX\b|\bOpenGL\b|\bGLES\b|\bWebGL\b|\bVk[A-Z]\w*|\bID3D\w*|\bMTL\w*|\bSwapchain\b|\bWindow\b" Source/Renderer/Public/Renderer Source/Renderer/Private
```

The boundary check should not find backend-specific or presentation-specific dependencies in Renderer forward rendering code.

2026-07-03 result: the word-boundary boundary check produced no matches. The original broad `Metal` token was tightened because it falsely matched the valid PBR field name `Metallic`.
