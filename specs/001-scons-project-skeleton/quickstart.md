# Quickstart: SCons Project Skeleton

**Feature Branch**: `001-scons-project-skeleton`

## Prerequisites

- **SCons** 4.10.1 or later: `pip install scons==4.10.1`
- **C++ Compiler**:
  - Windows: MSVC (Visual Studio 2019+ with C++ workload)
  - macOS: Apple Clang (Xcode Command Line Tools: `xcode-select --install`)
  - Linux: GCC 10+ or Clang 12+ (`sudo apt install g++` or `sudo apt install clang`)

## Build from Clean Checkout

```bash
# Clone the repository
git clone <repo-url> stoner-graphics-lab
cd stoner-graphics-lab

# Build (defaults to Debug configuration)
scons

# Build with Release configuration
scons config=release
```

**Expected output**: Static libraries for each layer in `Build/<Platform>/Debug/` (or `Release/`).

## Verify the Build

```bash
# Check that output artifacts exist
ls Build/

# Expected structure:
# Build/
# └── <Win64|Mac|Linux>/
#     └── <Debug|Release>/
#         ├── libCore.a          (or Core.lib on Windows)
#         ├── libRHI.a
#         ├── libRenderer.a
#         ├── libApplication.a
#         ├── libVulkanRHI.a
#         └── StonerTest         (test executable)
```

## Clean Build Artifacts

```bash
scons --clean
```

## Add a New Module

1. Create a directory under the appropriate layer:
   ```bash
   mkdir -p Source/Renderer/MyNewFeature/Public/MyNewFeature
   mkdir -p Source/Renderer/MyNewFeature/Private
   ```

2. Add a `SConscript` file:
   ```python
   # Source/Renderer/MyNewFeature/SConscript
   Import('env')
   local_env = env.Clone()
   sources = Glob('Private/*.cpp')
   Return('sources')
   ```

3. Add source files and build:
   ```bash
   scons
   ```
   The new module is automatically discovered — no changes to root `SConstruct` needed.

## Directory Overview

```
Source/
├── Core/           → Shared utilities (math, logging, platform)
├── Application/    → Game engine frontend (scene graph, input)
├── Renderer/       → High-level rendering (materials, lighting, RT, meshlets, GI)
├── RHI/            → Abstract hardware interface (IDevice, IBuffer)
└── Backend/        → API implementations (Vulkan, DX12, Metal, etc.)
```

## Dependency Rules

- Each layer can only include headers from its **immediate neighbor below** + **Core**
- Application → Renderer → RHI ← Backend (implements)
- All layers → Core
- Skip-level includes (e.g., Application → RHI) will cause **compile errors**
