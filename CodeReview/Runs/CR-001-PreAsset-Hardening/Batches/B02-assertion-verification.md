# B02-S18: Assertion And Platform-Break Verification

## Target

This packet independently verified implementation commit
`76063b27d6f3cbbb79fbcd488897af33a9504054` for:

- `CR001-B02-F013`: atomic assertion-handler replacement and dispatch.
- `CR001-B02-F014`: durable Debug/Release assertion behavior.
- `CR001-B02-F015`: resumable platform-break mapping.

No production, test, build, or Feature 005 file changed during verification.
All standalone probes remained under `/tmp`.

## Independent Probes

### Macro Matrix

A standalone program installed a custom handler and used independent counters
inside `SG_CHECK`, `SG_CHECKF`, and `SG_VERIFY`:

```text
Debug:   check=1 checkf=1 format=1 verify=1 handler=3
Release: check=0 checkf=0 format=0 verify=1 handler=0
```

Both optimized strict compilations and executions exited 0. This directly
proves expression evaluation, formatted-argument stripping, VERIFY retention,
and Release non-dispatch without relying on the maintained test runner.

### Default Handler Child

The maintained executable's private assertion mode was launched directly:

```text
Debug:   complete assertion diagnostic on stderr, exit=133 (SIGTRAP)
Release: empty stdout/stderr, exit=42
```

This independently confirms that Debug logs before the default break and that
Release removes `SG_CHECK` before the fallback return.

### Resumable Break

The Clang intrinsic was tested under LLDB. The process stopped at
`EXC_BREAKPOINT` immediately after `SG_DEBUG_BREAK()`. LLDB `continue` then
printed:

```text
continued_after_debug_break=1
Process exited with status = 0
```

A plain signal-handler attempt was intentionally abandoned after the Mach
exception stopped rather than dispatched through the process handler. That is
not the intrinsic's debugger-resume contract, so it is not counted as a
failure or as evidence.

For branch-level checking only, Apple Clang was compiled with `__clang__`
undefined so `SGPlatformBreak.h` selected its GCC/POSIX branch. Its installed
`SIGTRAP` handler observed `trap_count=1`, continued, and exited 0. This proves
the selected source branch behaves as designed, but it is not a substitute for
a real GCC build.

### Handler Stress

An independent ThreadSanitizer program used four concurrent setter threads and
four concurrent dispatcher threads. It performed 4096 assertion dispatches,
observed exactly 4096 handler calls, and exited 0:

```text
ThreadSanitizer reports=0
dispatch diagnostics=4096 complete lines
```

## Predefined Gates

- `fallback-strict`: passed strict graphics-disabled Debug build and full
  deterministic tests.
- `strict-release`: passed optimized `-Werror` build.
- `sanitizers`: passed strict ASan/UBSan build and full deterministic tests
  with the documented optional deferred-native skip.
- `git diff --check`: passed.

Machine-readable results are stored in:

- `Evidence/gate-fallback-strict.json`
- `Evidence/gate-strict-release.json`
- `Evidence/gate-sanitizers.json`

No GitHub Actions run was started.

## Verdict

F013, F014, and F015 remain `Fixed`. Their local behavior is independently
verified, but Feature 005's contract is cross-platform. A real Linux GCC run
must compile/execute the `raise(SIGTRAP)` branch, and Windows must compile/run
the MSVC child path before the findings can transition to `Verified`.

B02 batch-boundary CI owns that final evidence. This packet does not infer
cross-platform completion from Clang branch simulation.
