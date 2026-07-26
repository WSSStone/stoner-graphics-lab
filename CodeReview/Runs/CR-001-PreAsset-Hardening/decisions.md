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
