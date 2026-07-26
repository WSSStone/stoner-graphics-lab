# Build System Interface Contract

**Feature Branch**: `001-scons-project-skeleton`
**Date**: 2026-04-06
**Type**: CLI Build Interface

## Overview

The SCons build system exposes a command-line interface as its primary contract. Developers interact with the build system exclusively through `scons` commands at the project root.

## Command Interface

### Build (Default)

```bash
scons [config=<debug|release>] [strict=<0|1>] \
  [sanitizers=<none|address|undefined|address,undefined>]
```

| Parameter | Type | Default | Values | Description |
|-----------|------|---------|--------|-------------|
| `config` | string | `debug` | `debug`, `release` | Build configuration |
| `strict` | boolean | `0` | `0`, `1`, `false`, `true`, `no`, `yes`, `off`, `on` | Promote project compiler warnings to errors |
| `sanitizers` | string | `none` | `none`, `address`, `undefined`, `address,undefined` | Enable Clang/GCC sanitizer instrumentation |

Sanitizers require `config=debug` and a Clang/GCC toolchain on macOS or Linux.
Unsupported combinations fail explicitly instead of silently dropping a gate.

**Behavior**:
1. Validates SCons version ≥ 4.10.1
2. Detects host platform (Win64/Mac/Linux)
3. Selects default toolchain for platform
4. Applies config-specific compiler flags
5. Applies optional strict-warning and sanitizer instrumentation
6. Delegates to each layer's `SConscript`
7. Produces static libraries in `Build/<Platform>/<Config>/`

**Output Artifacts**:

| Artifact | Path | Description |
|----------|------|-------------|
| Core library | `Build/<Platform>/<Config>/libCore.a` | Shared utilities |
| RHI library | `Build/<Platform>/<Config>/libRHI.a` | Abstract interface |
| Renderer library | `Build/<Platform>/<Config>/libRenderer.a` | High-level rendering |
| Application library | `Build/<Platform>/<Config>/libApplication.a` | Engine frontend |
| Backend libraries | `Build/<Platform>/<Config>/libVulkanRHI.a` (etc.) | Per-API implementations |
| Test executable | `Build/<Platform>/<Config>/StonerTest` | Minimal test binary |

**Exit Codes**:

| Code | Meaning |
|------|---------|
| 0 | Build succeeded |
| 1 | Configuration error (bad SCons version, unknown config, no compiler) |
| 2 | Compilation error (source code issue) |

### Clean

```bash
scons --clean
```

**Behavior**: Removes all files under `Build/`. Source files, specs, and `.specify/` are untouched.

### Incremental Build

```bash
scons [config=<debug|release>]
```

**Behavior**: Same command as full build. SCons automatically detects changed files via its dependency scanner (`.sconsign.dblite`) and recompiles only what's needed.

## Include Path Contract

Each layer exposes headers via its `Public/` directory. The include path convention is:

```cpp
// From Application layer:
#include "Renderer/RendererMinimal.h"  // ✅ Adjacent dependency
#include "Core/CoreMinimal.h"          // ✅ Core dependency
#include "RHI/RHIMinimal.h"            // ❌ COMPILE ERROR — skip-level

// From Renderer layer:
#include "RHI/RHIMinimal.h"            // ✅ Adjacent dependency
#include "Core/CoreMinimal.h"          // ✅ Core dependency
```

## SConscript Contract

Each layer's `SConscript` receives the build environment from the parent and MUST:

1. Clone the environment (`env.Clone()`)
2. Append only its permitted include paths
3. Declare its source files
4. Return a static library target

```python
# Contract: SConscript signature
# Input: Import('env', 'platform', 'config')
# Output: Static library target via env.StaticLibrary()
```

## Error Messages Contract

| Condition | Message Format |
|-----------|---------------|
| SCons version too old | `ERROR: SCons {MINIMUM}+ required. Found: {actual}` |
| Unknown config | `ERROR: Unknown config '{value}'. Use 'debug' or 'release'.` |
| Invalid strict value | `ERROR: strict must be one of: ...` |
| Unknown sanitizer profile | `ERROR: sanitizers must be one of: ...` |
| Unsupported sanitizer combination | `ERROR: sanitizers require config=debug.` or platform-specific diagnostic |
| No compiler found | `ERROR: No supported C++ compiler found for {platform}. Expected: {list}` |
| Unknown platform | `ERROR: Unsupported platform '{sys.platform}'.` |
