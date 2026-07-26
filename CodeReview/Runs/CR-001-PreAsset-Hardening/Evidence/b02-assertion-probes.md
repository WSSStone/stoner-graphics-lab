# B02-S16 Assertion Probe Evidence

- Probe sources: `/tmp/cr001_assertion_*_probe.cpp`
- Host: macOS arm64
- Repository production changes from probes: none
- GitHub Actions used: none

## Handler ThreadSanitizer Probe

Build:

```text
-O0 -g -DNDEBUG -fsanitize=thread -fno-omit-frame-pointer
```

Observed:

```text
WARNING: ThreadSanitizer: data race
Read:  FLog::HandleAssertionFailure
Write: FLog::SetAssertionHandler
Location: Stoner::Core::GAssertionHandler
ThreadSanitizer: reported 1 warnings
exit=134
```

The external symbolizer emitted startup warnings but TSan identified both
functions and the exact global.

## Default Handler Child

```json
{
  "debug": {
    "returncode": -5,
    "stdout": "",
    "stderr": "[HH:MM:SS.mmm] Assertion: Fatal: Assertion failed: false at <probe>:7"
  },
  "release": {
    "returncode": 42,
    "stdout": "",
    "stderr": "reached-after-check"
  }
}
```

Debug logs before `SIGTRAP`. Release reaches the sentinel because `SG_CHECK`
is removed.

## Release Evaluation Probe

```text
check=0 checkf=0 verify=1 format=0
exit=0
```

This proves local implementation behavior:

- `SG_CHECK` expression is not evaluated.
- `SG_CHECKF` expression and format argument are not evaluated.
- `SG_VERIFY` expression is evaluated exactly once.

## Static Contract Evidence

- `spec.md` FR-011 requires a resumable debugger break.
- `research.md` Decision 5 says `__builtin_trap` is not resumable and rejects
  it for assertions.
- `SGPlatformBreak.h` line 21 uses `__builtin_trap()` for GCC.
- Maintained tests at lines 646-755 install a custom handler around every
  assertion failure and never execute the default branch.
