# Tasks: SCons Project Skeleton

**Input**: Design documents from `/specs/001-scons-project-skeleton/`
**Prerequisites**: plan.md ✅, spec.md ✅, research.md ✅, data-model.md ✅, contracts/ ✅, quickstart.md ✅

**Tests**: Not explicitly requested in spec — test tasks omitted. Manual build verification per quickstart.md.

**Organization**: Tasks grouped by user story. Each story is independently implementable and testable.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Path Conventions

- **Build scripts**: `SConstruct`, `site_scons/`, `Source/*/SConscript`
- **C++ source**: `Source/<Layer>/Public/<Layer>/`, `Source/<Layer>/Private/`
- **Backends**: `Source/Backend/<API>/`
- **Tests**: `Tests/`
- **Output**: `Build/<Platform>/<Config>/`

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create the complete directory skeleton and project-level configuration files

- [X] T001 Create the full directory tree for all 5 source layers with Public/Private sub-directories per plan.md structure under `Source/`
- [X] T002 [P] Create `.gitignore` at project root excluding `Build/`, `.sconsign.dblite`, `*.o`, `*.obj`, `*.a`, `*.lib`, `__pycache__/`
- [X] T003 [P] Create `.gitkeep` placeholder files in all empty directories: `Source/Renderer/RayTracing/.gitkeep`, `Source/Renderer/Meshlets/.gitkeep`, `Source/Renderer/GI/.gitkeep`, `Source/Backend/DX12/.gitkeep`, `Source/Backend/DX11/.gitkeep`, `Source/Backend/Metal/.gitkeep`, `Source/Backend/OpenGL/.gitkeep`, `Source/Backend/GLES/.gitkeep`, `Source/Backend/WebGL/.gitkeep`, `ThirdParty/.gitkeep`
- [X] T004 [P] Create `site_scons/` directory (lowercase per SCons convention, R-005) with empty `__init__.py`

**Checkpoint**: Directory skeleton exists. All paths from plan.md are present. No build logic yet.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Build system infrastructure that ALL user stories depend on. Implements the reusable SCons tool modules and root `SConstruct`.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [X] T005 Implement platform detection module in `site_scons/PlatformDetect.py` — `DetectPlatform()` returns `Win64`/`Mac`/`Linux` based on `sys.platform`; `ConfigureToolchain(env, platform)` sets C++20 flags per platform (R-002). Must handle unknown platform with `Exit(1)` and error message per contracts/build-interface.md.
- [X] T006 [P] Implement build configuration module in `site_scons/BuildConfig.py` — `GetBuildConfig()` reads `ARGUMENTS.get('config', 'debug')`, validates `debug`/`release`, returns config name; `ApplyConfig(env, config, platform)` applies Debug/Release compiler flags per data-model.md BuildConfiguration entity (R-003). Must handle unknown config with `Exit(1)` and error message per contracts/build-interface.md.
- [X] T007 [P] Implement layer builder helper in `site_scons/LayerBuilder.py` — `BuildLayer(env, layer_name, dependencies)` clones the environment, appends only permitted `CPPPATH` entries for the layer's allowed dependencies (per data-model.md Layer dependency rules and R-004), globs `Private/*.cpp` sources, and returns a `StaticLibrary` target. This enforces adjacent-only dependency rules at compile time (FR-014).
- [X] T008 Implement root `SConstruct` at project root — SCons version validation (R-006, FR-007), imports `PlatformDetect` and `BuildConfig` from `site_scons/`, creates base `Environment`, detects platform, applies config, delegates to each layer's `SConscript` via `SConscript()` with `variant_dir='Build/{platform}/{config}/{layer}'` and `duplicate=0` (R-001), exports `env`, `platform`, `config` variables. Must follow contracts/build-interface.md command interface behavior.

**Checkpoint**: Running `scons` at project root should succeed (even if no source files exist yet — empty libraries are OK). The build system infrastructure is complete.

---

## Phase 3: User Story 1 — Initialize and Build the Project from Scratch (Priority: P1) 🎯 MVP

**Goal**: A developer clones the repo and runs `scons` — the build completes with zero errors, producing static libraries for all 5 layers plus a test executable.

**Independent Test**: Run `scons` at project root on macOS/Linux. Verify `Build/<Platform>/Debug/` contains `libCore.a`, `libRHI.a`, `libRenderer.a`, `libApplication.a`, `libVulkanRHI.a`, and `StonerTest`.

### Implementation for User Story 1

- [X] T009 [P] [US1] Create Core layer placeholder header `Source/Core/Public/Core/CoreMinimal.h` — minimal header with include guard and a namespace declaration (`namespace Stoner::Core {}`)
- [X] T010 [P] [US1] Create Core layer placeholder source `Source/Core/Private/CoreModule.cpp` — includes `Core/CoreMinimal.h`, contains a placeholder function (e.g., `void CoreInit() {}`)
- [X] T011 [P] [US1] Create Core layer `Source/Core/SConscript` — imports `env`, uses `LayerBuilder.BuildLayer(env, 'Core', [])` (no dependencies), returns static library
- [X] T012 [P] [US1] Create RHI layer placeholder header `Source/RHI/Public/RHI/RHIMinimal.h` — minimal header with include guard, includes `Core/CoreMinimal.h`, declares namespace `Stoner::RHI`
- [X] T013 [P] [US1] Create RHI layer placeholder source `Source/RHI/Private/RHIModule.cpp` — includes `RHI/RHIMinimal.h`, contains placeholder function
- [X] T014 [P] [US1] Create RHI layer `Source/RHI/SConscript` — imports `env`, uses `LayerBuilder.BuildLayer(env, 'RHI', ['Core'])`, returns static library
- [X] T015 [P] [US1] Create Renderer layer placeholder header `Source/Renderer/Public/Renderer/RendererMinimal.h` — includes `RHI/RHIMinimal.h`, declares namespace `Stoner::Renderer`
- [X] T016 [P] [US1] Create Renderer layer placeholder source `Source/Renderer/Private/RendererModule.cpp` — includes `Renderer/RendererMinimal.h`, contains placeholder function
- [X] T017 [P] [US1] Create Renderer layer `Source/Renderer/SConscript` — imports `env`, uses `LayerBuilder.BuildLayer(env, 'Renderer', ['RHI', 'Core'])`, returns static library
- [X] T018 [P] [US1] Create Application layer placeholder header `Source/Application/Public/Application/ApplicationMinimal.h` — includes `Renderer/RendererMinimal.h`, declares namespace `Stoner::Application`
- [X] T019 [P] [US1] Create Application layer placeholder source `Source/Application/Private/ApplicationModule.cpp` — includes `Application/ApplicationMinimal.h`, contains placeholder function
- [X] T020 [P] [US1] Create Application layer `Source/Application/SConscript` — imports `env`, uses `LayerBuilder.BuildLayer(env, 'Application', ['Renderer', 'Core'])`, returns static library
- [X] T021 [P] [US1] Create Vulkan backend placeholder header `Source/Backend/Vulkan/Public/VulkanRHI/VulkanDevice.h` — includes `RHI/RHIMinimal.h`, declares namespace `Stoner::Backend::Vulkan`
- [X] T022 [P] [US1] Create Vulkan backend placeholder source `Source/Backend/Vulkan/Private/VulkanDevice.cpp` — includes `VulkanRHI/VulkanDevice.h`, contains placeholder function
- [X] T023 [P] [US1] Create Vulkan backend `Source/Backend/Vulkan/SConscript` — imports `env`, uses `LayerBuilder.BuildLayer(env, 'VulkanRHI', ['RHI', 'Core'])` with custom source path, returns static library
- [X] T024 [P] [US1] Create stub `SConscript` files for non-Vulkan backends: `Source/Backend/DX12/SConscript`, `Source/Backend/DX11/SConscript`, `Source/Backend/Metal/SConscript`, `Source/Backend/OpenGL/SConscript`, `Source/Backend/GLES/SConscript`, `Source/Backend/WebGL/SConscript` — each returns an empty source list (no sources to compile yet)
- [X] T025 [US1] Create Backend aggregator `Source/Backend/SConscript` — delegates to each backend sub-directory's `SConscript`, collects and returns all backend library targets
- [X] T026 [US1] Create test executable source `Tests/Main.cpp` — minimal `int main() { return 0; }` that includes `Application/ApplicationMinimal.h` to validate the full include chain
- [X] T027 [US1] Create test `Tests/SConscript` — imports `env`, clones environment, sets include paths for all layers, compiles `Main.cpp` and links against all layer static libraries to produce `StonerTest` executable
- [X] T028 [US1] End-to-end verification: run `scons` at project root, verify all expected output artifacts exist in `Build/<Platform>/Debug/` per contracts/build-interface.md output artifacts table

**Checkpoint**: User Story 1 complete. `scons` builds the entire project from a clean checkout. All acceptance scenarios (AS-1.1, AS-1.2, AS-1.3) are satisfied.

---

## Phase 4: User Story 2 — Add a New Module or Sub-Library (Priority: P2)

**Goal**: The build system auto-discovers new modules without modifying root `SConstruct`. A developer creates a directory + `SConscript` and it's picked up automatically.

**Independent Test**: Create a new directory `Source/Renderer/TestModule/` with a `SConscript` and a `.cpp` file, run `scons`, verify the new module compiles.

### Implementation for User Story 2

- [X] T029 [US2] Update `site_scons/LayerBuilder.py` to support sub-module auto-discovery — add a `DiscoverSubModules(layer_dir)` function that scans for sub-directories containing `SConscript` files and delegates to them via `SConscript()`. Integrate this into the Renderer layer's `SConscript` to auto-discover `RayTracing/`, `Meshlets/`, `GI/`, and any future sub-modules.
- [X] T030 [US2] Update `Source/Renderer/SConscript` to call `DiscoverSubModules()` after building the Renderer core library, so sub-module sources are automatically included
- [X] T031 [US2] Update `Source/Backend/SConscript` to use `DiscoverSubModules()` for auto-discovering backend implementations instead of hardcoding each backend path
- [X] T032 [US2] Verification: create a temporary test module `Source/Renderer/TestModule/` with a `SConscript` and a minimal `.cpp` file, run `scons`, verify it compiles, then remove the test module. Confirm no changes to `SConstruct` were needed (SC-002).

**Checkpoint**: User Story 2 complete. New modules are auto-discovered. Acceptance scenarios (AS-2.1, AS-2.2) are satisfied.

---

## Phase 5: User Story 3 — Configure Platform-Specific Build Variants (Priority: P3)

**Goal**: `scons config=debug` and `scons config=release` produce separate output directories with correct compiler flags.

**Independent Test**: Run `scons config=debug`, then `scons config=release`. Verify two separate output directories exist under `Build/<Platform>/` with different artifacts.

### Implementation for User Story 3

- [ ] T033 [US3] Enhance `site_scons/BuildConfig.py` — ensure `ApplyConfig()` applies the full flag set from data-model.md BuildConfiguration entity: Debug (`/Od /Zi /MDd /W4` on MSVC, `-O0 -g -Wall -Wextra` on GCC/Clang, `_DEBUG` define) and Release (`/O2 /MD /W4` on MSVC, `-O2 -Wall -Wextra` on GCC/Clang, `NDEBUG` define)
- [ ] T034 [US3] Update `SConstruct` to pass `config` into `variant_dir` path — ensure output goes to `Build/<Platform>/Debug/` or `Build/<Platform>/Release/` based on the `config` parameter (FR-006)
- [ ] T035 [US3] Implement default config fallback — verify that running `scons` with no `config` parameter defaults to `debug` (AS-3.3)
- [ ] T036 [US3] Verification: run `scons config=debug` and `scons config=release` sequentially, verify both `Build/<Platform>/Debug/` and `Build/<Platform>/Release/` directories exist with separate artifacts (SC-005)

**Checkpoint**: User Story 3 complete. Debug/Release configurations work. Acceptance scenarios (AS-3.1, AS-3.2, AS-3.3) are satisfied.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Edge cases, validation, and final compliance checks

- [ ] T037 [P] Implement SCons version validation edge case in `SConstruct` — test with a version string that has extra components (e.g., `4.10.1.dev0`) to ensure tuple comparison handles it gracefully (FR-007, edge case from spec)
- [ ] T038 [P] Implement compiler-not-found error handling in `site_scons/PlatformDetect.py` — if `env.Detect('cl')` (Windows) or `env.Detect('g++')` / `env.Detect('clang++')` (POSIX) fails, print error message per contracts/build-interface.md and `Exit(1)` (spec edge case)
- [ ] T039 [P] Verify `scons --clean` removes all build artifacts from `Build/` and leaves `Source/`, `specs/`, `.specify/` untouched (spec edge case, SC-004)
- [ ] T040 Verify layer dependency isolation — attempt to add `#include "RHI/RHIMinimal.h"` in `Source/Application/Private/ApplicationModule.cpp`, run `scons`, confirm compile error. Then revert the include. This validates FR-014 and the include path contract.
- [ ] T041 Verify PascalCase naming conventions across all C++ directories and source files (Constitution Principle VI, SC-006)
- [ ] T042 Run full quickstart.md validation — follow every step in `specs/001-scons-project-skeleton/quickstart.md` from scratch and verify all expected outcomes match

---

## Dependencies & Execution Order

### Phase Dependencies

```
Phase 1 (Setup)          → No dependencies — start immediately
Phase 2 (Foundational)   → Depends on Phase 1 — BLOCKS all user stories
Phase 3 (US1 - P1) 🎯   → Depends on Phase 2
Phase 4 (US2 - P2)       → Depends on Phase 3 (needs working build to test auto-discovery)
Phase 5 (US3 - P3)       → Depends on Phase 2 (can run in parallel with US1/US2)
Phase 6 (Polish)          → Depends on Phases 3, 4, 5
```

### User Story Dependencies

- **US1 (P1)**: Depends on Foundational (Phase 2) only. No dependencies on other stories. **This is the MVP.**
- **US2 (P2)**: Depends on US1 (needs existing layer SConscripts to modify for auto-discovery).
- **US3 (P3)**: Depends on Foundational (Phase 2) only. Can run in parallel with US1 if desired, but recommended after US1 for incremental validation.

### Within Each User Story

- Headers and sources (marked [P]) can be created in parallel
- SConscript files depend on their layer's source files existing
- Verification tasks depend on all prior tasks in the story

### Parallel Opportunities

**Phase 1**: T002, T003, T004 can all run in parallel
**Phase 2**: T006, T007 can run in parallel; T005 and T008 have sequential dependency (T008 imports from T005)
**Phase 3 (US1)**: T009–T024 can ALL run in parallel (different files, no dependencies). T025–T028 are sequential.
**Phase 4 (US2)**: T029 must complete before T030, T031. T032 depends on all.
**Phase 5 (US3)**: T033 before T034. T035–T036 sequential.
**Phase 6**: T037, T038, T039 can run in parallel. T040–T042 sequential.

---

## Parallel Example: User Story 1

```bash
# Launch ALL placeholder files in parallel (16 tasks simultaneously):
T009: Create Source/Core/Public/Core/CoreMinimal.h
T010: Create Source/Core/Private/CoreModule.cpp
T011: Create Source/Core/SConscript
T012: Create Source/RHI/Public/RHI/RHIMinimal.h
T013: Create Source/RHI/Private/RHIModule.cpp
T014: Create Source/RHI/SConscript
T015: Create Source/Renderer/Public/Renderer/RendererMinimal.h
T016: Create Source/Renderer/Private/RendererModule.cpp
T017: Create Source/Renderer/SConscript
T018: Create Source/Application/Public/Application/ApplicationMinimal.h
T019: Create Source/Application/Private/ApplicationModule.cpp
T020: Create Source/Application/SConscript
T021: Create Source/Backend/Vulkan/Public/VulkanRHI/VulkanDevice.h
T022: Create Source/Backend/Vulkan/Private/VulkanDevice.cpp
T023: Create Source/Backend/Vulkan/SConscript
T024: Create stub SConscripts for 6 non-Vulkan backends

# Then sequential:
T025: Create Source/Backend/SConscript (aggregator)
T026: Create Tests/Main.cpp
T027: Create Tests/SConscript
T028: End-to-end verification
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (directory skeleton) — ~4 tasks
2. Complete Phase 2: Foundational (build system modules) — ~4 tasks
3. Complete Phase 3: User Story 1 (all layer stubs + build) — ~20 tasks
4. **STOP and VALIDATE**: Run `scons` — verify all libraries build
5. This is a deployable MVP: the project skeleton is usable

### Incremental Delivery

1. Setup + Foundational → Build system ready
2. Add US1 → Full build works → **MVP! 🎯**
3. Add US2 → Auto-discovery works → Extensible skeleton
4. Add US3 → Debug/Release configs → Production-ready skeleton
5. Polish → Edge cases, compliance → Ship-quality

### Parallel Team Strategy

With multiple developers:

1. Team completes Setup + Foundational together
2. Once Foundational is done:
   - Developer A: User Story 1 (all layer stubs — highly parallelizable)
   - Developer B: User Story 3 (build config enhancement — independent of US1 stubs)
3. After US1 completes:
   - Developer B: User Story 2 (auto-discovery — needs US1's SConscripts)
4. All developers: Polish phase

---

## Notes

- [P] tasks = different files, no dependencies between them
- [Story] label maps task to specific user story for traceability
- No test tasks generated (not requested in spec) — manual verification via quickstart.md
- All C++ placeholder files contain minimal valid code (namespace + placeholder function)
- `site_scons/` uses lowercase (SCons convention) — exception to PascalCase rule per R-005
- Commit after each phase completion for clean git history
