# B02-S23: Platform Services Contract Fix

## Scope

This packet resolves the three accepted findings from B02-S22:

- `CR001-B02-F018` (S2): a short filesystem read was reported as successful.
- `CR001-B02-F019` (S2): POSIX module names could bypass explicit-path
  validation and enter loader search.
- `CR001-B02-F020` (S2): copied module handles allowed stale access and
  repeated native release.

`FPlatformTime` was not changed because inspection and concurrent stress found
its monotonic clock and duration conversions sound.

## Implementation

Commit `1a3c4de2a8bd2e45c22777c778f087ed82192fa4`:

1. Adds a bounded exact-read helper that rejects byte counts outside
   `streamsize`, catches allocation and stream exceptions, requires the actual
   byte count to match, and clears output on failure.
2. Adds maintained exact, short, empty, and stale-output regression checks.
3. Replaces character-marker module validation with native
   `std::filesystem::path` grammar. POSIX requires a parent path; Windows
   accepts native parent or root-name syntax.
4. Uses `LoadLibraryW` with the native wide path on Windows instead of
   interpreting the engine's UTF-8 bytes through the active ANSI code page.
5. Makes `FDynamicModuleHandle` an opaque move-only owner with noexcept move
   operations, RAII destruction, private native state, const-reference symbol
   lookup, and idempotent explicit release.
6. Adds compile-time ownership traits, runtime move-transfer tests, and direct
   native-path predicate coverage.
7. Synchronizes the Feature 006 data model, API contract, and quickstart with
   the move-only ownership contract.

The deterministic helper contracts live in Core private headers. Only the
test target adds the Core private include root; runtime layers continue to
consume the public Core boundary.

## Local Verification

- Strict Debug fallback build with `-Werror`: passed.
- Full deterministic suite with the established optional-native skip: passed.
- Core platform suite: 41 passed, 0 failed.
- Strict Release build with `-Werror`: passed.
- Strict ASan/UBSan build and full maintained suite: passed.
- Public `FPlatformProcess.h` isolation compile: passed.
- Filesystem race reproduction:
  - before fix: success with 268,435,456 output bytes after truncation to one;
  - after fix: 12/12 attempts failed cleanly with zero output bytes.
- Dynamic-module reproduction:
  - before fix: loader search succeeded and the handle was copyable;
  - after fix: loader search was rejected, the handle was non-copyable,
    move transfer and symbol resolution succeeded, and repeated explicit
    release remained a no-op.
- `git diff --check`: passed.

The first direct deterministic-suite invocation lacked sandbox permission to
write its validation report in the external CR worktree and therefore reached
the expected report-failure path. Re-running the identical binary with
worktree write permission passed; this was an execution-environment failure,
not a product assertion.

## Finding State

- `CR001-B02-F018`: Fixed at `1a3c4de`.
- `CR001-B02-F019`: Fixed at `1a3c4de`.
- `CR001-B02-F020`: Fixed at `1a3c4de`.

All three remain pending independent and hosted verification in B02-S24.
No GitHub Actions run was requested in this packet.
