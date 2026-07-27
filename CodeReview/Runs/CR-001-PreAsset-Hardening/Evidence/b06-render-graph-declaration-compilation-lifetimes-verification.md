# B06-S03 Evidence: Render Graph Culled Resource Verification

Finding: `CR001-B06-F001`.

Fix commit: `7292a65`.

Verification summary:

- Parent `7292a65^` resolved every `Graph.Resources` entry before invoking
  scheduled passes.
- Current HEAD derives `RequiredResources` from compiled scheduled pass
  accesses and skips culled branch resources.
- Regression tests for culled missing imports and culled transient resolution
  failures are present and pass in focused local output.
- `fallback-strict`, `strict-release`, and `sanitizers` gates all pass after the
  fix.

Decision: mark `CR001-B06-F001` Verified.
