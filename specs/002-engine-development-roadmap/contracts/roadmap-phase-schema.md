# Contract: Roadmap Phase Schema

**Feature**: 002-engine-development-roadmap
**Date**: 2026-04-21
**Last Amended**: 2026-09-06

## Overview

This contract defines the required structure for each Phase entry in `doc/roadmap.md`. Any tool or agent that reads or modifies the roadmap must respect this schema.

## Phase Section Contract

Every phase in the "Phase Details" section of `doc/roadmap.md` MUST contain the following subsections in order:

### Required Header Fields

```markdown
### Phase {NNN} — {Layer}: {Name}

**Layer**: {Core|Asset|RHI|Backend|Renderer|Application}
**Dependencies**: {comma-separated phase numbers, or "001 (SCons Skeleton)"}
**Complexity**: {S|M|L|XL} ({duration estimate})
**Critical Path**: {✅ Yes|❌ No} — {brief reason}
```

### Required Subsections

1. **Scope** — 1-3 paragraphs describing what the phase covers
2. **Key Deliverables** — Bulleted list of concrete outputs (class names, file names)
3. **What's Excluded** — Bulleted list of explicitly out-of-scope items
4. **Speckit Prompt** — A fenced code block containing a ready-to-use `/speckit.specify` prompt

### Phase Overview Table Row Contract

Each phase MUST have a corresponding row in the Phase Overview Table:

```markdown
| {NNN} | {Name} | {Layer} | {Dependencies} | {Complexity} | {Critical Path} | {Status} |
```

### Dependency Graph Node Contract

Each phase MUST appear as a node in the Mermaid dependency graph:

```markdown
P{NNN}[{NNN}: {Short Name}]
```

With edges for each dependency:

```markdown
P{DEP} --> P{NNN}
```

## Validation Rules

1. Runtime phase numbers are 3-digit, zero-padded, monotonically increasing and match their Speckit feature numbers
2. Dependencies only reference lower-numbered phases
3. No circular dependencies in the graph
4. Every deliverable name follows UE5 naming conventions
5. Every speckit prompt is self-contained (no "see above" references)
6. Status values are one of: ⬜ Todo, 🔄 In Progress, ✅ Done, ⏸️ Paused; a Done qualifier may explicitly identify an approved exception, never an inferred all-gates pass
7. Every phase in the table of contents, overview table, dependency graph, and detail sections uses the same number and title
8. Asset phases MUST preserve `Asset -> Core`; GPU realization belongs to Renderer/RHI and offline executables belong to Tools
9. A phase MUST own one primary responsibility boundary; offline build tools and runtime lifecycle services require separate phases
10. Graphics API backends MUST use separate phases when their platform, capability, or lifecycle validation differs
11. The active runtime phase set MUST be exactly 003-053, with completed 003-029 immutable; only unstarted phases migrate per `phase-index.json`. Historical completed documents retain delivery-time numbering resolved by `migration-3.1.md` and historical migration records.
12. Any phase changing formal image output MUST increment workload revision, create an exact-dimension Candidate, require explicit maintainer acceptance, prohibit alignment/cropping/scaling/resampling, and retain bounded PNG/JSON evidence; Feature 028 v2 remains historical evidence
13. Feature 046 Screen-Space GI MUST depend on and reuse Feature 031 motion-vector, jitter, history, reprojection, rejection, and invalidation contracts rather than define a duplicate temporal framework

14. Future phases MUST include Delivery Milestones; estimates follow specification rather than treating every XL as the same duration.
15. All future identities, owners and exact dependencies MUST match `phase-index.json`; every dependency must exist, precede its consumer and agree across table, DAG and details.
16. Shadow terminology MUST distinguish screen-space contact shadows, conventional maps/CSM, VarianceShadowMaps and VirtualShadowMaps; conventional fallback and off-screen limits are explicit.
17. The near-term renderer track is 030-042; geometry/lighting is 043-050, optional backends 051-053. The frame-layout and quality gates MUST show their real producer/consumer ordering.
18. Feature 033 owns SceneDepthPyramid. Temporal consumers extend 031 with signal-specific state, not separate global history/reprojection frameworks.
19. Feature 043 Meshlet dependencies MUST remain exactly 024/025/026/028; 048 includes 031, and 049 keeps dynamic radiance in Renderer.

20. Full profiling is Feature 041 after rendering effects 031-040 and a mandatory prerequisite of integrated acceptance 042. Effects MUST NOT depend on 041; retain debug outputs, resource/sample counters and bounded execution before full timing, performance views and budget checks.

21. The next phase MUST be 030 Application Interactive Rendering Lab & ImGui Integration, followed by temporal 031. Camera/UI/HDR scope, input capture, private ImGui/backend-neutral draw ownership, no mandatory per-frame synchronous CPU readback, UI-free formal capture defaults and bounded human review MUST be explicit.
22. Effects 031-040 MUST depend directly or transitively on the 030 interactive shell and reuse it; full profiling 041 reuses the same UI without being a prerequisite of 030. No VT phase is introduced by this amendment.
