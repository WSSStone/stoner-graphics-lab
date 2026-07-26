# B02-S13 Logging Probe Evidence

- Probe sources: `/tmp/cr001_logging_*_probe.cpp`
- Host: macOS arm64
- Repository production changes from probes: none
- GitHub Actions used: none

## Global Early-Out Probe

Both binaries compiled with `-Wall -Wextra -Werror`.

| Profile | Flags | Output | Exit |
|---|---|---|---:|
| Debug-like | `-O0 -g` | `side_effect_count=1` | 1 |
| Release | `-O2 -DNDEBUG` | `side_effect_count=1` | 1 |

The log itself was suppressed, but its increment argument was evaluated before
`FLog::LogMessage` applied the global threshold.

## Threshold Race Probes

The probes were compiled with Clang ThreadSanitizer and
`-fno-omit-frame-pointer`. The global-threshold probe reported:

```text
WARNING: ThreadSanitizer: data race
Write: FLog::SetGlobalMinSeverity
Read:  FLog::GetGlobalMinSeverity
Location: GGlobalMinSeverity
ThreadSanitizer: reported 1 warnings
```

The category-only `-O0` probe independently reported:

```text
WARNING: ThreadSanitizer: data race
Write: FLogCategory::SetMinSeverity
Read:  FLogCategory::GetMinSeverity
Location: LogCore
ThreadSanitizer: reported 1 warnings
```

Both instrumented runs exited `134` after the sanitizer report. External
symbolizer startup warnings did not prevent function and global identification.

## Fatal Isolation Probe

An optimized Release child binary called:

```cpp
SG_LOG(LogCore, Fatal, "fatal-probe");
```

Observed output and status:

```text
[HH:MM:SS.mmm] LogCore: Fatal: fatal-probe
exit=134
```

This demonstrates the current implementation's `stderr`-then-abort behavior.
Repository search confirms the maintained suite never executes Fatal:
`TestFLogMessageSeverityRouting` skips it, and `TestFatalLogBehavior` logs at
Error severity instead.
