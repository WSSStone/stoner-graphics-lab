# B02-S23 Platform Services Fix Evidence

- Fix commit: `1a3c4de2a8bd2e45c22777c778f087ed82192fa4`
- Host: macOS arm64
- GitHub Actions used: none

## Exact-Read Reproduction

The B02-S22 ASan/UBSan probe was rebuilt unchanged against the repaired
filesystem implementation. It creates a 256 MiB regular file and truncates it
to one byte after metadata sizing but before the stream read.

Before the fix:

```text
attempt=0 returned=1 size=268435456 final_file_size=1
exit=3
```

After the fix, all twelve attempts produced:

```text
returned=0 size=0 final_file_size=1
exit=0
```

No ASan or UBSan diagnostic was emitted. Maintained tests independently cover
complete, short, and empty streams plus clearing a caller's stale output.

## Module Path And Ownership Reproduction

The same probe library remained available only through
`DYLD_LIBRARY_PATH=/tmp` when the API received the POSIX name
`libstoner\probe.dylib` without a slash. The repaired implementation produced:

```text
copyable=0 search_rejected=1 ownership_transferred=1 symbol=1 release_idempotent=1
exit=0
```

The probe then loaded the library by its real absolute path, moved ownership
through construction and assignment, resolved and called the exported symbol,
and explicitly released the final owner twice. ASan/UBSan reported no error.

## Maintained Core Platform Checks

The platform suite added and passed these checks:

```text
[PASS] FPlatformFileSystem exact-read helper preserves complete bytes
[PASS] FPlatformFileSystem short read fails and clears partial output
[PASS] FPlatformFileSystem empty read succeeds with empty output
[PASS] FPlatformProcess accepts a relative path with a parent directory
[PASS] FPlatformProcess rejects a bare module name
[PASS] FPlatformProcess rejects POSIX names with non-separator path markers
[PASS] FPlatformProcess move construction transfers module ownership
[PASS] FPlatformProcess move assignment transfers module ownership
[INFO] Core platform tests passed=41 failed=0
```

The public process header also compiled in an isolated strict C++20
translation unit with no operating-system or higher-layer includes.

## Build And Runtime Gates

| Gate | Result |
|---|---|
| Strict Debug fallback | pass |
| Full deterministic suite | pass |
| Strict Release | pass |
| Strict ASan/UBSan build and suite | pass |

Authoritative machine gate records:

- `Evidence/gate-strict-release.json`
- `Evidence/gate-sanitizers.json`

The sanitizer profile used the repository's established
`STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1` setting and completed without an ASan
or UBSan report.
