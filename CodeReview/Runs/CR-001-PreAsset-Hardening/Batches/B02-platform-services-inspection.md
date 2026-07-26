# B02-S22: Platform Time, Filesystem, And Process Inspection

## Inspection Budget

The inspection covered one Core platform-services responsibility domain and
six production files, totaling 258 production lines:

1. `Source/Core/Public/Core/FPlatformTime.h`
2. `Source/Core/Private/FPlatformTime.cpp`
3. `Source/Core/Public/Core/FPlatformFileSystem.h`
4. `Source/Core/Private/FPlatformFileSystem.cpp`
5. `Source/Core/Public/Core/FPlatformProcess.h`
6. `Source/Core/Private/FPlatformProcess.cpp`

Supporting evidence included Feature 006 requirements, contract, data model,
maintained Core platform tests, all repository call sites, repository history,
strict public-header compilation, an ASan/UBSan filesystem race probe, an
ASan/UBSan dynamic-module probe, and concurrent timing stress. No production
implementation was changed.

## Requirement Mapping

- `006-FR-004` and `006-FR-005` require monotonic high-resolution timestamps
  and deterministic elapsed-time conversion without platform-specific call
  sites.
- `006-FR-006` through `006-FR-009` require local existence, recursive
  directory creation, byte-preserving read/write, and failure reporting that
  neither crashes nor marks partial work as success.
- `006-FR-010` and its clarification require dynamic modules to load only from
  explicit full or relative file paths, never an implicit loader search path.
- The Feature 006 data model requires a valid module handle to be released
  exactly once and invalid handles to remain recoverable no-ops.
- `006-FR-013` and `006-FR-014` require isolated Core public headers and direct
  unit coverage of these services.

## Findings

### CR001-B02-F018 - Accepted S2

`FPlatformFileSystem::ReadFile` sizes and value-initializes the output before
reading, then treats `eofbit` as success. When a regular file was truncated
from 256 MiB to one byte between sizing and reading, the function returned
`true` with a 256 MiB output. Only the first byte came from the file; the
remaining bytes were the vector's value-initialized zeros. This violates
`FR-009` by reporting a partial read as complete.

### CR001-B02-F019 - Accepted S2

`HasExplicitPathMarker` accepts slash, backslash, or colon on every platform.
On POSIX, backslash and colon are ordinary filename characters and do not stop
`dlopen` from searching configured loader paths. A macOS probe supplied a name
containing a backslash but no slash; the production validation accepted it and
`DYLD_LIBRARY_PATH` resolved the module. This bypasses the explicit-path-only
contract.

### CR001-B02-F020 - Accepted S2

`FDynamicModuleHandle` is a publicly writable, implicitly copyable raw pointer
wrapper. A copied handle remained `IsValid()` after the original was freed,
allowed stale lookup, and caused `FreeDynamicModule` to invoke `dlclose` again.
The type therefore cannot preserve the required exactly-once release
transition.

## Confirmed Strengths

- `FPlatformTime` uses `std::chrono::steady_clock`, which is steady on the
  current host. Eight threads completed 800,000 total ordered samples under
  ASan/UBSan, and exact 1.5-second conversions passed.
- `FPlatformFileSystem` uses `std::error_code` for metadata and directory
  operations, handles missing files and directory-as-file reads, recursively
  creates directories, and preserves UTF-8 path bytes through `u8path`
  construction.
- Empty and null symbol names are rejected before platform API calls.
- Missing module loads, missing symbol lookups, and release of an invalid
  handle return recoverable results on the maintained path.
- All three public headers compile independently with C++20, strict warnings,
  and only the Core public include root.
- No production caller beyond the maintained Core platform test currently
  owns a dynamic-module handle, so the move-only ownership correction has a
  contained migration surface.

## Coverage Gaps

- Filesystem tests cover stable files only and cannot detect short-read success
  after a concurrent size change.
- Dynamic-module tests cover a valid absolute path and a bare conventional
  module name, but not platform-native explicit-path grammar.
- Dynamic-module tests never copy a handle, so stale aliases and repeated
  release remain invisible.
- The success-path module test can be skipped when the hard-coded system path
  does not load, weakening cross-platform evidence.
- Large-file allocation failure remains a plausible boundary risk, but the
  macOS probe could not establish a reliable process memory limit; no separate
  finding was registered without reproducible evidence.

## B02-S23 Fix Packet

The next packet may repair only the three accepted findings:

1. Require exact byte-count completion and clear output on short reads.
2. Validate explicit module paths with native path grammar so POSIX names
   without `/` cannot enter loader search.
3. Make `FDynamicModuleHandle` opaque and single-owner/move-only, update
   lookup/release signatures, and add maintained lifecycle coverage.
