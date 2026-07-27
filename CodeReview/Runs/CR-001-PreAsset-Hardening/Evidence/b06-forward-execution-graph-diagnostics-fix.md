# B06-S17 Evidence: Forward Execution, Graph Declaration, And Diagnostics Fix

Step: `B06-S17`.

Finding fixed:

- `CR001-B06-F006`: Forward graph declarations omit material resource access
  edges.

Resolution:

- Forward graph declaration construction now records material resource reads for
  accepted opaque and transparent pass draw lists.
- Access declarations are deduplicated per pass and skip empty resource
  identifiers.

Regression evidence:

- `[PASS] Forward graph declaration includes material resource read accesses for opaque and transparent passes`

Gate evidence:

- `scons config=debug`: passed.
- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/gate-fallback-strict.json`
  records `fallback-strict` passed at `2026-07-27T07:17:32+00:00`.
- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/output/b06-forward-graph-fix-stonertest.txt`
  records the new forward regression as passing and the known local Deferred
  native intermittent failures separately tracked by `CR001-B08-F001`.

Code commit:

- `10c3c5c`: `fix(renderer): declare forward material resource accesses`
