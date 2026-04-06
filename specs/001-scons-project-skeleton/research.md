# Research: SCons Project Skeleton

**Feature Branch**: `001-scons-project-skeleton`
**Date**: 2026-04-06
**Status**: Complete

## Research Tasks

### R-001: SCons Hierarchical Build Best Practices

**Task**: Research best practices for SCons 4.x hierarchical multi-directory builds.

**Decision**: Use `SConscript()` variant-dir delegation with `duplicate=0`.

**Rationale**: SCons natively supports hierarchical builds via `SConscript()` calls with `variant_dir` to redirect build output. The `duplicate=0` flag avoids copying source files into the build directory, keeping the source tree clean. This is the idiomatic SCons approach for multi-module projects and directly satisfies FR-006 (build output separation) and FR-009 (incremental builds).

**Alternatives Considered**:
- **Glob-based monolithic SConstruct**: Rejected — violates Design Pattern Discipline (Constitution III). A single file scanning all directories becomes a God-script.
- **CMake migration**: Rejected — Constitution mandates SCons 4.10.1. CMake is out of scope.
- **Recursive SCons invocations** (`subprocess`): Rejected — loses SCons' dependency graph advantages and breaks incremental builds.

**Key Implementation Details**:
```python
# SConstruct delegates to each layer:
SConscript('Source/Core/SConscript', variant_dir='Build/{platform}/{config}/Core', duplicate=0)
SConscript('Source/RHI/SConscript', variant_dir='Build/{platform}/{config}/RHI', duplicate=0)
# ... etc.
```

---

### R-002: Cross-Platform Toolchain Detection in SCons

**Task**: Research how SCons detects and configures compilers across Windows, macOS, and Linux.

**Decision**: Use SCons `Environment()` with platform-conditional tool selection.

**Rationale**: SCons auto-detects the default compiler on each platform via its `Tool()` mechanism. On Windows, `Tool('msvc')` selects MSVC; on POSIX, `Tool('default')` picks GCC/Clang. We add explicit detection logic in `Site_Scons/PlatformDetect.py` to:
1. Detect `sys.platform` → map to platform name (`Win64`, `Mac`, `Linux`)
2. Select appropriate tool (`msvc` on Windows, `clang` on macOS, `gcc`/`clang` on Linux)
3. Set C++20 standard flag (`/std:c++20` for MSVC, `-std=c++20` for GCC/Clang)

**Alternatives Considered**:
- **Manual `CC`/`CXX` environment variables**: Rejected — requires developer intervention, violates Cross-Platform Compatibility (Constitution VII).
- **SCons `Configure` checks**: Considered for future use (checking for specific headers/libs), but overkill for skeleton. Deferred.

**Key Implementation Details**:
```python
# PlatformDetect.py
import sys

def DetectPlatform():
    if sys.platform == 'win32':
        return 'Win64'
    elif sys.platform == 'darwin':
        return 'Mac'
    else:
        return 'Linux'

def ConfigureToolchain(env, platform):
    if platform == 'Win64':
        env.Tool('msvc')
        env.Append(CXXFLAGS=['/std:c++20', '/EHsc', '/W4'])
    elif platform == 'Mac':
        env.Append(CXXFLAGS=['-std=c++20', '-Wall', '-Wextra'])
    else:  # Linux
        env.Append(CXXFLAGS=['-std=c++20', '-Wall', '-Wextra'])
```

---

### R-003: SCons Build Configuration Variants (Debug/Release)

**Task**: Research how to implement Debug/Release build configurations in SCons.

**Decision**: Use SCons `AddOption()` for `--config` parameter with environment cloning.

**Rationale**: SCons doesn't have built-in "configurations" like CMake. The standard approach is:
1. Define a command-line option via `AddOption('--config', ...)` or `ARGUMENTS.get('config', 'debug')`
2. Apply configuration-specific flags to the `Environment`
3. Use `variant_dir` to separate output per configuration

Using `ARGUMENTS.get()` is simpler and more SCons-idiomatic than `AddOption()`.

**Alternatives Considered**:
- **Multiple SConstruct files** (SConstruct.debug, SConstruct.release): Rejected — violates FR-001 (single entry point) and duplicates logic.
- **SCons `Variables`**: Considered but `ARGUMENTS.get()` is simpler for a single parameter.

**Key Implementation Details**:
```python
# BuildConfig.py
def GetBuildConfig():
    config = ARGUMENTS.get('config', 'debug').lower()
    if config not in ('debug', 'release'):
        print(f"ERROR: Unknown config '{config}'. Use 'debug' or 'release'.")
        Exit(1)
    return config

def ApplyConfig(env, config):
    if config == 'debug':
        if env['PLATFORM'] == 'win32':
            env.Append(CXXFLAGS=['/Od', '/Zi', '/MDd'])
            env.Append(CPPDEFINES=['_DEBUG'])
        else:
            env.Append(CXXFLAGS=['-O0', '-g'])
            env.Append(CPPDEFINES=['_DEBUG'])
    else:  # release
        if env['PLATFORM'] == 'win32':
            env.Append(CXXFLAGS=['/O2', '/MD'])
            env.Append(CPPDEFINES=['NDEBUG'])
        else:
            env.Append(CXXFLAGS=['-O2'])
            env.Append(CPPDEFINES=['NDEBUG'])
```

---

### R-004: Include Path Enforcement for Layer Isolation

**Task**: Research how to enforce adjacent-only layer dependencies via SCons include paths.

**Decision**: Each layer's `SConscript` explicitly declares only its permitted include paths. No global include path.

**Rationale**: SCons `Environment.Append(CPPPATH=[...])` controls the compiler's `-I` flags. By giving each layer only the include paths of its permitted dependencies, the compiler itself enforces the layering:
- **Core**: `CPPPATH = ['Source/Core/Public']`
- **RHI**: `CPPPATH = ['Source/RHI/Public', 'Source/Core/Public']`
- **Backend/Vulkan**: `CPPPATH = ['Source/Backend/Vulkan/Public', 'Source/RHI/Public', 'Source/Core/Public']`
- **Renderer**: `CPPPATH = ['Source/Renderer/Public', 'Source/RHI/Public', 'Source/Core/Public']`
- **Application**: `CPPPATH = ['Source/Application/Public', 'Source/Renderer/Public', 'Source/Core/Public']`

If Application tries to `#include "RHI/RHIMinimal.h"`, the compiler will fail because `Source/RHI/Public` is not in Application's include path. This is compile-time enforcement of FR-014.

**Alternatives Considered**:
- **Global include path with lint rules**: Rejected — relies on external tooling, not build-system-enforced.
- **Header include guards with `#error`**: Rejected — fragile, requires manual maintenance.

---

### R-005: SCons `site_scons` Directory Convention

**Task**: Research SCons conventions for reusable build tool modules.

**Decision**: Use `site_scons/` directory (lowercase, SCons convention) for shared build utilities.

**Rationale**: SCons automatically adds `site_scons/` to the Python path, making modules importable from any `SConscript`. This is the official SCons mechanism for project-specific build tools. However, SCons expects the directory name to be `site_scons` (lowercase with underscore). We'll use this convention.

**Note**: The plan initially proposed `Site_Scons/` (PascalCase) to align with UE5 naming. However, SCons only auto-discovers `site_scons/` (lowercase). Since this is a build-system directory (not C++ source), the SCons convention takes precedence over UE5 naming. PascalCase applies to C++ source directories only.

**Alternatives Considered**:
- **PascalCase `Site_Scons/`**: Rejected — SCons won't auto-discover it. Would require manual `sys.path` manipulation.
- **Inline all logic in SConstruct**: Rejected — violates Design Pattern Discipline (Constitution III).

---

### R-006: SCons Version Validation

**Task**: Research how to validate SCons version at build time.

**Decision**: Check `SCons.__version__` at the top of `SConstruct` and call `Exit(1)` if below 4.10.1.

**Rationale**: SCons exposes its version as `SCons.__version__` (string). A simple version comparison at the top of `SConstruct` satisfies FR-007.

**Key Implementation Details**:
```python
# Top of SConstruct
import SCons
from packaging.version import Version  # or manual tuple comparison

MINIMUM_SCONS_VERSION = '4.10.1'

scons_ver = SCons.__version__
if tuple(int(x) for x in scons_ver.split('.')[:3]) < tuple(int(x) for x in MINIMUM_SCONS_VERSION.split('.')):
    print(f"ERROR: SCons {MINIMUM_SCONS_VERSION}+ required. Found: {scons_ver}")
    Exit(1)
```

**Note**: Avoid importing `packaging` (external dependency). Use tuple comparison on split version string instead.

---

### R-007: Public/Private Header Convention

**Task**: Research header organization patterns for multi-layer C++ engines.

**Decision**: Each layer uses `Public/` and `Private/` sub-directories following UE5 convention.

**Rationale**: Unreal Engine uses `Public/` for headers exposed to other modules and `Private/` for internal implementation. This convention:
1. Makes the API surface of each layer visually obvious
2. Allows `CPPPATH` to include only `Public/` directories, naturally hiding `Private/` headers
3. Aligns with Constitution Principle VI (UE5-style naming)

The `Public/` directory contains a sub-directory matching the layer name (e.g., `Public/Core/`) to create a namespace in include paths: `#include "Core/CoreMinimal.h"`.

**Alternatives Considered**:
- **Flat `include/` + `src/`**: Common in open-source C++ but doesn't match UE5 conventions.
- **Single directory with `_Private.h` suffix**: Rejected — naming convention is fragile and not enforceable by build system.

## Summary

All research tasks resolved. No NEEDS CLARIFICATION items remain. Key decisions:

| # | Topic | Decision |
|---|-------|----------|
| R-001 | Hierarchical build | `SConscript()` with `variant_dir`, `duplicate=0` |
| R-002 | Toolchain detection | `Site_Scons/PlatformDetect.py` with `sys.platform` |
| R-003 | Build configs | `ARGUMENTS.get('config', 'debug')` + per-config flags |
| R-004 | Layer isolation | Per-layer `CPPPATH` — compiler enforces boundaries |
| R-005 | Build tools dir | `site_scons/` (lowercase, SCons convention) |
| R-006 | Version check | `SCons.__version__` tuple comparison in `SConstruct` |
| R-007 | Header layout | `Public/` + `Private/` per layer (UE5 convention) |
