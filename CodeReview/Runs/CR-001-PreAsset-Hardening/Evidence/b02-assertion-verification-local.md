# B02-S18 Local Verification Evidence

- Host: macOS arm64
- Verified implementation: `76063b27d6f3cbbb79fbcd488897af33a9504054`
- Verification-only repository changes before CR records: none
- GitHub Actions used: none

## Probe Results

| Probe | Configuration | Result |
|---|---|---|
| Macro matrix | Clang Debug, `-O2 -D_DEBUG -Werror` | `1/1/1/1/3`, exit 0 |
| Macro matrix | Clang Release, `-O2 -DNDEBUG -Werror` | `0/0/0/1/0`, exit 0 |
| Default child | Repository Debug executable | diagnostic + exit 133 |
| Default child | Repository Release executable | no output + exit 42 |
| Debugger resume | Clang Debug under LLDB | breakpoint, continue, exit 0 |
| GCC branch selection | Clang with `__clang__` undefined | trap count 1, exit 0 |
| Handler stress | TSan, 4 setters + 4 dispatchers | 4096 calls, exit 0, 0 reports |

The initial Clang resumability probe attempted to handle the intrinsic with a
POSIX signal handler. On macOS the Mach exception stopped the process rather
than invoking that handler, so the probe was interrupted and replaced by LLDB
stop/continue verification. No repository process remained running.

## Gate Results

```text
fallback-strict  passed=true
strict-release  passed=true
sanitizers      passed=true
```

The sanitizer profile used:

```text
-fsanitize=address,undefined
-fno-omit-frame-pointer
-fno-sanitize-recover=all
STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1
```

The independent TSan profile used:

```text
-fsanitize=thread
-fno-omit-frame-pointer
TSAN_OPTIONS=halt_on_error=1
```

No TSan report file was produced and neither captured stream contained
`ThreadSanitizer`, `WARNING:`, or `data race`.

## Remaining Cross-Platform Evidence

The local branch-selection compile is intentionally classified as structural
evidence only. Required final evidence remains:

- Linux CI: real GCC strict Debug/Release compile and assertion child run.
- Windows CI: MSVC strict Debug/Release compile and assertion child run.

Until those batch-boundary jobs pass, CR001-B02-F013 through F015 remain
`Fixed`, not `Verified`.
