# Research: Engine Development Roadmap

**Feature**: 002-engine-development-roadmap
**Date**: 2026-04-21
**Status**: Complete

## Research Tasks

This feature is a documentation/planning artifact. The "unknowns" are design decisions about the roadmap content itself, all of which were resolved during the `/speckit.clarify` sessions.

---

## Decision 1: RenderDependencyGraph vs FRenderGraph

**Decision**: Unify on **FRenderGraph** as the single render dependency management system. Use the canonical term **"Render Graph"** throughout the project.

**Rationale**: `RenderDependencyGraph` and `FRenderGraph` are overlapping concepts — both describe a DAG of render passes with resource dependencies. Having two names creates confusion. `FRenderGraph` aligns with modern engine conventions (Frostbite's Frame Graph, UE5's RDG) and follows the project's UE5-style `F`-prefix naming.

**Alternatives Considered**:
- Keep both as separate systems (rejected — redundant complexity, unclear ownership)
- Use "Frame Graph" as the canonical term (rejected — "Render Graph" is more widely adopted and matches the class name `FRenderGraph`)

**Impact on Roadmap**: Phase 012 retains `FRenderGraph`. No `RenderDependencyGraph` phase exists. All references use "Render Graph" terminology.

---

## Decision 2: Math Library — GLM vs Custom Implementation

**Decision**: **Implement from scratch.** Phase 003 (Core Math Library) will be a full custom implementation with SIMD optimization hooks.

**Rationale**: This is a learning-oriented project [[memory:9rx96rfq]]. The math library is one of the most educational subsystems to implement — it covers linear algebra fundamentals, SIMD intrinsics, cache-friendly data layout, and operator overloading patterns. Using GLM would bypass all of this learning.

**Alternatives Considered**:
- Use GLM directly (rejected — no learning value for a core subsystem)
- Use GLM initially, replace later (rejected — creates migration burden and the initial implementation is not complex enough to justify a placeholder)
- Use Eigen (rejected — overkill for graphics math, heavy template metaprogramming)

**Impact on Roadmap**: Phase 003 complexity remains M (3-5 days). Deliverables include `FVector2/3/4`, `FMatrix4x4`, `FQuat`, `FTransform`, `FMath`, `FColor`, and basic geometric primitives. SIMD optimization is designed-in but can use naive implementation initially.

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
- Phase 002 (Types & Memory): Custom `FString`, `FName`, `TArray<T>`, `TMap<K,V>`, memory allocators
- Phase 003 (Math): Full custom math library
- Phase 004 (Logging): Custom logging system (not spdlog)
- Phase 009 (Vulkan Resources): VMA is acceptable (memory allocator internals are Vulkan-specific, not general learning)
- Third-party acceptable: Vulkan SDK, GLFW (initial windowing), VMA, SPIRV-Cross

---

## Decision 5: Window System — GLFW vs Native

**Decision**: **GLFW first, native later.** Use GLFW in Phase 005/015 to quickly reach the "first triangle" rendering milestone. Add a later Phase (005b or post-017) for native platform window implementations.

**Rationale**: The core learning value is in the rendering pipeline (RHI, backends, render graph, materials), not in Win32/Cocoa/X11 window creation boilerplate. GLFW provides a proven, cross-platform windowing solution that lets us focus on what matters. Once the rendering pipeline is validated (Phase 017 Triangle Demo), native windowing can be implemented behind the same `IWindow`/`FWindow` abstraction.

**Alternatives Considered**:
- A: Native from day one (rejected — delays first triangle by weeks, learning value is low)
- B: SDL2 instead of GLFW (rejected — SDL2 is heavier, includes audio/networking we don't need)
- C: GLFW permanently, never go native (rejected — misses learning opportunity for platform APIs)

**Impact on Roadmap**:
- Phase 005 (Platform Abstraction): Includes `FPlatformWindow` handle types but NOT full windowing
- Phase 015 (Window & Input): Uses GLFW via ThirdParty/ directory
- Future Phase 005b (post-017): Native Win32/Cocoa/X11-Wayland implementations behind `IWindow`
- The `IWindow` interface is designed from the start to support both GLFW and native backends

---

## Dependency & Technology Summary

| Technology | Usage | Phase | Third-Party? |
|-----------|-------|-------|-------------|
| C++20 | Primary language (no modules) | All | No |
| SCons 4.10.1 | Build system | All | Yes (existing) |
| Vulkan SDK | First graphics API | 008-011 | Yes |
| GLFW | Initial windowing | 015 | Yes |
| VMA | Vulkan memory allocation | 009 | Yes (optional) |
| SPIRV-Cross | Shader cross-compilation | 011, 022 | Yes |
| Custom Math | FVector, FMatrix, etc. | 003 | No |
| Custom Logging | FLog, SG_LOG | 004 | No |
| Custom Types | FString, FName, TArray | 002 | No |

---

## Open Questions (None)

All clarifications have been resolved. No remaining unknowns block implementation.
