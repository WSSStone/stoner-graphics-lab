# B02-S24: Platform Time, Filesystem, And Process Verification

## Verified Heads

- Platform-service fix: `1a3c4de2a8bd2e45c22777c778f087ed82192fa4`
- Hosted-build correction: `c2427ea166764cacb3b3da9d25bded25e02d1380`
- Successful CI run: `30195707555`
- Successful Code Review Tools run: `30195707558`
- First hosted verification run: `30195411349` (failed and retained)

## Independent Parent Versus Current Probes

The same filesystem truncation probe was compiled against the fix parent and
current implementation:

```text
parent:  attempt=0 returned=1 size=268435456 final_file_size=1 exit=3
current: attempts=12 returned=0 size=0 final_file_size=1 exit=0
```

The current exact-read helper therefore rejects every observed short read and
clears the destination instead of reporting a zero-filled full payload.

The same dynamic-module ownership probe reported:

```text
parent:  copyable=1 search_load=1 owner_after_free=0 alias_after_free=1 exit=3
current: copyable=0 search_rejected=1 ownership_transferred=1
         symbol=1 release_idempotent=1 exit=0
```

The current platform-time probe additionally reported:

```text
steady=1 exact_conversions=1 threads_monotonic=1 exit=0
```

Isolated strict C++20 public-header compiles passed for `FPlatformTime.h`,
`FPlatformFileSystem.h`, and `FPlatformProcess.h`.

## Hosted Verification And Corrections

The first hosted run passed macOS but exposed two strict cross-platform build
gaps:

1. Linux rejected the macOS-only ownership-test `Record` helper as unused.
2. MSVC rejected the C++20 `__VA_OPT__` logging macro because the toolchain
   omitted `/Zc:preprocessor`.

Commit `c2427ea` conditionally compiles the helper with its macOS probe and
adds the standard MSVC preprocessor mode. A maintained Python unit test checks
the Windows C++20 flag contract.

The replacement CI run passed all seven project jobs:

1. Windows strict Debug and deterministic validation
2. macOS strict Debug and deterministic validation
3. Linux strict Debug, deterministic validation, and Lavapipe native gates
4. Windows strict Release
5. macOS strict Release
6. Linux strict Release
7. Linux ASan plus UBSan build and test suite

The separate Code Review Tools workflow passed its clean-environment test
suite. The Draft PR check summary contains only passing checks. GitHub's
Node.js 20 deprecation notices concern action runtimes and are not project
compiler warnings.

## Local Gates

- Strict Debug build: passed.
- Strict Release build: passed.
- Strict fallback build: passed.
- Earlier fix-packet ASan/UBSan and deterministic gates: passed.
- CR tool tests after the hosted-build correction: 20 passed.
- Patch whitespace validation: passed.

## Finding Decisions

- `CR001-B02-F004` through `CR001-B02-F017`: Verified. Their dedicated
  independent verification packets had passed locally and intentionally held
  them at `Fixed` until this final batch-boundary hosted matrix.
- `CR001-B02-F018`: Verified.
- `CR001-B02-F019`: Verified.
- `CR001-B02-F020`: Verified.

The independent probes are sensitive to each original defect, and the final
hosted matrix confirms that maintained behavior and ownership tests compile
and execute across all supported desktop hosts. Together with the earlier
verification of `CR001-B02-F001` through `CR001-B02-F003`, all twenty B02
findings are now `Verified`.
