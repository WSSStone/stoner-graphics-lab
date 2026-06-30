# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Stoner Graphics Lab is a cross-platform graphics engine built in C++20 with a strict 5-layer architecture. It follows a "Vulkan-first" approach with an abstract RHI (Render Hardware Interface) layer that backends into Vulkan, Metal, DX12, and OpenGL/GLES. The engine uses a spec-driven development workflow where every feature starts with a specification in the `specs/` directory.

## Build Commands

The project uses **SCons 4.10.1+** as its build system (not CMake).

```bash
# Build in Debug mode (default)
scons

# Build in Release mode
scons config=release

# Build with specific number of parallel jobs
scons -j8

# Clean build artifacts
scons -c
```

Build outputs go to `Build/<Platform>/<Config>/` (e.g., `Build/Mac/Debug/`).

### Running Tests

```bash
# Build and run all tests (from project root)
scons && Build/Mac/Debug/Tests/StonerTest
```

Test structure: each test module has a `.h` header declaring `Run*Tests()` returning `F*TestResult` (with `Passed`/`Failed` counts), and a `.cpp` implementation. `Tests/Main.cpp` calls all test runners sequentially and returns 0 only if all pass. To run a single test, temporarily modify `Main.cpp` to call only that test's `Run*Tests()`.

### Vulkan SDK

The Vulkan backend detects the SDK via the `VULKAN_SDK` environment variable. When the SDK is unavailable, the backend compiles with stub implementations (`STONER_VULKAN_SDK_AVAILABLE` preprocessor define). Set `VULKAN_SDK` to your Vulkan SDK path before building if you want the real Vulkan backend.

## Architecture

### Layer Dependency Graph

```
Application  (Scene Graph, Input, Demo)
    ↓
Renderer     (Materials, Render Graph, Lighting Pipelines)
    ↓
RHI          (IRHIDevice, IRHICommandBuffer, IRHIBuffer — pure virtual)
    ↓                ↓↓↓
Backend/Vulkan   Backend/Metal   Backend/DX12   Backend/OpenGL  (placeholder)
    ↓
Core         (Math, Memory, Logging, Platform Abstraction, Containers)
```

**Key rule**: Adjacent-only dependencies. Application never includes Backend headers directly. RHI never includes Core headers directly (it goes through its own public interface). Include paths are enforced at the build level via `LayerBuilder.BuildLayer()` which only adds permitted dependency `Public/` directories to `CPPPATH`.

### Directory Layout

Each layer under `Source/` follows the same pattern:
```
Source/<Layer>/
├── Public/<Layer>/    # Public headers (installed interface)
│   └── <Layer>Minimal.h  # Convenience header including the whole layer
├── Private/           # Implementation .cpp files
└── SConscript         # Build definition
```

For the Vulkan backend (which is under `Source/Backend/`), the structure is:
```
Source/Backend/Vulkan/
├── Public/VulkanRHI/  # Public Vulkan-RHI bridge headers
├── Private/            # Implementation .cpp files
└── SConscript
```

### Key Naming Conventions (UE5-style)

- `F` prefix for structs, classes, and value types: `FVector3`, `FMatrix4x4`, `FBox`
- `I` prefix for interfaces (pure virtual): `IRHIDevice`, `IRHICommandBuffer`
- `E` prefix for enums: `ERHIFormat`, `ERHIQueueType`, `ELogSeverity`
- `T` prefix for templates: `TArray<T>`, `TMap<K,V>`, `TSharedPtr<T>`
- `SG` prefix for engine-wide macros: `SG_LOG`, `SG_CHECK`, `SG_ASSERT`
- All types use `PascalCase` (no `snake_case` in type names)

### RHI Abstraction

The RHI layer (`Source/RHI/`) defines the graphics API contract. Backends implement these interfaces:
- `IRHIDevice` — device lifecycle, capability query
- `IRHICommandBuffer` — command recording
- `IRHICommandQueue` — submission
- `IRHIBuffer`, `IRHITexture`, `IRHISampler` — resources
- `IRHIGraphicsPipeline`, `IRHIComputePipeline` — pipeline state
- `IRHISwapchain` — presentation

Vulkan backend implements these in `Source/Backend/Vulkan/Public/VulkanRHI/` as `FVulkan*`.

### Core Layer (`Source/Core/Public/Core/`)

Provides the foundation: `FPlatformTypes.h` (fixed-width ints), `FString`, `FName`, `TArray`, `TMap`, `TSharedPtr`, `FMemory`, math types (`FVector2/3/4`, `FMatrix4x4`, `FQuat`, `FTransform`, `FMath`), logging (`FLog`, `SG_LOG`), assertions (`SG_CHECK`, `SG_ASSERT`), and platform abstraction (`FPlatformProcess`, `FPlatformFileSystem`, `FPlatformTime`, `FPlatformWindow`).

### Build System Internals

Custom SCons tools live in `site_scons/`:
- `PlatformDetect.py` — detects host OS, configures C++20 toolchain
- `BuildConfig.py` — applies Debug/Release compiler flags
- `LayerBuilder.py` — `BuildLayer()` compiles a layer as a static library with strict include isolation; `DiscoverSubModules()` auto-finds backend implementations

Each layer's `SConscript` calls `BuildLayer(env, lib_name, dependencies_list, ...)` which enforces that only declared dependencies are on the include path.

## Development Workflow

This project uses **spec-driven development** via the `/speckit` skill suite:

1. **Specify**: `/speckit.specify <description>` — creates a feature spec in `specs/<NNN>-<name>/spec.md`
2. **Plan**: `/speckit.plan` — creates technical plan in `specs/<NNN>-<name>/plan.md`
3. **Tasks**: `/speckit.tasks` — breaks plan into task list in `specs/<NNN>-<name>/tasks.md`
4. **Implement**: `/speckit.implement` — executes tasks
5. **Constitution**: `.specify/memory/constitution.md` — governing principles, must be checked on any architectural change

Feature branches follow the pattern `<NNN>-<short-description>` (e.g., `011-vulkan-pipeline-shader`).

## Current State

Phases 001–010 are complete (SCons skeleton through Vulkan command submission). Phase 011 (Vulkan Pipeline & Shader) is the current pending feature. See `doc/roadmap.md` for the full 24-phase plan and status of each phase.

## Important Files to Know

- `SConstruct` — root build entry point, delegates to each layer's SConscript
- `doc/roadmap.md` — full development roadmap with architecture principles
- `.specify/memory/constitution.md` — governing constitution (SSD, RHI abstraction, UE5 naming, cross-platform)
- `Source/Core/Public/Core/CoreMinimal.h` — includes everything in Core (use this to quickly get all Core types)
- `Source/RHI/Public/RHI/RHIMinimal.h` — includes all RHI interfaces
- `site_scons/LayerBuilder.py` — understanding this is key to adding new layers or backends
