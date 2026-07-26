# B02-S20 Platform And Memory Fix Evidence

- Fix commit: `7a78cc6db8ed80c4b6d373cd932677850f58abbe`
- Host: macOS arm64
- GitHub Actions used: none

## Compiler Matrix

Standalone and SCons-integrated runs produced:

```text
Platform identity matrix passed: Windows, macOS, Linux; Android, iOS, unknown rejected
```

The positive cases require successful compilation and exact
`SG_PLATFORM_WINDOWS`, `SG_PLATFORM_MAC`, and `SG_PLATFORM_LINUX` values. The
negative cases require compilation failure containing the public
unsupported-platform diagnostic.

Both strict Debug and strict Release SCons builds executed the matrix and
completed successfully.

## Mach Ownership Reproduction

The same focused probe used in B02-S19 was rebuilt against the repaired
production implementation:

```text
queries=1024 refs_before=1 refs_after=1 delta=0 bytes=23332093952
exit=0
```

The maintained test independently reported:

```text
[PASS] FPlatformMisc ownership probe reads Mach host send-right references
[PASS] FPlatformMisc repeated available-memory queries preserve Mach host send-right references
[PASS] FPlatformMisc ownership probe releases its Mach host send right
[INFO] Core platform ownership tests passed=3 failed=0
```

## Build And Runtime Gates

| Gate | Result |
|---|---|
| Strict Debug | pass |
| Strict Release | pass |
| Full deterministic suite | pass |
| Strict ASan/UBSan build and suite | pass |

The sanitizer suite used the established
`STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1` setting and reported no ASan or UBSan
failure.

## Non-Attributed Native Result

One unskipped local run returned `1` because three optional Feature 019 native
readback assertions failed. All new platform identity and ownership checks
passed in that run. The deterministic and sanitizer profiles then passed, so
the native result is retained as context rather than evidence against this
Core platform fix.
