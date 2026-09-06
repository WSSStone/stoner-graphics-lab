# Roadmap 3.1 — Interactive rendering lab first

Date: 2026-09-06. The maintainer accepted Application Interactive Rendering Lab
& ImGui Integration as next Feature 030, before TAA and further rendering effects.
This is a planning amendment, not a claim that GUI/HDR interaction is implemented.

## Previous-to-current mapping

Completed 003-029 are unchanged. New 030 has no prior identity.

| Roadmap 3.0.1 | Roadmap 3.1 | Responsibility |
|---|---|---|
| 030 | 031 | Anti-Aliasing & Temporal Reconstruction |
| 031 | 032 | Raster Shadow Maps & Cascades |
| 032 | 033 | Screen-Space Shadows & Shadow Filtering |
| 033 | 034 | Sky Atmosphere & Environment Lighting |
| 034 | 035 | Height Fog & Volumetric Fog |
| 035 | 036 | Volumetric Clouds |
| 036 | 037 | Exposure, Bloom & Color Grading |
| 037 | 038 | Depth of Field & Motion Blur |
| 038 | 039 | Virtual Shadow Maps |
| 039 | 040 | Screen-Space Ambient Occlusion & Reflections |
| 040 | 041 | Frame Profiling & Render Diagnostics |
| 041 | 042 | Complete Rendering Pipeline Integration & Quality Baseline |
| 042 | 043 | Meshlet Derived Data |
| 043 | 044 | GPU-Driven Visibility & LOD |
| 044 | 045 | Streaming & Residency |
| 045 | 046 | Screen-Space GI & Temporal |
| 046 | 047 | Ray Tracing & Vulkan Backend Foundation |
| 047 | 048 | Ray-Traced Renderer Effects |
| 048 | 049 | SDF & Surface Cache Assets |
| 049 | 050 | Hybrid GI Integration |
| 050 | 051 | DirectX 12 Backend |
| 051 | 052 | OpenGL Backend |
| 052 | 053 | GLES Backend |

The current [phase index](phase-index.json) maps previous 3.0.1 to 3.1 and
composes `oldToNew` from 2.3.2 directly to current identities. The original
[migration 3.0](migration-3.0.md), [migration 3.0.1](migration-3.0.1.md),
dated tasks/clarifications and their scan receipts are historical, unchanged.
In particular, old 030 TAA is now 031, old 031 Meshlet (2.3.2) is now 043, and
old 039 SSGI (2.3.2) is now 046. Completed 028/029 documents retain those old
numbers and their evidence hashes; they are not current work assignments.

## Dependency and ownership changes

New 030 depends on 004/008/013/015/016/017/018/019/027/028/029.
TAA 031 and shadow maps 032 explicitly require 030; the remaining near-term
effects reuse the interactive shell transitively. Meshlet 043 retains exactly
024/025/026/028, independent of UI, post-processing or virtual shadows.
Temporal services are 031; SceneDepthPyramid is 033. SSGI 046 and subsequent
temporal consumers reuse 031, never another temporal framework.
Full Profiling is 041 after effects 031-040; integration 042 explicitly needs it.

Application owns interaction and private ImGui adapters; Renderer owns public
backend-neutral UI draw packets, GPU lifetime and Render Graph composition
through RHI. UI follows scene post-processing in display-linear output space,
uses explicit reference-white/alpha/color rules and the sole 029 output transfer.
No exposure/TAA/DOF/bloom treatment of widgets. Interactive presentation must
not require synchronous CPU readback every frame.

Formal scene captures default to UI disabled and frozen settings. Interactive
presets are not Accepted evidence. Bounded UI smoke tests remain separate;
macOS PQ/EDR appearance needs live human authority, Windows remains SDR-only
validation. No inherited 029 closeout exception is applied to future work.

No runtime spec directory, library vendoring or implementation is created here.
No VT foundation/SVT/RVT phase is inserted: that discussion remains a proposal.
Full editor/native-widget-toolkit and full profiling are not part of 030.

Validation: [roadmap-3.1-scan.json](validation/roadmap-3.1-scan.json).
