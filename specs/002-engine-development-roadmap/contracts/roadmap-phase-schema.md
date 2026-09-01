# Contract: Roadmap Phase Schema

**Feature**: 002-engine-development-roadmap
**Date**: 2026-04-21
**Last Amended**: 2026-09-01

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
6. Status values are one of: ⬜ Todo, 🔄 In Progress, ✅ Done, ⏸️ Paused
7. Every phase in the table of contents, overview table, dependency graph, and detail sections uses the same number and title
8. Asset phases MUST preserve `Asset -> Core`; GPU realization belongs to Renderer/RHI and offline executables belong to Tools
9. A phase MUST own one primary responsibility boundary; offline build tools and runtime lifecycle services require separate phases
10. Graphics API backends MUST use separate phases when their platform, capability, or lifecycle validation differs
11. The active runtime phase set MUST be exactly 003-041, with completed 003-028 immutable and only future phases subject to the Roadmap 2.3 renumbering
12. Any phase changing formal image output MUST increment workload revision, create an exact-dimension Candidate, require explicit maintainer acceptance, prohibit alignment/cropping/scaling/resampling, and retain bounded PNG/JSON evidence; Feature 028 v2 remains historical evidence
13. Feature 039 Screen-Space GI MUST depend on and reuse Feature 030 motion-vector, jitter, history, reprojection, rejection, and invalidation contracts rather than define a duplicate temporal framework
