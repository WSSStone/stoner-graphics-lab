# B02-S16: Assertion And Platform-Break Inspection

## Inspection Budget

The inspection covered one assertion responsibility domain and four production
files, totaling 335 production lines:

1. `Source/Core/Public/Core/SGAssert.h`
2. `Source/Core/Public/Core/SGPlatformBreak.h`
3. `Source/Core/Public/Core/FLog.h`
4. `Source/Core/Private/FLog.cpp`

Supporting evidence included the Feature 005 spec, research, data model, API
contract, tasks, quickstart, SCons build defines, maintained assertion tests,
history, and standalone Debug/Release/ThreadSanitizer probes. No production
implementation was changed.

## Requirement Mapping

- `005-FR-008` and `SC-004`: `SG_CHECK` and `SG_CHECKF` compile out in the
  current Release configuration, but maintained tests do not use side effects
  to prove expression and format-argument stripping.
- `005-FR-009` and `SC-005`: `SG_VERIFY` evaluates once in both configurations;
  the Release suite does not explicitly prove a false result avoids the handler.
- `005-FR-010` and `FR-011`: custom-handler tests cover expression, location,
  and formatted message, while the default debugger-break branch is never
  executed by maintained tests.
- `005-SC-008` and `SC-009`: all public macros are claimed as three-platform
  covered, but `SG_DEBUG_BREAK` lacks durable behavior coverage and the GCC
  implementation contradicts the clarified resumability contract.
- Feature 005 makes the assertion handler replaceable. Its process-global
  function pointer is read and written without synchronization.

## Reproduction

### Handler Race

A Release ThreadSanitizer probe changed the assertion handler on one thread
while another called `HandleAssertionFailure`. TSan reported:

```text
Read:  FLog::HandleAssertionFailure
Write: FLog::SetAssertionHandler
Location: GAssertionHandler
ThreadSanitizer: reported 1 warnings
```

### Build-Mode Behavior

An external default-handler child produced:

| Profile | Exit | stderr |
|---|---:|---|
| Debug | `-5` (`SIGTRAP`) | complete assertion diagnostic |
| Release | `42` | post-check sentinel only |

An independent Release side-effect probe reported:

```text
check=0 checkf=0 verify=1 format=0
```

The implementation therefore behaves correctly on local Clang, but this
evidence is outside the maintained suite.

### GCC Mapping

Feature 005 research explicitly rejects `__builtin_trap()` because it is not
resumable. `SGPlatformBreak.h` nevertheless selects that intrinsic for GCC,
while the clarified assertion contract requires a soft debugger stop from
which execution can continue.

## Findings

### CR001-B02-F013 - Accepted S2

`GAssertionHandler` is a plain mutable function pointer. Concurrent
`SetAssertionHandler` and assertion dispatch are a C++ data race, directly
reproduced by ThreadSanitizer.

### CR001-B02-F014 - Accepted S2

The maintained suite always installs a custom handler, so it never executes
the default break. Release checks use no side effects for `SG_CHECK` or
`SG_CHECKF`, and false `SG_VERIFY` does not assert that the handler remains
untouched. SC-003 through SC-005 and SC-009 therefore have incomplete evidence.

### CR001-B02-F015 - Accepted S2

The GCC branch uses `__builtin_trap()`, an unrecoverable trap instruction
already rejected by the feature research. This violates the clarified
resumable assertion behavior and makes Linux GCC differ from Clang/MSVC.

## Confirmed Strengths

- All three assertion macros evaluate their expression at most once.
- Debug custom-handler reports include file, line, expression, and optional
  formatted message.
- `SG_CHECK` and `SG_CHECKF` are fully stripped by the current Release
  preprocessor branch; `SG_VERIFY` preserves expression evaluation.
- Assertion sink writes share the logging mutex and release it before invoking
  user code, avoiding handler re-entry deadlock.
- Assertions do not call `std::abort`; Fatal logging remains a separate hard
  termination path.
- Clang uses resumable `__builtin_debugtrap`, and MSVC uses `__debugbreak`.

## Observations

- `SGAssert.h` and `SGPlatformBreak.h` incorrectly state that SCons defines
  `_DEBUG` in Release; `BuildConfig.py` defines only `NDEBUG` there.
- `SG_CHECKF` still uses the extension spelling `##__VA_ARGS__` rather than
  C++20 `__VA_OPT__`.
- Assertion formatting duplicates the timestamp/sink block for message and
  no-message cases. These are low-risk local cleanup opportunities for B02-S17,
  not independent findings.

## B02-S17 Fix Packet

The next packet may repair only these three related findings:

1. Make assertion-handler selection atomic and add concurrent dispatch
   coverage.
2. Extend the native child harness for the default assertion break and add
   explicit Release stripping/VERIFY assertions.
3. Replace the GCC trap with a resumable POSIX `SIGTRAP` raise, align the
   feature documents, and retain MSVC/Clang behavior.
