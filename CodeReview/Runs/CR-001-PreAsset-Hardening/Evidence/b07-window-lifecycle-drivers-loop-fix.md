# B07-S02 Evidence: Window Lifecycle, Drivers, And Loop Fix

Step: `B07-S02`.

Findings fixed:

- `CR001-B07-F001`: Application loop ignores driver input events.
- `CR001-B07-F002`: Real-window create failure can preserve stale active window
  state.

Resolution:

- `FApplicationLoop::Run` queues driver input events into `FInputManager` before
  calling `PollFrame`.
- `FWindow::CreateRealWindow` clears transient state at entry and resets runtime
  lifecycle/platform/drawable state on validation and driver-create failures.

Regression evidence:

- `[PASS] Application real-window validation failure clears stale active runtime state`
- `[PASS] Application loop ingests native driver input events before deriving frame state`

Gate evidence:

- `scons config=debug`: passed.
- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/gate-fallback-strict.json`
  records `fallback-strict` passed at `2026-07-27T07:35:15+00:00`.
- `CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/output/b07-window-loop-fix-stonertest.txt`
  records the new Application regressions as passing and the known local
  Deferred native intermittent failures separately tracked by
  `CR001-B08-F001`.

Code commit:

- `89c8e0e`: `fix(application): harden window loop lifecycle`
