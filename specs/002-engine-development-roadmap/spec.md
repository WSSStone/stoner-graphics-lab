# Feature Specification: Engine Development Roadmap

**Feature Branch**: `002-engine-development-roadmap`  
**Created**: 2026-04-21  
**Status**: Draft  
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

- **RHI Abstraction**: The roadmap MUST respect the 5-layer architecture (Core → RHI → Renderer → Application, with Backend implementing RHI). All phases must be designed to maintain this layered separation.
- **Design Patterns**: The roadmap MUST plan for Strategy/Composite pattern usage in each layer, avoiding monolithic designs.
- **Advanced Graphics**: The roadmap MUST include phases for Ray Tracing, Meshlet optimization, and Global Illumination as specified in the constitution.
- **Naming Conventions**: All code deliverables referenced in the roadmap MUST follow UE5-style PascalCase naming conventions.
- **Cross-Platform Compatibility**: Every phase MUST consider Windows, macOS, and Linux support. Platform-specific phases (e.g., Metal backend) MUST be clearly marked.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The project MUST contain a `doc/` directory at the project root for all development documentation.
- **FR-002**: The roadmap MUST be written as `doc/roadmap.md` — a single master document covering all development phases.
- **FR-003**: The roadmap MUST organize development into clearly defined phases, ordered by dependency (bottom-up, from Core to Application).
- **FR-004**: Each phase MUST include: phase name, scope description, key deliverables, dependencies on prior phases, estimated complexity (S/M/L/XL), and a suggested speckit prompt.
- **FR-005**: The roadmap MUST include a visual dependency graph (using Mermaid or text-based diagram) showing phase relationships.
- **FR-006**: The roadmap MUST cover at minimum these areas: Core utilities, RHI abstraction, at least one Backend implementation, Renderer fundamentals, and Application layer basics.
- **FR-007**: Each phase MUST be scoped to be completable as a single speckit feature (one `/speckit.specify` → `/speckit.plan` → `/speckit.tasks` → `/speckit.implement` cycle).
- **FR-008**: The roadmap MUST include a phase overview table with columns for: phase number, name, layer, dependencies, complexity, and status.
- **FR-009**: The roadmap MUST identify which phases are critical path (blocking other phases) vs. which can be developed in parallel.
- **FR-010**: The roadmap MUST include an "Architecture Principles" section that summarizes the constitution's constraints as they apply to development ordering.

### Key Entities

- **Phase**: A discrete unit of development work that maps to one speckit feature cycle. Contains scope, deliverables, dependencies, and complexity estimate.
- **Layer**: One of the 5 architectural layers (Core, RHI, Backend, Renderer, Application) that phases are organized around.
- **Dependency**: A relationship between phases where one phase must be completed before another can begin.
- **Deliverable**: A concrete, verifiable output of a phase (e.g., "IDevice interface with Create/Destroy lifecycle", "FVector3 math type with SIMD support").

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The roadmap contains at least 12 distinct development phases covering all 5 architectural layers.
- **SC-002**: Any developer can read a phase description and produce a `/speckit.specify` prompt within 2 minutes.
- **SC-003**: The dependency graph has zero circular dependencies and forms a valid topological order.
- **SC-004**: 100% of phases include all required fields (name, scope, deliverables, dependencies, complexity, speckit prompt).
- **SC-005**: The roadmap clearly distinguishes critical-path phases from parallelizable phases.
- **SC-006**: The document is self-contained — no external references are required to understand the development plan.

## Clarifications

### Session 2026-04-21

- Q: What is the relationship between `RenderDependencyGraph` and Phase 012's `FRenderGraph`? → A: They are overlapping concepts. Ignore `RenderDependencyGraph`; retain `FRenderGraph` in Phase 012 as the single render dependency management system.
- Q: Should we use "Frame Graph" or "Render Graph" as the canonical term? → A: Unify on **"Render Graph"** throughout the project (consistent with class name `FRenderGraph` and modern engine conventions). Remove "frame graph" references.
- Q: Math library — use GLM or implement from scratch? → A: **Implement from scratch.** This repository is a learning-oriented project aimed at deepening knowledge and self-improvement. Prefer custom implementations over third-party wrappers wherever feasible. Phase 003 (Math Library) will be a full custom implementation with SIMD optimization (complexity: L).
- Q: Should we use C++20 Modules? → A: **No.** Stick with traditional header/source separation (Public/Private directory structure). C++20 Modules have inconsistent cross-compiler support (especially Clang on macOS) and poor SCons integration. Other C++20 features (concepts, constexpr, ranges, `std::span`, `std::format`, etc.) are fully embraced.
- Q: Window system — use GLFW/SDL or implement native platform wrappers from scratch? → A: **GLFW first, native later.** Use GLFW in Phase 005 to quickly reach the "first triangle" rendering milestone. Add a later Phase 005b for native platform window implementations (Win32/Cocoa/X11-Wayland) behind the same `IWindow` abstraction. This balances learning goals with momentum — the core learning value is in the rendering pipeline, not window creation.

## Assumptions

- The SCons project skeleton (spec 001) is complete and functional as the foundation for all subsequent development.
- Development will proceed bottom-up (Core first, Application last) to respect layer dependencies.
- Each phase targets a single speckit feature cycle; phases that are too large will be split during specification.
- The initial focus is on Vulkan as the first Backend implementation, with other backends following as separate phases.
- **Self-implementation preferred**: This is a learning-oriented project. Core subsystems (math, containers, memory, etc.) should be implemented from scratch to maximize knowledge gain. Third-party libraries are acceptable only for platform abstraction (e.g., Vulkan SDK, GLFW for initial windowing) or where custom implementation would not yield meaningful learning (e.g., image codec libraries). GLFW will be used initially for windowing, with a planned native replacement phase later.
- The roadmap is a living document that will be updated as phases are completed and new requirements emerge.
- C++20 features are available and should be leveraged where appropriate, **except C++20 Modules** which are excluded due to cross-platform toolchain immaturity and SCons integration gaps. Embraced features include: concepts, constexpr improvements, ranges, `std::span`, `std::format`, coroutines, and designated initializers.
