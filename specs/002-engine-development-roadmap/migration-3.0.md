# Roadmap 3.0 — Complete renderer first

**Amended**: 2026-09-06  
**Previous active roadmap**: 2.3.2  
**Current phase range**: 003-052 (50 runtime phases)  
**Completed identifiers and delivered evidence**: 003-029 unchanged  
**Next**: Feature 030 Anti-Aliasing & Temporal Reconstruction

## Purpose

The maintainer requested raster shadows (screen-space shadows, shadow maps,
CSM, variance filtering and virtual shadow maps), atmosphere, volumetric clouds,
fog and post-processing earlier in the roadmap. Features 030-041 now form the
near-term complete-renderer track on Vulkan/Metal. A complete baseline means
coherent lighting, environment, camera effects and display output, not every
feature of a production engine. New future entries are plans, not implementation
or acceptance claims.

Validation: [roadmap-3.0-scan.json](validation/roadmap-3.0-scan.json) records
zero structural/semantic findings, 16 passing mutation tests, protected
completed-phase text, and unchanged linked 029 evidence. No runtime feature
was implemented and no commit/push was performed by this amendment.

## Number migration

Only not-yet-started phases change. The table refers specifically to the
**previous 2.3.2 numbering**; it is not a second active phase list.

| Previous number | Current number | Semantic feature |
|---|---|---|
| 030 | 030 | Anti-Aliasing & Temporal Reconstruction |
| 031 | 042 | Meshlet Derived Data |
| 032 | 043 | GPU-Driven Visibility & LOD |
| 033 | 044 | Streaming & Residency |
| 034 | 050 | DirectX 12 Backend |
| 035 | 051 | OpenGL Backend |
| 036 | 052 | GLES Backend |
| 037 | 046 | Ray Tracing & Vulkan Backend Foundation |
| 038 | 047 | Ray-Traced Renderer Effects |
| 039 | 045 | Screen-Space GI & Temporal |
| 040 | 048 | SDF & Surface Cache Assets |
| 041 | 049 | Hybrid GI Integration |

New Features 031-041 have no previous phase identities:

- **031** — Frame Profiling & Render Diagnostics
- **032** — Raster Shadow Maps & Cascades
- **033** — Screen-Space Shadows & Shadow Filtering
- **034** — Sky Atmosphere & Environment Lighting
- **035** — Height Fog & Volumetric Fog
- **036** — Volumetric Clouds
- **037** — Exposure, Bloom & Color Grading
- **038** — Depth of Field & Motion Blur
- **039** — Virtual Shadow Maps
- **040** — Screen-Space Ambient Occlusion & Reflections
- **041** — Complete Rendering Pipeline Integration & Quality Baseline

The machine-readable [phase-index.json](phase-index.json) pins current names,
owners, dependencies and this migration. `doc/roadmap.md` remains the master
human-readable roadmap; the consistency scan checks the two representations.

## Historical references and provenance

Completed 028/029 specs, implementation HTML, task history and immutable
Validation records refer to their delivery-time phase numbering. For example,
their old Meshlet 031 now resolves to Meshlet 042, and old Screen-Space GI 039
now resolves to Screen-Space GI 045. These files and accepted evidence hashes
are not rewritten merely to change future numbering. Their local task IDs
(T001 etc.) also remain unchanged.

Current authority is `doc/roadmap.md`, `AGENTS.md`, the active sections of
`specs/002-engine-development-roadmap/`, and this migration/index. Earlier
dated clarification sessions, completed roadmap-amendment tasks and change-log
entries are historical and retain their original numbers. New specifications
must use current numbers; no new `specs/03x-*` directory is created by this
documentation amendment.

The 029 closeout exception stays scoped to software
`2ee7116ffb382c021ed575aff223c7b760a2ce7d`. A subsequent documentation or
roadmap-scanner change is not relabeled as that tested software and does not
reopen or broaden the exception. New rendering work still requires the normal
revisioned image and human HDR gates.

## Semantic dependencies and deliberate differences

- Meshlet 042 retains exactly 024/025/026/028: no new dependency on post,
  atmosphere, virtual shadows or the integration milestone.
- Feature 030 remains the shared motion-vector, jitter, history and reprojection
  owner. Fog/clouds use volume-specific history adapters, not opaque-surface
  velocities as a substitute for participating-media motion.
- Feature 033 owns the shared SceneDepthPyramid. 039/040/043 and, through 040,
  045 reuse it. RHI indirect-command work is explicit in 043; virtual-shadow
  resource/page support is explicit in 039.
- 047 ray-traced effects now explicitly reuse 030 and existing raster fallbacks;
  048 distinguishes immutable SDF/card/material data from Renderer-owned dynamic
  capture and radiance updates.
- Screen-space contact shadows supplement off-screen-capable shadow maps.
  VarianceShadowMaps (moments/filtering) and VirtualShadowMaps (pages/cache) are
  distinct names, never an ambiguous shared acronym in new contracts.
- Unreal's virtual shadows are designed around Nanite. The project deliberately
  starts with bounded atlas-backed virtualization on conventional indexed meshes;
  it retains conventional maps/CSM and makes no equivalent large-scene efficiency
  claim. Mesh shaders and hardware sparse residency are not prerequisites.
- Extra backends 050-052 are independent learning/platform tracks and do not
  gate the Vulkan/Metal renderer or 045 SSGI.

## Validation and retention

Validate unique contiguous phase IDs, names, owners, dependency DAG/edge parity,
anchors, complete phase/prompt/milestone fields, task and requirement references,
active stale-number references, shared temporal/depth ownership and protected
completed sections. Mutation tests must prove the scanner detects semantic and
structural drift, rather than only passing the current text.

Evidence remains bounded. Retain explicit Accepted images/JSON and live human
decisions; raw PPMs/DDC are regenerable and old publication duplicates may be
pruned only with exact targets, retained-equivalence checks and lease/process
safety. The maintainer's .gitignore/tutorial edits remain untouched and unstaged.
