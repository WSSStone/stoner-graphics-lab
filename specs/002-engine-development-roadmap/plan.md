# Implementation Plan: Engine Development Roadmap

**Branch**: `002-engine-development-roadmap` | **Date**: 2026-04-21 | **Last Amended**: 2026-07-24 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/002-engine-development-roadmap/spec.md`

## Summary

Maintain a comprehensive, phased, modular, agent-friendly development roadmap
(`doc/roadmap.md`) for the Stoner Graphics Lab cross-platform graphics engine.
Roadmap 2.0 contains runtime Features 003 through 032 across Core, Asset, RHI,
Backend, Renderer, and Application ownership areas. Each phase maps to one
Speckit feature cycle and uses the same number as its feature.

## Technical Context

**Language/Version**: Markdown (documentation feature — no compiled code)
**Primary Dependencies**: None (pure documentation output)
**Storage**: File system — `doc/roadmap.md` at project root
**Testing**: Manual review — verify topological ordering, completeness of required fields, and Mermaid diagram validity
**Target Platform**: N/A (documentation)
**Project Type**: Documentation / planning artifact
**Performance Goals**: N/A
**Constraints**: Must be self-contained; must follow all constitution principles in phase ordering; must be agent-parseable for `/speckit.specify` prompts
**Scale/Scope**: 30 runtime phases across 6 ownership areas, ~1000+ lines of structured Markdown

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### Pre-Phase 0 Check

- [x] **Spec-Driven Development**: Feature spec exists at `specs/002-engine-development-roadmap/spec.md` with full requirements, user stories, and acceptance criteria.
- [x] **Decoupled Architecture**: The roadmap enforces constitution v1.4.0 dependency directions for Core, Asset, RHI, Backend, Renderer, Application, and offline Tools.
- [x] **Design Pattern Discipline**: The roadmap notes Strategy/Composite pattern requirements in the Architecture Principles section. No god-classes are planned.
- [x] **Multi-API Support**: Vulkan is implemented; Features 030 (Metal), 031 (DX12), and 032 (OpenGL/GLES) remain planned behind RHI.
- [x] **Advanced Graphics Readiness**: Features 026 (Meshlets), 028 (Ray Tracing), and 029 (Global Illumination) consume the new Asset foundation instead of hard-coded content.
- [x] **Naming Conventions**: All deliverable names in the roadmap follow UE5-style PascalCase with appropriate prefixes (F, I, E, T).
- [x] **Cross-Platform Compatibility**: Every phase notes Win/Mac/Linux support. Platform-specific phases 030 and 031 are clearly marked.

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

**Structure Decision**: This remains a documentation-only feature. Roadmap 2.0
also synchronizes its constitution and Feature 002 contracts. It plans, but does
not create, `Source/Asset` or `Tools/AssetCooker`.

## Constitution Re-Check (Post Phase 1 Design)

- [x] **Spec-Driven Development**: Spec, research, data model, contracts, and quickstart all complete. Design is fully documented before implementation.
- [x] **Decoupled Architecture**: Data model enforces that Phase dependencies only reference lower-numbered phases, maintaining the bottom-up layered build order.
- [x] **Design Pattern Discipline**: The roadmap phase schema contract ensures each phase is modular and self-contained — no monolithic "do everything" phases.
- [x] **Multi-API Support**: Research confirms Vulkan-first strategy with Metal/DX12/GL phases planned. No design decisions conflict with multi-API support.
- [x] **Advanced Graphics Readiness**: Research confirms Meshlet, RT, and GI phases are retained with appropriate dependencies.
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
| Master Roadmap | `doc/roadmap.md` | ✅ Updated to v2.0.0 |
