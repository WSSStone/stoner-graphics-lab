# Clarification Log: SCons Project Skeleton

**Feature Branch**: `001-scons-project-skeleton`
**Date**: 2026-04-06
**Trigger**: User requested architecture review alongside directory structure design.

## Decisions

### Decision 1 — 4-Layer Architecture Adopted

**Question**: Should the spec adopt a 4-layer architecture (Application / Renderer / RHI / Backend) instead of the constitution's 3-layer model (Application / RHI / Graphics API)?

**Options Presented**: A (Adopt 4-layer), B (Keep 3-layer, Renderer as sub-module), C (4-layer with "GraphicsAPI" rename)

**User Choice**: **A** — Adopt the 4-layer model.

**Rationale**: The separation of Renderer from Application is a well-established pattern in production engines. It prevents the Application layer from becoming a God-class that mixes scene logic with rendering orchestration, aligning with Constitution Principle III.

**Spec Impact**: Architecture & Design Constraints rewritten. FR-003 updated with 4-layer directory structure.

---

### Decision 2 — Advanced Graphics in Renderer Layer

**Question**: Where should advanced graphics subsystems (Ray Tracing, Meshlets, GI) live?

**Options Presented**: A (Inside Renderer), B (Separate top-level directory), C (Inside RHI)

**User Choice**: **A** — Place inside the Renderer layer as sub-modules.

**User Clarification**: "The renderer layer shall take most of the rendering functionalities; the application layer shall serve like a game engine frontend, or a scene graph, etc."

**Spec Impact**: FR-012 added. Architecture constraints updated to specify Renderer sub-directories for RT, Meshlets, GI.

---

### Decision 3 — Strict Adjacent-Only + Shared Core Layer

**Question**: What inter-layer dependency direction should be enforced?

**Options Presented**: A (Strict top-down with skip-level), B (Strict adjacent-only), C (Adjacent-only + shared Core)

**User Choice**: **C** — Strict adjacent-only dependencies with a shared Core utility layer.

**Spec Impact**: Architecture now has 5 layers (Core, Application, Renderer, RHI, Backend). FR-013 and FR-014 added. Dependency diagram added to Architecture section.

---

### Decision 4 — Static Libraries Per Layer

**Question**: What build artifact type should each layer produce?

**Options Presented**: A (Static lib per layer), B (Static lib per module), C (Single monolith)

**User Choice**: **A** — Static libraries per layer, with modular intent.

**User Clarification**: "I do want a modular system with multiple modules."

**Spec Impact**: FR-011 added. Architecture constraints updated with Build Artifact Isolation rule.

---

### Decision 5 — Flat Source Root Layout

**Question**: What should the concrete top-level directory layout be?

**Options Presented**: A (Flat `Source/` root), B (`Engine/Source/` namespace wrapper)

**User Choice**: **A** — Flat Source Root.

**Spec Impact**: FR-003 updated with concrete directory tree diagram showing `Source/` at project root.

## Summary of Spec Changes

| Section | Change |
|---------|--------|
| Status | Draft → Draft (Clarified) |
| User Story 1, Scenario 3 | Updated to reference 5 source layers |
| Architecture & Design Constraints | Completely rewritten: 5-layer diagram, dependency rules, build artifact isolation, advanced graphics placement |
| FR-003 | Replaced with concrete directory tree |
| FR-008 | Updated to reference `Source/Backend/` path |
| FR-011 (new) | Static library per layer |
| FR-012 (new) | Renderer sub-directories for RT, Meshlets, GI |
| FR-013 (new) | Core layer definition and zero-dependency rule |
| FR-014 (new) | Adjacent-only dependency enforcement |
| Key Entities | Updated with Layer, Module, Core definitions |
| SC-003 | Updated to reference 5-layer names |

## Constitution Alignment

The 4-layer + Core architecture is a **refinement** of the constitution's `Application <-> RHI <-> Graphics API` model, not a contradiction. The constitution's 3-layer description is a high-level abstraction; the 5-directory implementation (Core, Application, Renderer, RHI, Backend) is a concrete realization that:

- Preserves the RHI abstraction boundary (Principle II)
- Adds the Renderer layer to prevent God-classes (Principle III)
- Maintains all Backend placeholders for Multi-API support (Principle IV)
- Places advanced graphics in Renderer for Advanced Graphics Readiness (Principle V)
- Follows PascalCase directory naming (Principle VI)
- Supports cross-platform via build system, not manual config (Principle VII)

A constitution amendment to formalize the 5-layer model is **recommended** but not blocking for this feature.
