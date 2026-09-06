# Implementation Plan: Engine Development Roadmap

**Branch**: `002-engine-development-roadmap` | **Date**: 2026-04-21 | **Last Amended**: 2026-09-06 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/002-engine-development-roadmap/spec.md`

## Summary

Maintain a comprehensive, phased, modular, agent-friendly development roadmap
(`doc/roadmap.md`) for the Stoner Graphics Lab cross-platform graphics engine.
Roadmap 3.1.0 contains runtime Features 003 through 053 across Core, Asset, RHI,
Backend, Renderer, and Application ownership areas. It preserves completed
Features 003-029, inserts next 030 interactive rendering lab/ImGui before 031 temporal,
and prioritizes 032-040 raster shadows,
atmosphere/environment, fog/clouds, post-processing and AO/SSR, followed by
041 full profiling and 042 integrated quality. Effects do not depend on full
profiling; they retain debug outputs, resource/sample counters and bounded execution.
Only unstarted former 030-052 migrate to 031-053; extra backends
are optional independent tracks. Each phase retains a responsibility boundary.
Feature 029 is now complete by the explicit revision-scoped maintainer
exception in `specs/029-hdr-output-transform/closeout.md`; Feature 030 is next.
This records a governance disposition, not a strict same-SHA authority pass.

## Technical Context

**Language/Version**: Markdown (documentation feature — no compiled code)
**Primary Dependencies**: None (pure documentation output)
**Storage**: File system — `doc/roadmap.md` at project root
**Testing**: Automated consistency scan plus manual review — verify numbering, dependency/topological ordering, anchors, task references, current phase names, required fields, and Mermaid node/edge parity
**Target Platform**: N/A (documentation)
**Project Type**: Documentation / planning artifact
**Performance Goals**: N/A
**Constraints**: Must be self-contained; must follow all constitution principles in phase ordering; must be agent-parseable for `/speckit.specify` prompts; must preserve and exclude user-owned tutorial, workflow, and `.gitignore` changes from the roadmap commit
**Scale/Scope**: 51 runtime phases across 6 ownership areas; 24 future phases with bounded milestones

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### Pre-Phase 0 Check

- [x] **Spec-Driven Development**: Feature spec exists at `specs/002-engine-development-roadmap/spec.md` with full requirements, user stories, and acceptance criteria.
- [x] **Decoupled Architecture**: The roadmap enforces constitution v1.4.0 dependency directions for Core, Asset, RHI, Backend, Renderer, Application, and offline Tools.
- [x] **Design Pattern Discipline**: The roadmap notes Strategy/Composite pattern requirements in the Architecture Principles section. No god-classes are planned.
- [x] **Multi-API Support**: Vulkan and Metal are implemented; Features 051 (DX12), 052 (OpenGL), and 053 (GLES) remain planned behind RHI.
- [x] **Advanced Graphics Readiness**: Features 030-042 prioritize the full raster renderer; 043-050 split derived data, GPU execution, residency, RT and GI; 051-053 provide optional backend tracks. Each is bounded by explicit milestones.
- [x] **Naming Conventions**: All deliverable names in the roadmap follow UE5-style PascalCase with appropriate prefixes (F, I, E, T).
- [x] **Cross-Platform Compatibility**: Every platform-sensitive phase notes Windows/macOS/Linux support. Platform-specific native backends are separate, and GLES explicitly excludes Android application lifecycle/packaging.

**GATE RESULT**: ✅ PASS — All constitution principles satisfied.

## Project Structure

### Documentation (this feature)

```text
specs/002-engine-development-roadmap/
├── plan.md              # This file
├── research.md          # Phase 0 output — technology decisions
├── data-model.md        # Phase 1 output — entity model for roadmap
├── quickstart.md        # Phase 1 output — how to use the roadmap
└── tasks.md             # Phase 2 output (created by /speckit.tasks)
```

### Source Code (repository root)

```text
doc/
└── roadmap.md           # The single master roadmap document (ALREADY EXISTS as draft)
```

**Structure Decision**: This remains a documentation-only feature. Roadmap 3.1
synchronizes Feature 002 contracts and active references without rewriting
completed 028/029 evidence. `phase-index.json` and `migration-3.1.md` describe
current identities and historical-number resolution. It plans, but does not
implement, Features 031-053.

## Constitution Re-Check (Post Phase 1 Design)

- [x] **Spec-Driven Development**: Spec, research, data model, contracts, and quickstart all complete. Design is fully documented before implementation.
- [x] **Decoupled Architecture**: Data model enforces that Phase dependencies only reference lower-numbered phases, maintaining the bottom-up layered build order.
- [x] **Design Pattern Discipline**: The roadmap phase schema contract ensures each phase is modular and self-contained — no monolithic "do everything" phases.
- [x] **Multi-API Support**: Research confirms Vulkan-first strategy with Metal/DX12/GL phases planned. No design decisions conflict with multi-API support.
- [x] **Advanced Graphics Readiness**: Research confirms distinct HDR output and AA/temporal foundations, unchanged Meshlet dependencies, and reusable temporal contracts for Screen-Space GI.
- [x] **Naming Conventions**: Contract enforces UE5 naming for all deliverable names.
- [x] **Cross-Platform Compatibility**: Research confirms GLFW for initial cross-platform windowing, with native backends planned later.
- [x] **Asset Boundary**: Research separates CPU/content ownership in Asset, GPU realization in Renderer/RHI, and offline processing in Tools.

**POST-DESIGN GATE RESULT**: ✅ PASS — No regressions from Phase 1 design work.

## Complexity Tracking

No constitution violations to justify. This is a documentation feature that plans future code — it does not introduce any architectural decisions itself.

## Amendment execution and validation

1. Freeze current completed 003-029 detail text and user-owned changes.
2. Define 031-053 identities, semantic dependencies and migration mapping.
3. Update the master roadmap, explicit frame layout, milestones and quality gates.
4. Synchronize governance and AGENTS; retain dated historical references.
5. Extend the read-only scanner and mutation tests, then scan until zero findings.
6. Retain bounded scan results; do not implement runtime features or commit unrelated edits.

## Generated Artifacts

| Artifact | Path | Status |
|----------|------|--------|
| Implementation Plan | `specs/002-engine-development-roadmap/plan.md` | ✅ Complete |
| Research | `specs/002-engine-development-roadmap/research.md` | ✅ Complete |
| Data Model | `specs/002-engine-development-roadmap/data-model.md` | ✅ Complete |
| Quickstart | `specs/002-engine-development-roadmap/quickstart.md` | ✅ Complete |
| Contracts | `specs/002-engine-development-roadmap/contracts/roadmap-phase-schema.md` | ✅ Complete |
| Constitution | `.specify/memory/constitution.md` | ✅ Amended to v1.4.0 |
| Phase Index | `specs/002-engine-development-roadmap/phase-index.json` | Current 3.1 identities |
| Migration | `specs/002-engine-development-roadmap/migration-3.1.md` | Current mapping and historical-reference chain |
| Master Roadmap | `doc/roadmap.md` | ✅ Updated to v3.1.0 |

## Interactive-first amendment execution

T091-T096 add the 030 phase and shift unstarted 030-052 to 031-053, synchronize
the index/table/graph/prompts and active references, add input/UI/HDR evidence
contracts, and extend read-only mutation checks. This plans ImGui integration;
it does not vendor a library, create a runtime feature spec or implement the UI.
Full Profiling remains 041 after effects, before integrated acceptance 042.
Current mapping: [migration-3.1.md](migration-3.1.md).
