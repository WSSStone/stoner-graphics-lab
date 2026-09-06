# Research: Engine Development Roadmap

**Feature**: 002-engine-development-roadmap
**Date**: 2026-04-21
**Status**: Complete
**Last Amended**: 2026-09-06

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
foundation before Meshlets. Feature 045 adds budget-driven streaming after
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

## Decision 11: Formal HDR Output Before Anti-Aliasing

**Decision**: Feature 029 owns one backend-neutral HDR `SceneColor` to display
pipeline shared by Forward and Deferred. It defines pre-tonemap/post-tonemap
insertion points, deterministic manual exposure, RGBA16F linear Rec.709/sRGB-D65
working space, three versioned SDR tone maps, a separate ACES-style HDR viewing
transform, SDR sRGB/Rec.709/gamma and 1000/2000-nit PQ/scRGB output-device
profiles, Render Graph integration, Vulkan/Metal native presentation/readback,
resize/mode changes, and debug bypass. Windows retains SDR validation but no HDR
authority. macOS Metal PQ/EDR visual acceptance is a live maintainer decision;
automation is limited to non-visual contracts and attestation completeness.
Feature 031 is separate: TAA is the pre-tonemap primary path and FXAA is the
post-tonemap fallback.

**Rationale**: Tone mapping and output transfer define the color domain in
which AA operates. Freezing that contract first prevents Forward, Deferred,
Vulkan, and Metal from growing incompatible output policy and makes the TAA/FXAA
ordering testable.

**Alternatives Considered**:
- Combine output transform, TAA, FXAA, bloom, and exposure in one general post-processing phase (rejected — too many ownership and validation changes in one cycle)
- Implement FXAA before a formal tone-map contract (rejected — its post-tonemap input would remain ambiguous)
- Adopt MSAA as the Deferred default (rejected — Deferred remains `sampleCount=1`; MSAA is a later extension)

---

## Decision 12: One Reusable Temporal Foundation

**Decision**: Feature 031 owns deterministic jitter, previous/current
`ViewProjection`, static/dynamic motion vectors, history ping-pong,
reprojection, depth/normal rejection, disocclusion handling, neighborhood
clamp, and camera-cut/resize/FOV invalidation. Feature 046 Screen-Space GI must
reuse and may extend these contracts; it must not create a duplicate temporal
framework.

**Rationale**: Parallel temporal systems drift in coordinate conventions,
invalidation, resource lifetime, and debugging. A shared foundation makes
static convergence, dynamic motion, camera cuts, and later GI accumulation
comparable across paths.

---

## Decision 13: Preserve Feature 028 Evidence and Version Future Output

**Decision**: Feature 028 v2 `sampleCount=1` and no-general-post-processing
references remain immutable historical correctness evidence. Features 029 and
031 must increment affected workload revisions. Changed SDR output generates
exact-dimension Candidates, requires explicit maintainer acceptance, and rejects
alignment, cropping, scaling, and resampling. HDR visual output uses a bounded
macOS live-view maintainer JSON attestation and no automated image comparison.
All evidence remains bounded PNG/JSON.

**Rationale**: The v2 references prove the accepted geometry, camera, lighting,
strict-cooked, semantic, lifecycle, and backend state before formal
post-processing. Reinterpreting or overwriting them would erase provenance;
geometric normalization could hide presentation-size or projection defects.

---

## Dependency & Technology Summary

| Technology | Usage | Phase | Third-Party? |
|-----------|-------|-------|-------------|
| C++20 | Primary language (no modules) | All | No |
| SCons 4.10.1 | Build system | All | Yes (existing) |
| Vulkan SDK | First graphics API | 009-012 | Yes |
| GLFW | Initial windowing | 016 | Yes |
| VMA | Vulkan memory allocation | 010 | Yes (optional) |
| SPIRV-Cross | Shader cross-compilation | 012, 027, 051-053 | Yes |
| glTF 2.0/GLB | Initial static-model source interchange | 024 | Standard |
| PNG/JPEG/HDR | Initial image source formats | 021 | Codec library selected during feature research |
| KTX2/Basis | Cooked cross-platform textures | 022 | Khronos standard/tooling |
| Custom Math | FVector, FMatrix, etc. | 004 | No |
| Custom Logging | FLog, SG_LOG | 005 | No |
| Custom Types | FString, FName, TArray | 003 | No |

---

## Open Questions (None)

All clarifications have been resolved. No remaining unknowns block implementation.

## Decision 15: Complete raster renderer before backend breadth (2026-09-06)

**Decision (current numbering, updated by 3.1)**: Put 030 interactive lab before
031 temporal and prioritize 032-040 shadows,
environment, fog/clouds, camera effects and AO/SSR, then 041 full profiling
and 042 integrated quality (retaining the profiling-after-effects decision). Move
unstarted phases per [migration-3.1.md](migration-3.1.md), which links the historical 3.0 migration.
Extra backends do not block SSGI or the Vulkan/Metal renderer.

**Shadow terminology**: Conventional maps/CSM are the baseline; screen-space
contact shadows are a limited supplement. VarianceShadowMaps filters stored
depth moments, while VirtualShadowMaps virtualizes page storage. Use explicit
names, separate strategy choices and fallback tests.
Sources: [Epic contact shadows](https://dev.epicgames.com/documentation/unreal-engine/contact-shadows-in-unreal-engine?lang=en-US),
[NVIDIA variance-shadow discussion](https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-8-summed-area-variance-shadow-maps).

**Deliberate platform difference**: Epic designs virtual shadows around Nanite;
our earlier phase instead starts with indexed meshes and a bounded physical
atlas. The portable prototype retains conventional maps/CSM and makes no
equivalent large-scene efficiency claim.
Source: [Epic Virtual Shadow Maps](https://dev.epicgames.com/documentation/unreal-engine/virtual-shadow-maps-in-unreal-engine).

**Environment coupling**: Sky, sun, environment lighting, fog and clouds need
explicit radiance/transmittance ordering. Volume temporal adapters share
lifecycle but account for density/light changes; opaque motion alone is
insufficient. Sources:
[Epic environmental lighting](https://dev.epicgames.com/documentation/en-us/unreal-engine/environmental-light-with-fog-clouds-sky-and-atmosphere-in-unreal-engine),
[Epic volumetric fog](https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-fog-in-unreal-engine).

**Post-processing**: Keep 029's unique output transform and declare effect
domains/order. The chosen DOF -> TAA -> motion blur/bloom ordering follows the
same broad ordering described for Unreal's temporal upscaling chain, without
claiming a TSR implementation. Auto exposure is optional; deterministic image
acceptance remains manually exposed.
Sources: [Epic rendering overview](https://dev.epicgames.com/documentation/unreal-engine/introduction-to-rendering-in-unreal-engine-for-unity-developers),
[Epic post-process effects](https://dev.epicgames.com/documentation/unreal-engine/post-process-effects-in-unreal-engine?lang=en-US).

**Tradeoff**: More near-term renderer phases, but each owns bounded milestones.
Full profiling at 041 makes the completed effects' cost measurable; 042 tests
interactions and quality presets. This intentionally delays performance-bottleneck
discovery: 031-040 keep debug outputs, resource/sample counters and bounded
execution, not a prerequisite on GPU query infrastructure or performance panels. Runtime
DDC/publishing policy is not mixed into residency; explicit manifest-based
cleanup is a cross-cutting maintenance obligation.

## Decision 16: Interactive lab before more effects (2026-09-06)

The maintainer accepted next Feature 030 Interactive Rendering Lab & ImGui
Integration. The current calibration controller already implements free-camera
keys and right-drag/FOV/reset/export, but configuration excludes HDR preview,
there is no GUI and the preview loop waits for/readbacks every rendered frame.
This phase promotes the useful controller instead of mistaking it for a complete
interactive app. Evidence: `Demo/StonerDemo/Private/FProductionCameraPreview.cpp`,
`FProductionCameraPreviewRun.cpp` and `FDemoConfiguration.cpp` in that directory.

Choose pinned Dear ImGui behind private adapters, with Application-owned input
and Renderer/RHI-owned draw snapshots. Keep engine input callbacks authoritative;
capture mouse/keyboard for widgets before deciding camera input. Add basic text,
clipboard, HiDPI and texture lifecycle integration. Dear ImGui distinguishes
platform and rendering adapters and supports custom renderer integration:
[official integration guide](https://github.com/ocornut/imgui/wiki/Getting-Started),
[backend contracts](https://github.com/ocornut/imgui/blob/master/docs/BACKENDS.md).
Select and record the exact dependency revision during the runtime feature plan.

Project decision: compose UI after all scene effects in display-linear output
gamut with explicit reference white, then use 029 transfer/packing. Widgets do
not inherit exposure or temporal/lens effects. Formal captures default to UI off;
manual scene/output changes are not baseline acceptance. No new HDR observations
or software evidence are claimed by this roadmap amendment.

Do not add full profiling, a full editor or VT here. The earlier VT placement
was a discussion proposal, not accepted phase creation. Migration is in
[migration-3.1.md](migration-3.1.md); full profiling stays after the effects.
