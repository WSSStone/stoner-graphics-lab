# Decisions

## D001 - Enforce Batch-Boundary GitHub Actions

- Date: 2026-07-26
- Status: Accepted
- Context: GitHub Actions quota is exhausted or constrained. Earlier CR steps
  were pushed individually even though `CodeReview/PROCESS.md` already says to
  push and run CI at batch boundaries.
- Decision: Inspection, fix, and local verification step commits remain local.
  Push once at the end of each batch and run one required remote matrix.
  Platform-sensitive changes may request an earlier remote run only with
  explicit user approval.
- Consequence: Findings may remain Fixed after local gates and become Verified
  when the batch-boundary remote evidence is available. Handoffs must distinguish
  local commits from the last pushed remote HEAD.
- Amendment: The repository was subsequently made public, so hosted-minute
  scarcity is no longer treated as a blocker. Batch-boundary CI remains the
  default because it produces one coherent cross-platform evidence set per
  review batch.

## D002 - Reject Unrepresentable TRS Operations

- Date: 2026-07-26
- Status: Accepted
- Context: `FTransform` stores editable translation, rotation, and scale.
  Rotated non-uniform scales can compose into affine shear, which cannot be
  represented exactly by those fields. Existing APIs returned a plausible but
  incorrect TRS and Scene PreserveWorld reparent reported success.
- Decision: Replace infallible transform composition with Try-based composition,
  inverse, and relative conversion. Return Identity plus failure when the exact
  matrix result is invalid, singular, or not representable as TRS. Scene
  hierarchy mutations validate representability before mutation and return
  `InvalidHierarchyOperation` on failure.
- Consequence: Existing callers migrate from `operator*` to explicit Try APIs.
  The Feature 004 and 017 contracts record this mathematical boundary; a future
  affine/shear transform type may broaden representability without changing the
  truthfulness of current APIs.

## D003 - Combine Atomic Logging Thresholds As Severity Masks

- Date: 2026-07-26
- Status: Accepted
- Context: Feature 005 requires category and global filtering to suppress macro
  arguments with at most one integer comparison, while both thresholds remain
  runtime-mutable. Plain enum storage caused data races, and applying the global
  threshold inside `FLog::LogMessage` evaluated already-suppressed arguments.
- Decision: Store category and global thresholds as relaxed atomics. `SG_LOG`
  converts each threshold to an enabled-severity mask, intersects the masks, and
  performs one final bit test before evaluating the format expression. Keep the
  internal `FLog::LogMessage` global check as a defensive direct-call and
  concurrent-reconfiguration guard.
- Consequence: Filter reads are race-free and do not need a registry-wide cached
  effective threshold. A concurrently changing configuration has snapshot
  semantics, while each individual threshold load/store remains atomic. Fatal
  behavior is verified in a native child process because assertion handlers do
  not and must not bypass process termination.
