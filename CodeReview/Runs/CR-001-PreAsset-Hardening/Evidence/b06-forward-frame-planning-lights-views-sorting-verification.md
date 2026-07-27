# B06-S15 Evidence: Forward Frame Planning, Lights, Views, And Sorting Verification

Step: `B06-S15`.

Findings verified:

- `CR001-B06-F004`: Transparent draw ordering can fall back to caller order for
  equal depth, material, and object keys.
- `CR001-B06-F005`: Forward renderer can prepare no-light geometry without the
  required ambient fallback.

Verification evidence:

- `git show --stat --oneline 9dcd4ea` confirms the fix touched
  `FForwardRenderer.cpp`, `FMeshDrawCommand.cpp`, and
  `RendererForwardPipelineTests.cpp`.
- Current `SortForwardTransparentDraws` uses mesh id as the final stable
  fallback key.
- Current `FForwardRenderer::PrepareFrame` emits ambient fallback whenever valid
  geometry has no accepted lights.
- Current tests and saved output include passing regressions for disabled
  fallback config and reversed same-object transparent final ties.
- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/gate-fallback-strict.json`
  records a passing `fallback-strict` gate at
  `2026-07-27T07:00:55+00:00`.

Result:

- `CR001-B06-F004` is Verified.
- `CR001-B06-F005` is Verified.
