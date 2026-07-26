# B02-S14: Logging System Fix

## Scope

This packet repaired the three accepted findings from B02-S13:

- `CR001-B02-F010`: globally filtered macro arguments were evaluated.
- `CR001-B02-F011`: runtime category/global thresholds had data races.
- `CR001-B02-F012`: Fatal routing and termination were not exercised.

The implementation commit is
`8303045d6b977ecc873033a2da3100756f347055`.

## Implementation

### Effective Filtering

`FLogCategory::MinSeverity` and `FLog::GlobalMinSeverity` now use relaxed
`std::atomic<ELogSeverity>` storage. `SG_LOG` converts both loaded thresholds to
enabled-severity masks, intersects them, and performs one final bit comparison
before evaluating the format expression.

This avoids a registry-wide cached effective threshold, which could become
stale during concurrent reconfiguration. `FLog::LogMessage` retains its
internal global check as defense for direct calls and threshold changes between
the macro snapshot and dispatch.

### Fatal Isolation

The test executable accepts a private Fatal child argument before running any
suite. The parent test launches the same executable using:

- `fork`, `exec`, and `waitpid` on POSIX;
- `CreateProcess`, redirected inheritable handles, and bounded wait on Windows.

Only child `stderr` is captured. The test requires the labeled Fatal message,
successful child launch/wait, and abnormal termination before fallback return
code 42. Debug debugger traps and Release aborts therefore both satisfy the
termination contract without terminating the parent runner.

### Contract Alignment

Feature 005 T011/T013 and its API verification contract now describe the
isolated Fatal test instead of the impossible assertion-handler override.
Decision D003 records the atomic severity-mask design and concurrency
semantics.

## Regression Coverage

- Global filtering now checks both output suppression and format-argument
  non-evaluation.
- A maintained concurrent read/write test exercises both runtime thresholds.
- Fatal labeling, `stderr` routing, and termination are tested in Debug and
  Release child processes.
- The logging suite reports `45 passed, 0 failed` in optimized Release.

## Finding State

F010, F011, and F012 are `Fixed` at `8303045`. They remain intentionally
unverified until B02-S15 performs independent verification and the B02
batch-boundary Windows/macOS/Linux CI supplies cross-platform child-process
evidence.

## Out Of Scope

Assertion-handler synchronization and assertion macro behavior remain assigned
to the later B02 assertion inspection/fix packets. No GitHub Actions run was
started by this packet.
