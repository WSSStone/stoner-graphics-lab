# B05-S05 Local Evidence

## Implementation

- Commit: `3dbfed0`
- Scope: queue submission atomicity, completion recovery, device provenance,
  explicit factory failure, and maintained regressions.
- Temporary edit mirror: removed before packet close.

## Strict Debug

Profile:

```text
crctl gate strict-debug
```

Result: exit 0. Project sources and maintained tests compiled with `-Werror`.

## Strict Deterministic Tests

Profile:

```text
crctl gate fallback-strict
```

Result: exit 0. The graphics-disabled strict Debug build and complete
maintained test executable passed, including:

- wait and signal preflight atomicity;
- duplicate/overlap rejection;
- command and synchronization device provenance;
- nonzero-timeout completion;
- not-ready and timeout recovery through wait-idle;
- idempotent observation after completion.

## Strict Release

Profile:

```text
crctl gate strict-release
```

Result: exit 0. Project sources and maintained tests compiled with `-Werror`.

## Graphics-Enabled Test Observation

The predefined `tests` profile built successfully but returned exit 1 in two
runs. A bounded five-run repetition of the same maintained executable produced
four passing runs and one failure containing exactly:

- `Deferred native validation completes a real Vulkan submission`;
- `Mapped attachment probes are finite, unique, and within semantic tolerances`;
- `Deferred native validation passes semantic probes and releases frame-owned objects`.

These assertions are already tracked by accepted finding
`CR001-B08-F001`. Every new B05 queue/synchronization regression passed in all
observed runs. No native implementation or native test was changed here.

## Repository Checks

- `git diff --check`: exit 0.
- Finding transitions: F004, F005, and F006 moved from Accepted to Fixed at
  `3dbfed0`.
- Production/test commit contains no CR state or report files.
- No remote CI or network operation was used.
