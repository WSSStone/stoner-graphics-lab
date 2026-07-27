# B08-S09: Deferred Frame Plan, Surfaces, And Graph Verification

## Scope

Verified `CR001-B08-F004` after the B08-S08 fix. This step did not change
production or test code.

## Verified Finding

### `CR001-B08-F004`

Status moved from `Fixed` to `Verified`.

The fix keeps deferred graph access ownership aligned with real stage roles:

- `SurfaceData` writes only surface attachments and depth.
- Lighting stages write `LightingAccumulation`.
- Composition reads `LightingAccumulation` without the surface pass claiming
  ownership of that target.

## Verification

Passed:

- `conda run -n stoner-cr python CodeReview/Tools/crctl.py gate fallback-strict --id CR-001`

Recorded at: `2026-07-27T09:27:29+00:00`.

Focused regression extraction:

- `Build/Mac/Debug/Tests/StonerTest | grep -E "Deferred graph ownership keeps lighting accumulation|Deferred empty frame graph keeps accumulation"`

Output:

- `[PASS] Deferred graph ownership keeps lighting accumulation out of surface writes`
- `[PASS] Deferred empty frame graph keeps accumulation as composition input only`

## Residual Risk

This verification is deterministic and does not execute the real Vulkan
deferred readback path. That is acceptable for this finding because the defect
was in Renderer frame-plan pass write ownership before native execution.
Native readback and failure-lifecycle behavior are handled by later B08 packets.
