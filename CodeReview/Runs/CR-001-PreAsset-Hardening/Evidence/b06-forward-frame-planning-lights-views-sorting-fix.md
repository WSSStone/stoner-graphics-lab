# B06-S14 Evidence: Forward Frame Planning, Lights, Views, And Sorting Fix

Step: `B06-S14`.

Findings fixed:

- `CR001-B06-F004`: Transparent draw ordering can fall back to caller order for
  equal depth, material, and object keys.
- `CR001-B06-F005`: Forward renderer can prepare no-light geometry without the
  required ambient fallback.

Resolution:

- Transparent sorting now falls back to mesh id after the specified depth,
  material id, and object id keys.
- Valid geometry with no accepted lights now always receives
  `AmbientFallback.bActive=true` and one `FWD-AMBIENT-FALLBACK` diagnostic.

Regression evidence:

- `[PASS] Forward renderer preserves required ambient fallback diagnostics when fallback config is disabled`
- `[PASS] Forward renderer breaks final transparent ties without relying on caller order`

Gate evidence:

- `scons config=debug`: passed.
- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/gate-fallback-strict.json`
  records `fallback-strict` passed at `2026-07-27T07:00:55+00:00`.
- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/output/b06-forward-planning-fix-stonertest.txt`
  records the new forward regressions as passing and the known local Deferred
  native intermittent failures separately tracked by `CR001-B08-F001`.

Code commit:

- `9dcd4ea`: `fix(renderer): stabilize forward fallback and transparent sorting`
