# Contract: Core Platform Abstraction Public API

**Feature**: 007-core-platform-abstraction  
**Date**: 2026-06-24

## Overview

This contract defines the public Core platform abstraction surface that all engine layers may depend on. The implementation may vary by supported operating system, but public names and observable behavior below must remain stable for this feature.

## Public Header Contract

All headers live under `Source/Core/Public/Core/` and are included as `Core/<Header>.h`.

| Header | Public Deliverable | Required Behavior |
|--------|--------------------|-------------------|
| `SGPlatform.h` | `SG_PLATFORM_WINDOWS`, `SG_PLATFORM_MAC`, `SG_PLATFORM_LINUX` | Exactly one supported platform macro active per build |
| `FPlatformMisc.h` | `FPlatformMisc` | OS name, CPU core count, available memory query |
| `FPlatformTime.h` | `FPlatformTime` | Monotonic timestamp and duration conversion helpers |
| `FPlatformFileSystem.h` | `FPlatformFileSystem` | Exists, read, write, recursive directory creation |
| `FPlatformProcess.h` | `FPlatformProcess` | Explicit-path dynamic module load, symbol lookup, release |
| `FPlatformWindow.h` | `FPlatformWindow` | Opaque native window handle wrapper |
| `CoreMinimal.h` | Core public aggregate | Updated to include platform abstraction headers |

## Namespace Contract

Public deliverables must be available through the Core namespace:

```cpp
namespace Stoner::Core
{
    struct FPlatformMisc;
    struct FPlatformTime;
    struct FPlatformFileSystem;
    struct FPlatformProcess;
    struct FPlatformWindow;
}
```

Macros (`SG_PLATFORM_WINDOWS`, `SG_PLATFORM_MAC`, `SG_PLATFORM_LINUX`) are in the global macro namespace as is standard for C++ macros.

## Behavioral Contract

### Platform Identity

- Exactly one of `SG_PLATFORM_WINDOWS`, `SG_PLATFORM_MAC`, and `SG_PLATFORM_LINUX` is `1`; the others are `0`.
- Public engine code uses `SG_PLATFORM_*` macros instead of raw OS/compiler macros.
- Unsupported platforms fail clearly rather than silently selecting an incorrect platform.

### `FPlatformMisc`

- Returns a non-empty operating system name for supported platforms.
- Returns a CPU core count of at least `1`.
- Returns available memory in bytes when the host platform can provide it.
- If available memory cannot be queried, returns a safe unavailable/zero-equivalent result without crashing.

### `FPlatformTime`

- Provides a monotonic timestamp suitable for elapsed-time measurements.
- Repeated timestamp reads never move backward during one process run.
- Provides conversion helpers for seconds, milliseconds, and microseconds.
- Does not use wall-clock time for elapsed measurements.

### `FPlatformFileSystem`

- Checks whether files or directories exist.
- Reads file contents byte-for-byte.
- Writes file contents byte-for-byte.
- Creates directories recursively, including missing parent directories.
- Reports failure for missing files, inaccessible paths, invalid paths, and file/directory type mismatches without crashing.
- Preserves paths with spaces. Non-ASCII paths either complete successfully or fail clearly without corrupting the path value.

### `FPlatformProcess`

- Loads dynamic modules only from explicit file paths.
- Bare module names and implicit platform search path lookup are out of scope.
- Resolves entry points by symbol name from a valid loaded module.
- Releases valid loaded modules.
- Treats missing modules, missing symbols, and invalid handles as recoverable failures.
- Releasing an invalid handle is a safe no-op.

### `FPlatformWindow`

- Represents an empty native window handle.
- Represents a non-empty platform-supplied native window handle.
- Supports validity checks.
- Can be copied, stored, and passed through public Core interfaces.
- Does not include OS, windowing framework, RHI, Backend, Renderer, Application, or graphics API headers in public Core headers.

## Verification Contract

The Core platform verification suite must cover:

- Exactly one `SG_PLATFORM_*` macro is active.
- `FPlatformMisc` returns non-empty OS name and CPU core count >= 1.
- Available memory query succeeds or reports unavailable safely.
- At least 1,000 rapid `FPlatformTime` samples never move backward.
- Duration conversion helpers produce non-negative elapsed values.
- File write-read roundtrip preserves 100% of bytes for representative text and binary payloads up to 1 MB.
- Recursive directory creation creates missing parent directories.
- Missing file, directory-as-file, and inaccessible/invalid paths fail without crashing.
- Dynamic module loading rejects missing explicit paths cleanly.
- Missing symbol lookup reports failure cleanly.
- Invalid dynamic module handles can be released safely.
- Empty `FPlatformWindow` handles are invalid but safe.
- Wrapped native window handle values report valid.
- Platform public headers compile without higher-layer or graphics API dependencies.

## Exclusions

- No full window creation or input/event system.
- No threading primitives.
- No networking.
- No process spawning.
- No environment variable API.
- No file watching, streaming I/O, permissions management, or virtual filesystem.
- No plugin lifecycle management.
- No RHI, Backend, Renderer, Application, windowing framework, or graphics API dependencies.
