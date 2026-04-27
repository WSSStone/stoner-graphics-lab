# Tasks: Engine Development Roadmap

**Input**: Design documents from `/specs/002-engine-development-roadmap/`
**Prerequisites**: plan.md (required), spec.md (required), research.md, data-model.md, contracts/roadmap-phase-schema.md, quickstart.md

**Tests**: Automated tests are not requested for this documentation-only feature. Manual validation tasks are included in the final phase.

**Organization**: Tasks are grouped by user story so each story can be implemented and reviewed independently.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1, US2, US3)
- Each task names the exact file path to create, update, or validate

## Path Conventions

- **Roadmap deliverable**: `doc/roadmap.md`
- **Feature docs**: `specs/002-engine-development-roadmap/`
- **Schema contract**: `specs/002-engine-development-roadmap/contracts/roadmap-phase-schema.md`

---

## Phase 1: Setup (Shared Documentation Infrastructure)

**Purpose**: Confirm the documentation target and source artifacts before editing the roadmap.

- [X] T001 Create or verify the documentation directory and roadmap file at `doc/roadmap.md`
- [X] T002 Review feature requirements and user stories in `specs/002-engine-development-roadmap/spec.md`
- [X] T003 Review implementation constraints and deliverable scope in `specs/002-engine-development-roadmap/plan.md`
- [X] T004 Review clarification decisions for Render Graph terminology, custom math, C++20 modules, learning-oriented implementation, and GLFW-first windowing in `specs/002-engine-development-roadmap/research.md`
- [X] T005 Review the phase entity fields, document structure, and validation rules in `specs/002-engine-development-roadmap/data-model.md`
- [X] T006 Review required phase section schema and status values in `specs/002-engine-development-roadmap/contracts/roadmap-phase-schema.md`

---

## Phase 2: Foundational (Blocking Roadmap Requirements)

**Purpose**: Establish the shared roadmap framing that all user stories depend on.

**CRITICAL**: No user story work should begin until this phase is complete.

- [X] T007 Update roadmap title metadata, version, active status, and Phase 001 prerequisite note in `doc/roadmap.md`
- [X] T008 Update the overview and current-state summary for the post-SCons skeleton repository state in `doc/roadmap.md`
- [X] T009 Update architecture principles to include the 5-layer structure, adjacent-only dependencies, RHI abstraction, Strategy/Composite discipline, UE5 naming, and cross-platform rules in `doc/roadmap.md`
- [X] T010 Normalize roadmap terminology to use "Render Graph" and `FRenderGraph`, removing conflicting "Frame Graph" or `RenderDependencyGraph` wording in `doc/roadmap.md`
- [X] T011 Add roadmap-wide technology decisions for custom Core implementation, no C++20 modules, GLFW-first windowing, Vulkan-first backend, and later native backends in `doc/roadmap.md`

**Checkpoint**: Shared roadmap context is complete and user story sections can now be implemented.

---

## Phase 3: User Story 1 - Understand the Full Development Path (Priority: P1) MVP

**Goal**: A developer or AI agent can open `doc/roadmap.md`, understand the full phase order, identify dependencies, and know what to build next.

**Independent Test**: Read `doc/roadmap.md` and verify it presents an ordered set of development phases with enough context to start a `/speckit.specify` cycle for any phase.

### Implementation for User Story 1

- [X] T012 [US1] Build or refresh the table of contents with links for all major roadmap sections and phases 002 through 024 in `doc/roadmap.md`
- [X] T013 [US1] Build or refresh the phase overview table with phases 002 through 024, layer, dependencies, complexity, critical path flag, and status in `doc/roadmap.md`
- [X] T014 [US1] Build or refresh the Mermaid dependency graph with nodes P001 through P024 and all topological dependency edges in `doc/roadmap.md`
- [X] T015 [US1] Document parallel development tracks for Core/RHI, Vulkan Backend, Renderer, Application, Integration Milestone, Advanced Rendering, and Additional Backends in `doc/roadmap.md`
- [X] T016 [US1] Document the recommended solo-developer execution order from phase 002 through phase 024 in `doc/roadmap.md`
- [X] T017 [US1] Document the "How to Use This Roadmap" workflow for choosing a phase, running speckit commands, implementing, and updating status in `doc/roadmap.md`
- [X] T018 [US1] Validate that every phase listed in the overview table also appears in the dependency graph, table of contents, and execution order in `doc/roadmap.md`

**Checkpoint**: User Story 1 is independently reviewable by confirming the roadmap clearly answers "what comes next and why."

---

## Phase 4: User Story 2 - Plan a Specific Development Phase (Priority: P2)

**Goal**: Any phase section contains enough detail to launch a specific `/speckit.specify` command without additional research.

**Independent Test**: Select any phase in `doc/roadmap.md` and verify it contains scope, key deliverables, dependencies, complexity, exclusions, and a self-contained speckit prompt.

### Implementation for User Story 2

- [X] T019 [US2] Write or update Core phase detail sections 002 through 005 with required header fields, scope, deliverables, exclusions, and speckit prompts in `doc/roadmap.md`
- [X] T020 [US2] Write or update RHI phase detail sections 006 through 007 with required header fields, scope, deliverables, exclusions, and speckit prompts in `doc/roadmap.md`
- [X] T021 [US2] Write or update Vulkan Backend phase detail sections 008 through 011 with required header fields, scope, deliverables, exclusions, and speckit prompts in `doc/roadmap.md`
- [X] T022 [US2] Write or update Renderer foundation phase detail sections 012 through 014 with required header fields, scope, deliverables, exclusions, and speckit prompts in `doc/roadmap.md`
- [X] T023 [US2] Write or update Application and integration milestone phase detail sections 015 through 017 with required header fields, scope, deliverables, exclusions, and speckit prompts in `doc/roadmap.md`
- [X] T024 [US2] Write or update advanced Renderer phase detail sections 018 through 021 with required header fields, scope, deliverables, exclusions, and speckit prompts in `doc/roadmap.md`
- [X] T025 [US2] Write or update additional Backend phase detail sections 022 through 024 with required header fields, scope, deliverables, exclusions, and speckit prompts in `doc/roadmap.md`
- [X] T026 [US2] Validate every phase detail section against `specs/002-engine-development-roadmap/contracts/roadmap-phase-schema.md` and fix missing fields in `doc/roadmap.md`
- [X] T027 [US2] Validate deliverable names for UE5-style prefixes and document any intentional exceptions in `doc/roadmap.md`
- [X] T028 [US2] Validate that every speckit prompt is self-contained and does not rely on "see above" or external context in `doc/roadmap.md`

**Checkpoint**: User Story 2 is independently reviewable by selecting any phase and using its prompt to start a new spec.

---

## Phase 5: User Story 3 - Track Overall Project Progress (Priority: P3)

**Goal**: A project lead can see all phases, statuses, critical path work, and update progress as phases complete.

**Independent Test**: Inspect `doc/roadmap.md` and verify the overview table and status guidance make project progress visible and updatable.

### Implementation for User Story 3

- [X] T029 [US3] Ensure the phase overview table includes a status column for every phase 002 through 024 in `doc/roadmap.md`
- [X] T030 [US3] Ensure the phase overview table clearly distinguishes critical-path phases from parallelizable phases in `doc/roadmap.md`
- [X] T031 [US3] Add or update the status legend using the allowed status values from the phase schema contract in `doc/roadmap.md`
- [X] T032 [US3] Add or update instructions for marking a phase done after implementation and verification in `doc/roadmap.md`
- [X] T033 [US3] Add or update the risk register with risks for Vulkan SDK availability, MoltenVK limitations, shader tooling, render graph scope, C++20 support, third-party dependencies, and phase scope creep in `doc/roadmap.md`
- [X] T034 [US3] Validate that all phase statuses in `doc/roadmap.md` use only values allowed by `specs/002-engine-development-roadmap/contracts/roadmap-phase-schema.md`

**Checkpoint**: User Story 3 is independently reviewable by updating a single phase status and confirming project progress remains clear.

---

## Phase 6: Polish & Cross-Cutting Validation

**Purpose**: Final review across all stories and documentation acceptance criteria.

- [X] T035 Validate that `doc/roadmap.md` satisfies SC-001 by containing at least 12 distinct development phases across all 5 architectural layers
- [X] T036 Validate that `doc/roadmap.md` satisfies SC-002 by confirming a developer can produce a `/speckit.specify` prompt from any phase in under 2 minutes
- [X] T037 Validate that `doc/roadmap.md` satisfies SC-003 by confirming the dependency graph has no circular dependencies and every dependency points to an earlier phase
- [X] T038 Validate that `doc/roadmap.md` satisfies SC-004 by confirming 100% of phases include name, scope, deliverables, dependencies, complexity, and speckit prompt
- [X] T039 Validate that `doc/roadmap.md` satisfies SC-005 by confirming critical-path phases are distinguished from parallelizable phases
- [X] T040 Validate that `doc/roadmap.md` satisfies SC-006 by confirming the roadmap is self-contained and understandable without external references
- [X] T041 Validate Mermaid graph syntax and node consistency in `doc/roadmap.md`
- [X] T042 Run the quickstart walkthrough from `specs/002-engine-development-roadmap/quickstart.md` against `doc/roadmap.md`
- [X] T043 Proofread internal anchors, table formatting, terminology consistency, and Markdown readability in `doc/roadmap.md`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion - blocks all user stories
- **User Story 1 (Phase 3)**: Depends on Foundational phase completion
- **User Story 2 (Phase 4)**: Depends on Foundational phase completion; benefits from US1 phase ordering but remains independently reviewable
- **User Story 3 (Phase 5)**: Depends on Foundational phase completion; benefits from US1 overview table but remains independently reviewable
- **Polish (Phase 6)**: Depends on all selected user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: MVP. Can start after Foundational phase. Provides the full development path.
- **User Story 2 (P2)**: Can start after Foundational phase. Uses the phase set from US1 if available.
- **User Story 3 (P3)**: Can start after Foundational phase. Uses the phase overview table from US1 if available.

### Within Each User Story

- Build structure before validation
- Populate overview before dependency graph validation
- Populate phase details before schema validation
- Populate tracking fields before status validation

### Parallel Opportunities

- No implementation tasks are marked `[P]` because the feature has a single output file, `doc/roadmap.md`, and parallel writes would conflict.
- Setup review tasks T002 through T006 can be performed by separate reviewers before edits begin, but they are intentionally not marked `[P]` because their findings converge into the same roadmap file.
- After implementation is complete, independent manual validation tasks T035 through T042 can be split across reviewers.

---

## Parallel Example: User Story 1

No User Story 1 write tasks should run in parallel because T012 through T018 all modify or validate the same file, `doc/roadmap.md`.

---

## Parallel Example: User Story 2

No User Story 2 write tasks should run in parallel because T019 through T028 all modify or validate phase sections in the same file, `doc/roadmap.md`.

---

## Parallel Example: User Story 3

No User Story 3 write tasks should run in parallel because T029 through T034 all modify or validate status and risk sections in the same file, `doc/roadmap.md`.

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational roadmap framing
3. Complete Phase 3: User Story 1
4. Stop and validate that the roadmap answers "what comes next and why"

### Incremental Delivery

1. Add User Story 1 for full phase ordering and dependency visibility
2. Add User Story 2 for phase-level speckit-ready planning detail
3. Add User Story 3 for progress tracking and risk visibility
4. Finish with cross-cutting validation against success criteria and quickstart usage

### Solo Agent Strategy

Because this is a single-file documentation deliverable, implement tasks sequentially in task ID order. Commit or checkpoint after each completed user story phase.

---

## Notes

- The roadmap is a documentation deliverable only; do not create or modify engine source code for this feature.
- Keep every phase scoped to a single future speckit feature cycle.
- Preserve the clarification decisions from `specs/002-engine-development-roadmap/research.md`.
- Prefer precise, agent-ready prompts over broad wishlist language.
