# B06-S14: Forward Frame Planning, Lights, Views, And Sorting Fix

## Scope

Fixed `CR001-B06-F004` and `CR001-B06-F005`.

Code changed:

- `Source/Renderer/Private/FMeshDrawCommand.cpp`
- `Source/Renderer/Private/FForwardRenderer.cpp`
- `Tests/RendererForwardPipelineTests.cpp`

## Fixes

### `CR001-B06-F004`

- `SortForwardTransparentDraws` now preserves the specified ordering
  precedence of camera-space depth descending, material id, and object id.
- It then uses mesh id as a final stable fallback key when depth, material id,
  and object id are equal.
- This avoids relying on caller submission order for same-object, same-material
  transparent multi-mesh draws.

### `CR001-B06-F005`

- `FForwardRenderer::PrepareFrame` now always activates the ambient-only
  fallback record and `FWD-AMBIENT-FALLBACK` diagnostic when a valid frame has
  renderable geometry but no accepted lights.
- The legacy `bEnableAmbientFallback` configuration flag no longer permits a
  valid silent no-light geometry plan, preserving the Feature 015 contract.

## Regression Coverage

Added tests for:

- disabled fallback configuration still producing the required ambient fallback
  record and diagnostic for no-light geometry;
- reversed submission order for equal depth/material/object transparent draws
  producing a stable mesh-id final order.

## Verification

- `scons config=debug`: passed.
- `Build/Mac/Debug/Tests/StonerTest`: the new forward regressions passed; this
  local graphics-enabled run hit the known intermittent Deferred native
  MoltenVK failure recorded separately as `CR001-B08-F001`.
- `crctl gate fallback-strict --id CR-001`: passed at
  `2026-07-27T07:00:55+00:00`.

## Commit

- `9dcd4ea`: `fix(renderer): stabilize forward fallback and transparent sorting`
