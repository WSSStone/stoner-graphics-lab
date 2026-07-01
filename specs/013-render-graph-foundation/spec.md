# Feature Specification: Render Graph Foundation

**Feature Branch**: `013-render-graph-foundation`  
**Created**: 2026-07-01  
**Status**: Implemented  
**Input**: User description: "根据roadmap制定下一个大feature的spec"

## Clarifications

### Session 2026-07-01

- Q: How should compiled graph execution behave when a pass reports failure? → A: Stop execution immediately on first pass failure and report the failing pass.
- Q: How should graphs with no marked outputs be treated? → A: Require at least one graph output unless the graph contains side-effect-preserving passes.
- Q: How should automatic resource transitions be handled in this foundation phase? → A: Compile an inspectable transition plan and emit planned transitions during graph execution.
- Q: How should virtual graph resources resolve to concrete backing resources in this phase? → A: Resolve transient resources during execution; imported resources are caller-supplied.
- Q: How far should resource aliasing go in this foundation phase? → A: Report aliasing eligibility only; do not reuse backing storage yet.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Declare Render Work as a Graph (Priority: P1)

An engine developer can describe rendering work as named graphics or compute passes, declare each pass's resource reads and writes, and receive a graph that exposes pass dependencies before any work is executed.

**Why this priority**: Every later renderer feature depends on a reliable way to express pass ordering and resource usage. This is the smallest independently useful slice of the render graph.

**Independent Test**: Can be tested by declaring multiple passes with virtual resources, verifying the graph records read/write relationships, and confirming the compiled order respects declared dependencies.

**Acceptance Scenarios**:

1. **Given** a new render graph builder, **When** a developer declares passes with resource reads and writes, **Then** the system records pass names, pass kinds, declared resources, and dependency edges in a queryable graph.
2. **Given** two passes where one writes a resource and the next reads it, **When** the graph is compiled, **Then** the consuming pass is ordered after the producing pass.
3. **Given** a graph with independent passes, **When** the graph is compiled, **Then** the system produces a deterministic valid order without inventing dependencies.

---

### User Story 2 - Manage Virtual Resource Lifetimes (Priority: P1)

An engine developer can create virtual render resources in the graph and rely on compilation to determine which resources are needed, when they become live, and when they can be released or reused.

**Why this priority**: Render pipelines need many temporary textures and buffers. Lifetime tracking prevents later renderer phases from manually managing transient resource ownership pass by pass.

**Independent Test**: Can be tested by creating virtual resources across several passes, compiling the graph, and checking first-use, last-use, allocation, release, and aliasing eligibility summaries.

**Acceptance Scenarios**:

1. **Given** a virtual resource written by one pass and read by later passes, **When** the graph is compiled, **Then** the resource lifetime begins at first writer or imported availability and ends after the last reader or writer.
2. **Given** two transient resources with non-overlapping lifetimes and compatible descriptions, **When** the graph is compiled, **Then** the system reports them as eligible to share backing storage without reusing backing storage during execution.
3. **Given** an imported external resource, **When** the graph is compiled, **Then** the system preserves its external ownership and excludes it from transient aliasing.

---

### User Story 3 - Produce Synchronization and Transition Plans (Priority: P1)

An engine developer can compile a graph and obtain the required resource state transitions between passes so execution can be validated through RHI-level command recording without each pass manually solving synchronization.

**Why this priority**: Automatic transition planning is the core value of a render graph and is required before forward rendering, materials, post-processing, meshlets, ray tracing, or global illumination can compose safely.

**Independent Test**: Can be tested by building graphs with read-after-write, write-after-read, write-after-write, graphics-to-compute, and compute-to-graphics resource usage and verifying the compiled transition plan.

**Acceptance Scenarios**:

1. **Given** a resource changes from writable output in one pass to readable input in another, **When** the graph is compiled, **Then** the system records a transition before the consumer pass.
2. **Given** multiple usages of a resource with the same access requirements, **When** the graph is compiled, **Then** redundant transitions are omitted from the plan.
3. **Given** a pass declares incompatible resource usage, **When** compilation is requested, **Then** the system rejects the graph with a clear validation result.

---

### User Story 4 - Cull Unused Work and Execute the Compiled Graph (Priority: P2)

An engine developer can mark graph outputs, compile the graph, remove passes that cannot affect those outputs, and execute the remaining pass sequence through RHI-facing command recording callbacks.

**Why this priority**: Pass culling and execution make the graph useful beyond static validation. It can follow the P1 graph model after dependency and resource planning are stable.

**Independent Test**: Can be tested by creating a graph with required and unused branches, compiling it with explicit outputs, verifying unused passes are removed, and executing the remaining passes against a mock RHI recorder.

**Acceptance Scenarios**:

1. **Given** a graph containing passes that do not contribute to requested outputs, **When** the graph is compiled, **Then** those passes are excluded from the executable schedule unless explicitly preserved for side effects.
2. **Given** a valid compiled graph, **When** execution is requested, **Then** transient resources are resolved during execution, caller-supplied imported resources are validated, and planned resource transitions are emitted through the RHI-facing command context before affected passes run in compiled order.
3. **Given** execution of one pass reports failure, **When** the failure is observed, **Then** graph execution stops immediately, reports the failed pass, and does not invoke later scheduled passes.

---

### User Story 5 - Inspect Graph Structure for Debugging (Priority: P3)

An engine developer can request a text debug dump of the graph that shows passes, resources, dependencies, culling decisions, lifetimes, aliasing eligibility, and transition plans.

**Why this priority**: Debug visibility is essential once render pipelines become more complex, but it depends on the compiled graph metadata produced by higher-priority stories.

**Independent Test**: Can be tested by compiling representative graphs and comparing the debug dump against expected pass order, resource lifetimes, culling decisions, and transition summaries.

**Acceptance Scenarios**:

1. **Given** a compiled graph, **When** a debug dump is requested, **Then** the output includes pass names, pass kinds, dependencies, declared resources, lifetime ranges, transition summaries, and culling status.
2. **Given** graph compilation fails, **When** a debug dump is requested, **Then** the output includes enough validation context to identify the invalid pass or resource declaration.
3. **Given** the same graph is compiled repeatedly, **When** debug output is requested, **Then** the text is deterministic for test comparison and developer review.

### Edge Cases

- What happens when the graph contains a dependency cycle?
- What happens when a pass reads a transient resource before any producing pass writes it?
- What happens when a pass writes to an imported resource that is declared read-only for the graph?
- What happens when multiple passes write the same resource without an ordering dependency?
- What happens when a resource is declared with incompatible usages across graphics and compute passes?
- What happens when no graph outputs are marked and no side-effect-preserving pass exists?
- What happens when a pass has side effects and would otherwise be culled?
- What happens when transient resources have compatible descriptions but overlapping lifetimes?
- What happens when aliasing is disabled for a resource that would otherwise be eligible?
- What happens when graph compilation succeeds but execution is attempted with missing or invalid RHI resources?
- What happens when execution is requested before compilation?
- What happens when a graph is reset or destroyed after resources or callbacks were registered?

## Architecture & Design Constraints *(mandatory)*

- **RHI Abstraction**: The feature MUST express execution, resource creation, transitions, barriers, and command recording through established RHI-facing contracts and MUST NOT depend on Vulkan, Metal, DX12, OpenGL, or platform-specific graphics APIs.
- **Design Patterns**: Graph building, validation, compilation, resource lifetime planning, aliasing decisions, transition planning, execution, and debug dumping MUST remain separable responsibilities. The feature MUST avoid a single catch-all renderer object with hidden ownership rules.
- **Advanced Graphics**: The graph model MUST leave room for future forward, deferred, post-processing, meshlet, ray tracing, and global illumination workloads, including graphics and compute passes, imported resources, transient resources, and explicit graph outputs.
- **Naming Conventions**: Public project-facing concepts introduced by this feature MUST follow the project's UE5-style naming conventions.
- **Cross-Platform Compatibility**: The feature MUST compile and run on supported desktop platforms and MUST provide deterministic behavior in headless test environments where no graphics runtime is available.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST allow developers to create a render graph builder, add named passes, classify each pass as graphics, compute, copy-like utility, or side-effect-preserving work, and declare the resources each pass reads, writes, creates, imports, or exports.
- **FR-002**: System MUST represent virtual graph resources with a stable handle, resource kind, usage intent, dimensions or element counts, format or layout requirements, ownership mode, aliasing eligibility, and debug name.
- **FR-003**: System MUST validate that every transient resource read has a producer, every imported resource has a declared external availability state, and every exported graph output is produced or imported before use.
- **FR-004**: System MUST detect dependency cycles and reject cyclic graphs with a validation result that identifies at least one involved pass or resource.
- **FR-005**: System MUST compile valid graph declarations into a deterministic pass schedule that respects all declared read/write dependencies and preserves stable ordering for otherwise independent passes.
- **FR-006**: System MUST compute first-use and last-use lifetime information for each virtual resource in the compiled graph.
- **FR-007**: System MUST identify transient resources that are eligible to share backing storage when their lifetimes do not overlap and their resource descriptions are compatible, but MUST NOT reuse backing storage during this phase.
- **FR-008**: System MUST never mark imported resources, exported resources while externally owned, resources with explicit no-alias policy, or incompatible resource descriptions as aliasable with unrelated transient resources.
- **FR-009**: System MUST derive inspectable transition or barrier plans for resource access changes between scheduled passes, including read-after-write, write-after-read, write-after-write, graphics-to-compute, and compute-to-graphics cases.
- **FR-010**: System MUST avoid redundant transitions when consecutive usages already match the required access and layout state.
- **FR-011**: System MUST reject incompatible resource usage declarations, including read-only imported resources written by graph passes, resources used with unsupported access modes, and resource descriptions that cannot satisfy declared usage.
- **FR-012**: System MUST support marking one or more graph outputs and MUST cull passes that cannot affect those outputs unless they are explicitly declared as side-effect-preserving.
- **FR-012a**: System MUST reject executable graph compilation when no graph output is marked and no side-effect-preserving pass exists.
- **FR-013**: System MUST expose compiled graph metadata including scheduled pass order, culled passes, resource lifetimes, aliasing decisions, transition plans, and validation diagnostics.
- **FR-014**: System MUST execute compiled graphs by resolving transient resources through RHI-facing resource creation, validating caller-supplied imported resources, emitting planned resource transitions through RHI-facing command context abstractions, and then invoking pass callbacks in scheduled order through RHI-facing command context abstractions and resource handles only.
- **FR-015**: System MUST reject execution before successful compilation, execution after graph reset or invalidation, execution when transient resource resolution fails, and execution with missing or invalid required imported resources.
- **FR-016**: System MUST stop compiled graph execution immediately when a pass reports failure and MUST report the pass name, pass index, and failure category without invoking later scheduled passes.
- **FR-017**: System MUST allow graph instances to be reset or discarded without leaking virtual resource handles, compiled schedules, transition plans, or pass callback state.
- **FR-018**: System MUST provide deterministic text debug output for both successful and failed graph compilation.
- **FR-019**: System MUST provide test coverage for graph declaration, dependency ordering, cycle rejection, virtual resource lifetime tracking, aliasing eligibility, transition planning, culling, execution ordering, execution failure, debug dump output, reset, and invalidation.
- **FR-020**: System MUST preserve existing Core, RHI, and Vulkan backend test outcomes while adding render graph coverage.
- **FR-021**: System MUST keep material systems, shader permutation management, concrete forward/deferred render passes, scene graph integration, visible swapchain presentation, and triangle demo application flow outside this feature's delivered scope.

### Key Entities *(include if feature involves data)*

- **Render Graph**: Represents a directed acyclic set of declared rendering and compute work, virtual resources, dependencies, compiled schedule, and diagnostics.
- **Render Graph Builder**: Represents the declaration interface used to add passes, resources, dependencies, graph outputs, imported resources, and execution callbacks before compilation.
- **Render Graph Pass**: Represents one named unit of graphics, compute, copy-like, or side-effect-preserving work with declared resource access and execution behavior.
- **Render Graph Resource**: Represents a virtual texture, buffer, or external resource used by passes before it is resolved to concrete backing storage.
- **Resource Access Declaration**: Represents how a pass reads, writes, creates, imports, exports, or preserves a resource.
- **Compiled Graph Schedule**: Represents the deterministic pass order, culled passes, resource lifetimes, aliasing decisions, and transition plan produced by compilation.
- **Resource Lifetime**: Represents the first and last scheduled pass where a resource is needed.
- **Aliasing Decision**: Represents whether two transient resources may share backing storage and the reason they are eligible or rejected.
- **Transition Plan**: Represents the access and layout changes needed between passes for each resource.
- **Graph Debug Dump**: Represents deterministic text output used to inspect graph structure, validation diagnostics, and compiled decisions.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer can declare, compile, inspect, and execute a representative graph containing at least five passes, four virtual resources, one imported resource, one exported output, and both graphics and compute pass kinds in under 60 seconds using the project verification flow.
- **SC-002**: Graph compilation rejects 100% of covered cyclic dependency, read-before-write, invalid imported write, incompatible usage, missing output, and execution-before-compile cases without crashes or partially executable schedules.
- **SC-003**: Compiled schedules are deterministic across at least 20 repeated compilations of the same graph, including pass order, culling decisions, lifetimes, aliasing decisions, transition plans, and debug output.
- **SC-004**: Resource lifetime tracking covers at least one success and one negative path each for transient resources, imported resources, exported resources, side-effect-preserving passes, and reset or invalidation behavior.
- **SC-005**: Aliasing eligibility covers at least one success path for compatible non-overlapping transient resources and negative paths for overlapping lifetimes, incompatible descriptions, imported resources, exported external ownership, and explicit no-alias policy, while execution validation confirms eligible resources still receive separate backing storage in this phase.
- **SC-006**: Transition planning and execution covers at least one success path and one redundant-transition-elision path each for read-after-write, write-after-read, write-after-write, graphics-to-compute, and compute-to-graphics resource usage, with emitted transition records matching the compiled plan.
- **SC-007**: Pass culling removes unused branches from at least one representative graph while preserving explicitly marked side-effect passes and all passes required to produce requested outputs.
- **SC-007a**: Graph validation covers at least one rejected zero-output graph with no side-effect-preserving pass and at least one accepted zero-output graph containing side-effect-preserving work.
- **SC-008**: Execution ordering through a mock RHI command context matches the compiled schedule for 100% of covered valid graphs, transient resource resolution and imported resource validation are covered, and pass failure reports include the failing pass name and index.
- **SC-009**: Existing Core, RHI, and Vulkan backend tests continue to pass after render graph foundation support is added.
- **SC-010**: Renderer-facing code can use the graph without depending on backend-specific runtime details, and headless tests can validate graph behavior without a physical graphics device.

## Assumptions

- The previous RHI resource, descriptor, pipeline, render pass, framebuffer, command buffer, and synchronization contracts are available for graph execution planning and mock tests.
- This feature maps to the roadmap's Phase 012, while the Speckit feature directory uses `013` because existing feature directories already occupy numbers through `012`.
- The first render graph release focuses on deterministic correctness and testability rather than aggressive memory optimization.
- Resource aliasing is represented as eligibility and diagnostics only in this phase; actual backing-storage reuse is deferred.
- Transient graph resources are resolved during graph execution through RHI-facing creation paths, while imported resources are supplied and owned by the caller.
- Pass execution can be validated through mock RHI command contexts and does not require visible presentation, material evaluation, scene data, or a window.
- Text debug visualization is sufficient for this phase; graphical graph visualization is deferred.
- Async compute scheduling, queue overlap, multi-threaded graph compilation, shader/material integration, and concrete forward/deferred pass implementations are deferred to later roadmap phases.

## Implementation Status

Implemented on 2026-07-01 in `013-render-graph-foundation`.

- Added Renderer public contracts for `FRenderGraph`, `FRenderGraphBuilder`, `FCompiledRenderGraph`, graph resources, passes, diagnostics, transition planning, and execution contexts.
- Added Renderer private implementation for deterministic compilation, dependency edges, culling, resource lifetimes, aliasing eligibility diagnostics, transition planning, transient/imported resource execution validation, fail-fast pass execution, reset/invalidation, and deterministic text dumps.
- Added `Tests/RendererRenderGraphTests.cpp` coverage for representative valid graphs, repeated deterministic compilation, read-before-write, cross-graph handles, zero-output validation, imported resources, aliasing, all required transition reasons, culling, execution failure, reset/invalidation, and debug dump stability.
- Verification passed: `conda run -n godot scons`, `Build/Mac/Debug/Tests/StonerTest`, and backend-boundary grep with no Renderer render graph matches.
