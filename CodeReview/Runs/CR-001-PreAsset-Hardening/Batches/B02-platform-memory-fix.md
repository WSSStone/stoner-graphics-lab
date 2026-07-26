# B02-S20: Platform Selection And Memory Ownership Fix

## Scope

This packet resolves the two accepted findings from B02-S19:

- `CR001-B02-F016` (S2): Android and non-macOS Apple targets were silently
  classified as supported desktop platforms.
- `CR001-B02-F017` (S2): each macOS available-memory query leaked one Mach
  host send-right user reference.

The opaque platform-window representation and process RSS implementation were
not changed because inspection found their current contracts and ownership
paths sound.

## Implementation

Commit `7a78cc6db8ed80c4b6d373cd932677850f58abbe`:

1. Rejects Android before the generic Linux branch.
2. Uses Apple's canonical `TARGET_OS_OSX` classification and rejects every
   non-macOS Apple target with a clear compile-time diagnostic.
3. Adds a build-integrated compiler matrix that synthesizes supported
   Windows/macOS/Linux targets and unsupported Android/iOS/unknown targets.
4. Balances the send right returned by `mach_host_self()` with
   `mach_port_deallocate()` immediately after the host statistics call.
5. Adds a macOS-only maintained ownership test that performs 1,024 production
   queries and requires the host send-right reference count to remain stable.

The compiler matrix uses the toolchain already selected by SCons, passes
arguments directly to `subprocess` without a shell, and creates only an ignored
build stamp after all cases pass.

## Local Verification

- Standalone compiler matrix: passed.
- Strict Debug build with `-Werror`: passed and ran the matrix.
- Strict Release build with `-Werror`: passed and ran the matrix.
- Maintained macOS ownership suite: 3 passed, 0 failed.
- Original focused Mach probe:
  - before fix: `refs_before=1 refs_after=1025 delta=1024`;
  - after fix: `refs_before=1 refs_after=1 delta=0`.
- Full deterministic test executable with the repository's established
  optional-native skip: passed.
- Strict ASan/UBSan build and full deterministic suite: passed.
- `git diff --check`: passed.

The authoritative gate records are:

- `Evidence/gate-strict-debug.json`
- `Evidence/gate-strict-release.json`
- `Evidence/gate-sanitizers.json`
- `Evidence/b02-platform-memory-fix-probes.md`

An unskipped local test run passed all new Core platform checks but failed
three existing optional Feature 019 native readback assertions. The same
binary passed with `STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1`; this variability
is outside the B02 repair and is recorded rather than attributed to it.

## Finding State

- `CR001-B02-F016`: Fixed at `7a78cc6`; Windows/Linux/MSVC behavior remains
  pending hosted verification.
- `CR001-B02-F017`: Fixed at `7a78cc6`; local macOS ownership is directly
  verified, while the finding remains Fixed until B02-S21 completes the
  batch's cross-platform verification protocol.

No GitHub Actions run was requested in this packet.
