# Implementation Plan: Engine Development Roadmap

**Branch**: `002-engine-development-roadmap` | **Date**: 2026-04-21 | **Last Amended**: 2026-09-02 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/002-engine-development-roadmap/spec.md`

## Summary

Maintain a comprehensive, phased, modular, agent-friendly development roadmap
(`doc/roadmap.md`) for the Stoner Graphics Lab cross-platform graphics engine.
Roadmap 2.3.1 contains runtime Features 003 through 041 across Core, Asset, RHI,
Backend, Renderer, and Application ownership areas. It preserves completed
Features 003-028, inserts the backend-neutral HDR output and AA/temporal phases
at 029-030, and renumbers only the former future Features 029-039 to 031-041.
Each phase maps to one Speckit feature cycle and uses the same number as its
feature.

## Technical Context

**Language/Version**: Markdown (documentation feature — no compiled code)
**Primary Dependencies**: None (pure documentation output)
**Storage**: File system — `doc/roadmap.md` at project root
**Testing**: Automated consistency scan plus manual review — verify numbering, dependency/topological ordering, anchors, task references, current phase names, required fields, and Mermaid node/edge parity
**Target Platform**: N/A (documentation)
**Project Type**: Documentation / planning artifact
**Performance Goals**: N/A
**Constraints**: Must be self-contained; must follow all constitution principles in phase ordering; must be agent-parseable for `/speckit.specify` prompts; must preserve and exclude user-owned tutorial, workflow, and `.gitignore` changes from the roadmap commit
**Scale/Scope**: 39 runtime phases across 6 ownership areas, ~1500+ lines of structured Markdown

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### Pre-Phase 0 Check

- [x] **Spec-Driven Development**: Feature spec exists at `specs/002-engine-development-roadmap/spec.md` with full requirements, user stories, and acceptance criteria.
- [x] **Decoupled Architecture**: The roadmap enforces constitution v1.4.0 dependency directions for Core, Asset, RHI, Backend, Renderer, Application, and offline Tools.
- [x] **Design Pattern Discipline**: The roadmap notes Strategy/Composite pattern requirements in the Architecture Principles section. No god-classes are planned.
- [x] **Multi-API Support**: Vulkan and Metal are implemented; Features 034 (DX12), 035 (OpenGL), and 036 (GLES) remain planned behind RHI.
- [x] **Advanced Graphics Readiness**: Features 029-033 and 037-041 split output transform, temporal reconstruction, derived data, GPU execution, backend infrastructure, renderer effects, and GI integration into bounded Speckit cycles.
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

**Structure Decision**: This remains a documentation-only feature. Roadmap 2.3
synchronizes its Feature 002 contracts and the completed Feature 028 evidence
policy. It plans, but does not implement, Features 029-041.

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

## Generated Artifacts

| Artifact | Path | Status |
|----------|------|--------|
| Implementation Plan | `specs/002-engine-development-roadmap/plan.md` | ✅ Complete |
| Research | `specs/002-engine-development-roadmap/research.md` | ✅ Complete |
| Data Model | `specs/002-engine-development-roadmap/data-model.md` | ✅ Complete |
| Quickstart | `specs/002-engine-development-roadmap/quickstart.md` | ✅ Complete |
| Contracts | `specs/002-engine-development-roadmap/contracts/roadmap-phase-schema.md` | ✅ Complete |
| Constitution | `.specify/memory/constitution.md` | ✅ Amended to v1.4.0 |
| Master Roadmap | `doc/roadmap.md` | ✅ Updated to v2.3.1 |
