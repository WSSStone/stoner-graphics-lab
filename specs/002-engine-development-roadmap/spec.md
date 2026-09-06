# Feature Specification: Engine Development Roadmap

**Feature Branch**: `002-engine-development-roadmap`
**Created**: 2026-04-21
**Status**: Complete (living roadmap; amended 2026-09-06)
**Input**: User description: "Research and create a comprehensive, phased, modular, agent-friendly development roadmap for the Stoner Graphics Lab cross-platform graphics engine. Create doc/ directory at project root and produce the roadmap as markdown documents."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Understand the Full Development Path (Priority: P1)

A developer (or AI agent) opens the project for the first time after the SCons skeleton is in place. They need to understand what to build next, in what order, and why. They navigate to `doc/` and find a master roadmap document that lays out all development phases, their dependencies, and the recommended execution order. Each phase is broken into spec-sized units that can be independently specified, planned, and implemented via the speckit workflow.

**Why this priority**: Without a clear roadmap, development stalls or proceeds in a disorganized manner. The roadmap is the single source of truth for "what comes next" and prevents wasted effort on premature features.

**Independent Test**: Can be tested by reading `doc/roadmap.md` and verifying that it provides a clear, ordered sequence of development phases with enough detail to initiate a `/speckit.specify` command for any listed phase.

**Acceptance Scenarios**:

1. **Given** the project with the SCons skeleton complete, **When** a developer reads `doc/roadmap.md`, **Then** they can identify the immediate next development phase and understand its prerequisites.
2. **Given** the roadmap document, **When** an AI agent reads any phase description, **Then** it contains sufficient context to generate a feature specification via `/speckit.specify` without additional research.
3. **Given** the roadmap document, **When** a developer inspects the phase dependency graph, **Then** no phase depends on a phase that appears later in the sequence (topological ordering is valid).

---

### User Story 2 - Plan a Specific Development Phase (Priority: P2)

A developer wants to start working on a specific phase (e.g., "Core Foundation Layer"). They find the corresponding section in the roadmap that describes the scope, key deliverables, success criteria, and estimated complexity. This information is sufficient to run `/speckit.specify` and produce a detailed feature spec.

**Why this priority**: Each phase must be self-contained enough to be independently specifiable. Without this granularity, the roadmap is just a wish list rather than an actionable plan.

**Independent Test**: Can be tested by selecting any phase from the roadmap and verifying it contains: scope description, key deliverables list, dependencies, estimated complexity, and a suggested `/speckit.specify` prompt.

**Acceptance Scenarios**:

1. **Given** any phase in the roadmap, **When** a developer reads its description, **Then** they find a clear scope boundary (what's included and excluded).
2. **Given** any phase in the roadmap, **When** they look at the deliverables, **Then** each deliverable is concrete and verifiable.
3. **Given** any phase in the roadmap, **When** they check dependencies, **Then** all listed dependencies reference phases that appear earlier in the roadmap.

---

### User Story 3 - Track Overall Project Progress (Priority: P3)

A project lead wants to understand the overall scope of the graphics engine and track which phases have been completed, which are in progress, and which are upcoming. The roadmap provides a high-level overview with status tracking capability.

**Why this priority**: Progress visibility is important for project management but is secondary to having the roadmap content itself.

**Independent Test**: Can be tested by verifying the roadmap includes a summary table or checklist that can be updated as phases are completed.

**Acceptance Scenarios**:

1. **Given** the roadmap document, **When** a project lead reads the overview section, **Then** they can see all phases with their current status at a glance.
2. **Given** a completed phase, **When** the status is updated in the roadmap, **Then** the overall progress is immediately visible.

---

### Edge Cases

- What if a phase turns out to be too large during specification? The roadmap should note that phases can be split into sub-phases during the `/speckit.specify` step.
- What if platform constraints make a phase irrelevant (e.g., Metal on Linux)? The roadmap should clearly mark platform-specific phases and their applicability.
- What if third-party dependencies change? The roadmap should identify external dependency risks and suggest mitigation strategies.

## Architecture & Design Constraints *(mandatory)*

- **RHI and Asset Boundaries**: The roadmap MUST preserve the explicit Core, Asset, RHI, Backend, Renderer, and Application dependency directions defined by constitution v1.4.0. Asset remains CPU/content-only; Renderer owns RHI/GPU realization; runtime modules never depend on offline Tools.
- **Design Patterns**: The roadmap MUST plan for Strategy/Composite pattern usage in each layer, avoiding monolithic designs.
- **Advanced Graphics**: The roadmap MUST include phases for Ray Tracing, Meshlet optimization, and Global Illumination as specified in the constitution.
- **Naming Conventions**: All code deliverables referenced in the roadmap MUST follow UE5-style PascalCase naming conventions.
- **Cross-Platform Compatibility**: Every phase MUST consider Windows, macOS, and Linux support. Platform-specific phases (e.g., Metal backend) MUST be clearly marked.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The project MUST contain a `doc/` directory at the project root for all development documentation.
- **FR-002**: The roadmap MUST be written as `doc/roadmap.md` — a single master document covering all development phases.
- **FR-003**: The roadmap MUST organize development into clearly defined phases ordered by a valid dependency DAG, including the cross-cutting Asset track.
- **FR-004**: Each phase MUST include: phase name, scope description, key deliverables, dependencies on prior phases, estimated complexity (S/M/L/XL), and a suggested speckit prompt.
- **FR-005**: The roadmap MUST include a visual dependency graph (using Mermaid or text-based diagram) showing phase relationships.
- **FR-006**: The roadmap MUST cover at minimum these areas: Core utilities, RHI abstraction, at least one Backend implementation, Renderer fundamentals, and Application layer basics.
- **FR-007**: Each phase MUST be scoped to be completable as a single speckit feature (one `/speckit.specify` → `/speckit.plan` → `/speckit.tasks` → `/speckit.implement` cycle).
- **FR-008**: The roadmap MUST include a phase overview table with columns for: phase number, name, layer, dependencies, complexity, and status.
- **FR-009**: The roadmap MUST identify which phases are critical path (blocking other phases) vs. which can be developed in parallel.
- **FR-010**: The roadmap MUST include an "Architecture Principles" section that summarizes the constitution's constraints as they apply to development ordering.
- **FR-011**: Runtime phase numbers MUST match their Speckit feature numbers; Feature 002 remains the roadmap meta-feature and runtime phases therefore begin at 003.
- **FR-012**: The roadmap MUST plan separate phases for Asset identity/registry, image/texture ingestion, KTX2 cooking, material/shader assets, static mesh/model ingestion, offline cooking/manifests, runtime asset management, and streaming/residency.
- **FR-013**: Asset importer and resolver contracts MUST permit later FBX, OBJ, USD, and TGA extensions without changing existing asset identity or runtime payload contracts.
- **FR-014**: Material and shader asset contracts MUST precede static model ingestion so glTF material subresources target an established schema.
- **FR-015**: Advanced rendering MUST separate derived meshlet data from GPU visibility, ray-tracing backend infrastructure from renderer effects, and screen-space GI from SDF/surface-cache data and final hybrid integration.
- **FR-016**: Native Backend phases MUST be independently scoped by graphics API, appear early enough to validate backend-neutral contracts, and keep Android application lifecycle/packaging outside the GLES backend phase.
- **FR-017**: The roadmap MUST place one backend-neutral HDR SceneColor-to-display phase after completed Feature 028, with RGBA16F linear Rec.709/sRGB-D65 SceneColor, explicit pre-tonemap/post-tonemap insertion points, manual exposure, three versioned SDR tone maps, a separate versioned ACES-style HDR viewing transform, SDR sRGB/Rec.709/gamma plus 1000/2000-nit PQ/scRGB output-device profiles, Forward/Deferred unification, Render Graph integration, Vulkan/Metal presentation/readback, resize/mode changes, and debug bypass. Windows retains SDR validation but no HDR authority; macOS Metal PQ/EDR visual acceptance requires live maintainer inspection and MUST NOT be automated.
- **FR-018**: The roadmap MUST place a separate TAA-primary/FXAA-fallback phase after the HDR output phase. TAA MUST run before tone mapping, FXAA MUST run after tone mapping, and Deferred MUST retain `sampleCount=1` as its default rather than adopting MSAA.
- **FR-019**: The temporal phase MUST own deterministic jitter, previous/current `ViewProjection`, motion vectors, history ping-pong, reprojection, depth/normal rejection, disocclusion handling, neighborhood clamp, and camera-cut/resize/FOV invalidation. Later Screen-Space GI MUST reuse that foundation and MUST NOT create a duplicate temporal framework.
- **FR-020**: Completed Features 003-029 MUST retain their identifiers and delivered evidence. Roadmap 3.1 MUST insert 030 Application Interactive Rendering Lab & ImGui Integration as the next phase, move TAA to 031, and migrate only the unstarted former 030-052 phases according to `phase-index.json`. Meshlet Derived Data at 043 MUST retain exactly 024/025/026/028 dependencies without depending on post-processing or virtual shadows. Historical completed documents retain delivery-time numbering, resolved through `migration-3.1.md` and its historical mapping chain.
- **FR-021**: Feature 028 v2 `sampleCount=1`/no-general-post-processing references MUST remain historical correctness evidence. Later SDR output changes MUST increment workload revision, generate a new exact-dimension Candidate, require explicit maintainer acceptance, prohibit automatic alignment/cropping/scaling/resampling, and retain bounded PNG/JSON evidence. HDR visual output MUST use a bounded macOS Metal live-view maintainer attestation; automation MUST NOT score, compare, or accept HDR appearance.
- **FR-022**: A Roadmap 3.1 amendment commit, if requested, MUST include only the roadmap, Feature 002 governance/migration/index, project memory, consistency scanner/tests and bounded amendment evidence. It MUST NOT rewrite completed 028/029 evidence or mix in runtime implementation. User-owned `.gitignore`, `.github/workflows/tutorial-docs.yml`, `Tools/Tutorial/`, and `doc/tutorial/` changes MUST remain untouched and unstaged.

- **FR-023**: Features 030-042 MUST prioritize a complete raster/environment/post-processing renderer on Vulkan/Metal before optional extra backends and advanced geometry/GI in the recommended queue; independent DAG branches MUST remain independently executable.
- **FR-024**: Shadow development MUST establish directional/local depth maps and stable CSM, add screen-space contact shadows as an off-screen-limited supplement, distinguish VarianceShadowMaps filtering from VirtualShadowMaps virtualization, and retain a conventional-map fallback for virtual-page cache/atlas failures.
- **FR-025**: Atmosphere/environment lighting, height/volumetric fog and volumetric clouds MUST have separate responsibility-focused phases, explicit HDR radiance/transmittance ownership, sun/sky/cloud-shadow coupling, transparent participation and tests preventing double extinction.
- **FR-026**: Post-processing phases MUST reuse 029 output ownership and 031 temporal services; cover optional auto exposure, HDR bloom, HDR-safe grading, display finishing, DOF and motion blur; explicitly declare effect domains/order and exposure-history behavior without adding a second tone map or transfer.
- **FR-027**: Feature 041 MUST establish capability-correct GPU/CPU profiling, memory reporting, performance views and scene/device/resolution-specific budget checks after rendering effects 031-040. Effects 031-040 MUST retain debug outputs, resource/sample counters and bounded execution without depending on full profiling. Feature 042 MUST explicitly depend on 041. New phases MUST state bounded milestones and Feature 042 MUST validate their integrated indoor/outdoor/day-night/weather/camera interactions, not merely isolated outputs.
- **FR-028**: Feature 033 MUST own a reusable SceneDepthPyramid; 039/040/044/046 MUST reuse it directly or transitively. Temporal effects MUST reuse 031 services through signal-specific adapters; 048 MUST explicitly depend on 031, and 049 MUST keep runtime radiance ownership in Renderer.
- **FR-029**: The migration MUST preserve completed phase text and evidence hashes, synchronize active roadmap/index/spec/plan/research/model/quickstart/contracts/checklist/tasks/AGENTS references, add mutation-tested consistency checks, and keep user-owned tutorial/.gitignore changes untouched and unstaged.

- **FR-030**: Feature 030 MUST deliver the interactive rendering lab before 031 TAA and subsequent effects: reusable WASD/QE/Shift/right-mouse camera, cursor/focus lifecycle, FOV/speed/reset/presets and a pinned Dear ImGui control surface for existing scene/output/debug settings. It MUST NOT become a full editor or require full profiling.
- **FR-031**: Application MUST own input/UI state behind private ImGui adapters; Renderer MUST consume backend-neutral draw snapshots through Render Graph/RHI on Vulkan/Metal. UI keyboard/mouse capture, UTF-8 text, clipboard basics, HiDPI scissor/font/texture lifetime and bounded frames-in-flight MUST be explicit. Interactive presentation MUST NOT require synchronous CPU readback every frame.
- **FR-032**: UI MUST composite in display-linear output space after scene effects and before 029 transfer/packing with explicit reference-white brightness, color decoding/gamut conversion and alpha blending. It MUST bypass scene exposure, TAA, DOF, motion blur and bloom; live SDR/PQ/EDR switching MUST handle capability/lifecycle failures.
- **FR-033**: Formal scene captures MUST default to UI disabled and frozen settings; preview/preset exports MUST NOT become Accepted evidence. The phase MUST include maintainer hands-on controls/navigation review, bounded separate UI smoke evidence and human-only macOS HDR appearance review. Later effects and profiling MUST reuse the shell; VT remains outside this amendment.

### Key Entities

- **Phase**: A discrete unit of development work that maps to one speckit feature cycle. Contains scope, deliverables, dependencies, and complexity estimate.
- **Layer**: One of the runtime ownership areas (Core, Asset, RHI, Backend, Renderer, Application) that phases are organized around.
- **Dependency**: A relationship between phases where one phase must be completed before another can begin.
- **Deliverable**: A concrete, verifiable output of a phase (e.g., "IDevice interface with Create/Destroy lifecycle", "FVector3 math type with SIMD support").

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The roadmap contains runtime phases 003 through 053 (51 phases) and covers all 6 runtime ownership areas.
- **SC-002**: Any developer can read a phase description and produce a `/speckit.specify` prompt within 2 minutes.
- **SC-003**: The dependency graph has zero circular dependencies and forms a valid topological order.
- **SC-004**: 100% of phases include all required fields (name, scope, deliverables, dependencies, complexity, speckit prompt).
- **SC-005**: The roadmap clearly distinguishes critical-path phases from parallelizable phases.
- **SC-006**: The document is self-contained — no external references are required to understand the development plan.

- **SC-007**: All 24 future phases 030-053 have complete owner/dependency/scope/deliverable/exclusion/prompt/milestone records, and the active index matches TOC/table/DAG/details.
- **SC-008**: Structural and semantic consistency scans report zero findings; mutation tests reject wrong phase identities/dependencies, missing sections, stale active numbering, duplicate temporal/depth ownership and missing quality gates.

## Clarifications

### Session 2026-04-21

- Q: What is the relationship between `RenderDependencyGraph` and Feature 013's `FRenderGraph`? → A: They are overlapping concepts. Ignore `RenderDependencyGraph`; retain `FRenderGraph` in Feature 013 as the single render dependency management system.
- Q: Should we use "Frame Graph" or "Render Graph" as the canonical term? → A: Unify on **"Render Graph"** throughout the project (consistent with class name `FRenderGraph` and modern engine conventions). Remove "frame graph" references.
- Q: Math library — use GLM or implement from scratch? → A: **Implement from scratch.** This repository is a learning-oriented project aimed at deepening knowledge and self-improvement. Prefer custom implementations over third-party wrappers wherever feasible. Feature 004 (Math Library) is a full custom implementation with SIMD optimization hooks.
- Q: Should we use C++20 Modules? → A: **No.** Stick with traditional header/source separation (Public/Private directory structure). C++20 Modules have inconsistent cross-compiler support (especially Clang on macOS) and poor SCons integration. Other C++20 features (concepts, constexpr, ranges, `std::span`, `std::format`, etc.) are fully embraced.
- Q: Window system — use GLFW/SDL or implement native platform wrappers from scratch? → A: **GLFW first, native later.** Feature 006 provides platform handles and Feature 016 provides the GLFW-backed window system used by Feature 018. Native Win32/Cocoa/X11-Wayland adapters remain a future extension behind the same public boundary. This balances learning goals with momentum — the core learning value is in the rendering pipeline, not window creation.

### Session 2026-07-24

- Q: Should mesh, texture, and other asset handling be added before Meshlets? → A: **Yes.** Add a dedicated Asset layer and six format/responsibility-specific phases before Meshlets, then add streaming/residency after Meshlets.
- Q: How should source and cooked assets coexist? → A: **Hybrid development/cooked paths.** Both paths share a stable logical-path `FAssetId`; source/content/cook hashes are version and cache keys, not identity.
- Q: Which initial formats are supported? → A: **glTF/GLB static models plus PNG/JPEG/HDR images.** KTX2/Basis is a separate cooked-texture phase. Importer/resolver registration must allow later FBX, OBJ, USD, and TGA support.
- Q: Should historical roadmap numbers remain offset from Speckit features? → A: **No.** Normalize all runtime phases to actual feature numbers; Deferred remains Feature 019 and Asset Core becomes Feature 020.

### Session 2026-07-28

- Q: Should glTF model ingestion precede persistent material/shader assets? → A: **No.** Material and shader assets move to Feature 023; static mesh/model ingestion moves to Feature 024 and consumes that schema.
- Q: Should offline cooking and runtime asset management share one feature? → A: **No.** Feature 025 owns the offline cooker, manifests, target profiles, and derived-data cache; Feature 026 owns asynchronous runtime requests, dependency scheduling, handles, caching, cancellation, and unload.
- Q: How should advanced rendering be sized? → A: Split meshlet derived data from GPU visibility, split ray-tracing RHI/backend work from renderer effects, and split GI into screen-space, SDF/surface-cache, and hybrid integration phases.
- Q: When should additional native backends be developed? → A: Bring native Metal forward immediately after the Asset delivery foundation, then add DX12, desktop OpenGL, and GLES as independent phases before ray tracing and GI integration. GLES excludes Android lifecycle and packaging.

### Session 2026-09-01

- Q: What image-pipeline work follows completed Feature 028? → A: Insert Feature 029 HDR Post-Processing & Output Transform and Feature 030 Anti-Aliasing & Temporal Reconstruction before Meshlet work; renumber only the not-yet-started former Features 029-039 to 031-041.
- Q: How are tone mapping and anti-aliasing ordered? → A: Feature 029 owns the shared backend-neutral HDR SceneColor-to-display path. Feature 030 places TAA before tone mapping and FXAA after tone mapping; Deferred defaults to `sampleCount=1`, while MSAA and DLSS/FSR/XeSS remain later extensions.
- Q: What temporal infrastructure may Screen-Space GI own? → A: It may extend Feature 030's motion-vector, jitter, history, reprojection, rejection, and invalidation contracts for GI signals, but it may not create a duplicate temporal framework.
- Q: How are Feature 028 references handled after formal output changes? → A: Keep v2 as historical correctness evidence. New output requires a workload revision bump, exact-dimension Candidate, explicit maintainer acceptance, no alignment/crop/scale/resampling, and bounded PNG/JSON evidence.

### Session 2026-09-02

- Q: Which color and display profiles does Feature 029 own? → A: Freeze RGBA16F linear Rec.709/sRGB-D65 SceneColor; implement SDR sRGB/Rec.709/gamma with Khronos PBR Neutral, ACES fitted, and Extended Reinhard tone maps, plus separate ACES-style 1000/2000-nit PQ Rec.2020 and scRGB/EDR HDR transforms.
- Q: How is HDR visual output accepted? → A: Windows performs no HDR validation. macOS Metal validates PQ and EDR/scRGB through live maintainer inspection; automation may validate non-visual contracts and attestation completeness but may not judge HDR appearance.
- Q: How does the Feature 028 evidence policy extend? → A: v2 remains historical; SDR uses successor exact-dimension Candidates and the existing no-alignment policy, while HDR uses bounded manual JSON attestations rather than automated image reference comparison.

### Session 2026-09-06

- Feature 029 is complete by the maintainer's explicit one-time exception at
  `2ee7116`; see [closeout.md](../029-hdr-output-transform/closeout.md).
  Hosted 14/14 and current M4 SDR/four HDR machine runs passed. The maintainer
  accepted current M4 SDR, waived a current Windows physical rerun and repeat
  HDR viewing/separate attestation, and authorized use of historical
  `1f46352` Windows SDR and +3 EV live HDR acceptance without relabeling.
  This revision-specific decision supersedes only the corresponding original
  closeout obligations, including the independent attestation form required
  by FR-021; the strict same-SHA aggregate is not passed and future gates
  remain unchanged. Feature 030 is next; 031 dependencies remain 024/025/026/028,
  and 039 reuses the Feature 030 temporal framework.

### Session 2026-09-06 — Complete renderer first (Roadmap 3.0)

- Request: prioritize raster shadows (screen-space, shadow maps, CSM, variance
  and virtual shadow maps), atmosphere, volumetric clouds, fog and post-processing.
- Decision: retain completed 003-029 and next 030; add 031-041 complete-renderer
  work, migrate only unstarted phases to 042-052, and move additional backends
  off the mandatory solo queue. See [migration-3.0.md](migration-3.0.md).
- Clarification: earlier sessions and closeout notes above describe their dated
  numbering. Current semantic identities are pinned in [phase-index.json](phase-index.json).

### Session 2026-09-06 — Interactive lab first (Roadmap 3.1)

- The maintainer accepted Application Interactive Rendering Lab & ImGui Integration
  as next 030 before TAA; unstarted former 030-052 shift to 031-053.
- Existing calibration camera controls are reused; a formal GUI/native interactive
  SDR/PQ/EDR path is planned, not claimed delivered by this amendment.
- Full Profiling remains after effects (041), before acceptance (042). The separate
  VT discussion remains a proposal, not an inserted feature.
- Earlier clarification sessions retain historical numbers; use
  [migration-3.1.md](migration-3.1.md) and the current phase index.

## Assumptions

- The SCons project skeleton (spec 001) is complete and functional as the foundation for all subsequent development.
- Development will proceed bottom-up (Core first, Application last) to respect layer dependencies.
- Each phase targets a single speckit feature cycle; phases that are too large will be split during specification.
- The initial focus is on Vulkan as the first Backend implementation, with other backends following as separate phases.
- **Self-implementation preferred**: This is a learning-oriented project. Core subsystems (math, containers, memory, etc.) should be implemented from scratch to maximize knowledge gain. Third-party libraries are acceptable only for platform abstraction (e.g., Vulkan SDK, GLFW for initial windowing) or where custom implementation would not yield meaningful learning (e.g., image codec libraries). GLFW will be used initially for windowing, with a planned native replacement phase later.
- The roadmap is a living document that will be updated as phases are completed and new requirements emerge.
- C++20 features are available and should be leveraged where appropriate, **except C++20 Modules** which are excluded due to cross-platform toolchain immaturity and SCons integration gaps. Embraced features include: concepts, constexpr improvements, ranges, `std::span`, `std::format`, coroutines, and designated initializers.
