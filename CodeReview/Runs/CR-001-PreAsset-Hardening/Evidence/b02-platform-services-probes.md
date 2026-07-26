# B02-S22 Platform Services Probe Evidence

- Probe sources: `/tmp/cr001_b02_*_probe.cpp`
- Host: macOS arm64
- Repository production changes from probes: none
- GitHub Actions used: none

## Timing And Conversion

The probe statically required `steady_clock::is_steady`, checked exact
conversions for 1,500 milliseconds, and ran 100,000 ordered reads in each of
eight threads under ASan/UBSan:

```text
steady=1 exact_conversions=1 threads_monotonic=1
exit=0
```

No sanitizer diagnostic was emitted.

## Filesystem Short Read

The probe created a 256 MiB regular file, started the production
`ReadFile`, waited while its output allocation was in progress, and truncated
the file to one byte before the stream read. The ASan/UBSan build reported:

```text
attempt=0 returned=1 size=268435456 final_file_size=1
exit=3
```

The probe's exit `3` means the invalid state was reproduced: success was
reported with an output sized to the stale pre-truncation metadata. The
implementation's `File.good() || File.eof()` accepts the short read because
EOF and fail bits are set together.

## Loader Search Bypass

A probe dynamic library was created at:

```text
/tmp/libstoner\probe.dylib
```

The supplied API value was only `libstoner\probe.dylib`: it contained no POSIX
directory separator. Running the production loader with
`DYLD_LIBRARY_PATH=/tmp` produced:

```text
copyable=1
search_load=1
symbol=42
owner_after_free=0 alias_after_free=1
stale_symbol=0
alias_after_second_free=0
exit=3
```

`search_load=1` proves that the production marker check admitted a loader
search rather than requiring an explicit POSIX path.

## Module Ownership

The same ASan/UBSan probe loaded the module by its real absolute path and
copied the returned handle. It produced the same lifecycle tail:

```text
copyable=1
owner_after_free=0 alias_after_free=1
stale_symbol=0
alias_after_second_free=0
exit=3
```

The original wrapper was invalidated, but its copy still claimed validity.
The final call entered `dlclose` with the stale copied native handle. The host
did not crash, but the result is platform-dependent and the API has already
violated its exactly-once ownership contract.

## Public Header Isolation

Minimal translation units including only one of the following headers compiled
with `-std=c++20 -Wall -Wextra -Werror -fsyntax-only`:

```text
Core/FPlatformTime.h
Core/FPlatformFileSystem.h
Core/FPlatformProcess.h
```

All three exited `0` with no diagnostics.

## Discarded Allocation Probe

A sparse-file probe was also attempted to test allocation failure escaping
the boolean `ReadFile` API. macOS rejected the requested process data/address
limit with `EINVAL`, so the experiment did not provide controlled evidence.
No finding was created from that hypothesis.
