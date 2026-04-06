# Feature Specification: SCons Project Skeleton

**Feature Branch**: `001-scons-project-skeleton`
**Created**: 2026-04-06
**Status**: Draft (Clarified)
**Input**: User description: "As we promoted SCons as the compiling utils, we first need a file structure and necessary files for the SCons building-oriented project. This over-the-top files/dirs structure design is essential for the whole coming project as it works as a skeleton."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Initialize and Build the Project from Scratch (Priority: P1)

A developer clones the repository for the first time and wants to build the entire project on their platform (Windows, macOS, or Linux). They run a single top-level SCons command and the build system discovers all source directories, resolves dependencies, and produces the expected output artifacts without any manual configuration beyond installing SCons and a supported C++ compiler.

**Why this priority**: Without a working build from a clean checkout, no other feature can be developed, tested, or integrated. This is the absolute foundation.

**Independent Test**: Can be fully tested by cloning the repo on each supported platform, running `scons` at the project root, and verifying that the build completes with zero errors and produces at least one placeholder output artifact (e.g., a static library or test executable).

**Acceptance Scenarios**:

1. **Given** a fresh clone of the repository on Windows with MSVC installed, **When** the developer runs `scons` at the project root, **Then** the build completes successfully with zero errors and produces the expected output artifacts.
2. **Given** a fresh clone of the repository on macOS (Apple Clang) or Linux (GCC/Clang), **When** the developer runs `scons` at the project root, **Then** the build completes successfully with zero errors and produces the expected output artifacts.
3. **Given** the project root, **When** the developer inspects the directory tree, **Then** they find a clear, well-organized folder hierarchy with 5 distinct source layers (Core, Application, Renderer, RHI, Backend), plus separate directories for third-party dependencies, build output, tests, and specification documents.

---

### User Story 2 - Add a New Module or Sub-Library (Priority: P2)

A developer wants to add a new engine module (e.g., a new RHI backend or a new rendering subsystem). They create a new directory following the established naming and structure conventions, add a local `SConscript` file using the project's conventions, and the top-level build automatically picks it up without modifying the root `SConstruct`.

**Why this priority**: The skeleton must be extensible. If adding new modules requires editing many unrelated files, the architecture fails its purpose as a scalable foundation.

**Independent Test**: Can be tested by creating a minimal stub module directory with a `SConscript`, running `scons`, and verifying the new module is discovered and compiled.

**Acceptance Scenarios**:

1. **Given** the established project skeleton, **When** a developer creates a new module directory with a conforming `SConscript` under the designated source tree, **Then** running `scons` at the root automatically discovers and builds the new module.
2. **Given** a new module with an intentional compile error, **When** the developer runs `scons`, **Then** the build reports the error clearly with the correct file path and line number.

---

### User Story 3 - Configure Platform-Specific Build Variants (Priority: P3)

A developer wants to build the project in different configurations (Debug, Release, or a specific platform target). They pass a build variant parameter to SCons (e.g., `scons config=debug` or `scons config=release`) and the build system applies the correct compiler flags, preprocessor definitions, and output directories for that variant.

**Why this priority**: Build variants are essential for day-to-day development (Debug) and shipping (Release), but the skeleton can function with a single default configuration initially.

**Independent Test**: Can be tested by running `scons config=debug` and `scons config=release` and verifying that output artifacts land in separate directories and that debug builds include debug symbols while release builds enable optimizations.

**Acceptance Scenarios**:

1. **Given** the project skeleton, **When** the developer runs `scons config=debug`, **Then** the output artifacts are placed in a debug-specific output directory and compiled with debug flags (e.g., `-g`, `/Zi`).
2. **Given** the project skeleton, **When** the developer runs `scons config=release`, **Then** the output artifacts are placed in a release-specific output directory and compiled with optimization flags (e.g., `-O2`, `/O2`).
3. **Given** no explicit config parameter, **When** the developer runs `scons`, **Then** the build defaults to the Debug configuration.

---

### Edge Cases

- What happens when SCons is not installed or is the wrong version? The build should fail early with a clear error message stating the required SCons version (4.10.1).
- What happens when no supported C++ compiler is found? The build should fail early with a diagnostic message listing supported compilers per platform.
- What happens when a `SConscript` file has a syntax error? SCons should report the error with the file path and line number, and the rest of the build should not proceed.
- What happens when the developer runs `scons --clean`? All generated build artifacts should be removed, but source files and specification documents should remain untouched.

## Architecture & Design Constraints *(mandatory)*

- **5-Layer Architecture**: The project adopts a 5-layer architecture with strict adjacent-only dependencies:

  ```
  Application  ──→  Renderer  ──→  RHI  ←──  Backend (implements RHI interfaces)
       │                │            │            │
       └────────────────┴────────────┴────────────┘
                         │
                      Core/Common
  ```

  - **Core**: Shared utilities (math, containers, logging, platform abstraction). Zero dependencies on any rendering layer. All other layers may depend on Core.
  - **Application**: Game engine frontend — scene graph, input, physics, high-level orchestration. Depends on Renderer and Core only.
  - **Renderer**: High-level rendering — materials, lighting, render passes, ray tracing, meshlet processing, global illumination. Depends on RHI and Core only.
  - **RHI**: Abstract hardware interface — IDevice, ICommandBuffer, IBuffer, IPipeline. Depends on Core only.
  - **Backend**: API-specific implementations (Vulkan, DX12, DX11, Metal, OpenGL, GLES, WebGL). Implements RHI interfaces. Depends on RHI and Core only.

  Each layer can ONLY depend on its immediate neighbor below (plus Core). Skip-level dependencies (e.g., Application including RHI headers directly) are PROHIBITED.

- **RHI Abstraction**: The directory structure MUST enforce the layered architecture by physically separating all 5 layers into distinct directory subtrees under `Source/`. No source file in a higher layer may include headers from a lower non-adjacent layer or from a specific Backend directory.
- **Build Artifact Isolation**: Each layer MUST compile into its own static library (`.a`/`.lib`). Each Backend implementation MUST also compile as a separate static library. The final executable links all required layer libraries. This enforces layer boundaries at link time.
- **Design Patterns**: The build system itself MUST avoid monolithic build scripts. Build logic MUST be decomposed into reusable SCons tool modules and per-directory `SConscript` files following the Composite pattern.
- **Advanced Graphics**: Advanced rendering subsystems (Ray Tracing, Meshlet processing, Global Illumination) MUST reside within the Renderer layer as sub-modules (e.g., `Source/Renderer/RayTracing/`). They are rendering techniques that consume RHI primitives, not RHI-level abstractions.
- **Naming Conventions**: All directory names and file names MUST follow the project's PascalCase, UE5-style naming conventions where applicable to C++ source. Build script files follow SCons conventions (`SConstruct`, `SConscript`).
- **Cross-Platform Compatibility**: The build configuration MUST work identically on Windows, macOS, and Linux. Platform-specific compiler flags and toolchain detection MUST be handled by the build system, not by the developer manually editing files.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The project MUST have a single top-level `SConstruct` file that serves as the entry point for all builds.
- **FR-002**: Each major module or subsystem MUST have its own `SConscript` file that defines its build targets, source files, and local dependencies.
- **FR-003**: The directory structure MUST follow a flat `Source/` root layout with 5 distinct layer directories:

  ```
  stoner-graphics-lab/
  ├── SConstruct                  # Root build entry point
  ├── Source/
  │   ├── Core/                   # Shared utilities (math, containers, logging, platform)
  │   │   └── SConscript
  │   ├── Application/            # Game engine frontend (Scene Graph, Input, Physics)
  │   │   └── SConscript
  │   ├── Renderer/               # High-level rendering (Materials, Lighting, Passes, RT, Meshlets, GI)
  │   │   └── SConscript
  │   ├── RHI/                    # Abstract interface (IDevice, ICommandBuffer, IBuffer)
  │   │   └── SConscript
  │   └── Backend/                # API-specific implementations
  │       ├── Vulkan/
  │       ├── DX12/
  │       ├── DX11/
  │       ├── Metal/
  │       ├── OpenGL/
  │       ├── GLES/
  │       ├── WebGL/
  │       └── SConscript
  ├── ThirdParty/                 # External dependencies
  ├── Tests/                      # Test suites
  ├── Build/                      # Output (gitignored): Build/<Platform>/<Config>/
  ├── specs/                      # Feature specifications
  └── .specify/                   # Spec-kit configuration
  ```
- **FR-004**: The build system MUST automatically detect the host operating system and select the appropriate default compiler toolchain (MSVC on Windows, Apple Clang on macOS, GCC or Clang on Linux).
- **FR-005**: The build system MUST support at least two build configurations (Debug and Release) selectable via a command-line parameter.
- **FR-006**: The build output MUST be placed in a dedicated output directory (outside the source tree) organized by platform and configuration (e.g., `Build/Win64/Debug/`).
- **FR-007**: The build system MUST validate that the installed SCons version meets the minimum requirement (4.10.1) and fail with a clear message if not.
- **FR-008**: The directory structure MUST include placeholder directories for all planned Backend implementations (Vulkan, DX12, DX11, Metal, OpenGL, GLES, WebGL) under `Source/Backend/` to establish the architectural skeleton.
- **FR-011**: Each of the 5 source layers (Core, Application, Renderer, RHI, Backend) MUST compile into its own static library. Each Backend sub-directory (Vulkan, DX12, etc.) MUST also compile as a separate static library.
- **FR-012**: The Renderer layer MUST include sub-directories for advanced rendering subsystems: `RayTracing/`, `Meshlets/`, and `GI/` (Global Illumination), even if initially empty.
- **FR-013**: The Core layer MUST provide shared utilities (math types, containers, logging, platform abstraction) that all other layers may depend on. Core MUST have zero dependencies on any rendering layer.
- **FR-014**: Inter-layer dependencies MUST be strictly adjacent-only (plus Core). The build system MUST enforce this by controlling include paths and link dependencies per layer.
- **FR-009**: The build system MUST support incremental builds — only recompiling files that have changed since the last build.
- **FR-010**: The project MUST include a `.gitignore` file that excludes build output directories and SCons cache/database files from version control.

### Key Entities

- **SConstruct**: The root build definition file. Defines global build environment, platform detection, configuration variants, and delegates to layer `SConscript` files.
- **SConscript**: Per-layer and per-module build definition files. Each defines the sources, include paths, link dependencies, and build targets for its scope.
- **Build Environment**: The SCons `Environment` object configured with platform-appropriate compiler, flags, and paths. Shared across layers via the hierarchical `SConscript` structure.
- **Layer**: One of the 5 architectural layers (Core, Application, Renderer, RHI, Backend). Each layer maps to a top-level directory under `Source/` and compiles into a static library.
- **Module**: A sub-unit within a layer (e.g., `Renderer/Materials`, `Renderer/RayTracing`, `Backend/Vulkan`). Each module has its own `SConscript` and may compile into the layer's library or its own sub-library.
- **Build Configuration**: A named set of compiler flags and preprocessor definitions (Debug, Release) that can be selected at build time.
- **Core**: The foundational utility layer providing math types (`FVector`, `FMatrix`), containers, logging, and platform abstraction. Depended upon by all other layers.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A new developer can clone the repository and successfully build the project on any supported platform (Windows, macOS, Linux) within 5 minutes of installing prerequisites (SCons + compiler).
- **SC-002**: Adding a new engine module requires creating no more than 2 files (a source file and a `SConscript`) and modifying zero existing files.
- **SC-003**: The directory structure clearly communicates the 5-layer architecture — any developer can identify which layer (Core, Application, Renderer, RHI, Backend) a file belongs to by its path alone.
- **SC-004**: Build output is 100% separated from source — no generated files appear in source directories.
- **SC-005**: Switching between Debug and Release configurations requires only changing a single command-line parameter.
- **SC-006**: The project skeleton passes all constitution checks (RHI abstraction, design patterns, naming conventions, cross-platform compatibility).

## Assumptions

- Developers have SCons 4.10.1 (or compatible) and a supported C++ compiler pre-installed on their system.
- The project targets C++20 or later as specified in the constitution; the build system will configure this standard by default.
- Third-party dependencies will be managed as source or pre-built libraries placed in a designated directory; a package manager integration (e.g., Conan, vcpkg) is out of scope for this skeleton feature.
- The initial skeleton will include placeholder/stub source files to validate the build pipeline, but no actual engine logic.
- The `.specify/` directory and its contents are not part of the build and will be excluded from compilation.
- WebGL backend may require a separate toolchain (Emscripten); the skeleton will include the directory placeholder but full WebGL build support is deferred to a future feature.
