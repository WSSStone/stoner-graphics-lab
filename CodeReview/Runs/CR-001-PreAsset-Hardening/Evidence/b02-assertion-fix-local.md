# B02-S17 Local Assertion Fix Evidence

- Host: macOS arm64
- Compiler: Apple Clang through the repository SCons toolchain
- Implementation: `76063b27d6f3cbbb79fbcd488897af33a9504054`
- GitHub Actions used: none

## Strict Debug

```text
conda run -n stoner-cr scons config=debug strict=1
exit=0
flags=-O0 -g -Wall -Wextra -Werror -D_DEBUG
```

Final deterministic suite:

```text
STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1 Build/Mac/Debug/Tests/StonerTest
exit=0
Logging & Assertion tests passed=57 failed=0
Default assertion handler logs failure before Debug break: PASS
Default assertion handler triggers Debug break before fallback: PASS
Assertion handler replacement is safe during concurrent dispatch: PASS
```

## Strict Release

```text
conda run -n stoner-cr scons config=release strict=1
exit=0
flags=-O2 -Wall -Wextra -Werror -DNDEBUG
```

Final deterministic suite:

```text
STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1 Build/Mac/Release/Tests/StonerTest
exit=0
Logging & Assertion tests passed=50 failed=0
SG_CHECK strips expression evaluation and dispatch in Release: PASS
SG_CHECKF strips expression, format args, and dispatch in Release: PASS
SG_VERIFY false result does not dispatch in Release: PASS
Release assertion child reaches fallback after SG_CHECK stripping: PASS
Assertion handler replacement is safe during concurrent dispatch: PASS
```

## ThreadSanitizer Reproducer

The B02-S16 reproducer was rerun against the fixed `FLog.cpp`, after installing
an initial custom handler before the worker threads:

```text
/usr/bin/clang++ -std=c++20 -O0 -g -DNDEBUG \
  -fsanitize=thread -fno-omit-frame-pointer ...
TSAN_OPTIONS=halt_on_error=1 /tmp/cr001_assertion_handler_tsan_probe_fixed
exit=0
ThreadSanitizer reports=0
```

Before the fix, the same concurrent setter/dispatcher pattern exited 134 with
a data-race report for `GAssertionHandler`.

## Native Scope Note

The unskipped macOS run exited 1 only in three pre-existing optional deferred
native readback/semantic assertions. Every logging/assertion test passed in
that run. The documented deterministic profile skipped this optional path and
the full suite exited 0. B02-S17 did not change deferred rendering.

Linux GCC and Windows MSVC evidence remains assigned to the B02 batch-boundary
CI rather than being inferred from this macOS run.
