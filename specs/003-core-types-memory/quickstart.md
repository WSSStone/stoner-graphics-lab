# Quickstart: Core Foundation Types & Memory

**Feature**: 003-core-types-memory  
**Date**: 2026-04-27

## What This Feature Produces

This feature turns the Core layer skeleton into a usable foundation for later engine work. It provides:

- Project-wide fixed-width and size-related type vocabulary
- `FString`
- `FName`
- `TSharedPtr<T>` and `TUniquePtr<T>`
- `FMemory`
- `TArray<T>` and `TMap<K, V>`
- Core foundation verification inside the existing `StonerTest` executable

## Prerequisites

- Phase 001 SCons skeleton is complete.
- Current branch is `003-core-types-memory`.
- A supported C++20 compiler is available.
- SCons 4.10.1 or newer is available.

## Expected Public Include Usage

After implementation, a Core-only user should be able to include either the aggregate header:

```cpp
#include "Core/CoreMinimal.h"
```

Or focused headers:

```cpp
#include "Core/FPlatformTypes.h"
#include "Core/FString.h"
#include "Core/FName.h"
#include "Core/FMemory.h"
#include "Core/TArray.h"
#include "Core/TMap.h"
```

## Build

From the repository root:

```bash
scons
```

Expected result:

- Core static library builds successfully.
- Existing downstream skeleton layers still build.
- `StonerTest` builds successfully.

## Run Verification

Run the generated test executable from the build output directory for your platform/configuration.

Examples:

```bash
# Windows Debug example from the current SCons platform detector
Build/Win64/Debug/Tests/StonerTest.exe

# macOS Debug example
Build/macOS/Debug/Tests/StonerTest

# Linux Debug example
Build/Linux/Debug/Tests/StonerTest
```

The exact platform folder name is determined by the existing platform detection script. On the current Windows environment it is `Win64`.

Expected result:

- Process exits with code `0`.
- Core foundation verification reports no failures.

## Manual Validation Checklist

1. Verify `Core/CoreMinimal.h` exposes the new foundation headers.
2. Verify code can include focused headers without including higher engine layers.
3. Verify fixed-width type size checks pass.
4. Verify empty and non-empty string/name cases pass.
5. Verify aligned allocation returns correctly aligned memory for valid alignments.
6. Verify invalid memory requests fail deterministically.
7. Verify empty, insert, retrieve, copy, and move behavior for `TArray` and `TMap`.
8. Verify RHI, Backend, Renderer, and Application do not need changes beyond existing include-chain compatibility.

## Cross-Platform Verification

Run the build and verification flow on:

- Windows with MSVC
- macOS with Apple Clang or Clang
- Linux with GCC or Clang

The feature is complete only when the public behavior is consistent across all supported platforms.

## Troubleshooting

- If includes fail, confirm files are under `Source/Core/Public/Core/` and included as `Core/<Header>.h`.
- If downstream layers fail, confirm `CoreMinimal.h` remains compatible with existing skeleton includes.
- If aligned allocation fails on one platform, verify invalid alignment handling and the matching aligned deallocation path.
- If tests do not link, confirm new `.cpp` files are placed where `Source/Core/SConscript` and `Tests/SConscript` discover them.
