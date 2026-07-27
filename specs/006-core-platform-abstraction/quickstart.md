# Quickstart: Core Foundation - Platform Abstraction Layer

**Feature**: 006-core-platform-abstraction  
**Date**: 2026-06-24

## What This Feature Produces

This feature adds the engine's Core platform abstraction layer. It provides:

- `SGPlatform.h` - supported platform detection macros
- `FPlatformMisc` - operating system, CPU, and memory diagnostics
- `FPlatformTime` - monotonic timestamps and duration conversion
- `FPlatformFileSystem` - basic local file existence, read, write, and recursive directory creation
- `FPlatformProcess` - explicit-path dynamic module loading, symbol lookup, and release
- `FPlatformWindow` - opaque native window handle representation
- Core platform verification in the existing `StonerTest` executable

## Prerequisites

- Core Types & Memory, Core Math, and Core Logging & Assertions are complete.
- Current branch is `006-core-platform-abstraction`.
- A supported C++20 compiler is available.
- SCons 4.10.1 or newer is available.

## Expected Public Include Usage

After implementation, any engine source file can use platform abstraction through focused headers:

```cpp
#include "Core/SGPlatform.h"
#include "Core/FPlatformMisc.h"
#include "Core/FPlatformTime.h"
#include "Core/FPlatformFileSystem.h"
#include "Core/FPlatformProcess.h"
#include "Core/FPlatformWindow.h"
```

Or via the aggregate header:

```cpp
#include "Core/CoreMinimal.h"
```

### Platform Identity

```cpp
#if SG_PLATFORM_WINDOWS
    // Windows-specific implementation boundary
#elif SG_PLATFORM_MAC
    // macOS-specific implementation boundary
#elif SG_PLATFORM_LINUX
    // Linux-specific implementation boundary
#endif
```

Exactly one supported platform macro should be active in a build.

### Platform Information

```cpp
const FString OSName = FPlatformMisc::GetOSName();
const uint32 CPUCores = FPlatformMisc::GetCPUCoreCount();
const uint64 AvailableMemory = FPlatformMisc::GetAvailableMemoryBytes();
```

Expected behavior:

- OS name is non-empty.
- CPU core count is at least `1`.
- Available memory is either a useful byte count or a documented unavailable/zero-equivalent result.

### Monotonic Timing

```cpp
const auto Start = FPlatformTime::Now();
// work...
const auto End = FPlatformTime::Now();
const double ElapsedMs = FPlatformTime::ToMilliseconds(End - Start);
```

Expected behavior:

- `End` is greater than or equal to `Start`.
- Converted durations are non-negative.

### File Operations

```cpp
FPlatformFileSystem::CreateDirectory("Build/Temp/CorePlatform/Nested");
FPlatformFileSystem::WriteFile("Build/Temp/CorePlatform/Nested/sample.bin", Payload);
const bool bExists = FPlatformFileSystem::Exists("Build/Temp/CorePlatform/Nested/sample.bin");
const TArray<uint8> ReadBack = FPlatformFileSystem::ReadFile("Build/Temp/CorePlatform/Nested/sample.bin");
```

Expected behavior:

- Missing parent directories are created recursively.
- Read-back payload matches the written payload byte-for-byte.
- Missing or inaccessible paths fail clearly without crashing.

### Dynamic Module Loading

```cpp
auto Module = FPlatformProcess::LoadDynamicModule("explicit/path/to/module");
const auto Symbol = FPlatformProcess::GetSymbol(Module, "EntryPointName");
FPlatformProcess::FreeDynamicModule(Module);
```

Expected behavior:

- The module path must be explicit.
- Bare module names and implicit platform search paths are not supported.
- Missing modules and missing symbols fail cleanly.
- Module handles are move-only single owners. Explicit release is optional
  because destruction releases remaining ownership; releasing invalid handles
  is safe.

### Native Window Handles

```cpp
FPlatformWindow EmptyWindow;
const bool bIsValid = EmptyWindow.IsValid();
```

Expected behavior:

- Empty handles are invalid but safe to copy and pass around.
- Wrapped native handles are opaque and do not require OS/windowing/graphics headers in public Core headers.

## Build

From the repository root:

```bash
scons
```

Expected result:

- Core static library builds successfully with new platform abstraction source files.
- Existing downstream skeleton layers still build.
- `StonerTest` builds successfully with the new Core platform test suite.

## Run Verification

Run the generated test executable from the build output directory:

```bash
# macOS Debug
Build/Mac/Debug/Tests/StonerTest

# Windows Debug
Build\Win64\Debug\Tests\StonerTest.exe

# Linux Debug
Build/Linux/Debug/Tests/StonerTest
```

Expected result:

- Process exits with code `0`.
- Existing Core Foundation, Core Math, Logging & Assertion tests still pass.
- Core platform verification reports no failures.

## Manual Validation Checklist

1. Verify `Core/CoreMinimal.h` includes the new platform abstraction headers.
2. Verify exactly one `SG_PLATFORM_*` macro is active.
3. Verify platform public headers compile without RHI, Backend, Renderer, Application, windowing framework, or graphics API headers.
4. Verify platform info returns a non-empty OS name and CPU core count >= 1.
5. Verify repeated timestamp samples never move backward.
6. Verify file write-read roundtrip preserves payload bytes up to 1 MB.
7. Verify recursive directory creation creates missing parent directories.
8. Verify missing file and directory-as-file cases fail without crashing.
9. Verify dynamic module loading requires explicit file paths.
10. Verify missing module and missing symbol cases fail cleanly.
11. Verify invalid dynamic module release is safe.
12. Verify empty and wrapped native window handles report validity correctly.

## Cross-Platform Verification

Run the build and verification flow on:

- Windows with MSVC
- macOS with Apple Clang or Clang
- Linux with GCC or Clang

The feature is complete only when the public behavior is consistent across all supported platforms.

## Troubleshooting

- If includes fail, confirm new headers are under `Source/Core/Public/Core/` and included as `Core/<Header>.h`.
- If downstream layers fail, confirm `CoreMinimal.h` remains compatible with existing includes.
- If platform macros conflict, confirm exactly one supported platform branch is selected.
- If file tests fail on paths with spaces, verify path strings are passed through without manual splitting.
- If dynamic module tests behave differently by OS, verify tests use explicit file paths and platform-conditional expected module suffixes.
- If timing tests fail, verify elapsed measurements use monotonic time rather than wall-clock time.
