# B03-S02: RHI Device And Runtime Fix

## Scope

This packet fixes the two related Accepted S2 findings from B03-S01:

- `CR001-B03-F001`: live-object aggregation can wrap to zero.
- `CR001-B03-F002`: native proof can accept a deterministic request.

The production change is limited to
`Source/RHI/Public/RHI/FRHIRuntimeSnapshot.h`; the regression matrix is in
`Tests/RHICoreTests.cpp`. No backend ownership, capability, resource, or
presentation behavior changed.

## Implementation

### Non-Wrapping Live Count

`GetTotalLiveObjectCount()` now returns `Core::uint64`. The first category is
promoted before addition, so every subsequent operation is performed in
64-bit arithmetic. Individual category counters remain `uint32`, preserving
their compact diagnostic contract.

### Consistent Native Proof

`ProvesNativeExecution()` now requires all of the following:

1. The requested mode is explicitly `Native` or `NativeHeadless`.
2. The object mode is `RealRuntime`.
3. At least one native instance and one native device are live.

Unknown, deterministic, fallback, and incomplete states therefore fail closed.

## Regression Coverage

`TestRuntimeAndPresentationContracts` now checks:

- the existing positive `Native` proof;
- positive `NativeHeadless` proof;
- `UINT32_MAX + 1` produces `4294967296`, not zero;
- a deterministic request with real-runtime objects is rejected;
- deterministic fallback is rejected;
- zero native instances are rejected;
- zero native devices are rejected.

## Local Gates

- Strict Debug compile: passed with `-Werror`.
- Strict Release compile: passed with `-Werror`.
- Standalone strict C++20 contract probe: passed.
- Strict graphics-disabled deterministic suite: passed, 757 result lines and
  no `[FAIL]` records.
- ASan/UBSan suite: passed with optional deferred-native validation skipped by
  the predefined sanitizer profile.

The full native-enabled test binary still reproduces the three mapped
attachment failures already tracked by Accepted finding `CR001-B08-F001`.
Both authorized attempts produced the same log hash. The triangle report
failure seen in the first sandboxed attempt was separately reproduced as
`EPERM` when the process tried to write into the external CR worktree; an
authorized run removed that false failure. Neither condition changes the
status of the two RHI fixes.

## Commit

`98c97a5 fix(rhi): harden runtime snapshot proof`

Both findings are `Fixed`. Independent parent-defect reproduction and
verification remain the responsibility of B03-S03.
