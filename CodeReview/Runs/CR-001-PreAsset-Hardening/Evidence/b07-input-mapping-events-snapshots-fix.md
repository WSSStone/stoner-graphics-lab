# B07-S05 Evidence: Input Mapping, Events, And Snapshots Fix

Step: `B07-S05`.

Findings fixed:

- `CR001-B07-F003`: GLFW input mapping omits declared key and mouse vocabulary.
- `CR001-B07-F004`: Input snapshot focus and reset paths can preserve stale
  state.

Resolution:

- GLFW translation now covers declared navigation, modifier, function, and extra
  mouse-button vocabulary.
- Input snapshot focus is restored from the owning window on focused frames.
- Full input reset paths clear pointer position, pointer delta, has-pointer
  state, key/button state, and focus state.

Regression evidence:

- `[PASS] Application input restores focused snapshot state on focused frame`
- `[PASS] Application input invalid lifecycle clears stale pointer and focus snapshot state`
- `[PASS] Application GLFW input mapping covers declared navigation modifier function and extra mouse vocabulary when available`

Gate evidence:

- `scons config=debug`: passed.
- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/gate-fallback-strict.json`
  records `fallback-strict` passed at `2026-07-27T07:51:11+00:00`.
- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/output/b07-input-snapshots-fix-stonertest.txt`
  records the new Application regressions as passing and the known local
  Deferred native intermittent failures separately tracked by
  `CR001-B08-F001`.

Code commit:

- `21629d7`: `fix(application): complete input mapping snapshots`
