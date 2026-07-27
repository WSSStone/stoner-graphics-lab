# B06-S15: Forward Frame Planning, Lights, Views, And Sorting Verification

## Scope

Verified `CR001-B06-F004` and `CR001-B06-F005` after code commit
`9dcd4ea`.

## Source Verification

### `CR001-B06-F004`

- `SortForwardTransparentDraws` preserves the specified transparent ordering
  precedence:
  - camera-space depth descending;
  - material id ascending;
  - object id ascending.
- It now adds mesh id as the final fallback key for equal
  depth/material/object cases.
- This gives same-object, same-material transparent multi-mesh draws a stable
  order independent of caller submission order.

### `CR001-B06-F005`

- `FForwardRenderer::PrepareFrame` now checks only
  `OutPlan.HasRenderableGeometry() && !OutPlan.LightSet.HasAcceptedLights()`
  before activating ambient fallback.
- The required `FWD-AMBIENT-FALLBACK` diagnostic is emitted even when the legacy
  fallback configuration flag is false.

## Regression Verification

`Tests/RendererForwardPipelineTests.cpp` contains regressions for:

- disabled fallback configuration still producing exactly one ambient fallback
  diagnostic for no-light geometry;
- reversed caller submission order for equal depth/material/object transparent
  draws producing the same mesh-id final ordering.

Saved output confirms both regressions passed:

- `[PASS] Forward renderer preserves required ambient fallback diagnostics when fallback config is disabled`
- `[PASS] Forward renderer breaks final transparent ties without relying on caller order`

## Gate Verification

- `scons config=debug`: passed.
- The graphics-enabled `Build/Mac/Debug/Tests/StonerTest` run recorded both new
  forward regressions as passing; the run hit the known Deferred native local
  intermittent failures tracked by `CR001-B08-F001`.
- `crctl gate fallback-strict --id CR-001`: passed at
  `2026-07-27T07:00:55+00:00`.

## Decision

- `CR001-B06-F004`: Verified.
- `CR001-B06-F005`: Verified.
