# Research: Engine Development Roadmap

**Feature**: 002-engine-development-roadmap
**Date**: 2026-04-21
**Status**: Complete
**Last Amended**: 2026-07-28

## Research Tasks

This feature is a documentation/planning artifact. The "unknowns" are design decisions about the roadmap content itself, all of which were resolved during the `/speckit.clarify` sessions.

---

## Decision 1: RenderDependencyGraph vs FRenderGraph

**Decision**: Unify on **FRenderGraph** as the single render dependency management system. Use the canonical term **"Render Graph"** throughout the project.

**Rationale**: `RenderDependencyGraph` and `FRenderGraph` are overlapping concepts — both describe a DAG of render passes with resource dependencies. Having two names creates confusion. `FRenderGraph` aligns with modern engine conventions (Frostbite's Frame Graph, UE5's RDG) and follows the project's UE5-style `F`-prefix naming.

**Alternatives Considered**:
- Keep both as separate systems (rejected — redundant complexity, unclear ownership)
- Use "Frame Graph" as the canonical term (rejected — "Render Graph" is more widely adopted and matches the class name `FRenderGraph`)

**Impact on Roadmap**: Feature 013 retains `FRenderGraph`. No `RenderDependencyGraph` phase exists. All references use "Render Graph" terminology.

---

## Decision 2: Math Library — GLM vs Custom Implementation

**Decision**: **Implement from scratch.** Feature 004 (Core Math Library) will be a full custom implementation with SIMD optimization hooks.

**Rationale**: This is a learning-oriented project [[memory:9rx96rfq]]. The math library is one of the most educational subsystems to implement — it covers linear algebra fundamentals, SIMD intrinsics, cache-friendly data layout, and operator overloading patterns. Using GLM would bypass all of this learning.

**Alternatives Considered**:
- Use GLM directly (rejected — no learning value for a core subsystem)
- Use GLM initially, replace later (rejected — creates migration burden and the initial implementation is not complex enough to justify a placeholder)
- Use Eigen (rejected — overkill for graphics math, heavy template metaprogramming)

**Impact on Roadmap**: Feature 004 complexity remains L. Deliverables include `FVector2/3/4`, `FMatrix4x4`, `FQuat`, `FTransform`, `FMath`, `FColor`, and basic geometric primitives. SIMD optimization is designed-in but can use naive implementation initially.

---

## Decision 3: C++20 Modules

**Decision**: **Do not use C++20 Modules.** Stick with traditional header/source separation using Public/Private directory structure.

**Rationale**: C++20 Modules have inconsistent cross-compiler support — particularly Clang on macOS (Apple Clang lags behind upstream Clang). SCons has no first-class module support, requiring custom build rules that would be fragile and hard to maintain. The Public/Private directory convention already provides clear API boundaries.

**Alternatives Considered**:
- Use modules for Core layer only (rejected — partial adoption creates two mental models)
- Wait for compiler maturity and adopt later (rejected — would require massive refactoring; better to design for headers from the start)

**Impact on Roadmap**: All phases use `#include`-based headers. The Public/ directory contains the public API headers; Private/ contains implementation details. No module-related build infrastructure is needed.

**C++20 Features That ARE Embraced**:
- `concepts` — for template constraints (e.g., `template<typename T> requires Arithmetic<T>`)
- `constexpr` improvements — more compile-time computation
- `ranges` — for pipeline-style algorithms
- `std::span` — non-owning array views
- `std::format` — type-safe formatting (for logging)
- Coroutines — potential use in async resource loading
- Designated initializers — cleaner struct initialization

---

## Decision 4: Project Philosophy — Learning vs Production

**Decision**: **Learning-oriented.** Self-implementation is preferred for all core subsystems.

**Rationale**: The user explicitly stated this repository is "用来巩固知识超越自我" (for consolidating knowledge and self-improvement). The primary value is in the implementation journey, not just the end result.

**Alternatives Considered**:
- Production-first approach with third-party libraries (rejected — defeats the purpose)
- Hybrid with clear boundaries (adopted partially — GLFW for windowing, Vulkan SDK for API)

**Impact on Roadmap**: 
- Feature 003 (Types & Memory): Custom `FString`, `FName`, `TArray<T>`, `TMap<K,V>`, memory allocators
- Feature 004 (Math): Full custom math library
- Feature 005 (Logging): Custom logging system (not spdlog)
- Feature 010 (Vulkan Resources): VMA is acceptable (memory allocator internals are Vulkan-specific, not general learning)
- Third-party acceptable: Vulkan SDK, GLFW (initial windowing), VMA, SPIRV-Cross

---

## Decision 5: Window System — GLFW vs Native

**Decision**: **GLFW first, native later.** Use Core platform handles from
Feature 006 and the GLFW-backed Application window system from Feature 016 to
reach the first-triangle milestone. Native window adapters remain a later
extension behind the same public boundary.

**Rationale**: The core learning value is in the rendering pipeline (RHI, backends, render graph, materials), not in Win32/Cocoa/X11 window creation boilerplate. GLFW provides a proven, cross-platform windowing solution that lets us focus on what matters. Once the rendering pipeline is validated (Feature 018 Triangle Demo), native windowing can be implemented behind the same public window abstraction.

**Alternatives Considered**:
- A: Native from day one (rejected — delays first triangle by weeks, learning value is low)
- B: SDL2 instead of GLFW (rejected — SDL2 is heavier, includes audio/networking we don't need)
- C: GLFW permanently, never go native (rejected — misses learning opportunity for platform APIs)

**Impact on Roadmap**:
- Feature 006 (Platform Abstraction): Includes `FPlatformWindow` handle types but not full windowing
- Feature 016 (Window & Input): Uses GLFW through the platform adapter boundary
- Future extension: Native Win32/Cocoa/X11-Wayland implementations behind the same window contract
- The `IWindow` interface is designed from the start to support both GLFW and native backends

---

## Decision 6: Dedicated Asset Layer and Ownership

**Decision**: Add Asset as a runtime layer that depends only on Core. Asset owns
CPU-side content data, logical identity, metadata, dependencies, and
import/cook/load contracts. Renderer owns RHI/GPU realization; offline
executables live under Tools.

**Rationale**: Existing material resource references and scene mesh identifiers
have no formal path to decoded CPU data or GPU resources. Placing importers in
Renderer would couple generic content management to rendering, while placing
GPU objects in Asset would reverse the RHI boundary.

**Impact on Roadmap**: Features 020 through 026 establish the Asset delivery
foundation before Meshlets. Feature 030 adds budget-driven streaming after
meshlet-derived data and GPU visibility expose chunk and residency behavior.

---

## Decision 7: Asset Identity and Development/Cooked Paths

**Decision**: `FAssetId` is a typed canonical logical path with an optional
subresource. Source, content, and cook hashes form `FAssetVersion` and
derived-data keys; they do not change identity. Development may import source
assets, while cooked runtime mode consumes manifests and derived payloads
without implicit source fallback.

**Rationale**: Logical paths remain readable and manually editable. Separate
version hashes provide deterministic invalidation without breaking references
whenever content changes.

---

## Decision 8: Initial Formats and Extensibility

**Decision**: Begin with glTF 2.0/GLB static model packages and PNG/JPEG/HDR
images. Add KTX2/Basis as a separate cooked-texture feature. Format dispatch is
registration-based and permits one source to emit multiple typed subresources.

**Rationale**: glTF provides a documented runtime-oriented model and material
interchange format. KTX2 preserves texture metadata and mip levels and supports
portable Basis transcoding. FBX, OBJ, USD, and TGA remain future importer
plugins; USD scene composition belongs to a later Scene/Prefab track rather
than the initial static-mesh importer.

**References**:
- https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html
- https://www.khronos.org/ktx/
- https://openusd.org/release/api/ar_page_front.html
- https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-management-in-unreal-engine

---

## Decision 9: Material/Model Ordering and Asset Delivery Split

**Decision**: Persistent material and shader assets precede static model
ingestion. Offline cooking/manifests and runtime asset management are separate
features.

**Rationale**: A glTF importer can emit stable material subresources only after
their schema and shader dependencies exist. The offline cooker is a deterministic
build tool, while the runtime manager is a concurrent lifecycle service; joining
them would combine unrelated failure modes and exceed one bounded feature.

**Impact on Roadmap**: Feature 023 defines material/shader assets, Feature 024
imports static models, Feature 025 cooks manifests and derived payloads, and
Feature 026 manages runtime requests and handles.

---

## Decision 10: Advanced Rendering and Native Backend Granularity

**Decision**: Split meshlet data construction from GPU visibility, split
ray-tracing RHI/backend support from renderer effects, and split GI into
screen-space, SDF/surface-cache, and hybrid integration phases. Native Metal is
scheduled immediately after Asset delivery, with DX12, desktop OpenGL, and GLES
as separate later phases.

**Rationale**: Each split creates a testable fallback boundary and prevents a
single feature from simultaneously changing asset formats, RHI contracts,
backend execution, and renderer policy. Early Metal validates that RHI and
shader assets are genuinely backend-neutral on the project's primary Apple
hardware. OpenGL and GLES have different platform, capability, and lifecycle
constraints; Android application packaging is not a GLES backend responsibility.

---

## Dependency & Technology Summary

| Technology | Usage | Phase | Third-Party? |
|-----------|-------|-------|-------------|
| C++20 | Primary language (no modules) | All | No |
| SCons 4.10.1 | Build system | All | Yes (existing) |
| Vulkan SDK | First graphics API | 009-012 | Yes |
| GLFW | Initial windowing | 016 | Yes |
| VMA | Vulkan memory allocation | 010 | Yes (optional) |
| SPIRV-Cross | Shader cross-compilation | 012, 027, 031-033 | Yes |
| glTF 2.0/GLB | Initial static-model source interchange | 024 | Standard |
| PNG/JPEG/HDR | Initial image source formats | 021 | Codec library selected during feature research |
| KTX2/Basis | Cooked cross-platform textures | 022 | Khronos standard/tooling |
| Custom Math | FVector, FMatrix, etc. | 004 | No |
| Custom Logging | FLog, SG_LOG | 005 | No |
| Custom Types | FString, FName, TArray | 003 | No |

---

## Open Questions (None)

All clarifications have been resolved. No remaining unknowns block implementation.
