# B02-S17: Assertion And Platform-Break Fix

## Scope

This packet repaired the three accepted findings from B02-S16:

- `CR001-B02-F013`: assertion-handler replacement raced with dispatch.
- `CR001-B02-F014`: maintained tests did not prove default-break or Release
  macro behavior.
- `CR001-B02-F015`: GCC used the non-resumable `__builtin_trap()`.

The implementation commit is
`76063b27d6f3cbbb79fbcd488897af33a9504054`.

## Implementation

`GAssertionHandler` is now an atomic function pointer. Replacement and
dispatch use relaxed store/load operations because handlers have static
lifetime and no associated state-publication contract.

`SG_DEBUG_BREAK()` retains `__debugbreak()` on MSVC and
`__builtin_debugtrap()` on Clang. GCC/POSIX now uses `raise(SIGTRAP)`, which a
debugger can intercept and resume. Feature 005's spec, plan, research, data
model, API contract, and completed task record describe the corrected mapping.

The shared logging child-process harness now accepts both Fatal and assertion
probe modes. Debug tests require the default assertion handler to log before
the child traps. Release tests require the child to emit no assertion and
reach sentinel exit code 42.

## Maintained Coverage

- `SG_CHECK` uses an expression-side-effect counter: once in Debug, zero in
  Release.
- `SG_CHECKF` separately counts expression and format-argument evaluation:
  once each in Debug, zero in Release.
- False `SG_VERIFY` evaluates once in both profiles and explicitly avoids
  handler dispatch in Release.
- Concurrent handler replacement and dispatch invokes exactly 256 installed
  handlers without entering the default break.

## Verification

The exact implementation commit passed:

```text
strict Debug build:   exit 0 (-Wall -Wextra -Werror)
Debug deterministic:  exit 0, logging/assertion 57 passed, 0 failed
strict Release build: exit 0 (-Wall -Wextra -Werror)
Release deterministic: exit 0, logging/assertion 50 passed, 0 failed
post-fix TSan probe:  exit 0, no ThreadSanitizer report
git diff --check:     exit 0
```

The first strict Release build correctly rejected two test-local lambdas that
became unused after macro stripping. The final tests place the side-effect
expressions directly in the macro arguments, which better proves that Release
does not materialize or evaluate them.

An unskipped local macOS native run still reported three existing deferred
attachment semantic/readback failures. Re-running the documented deterministic
profile with `STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1` passed. This packet did
not alter deferred rendering, and it does not claim local native validation.

## Finding State

F013, F014, and F015 are `Fixed` at `76063b2`. B02-S18 must independently
verify them. The B02 batch-boundary CI must compile and run the GCC/POSIX path
on Linux and the MSVC child-process path on Windows before they become
`Verified`.

No GitHub Actions run was started by this packet.
