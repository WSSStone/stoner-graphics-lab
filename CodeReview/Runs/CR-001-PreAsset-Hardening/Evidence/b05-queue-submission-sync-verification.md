# B05-S06 Verification Evidence

## Revisions

- Defective implementation parent: `2f30cfd`
- Fix commit: `3dbfed0`
- Verification source state: `c4eab25`

Historical evidence was gathered by static Git comparison. Current behavior
was verified with maintained tests and predefined ordinary local gates.

## Requirement Matrix

| Finding | Parent signal | Current evidence | Result |
|---|---|---|---|
| CR001-B05-F004 | Validation and mutation interleave, so a later rejection can consume or signal earlier objects | Full preflight, tracking allocation before no-fail commits, rejection-state tests | Verified |
| CR001-B05-F005 | Retryable observation can strand submitted command state and completion publication is not idempotent | Shared completion transition, resettable-before-completed ordering, timeout/not-ready recovery tests | Verified |
| CR001-B05-F006 | Synchronization wrappers lack shared device provenance and construction/lifecycle authority is externally reachable | Shared active owner identity, private construction/mutation, foreign-device and shutdown tests | Verified |

## Maintained Signals

The fallback strict suite returned exit 0 and includes:

- a later unready wait preserves all earlier inputs and the submission count;
- an already-signaled output preserves command, fence, and semaphore state;
- duplicate waits and wait/signal overlap reject atomically;
- foreign-device command buffers and semaphores reject without mutation;
- nonzero-timeout success leaves the command buffer resettable;
- `NotReady` and `Timeout` remain recoverable through queue `WaitIdle`;
- repeated completion observation succeeds idempotently;
- device shutdown invalidates retained queue and synchronization wrappers.

## Gate Files

- `gate-strict-debug.json`
  - `scons config=debug strict=1`
  - exit 0
- `gate-fallback-strict.json`
  - `scons config=debug strict=1 graphics=disabled`
  - `Build/Mac/Debug/Tests/StonerTest`
  - both exit 0
- `gate-strict-release.json`
  - `scons config=release strict=1`
  - exit 0

All files are under
`CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/`.

## Boundaries

- No production or maintained test source changed in B05-S06.
- No debugger, custom fault-injection probe, memory-check tool, or network
  action was used.
- The intermittent native deferred-rendering assertions remain tracked by
  accepted `CR001-B08-F001` and are outside this packet's deterministic scope.
