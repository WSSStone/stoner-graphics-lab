# Data Model: SCons Project Skeleton

**Feature Branch**: `001-scons-project-skeleton`
**Date**: 2026-04-06

## Entities

### Layer

A top-level architectural unit that maps to a directory under `Source/` and compiles into a static library.

| Field | Type | Description |
|-------|------|-------------|
| Name | string | PascalCase layer name (e.g., `Core`, `Application`, `Renderer`, `RHI`, `Backend`) |
| SourceDir | path | `Source/{Name}/` |
| PublicIncludeDir | path | `Source/{Name}/Public/` |
| PrivateDir | path | `Source/{Name}/Private/` |
| OutputLibrary | path | `Build/{Platform}/{Config}/lib{Name}.a` (or `{Name}.lib` on Windows) |
| AllowedDependencies | Layer[] | Layers this layer may depend on (adjacent-only + Core) |

**Dependency Rules** (strict adjacent-only + Core):

| Layer | Allowed Dependencies |
|-------|---------------------|
| Core | (none) |
| RHI | Core |
| Backend/* | RHI, Core |
| Renderer | RHI, Core |
| Application | Renderer, Core |

### Module

A sub-unit within a Layer. May compile into the parent layer's library or its own sub-library.

| Field | Type | Description |
|-------|------|-------------|
| Name | string | PascalCase module name (e.g., `RayTracing`, `Meshlets`, `Vulkan`) |
| ParentLayer | Layer | The layer this module belongs to |
| SourceDir | path | `Source/{ParentLayer.Name}/{Name}/` |
| SConscript | path | `Source/{ParentLayer.Name}/{Name}/SConscript` |
| HasStub | boolean | Whether the module has placeholder source files (vs `.gitkeep` only) |

### BackendImplementation (extends Module)

An API-specific implementation of the RHI interfaces.

| Field | Type | Description |
|-------|------|-------------|
| APIName | string | Graphics API name (Vulkan, DX12, DX11, Metal, OpenGL, GLES, WebGL) |
| OutputLibrary | path | `Build/{Platform}/{Config}/lib{APIName}RHI.a` |
| PlatformSupport | Platform[] | Which platforms this backend supports |
| IsStubbed | boolean | Whether this backend has a full stub (true for Vulkan) or just `.gitkeep` |

**Platform Support Matrix**:

| Backend | Windows | macOS | Linux |
|---------|---------|-------|-------|
| Vulkan | ✅ | ✅ | ✅ |
| DX12 | ✅ | ❌ | ❌ |
| DX11 | ✅ | ❌ | ❌ |
| Metal | ❌ | ✅ | ❌ |
| OpenGL | ✅ | ✅ | ✅ |
| GLES | ❌ | ❌ | ✅ |
| WebGL | (Emscripten) | (Emscripten) | (Emscripten) |

### BuildConfiguration

A named set of compiler flags and preprocessor definitions.

| Field | Type | Description |
|-------|------|-------------|
| Name | string | `debug` or `release` (lowercase for CLI) |
| CxxFlags_MSVC | string[] | MSVC-specific compiler flags |
| CxxFlags_GCC | string[] | GCC/Clang-specific compiler flags |
| Defines | string[] | Preprocessor definitions |
| OutputSubDir | string | `Debug` or `Release` (PascalCase for output path) |

**Configuration Values**:

| Config | MSVC Flags | GCC/Clang Flags | Defines |
|--------|-----------|-----------------|---------|
| debug | `/Od /Zi /MDd /W4` | `-O0 -g -Wall -Wextra` | `_DEBUG` |
| release | `/O2 /MD /W4` | `-O2 -Wall -Wextra` | `NDEBUG` |

### Platform

The detected host platform.

| Field | Type | Description |
|-------|------|-------------|
| Name | string | `Win64`, `Mac`, or `Linux` |
| Toolchain | string | Default compiler toolchain |
| SysPlatform | string | `sys.platform` value |

**Platform Detection Map**:

| sys.platform | Platform.Name | Default Toolchain |
|-------------|---------------|-------------------|
| `win32` | `Win64` | MSVC |
| `darwin` | `Mac` | Apple Clang |
| `linux` | `Linux` | GCC (or Clang) |

## Relationships

```
Layer 1──* Module
Layer *──* Layer (dependency, constrained by adjacency rules)
BackendImplementation ──extends── Module
BackendImplementation *──* Platform (support matrix)
BuildConfiguration ──applied-to── Layer (all layers share same config)
Platform ──detected-by── SConstruct (at build time)
```

## State Transitions

### Build Lifecycle

```
[Clean Checkout] ──scons──→ [Configured] ──compile──→ [Built]
     │                           │                        │
     │                     (detect platform,         (static libs +
     │                      validate SCons,           test executable
     │                      select config)            in Build/)
     │                           │                        │
     └──────────────────────────────────────────── scons --clean ──→ [Clean Checkout]
```

### Module Discovery

```
[New Directory Created] ──has SConscript?──→ [Yes] ──→ [Auto-discovered by parent SConscript]
                              │
                             [No] ──→ [Ignored by build]
```

## Validation Rules

1. **Layer dependency**: A layer's `CPPPATH` MUST only include `Public/` directories of its allowed dependencies. Violation = compile error.
2. **SCons version**: `SCons.__version__` MUST be ≥ 4.10.1. Violation = `Exit(1)` with message.
3. **Config parameter**: `config` MUST be `debug` or `release`. Any other value = `Exit(1)` with message.
4. **Platform detection**: `sys.platform` MUST map to a known platform. Unknown platform = `Exit(1)` with message.
5. **Build output isolation**: All generated files MUST land under `Build/{Platform}/{Config}/`. Zero generated files in `Source/`.
