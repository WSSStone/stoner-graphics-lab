# B08-S08: Deferred Frame Plan, Surfaces, And Graph Fix

## Scope

Fixed `CR001-B08-F004` in deferred frame planning and added deterministic
regression coverage for render-graph write ownership. This step did not modify
RHI, Vulkan native readback, shaders, or CI.

Production files changed:

- `Source/Renderer/Private/FDeferredRenderer.cpp`

Test files changed:

- `Tests/DeferredRenderingTests.cpp`

## Fix

`FDeferredRenderer::PrepareFrame` no longer declares `LightingAccumulation` as
a `SurfaceData` pass write. The surface pass write set now matches the actual
surface render pass/framebuffer ownership:

- `BaseColorAO`
- `NormalRoughness`
- `EmissiveMetallic`
- `Depth`

The lighting stages remain the writers of `LightingAccumulation`, and
composition remains a reader of that resource.

## Regression Coverage

Added focused planning assertions:

- lit frame: `SurfaceData` does not write `LightingAccumulation`,
  `DirectionalLighting` does write it, and `Composition` reads it;
- empty/no-light frame: `SurfaceData` still does not write
  `LightingAccumulation`, while composition keeps its accumulation input.

This prevents future graph/report changes from reintroducing a false surface
write for the lighting target.

## Verification

Passed:

- `conda run -n stoner-cr python CodeReview/Tools/crctl.py gate fallback-strict --id CR-001`

Recorded at: `2026-07-27T09:23:16+00:00`.

Additional focused output extraction:

- `Build/Mac/Debug/Tests/StonerTest | grep -E "Deferred graph ownership keeps lighting accumulation|Deferred empty frame graph keeps accumulation"`

Output:

- `[PASS] Deferred graph ownership keeps lighting accumulation out of surface writes`
- `[PASS] Deferred empty frame graph keeps accumulation as composition input only`

## Finding Status

- `CR001-B08-F004`: Fixed by commit `d42714b`.
