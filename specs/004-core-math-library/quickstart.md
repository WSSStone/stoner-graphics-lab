# Quickstart: Core Foundation Math Library

**Feature**: 004-core-math-library  
**Date**: 2026-04-27

## What This Feature Produces

This feature adds the shared math vocabulary needed by later engine layers:

- `FMath`
- `FVector2`, `FVector3`, `FVector4`
- `FMatrix4x4`
- `FQuat`
- `FTransform`
- `FColor`
- `FBox`, `FSphere`, `FPlane`
- Core math verification inside the existing `StonerTest` executable

## Prerequisites

- Phase 001 SCons skeleton is complete.
- Phase 003 Core Types & Memory implementation is complete.
- Current branch is `004-core-math-library`.
- A supported C++20 compiler is available.
- SCons 4.10.1 or newer is available.

## Expected Public Include Usage

After implementation, a Core-only user should be able to include either the aggregate header:

```cpp
#include "Core/CoreMinimal.h"
```

Or focused headers:

```cpp
#include "Core/FMath.h"
#include "Core/FVector3.h"
#include "Core/FMatrix4x4.h"
#include "Core/FQuat.h"
#include "Core/FTransform.h"
#include "Core/FColor.h"
#include "Core/FBox.h"
#include "Core/FSphere.h"
#include "Core/FPlane.h"
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
Build/Mac/Debug/Tests/StonerTest

# Linux Debug example
Build/Linux/Debug/Tests/StonerTest
```

Expected result:

- Process exits with code `0`.
- Core foundation and Core math verification both report no failures.

## Manual Validation Checklist

1. Verify `Core/CoreMinimal.h` exposes the new math headers.
2. Verify focused math headers can be included without higher engine layers.
3. Verify vector arithmetic, dot product, cross product, length, and safe normalization.
4. Verify matrix identity, multiplication, transpose, inverse, and point/vector transforms.
5. Verify quaternion identity, normalization, composition, and rotation behavior.
6. Verify transform identity, composition, inverse, point transform, and direction transform.
7. Verify scalar math helpers, constants, and tolerance comparisons.
8. Verify color channel ordering and float/byte conversion behavior.
9. Verify box, sphere, and plane validity, containment, combination, and classification behavior.
10. Verify invalid and boundary cases are deterministic and documented.

## Cross-Platform Verification

Run the build and verification flow on:

- Windows with MSVC
- macOS with Apple Clang or Clang
- Linux with GCC or Clang

The feature is complete only when public behavior is consistent across all supported platforms within documented floating-point tolerances.

## Troubleshooting

- If includes fail, confirm files are under `Source/Core/Public/Core/` and included as `Core/<Header>.h`.
- If floating-point checks fail on one platform, confirm the test uses documented tolerance helpers rather than exact equality for computed values.
- If inverse tests fail, confirm singular matrix or transform cases report deterministic failure instead of undefined results.
- If downstream layers fail, confirm `CoreMinimal.h` remains compatible with existing skeleton includes.
- If tests do not link, confirm new `.cpp` files are placed where `Tests/SConscript` discovers them.
