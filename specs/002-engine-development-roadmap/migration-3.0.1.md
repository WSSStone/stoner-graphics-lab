# Roadmap 3.0.1 — Full profiling after rendering effects

Date: 2026-09-06. The maintainer requested full profiling after the rendering
pipeline's effects. This amendment puts measurement before integrated acceptance,
not after the later optional geometry/GI/RT extensions.

| Roadmap 3.0.0 | Roadmap 3.0.1 | Responsibility |
|---|---|---|
| 031 | 040 | Frame Profiling & Render Diagnostics |
| 032 | 031 | Raster Shadow Maps & Cascades |
| 033 | 032 | Screen-Space Shadows & Shadow Filtering |
| 034 | 033 | Sky Atmosphere & Environment Lighting |
| 035 | 034 | Height Fog & Volumetric Fog |
| 036 | 035 | Volumetric Clouds |
| 037 | 036 | Exposure, Bloom & Color Grading |
| 038 | 037 | Depth of Field & Motion Blur |
| 039 | 038 | Virtual Shadow Maps |
| 040 | 039 | Screen-Space Ambient Occlusion & Reflections |

Completed 003-029, next 030, acceptance 041 and phases 042-052 keep their identities.
The [original migration](migration-3.0.md), Phase 11 task records and
[original scan](validation/roadmap-3.0-scan.json) retain their 3.0.0 historical
meaning; they are not the current assignment index.
[phase-index.json](phase-index.json) records both migration steps.

Effects 030-039 have no direct or transitive prerequisite on full profiling.
They retain debug outputs, resource/sample counters and bounded execution.
Feature 040 adds complete GPU/CPU timing, performance views and budget checks
over the delivered effects; its prerequisites cover clouds, camera post effects,
virtual shadows and AO/SSR (and their dependency closures). Feature 041 explicitly
requires 040. Later consumers retain their semantic profiling prerequisite,
now numbered 040. SceneDepthPyramid moves to 032; consumers share it, and all
temporal effects still share 030. Meshlet 042 retains exactly 024/025/026/028.

The accepted tradeoff is later performance-bottleneck discovery; it does not
waive final performance measurement or quality acceptance. No runtime code,
completed phase details, accepted artifacts or user tutorial files are changed.
Validation: [roadmap-3.0.1-scan.json](validation/roadmap-3.0.1-scan.json).
