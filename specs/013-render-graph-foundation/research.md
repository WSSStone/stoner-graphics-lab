# Research: Render Graph Foundation

## Decision: Renderer-layer render graph remains backend-agnostic

**Rationale**: The graph is a Renderer feature that schedules work through RHI-facing abstractions. Keeping Vulkan/Metal/DX/OpenGL concepts out of the public graph contract preserves the architecture boundary and lets future backends share the same graph behavior.

**Alternatives considered**:

- Backend-specific graph implementation: rejected because it would duplicate scheduling logic and violate the Renderer-to-RHI boundary.
- RHI-layer graph implementation: rejected because the graph owns higher-level render pipeline scheduling policy rather than raw hardware commands.

## Decision: Deterministic topological schedule with stable insertion-order tie breaking

**Rationale**: The spec requires deterministic pass order and debug output across repeated compilations. A stable DAG schedule makes tests reliable while still allowing independent passes to coexist without invented dependencies.

**Alternatives considered**:

- Opportunistic order based on container iteration: rejected because it risks non-deterministic tests.
- User-provided total ordering for every pass: rejected because it weakens dependency-driven graph value.

## Decision: Explicit graph outputs plus side-effect-preserving passes drive culling

**Rationale**: Requiring outputs prevents accidentally compiling an executable graph that does no useful work. Side-effect-preserving passes provide an explicit escape hatch for utility work that intentionally has no exported resource.

**Alternatives considered**:

- Execute zero-output graphs by default: rejected because culling semantics become ambiguous.
- Debug-only zero-output graphs: rejected because side-effect passes are a clearer executable case.

## Decision: Transient resources resolve during execution; imported resources are caller-supplied

**Rationale**: This makes the graph executable in the foundation phase while preserving external ownership for swapchain-like, history, or caller-managed resources. It also gives tests concrete validation points for missing imported resources and transient creation failures.

**Alternatives considered**:

- Plan virtual resources only: rejected because it delays too much executable value.
- Require callers to supply all resources: rejected because it undermines graph-managed transient lifetime.

## Decision: Aliasing is eligibility and diagnostics only in this phase

**Rationale**: Lifetime analysis should be designed now, but actual backing-storage reuse has higher correctness risk and is not required for the first executable render graph. Reporting eligibility keeps future optimization paths visible without overloading this phase.

**Alternatives considered**:

- Reuse compatible non-overlapping transient resources now: rejected because it increases implementation and test risk.
- Make aliasing execution opt-in: rejected because it still requires implementing physical reuse behavior.

## Decision: Compile inspectable transition plans and emit them during execution

**Rationale**: Debuggability and execution behavior should match. The compiled transition plan gives developers a stable artifact to inspect, while execution emits planned transitions through RHI-facing command context behavior.

**Alternatives considered**:

- Plan transitions only: rejected because the graph would not fully satisfy execution requirements.
- Emit transitions without exposing the plan: rejected because it reduces testability and debugging value.

## Decision: Fail fast on pass execution failure

**Rationale**: Stopping immediately keeps resource state and diagnostics deterministic. Later partial execution policies can be added only when real renderer use cases justify them.

**Alternatives considered**:

- Continue independent later passes: rejected because it complicates dependency failure semantics.
- Configurable failure policy: rejected because the foundation phase benefits from one clear behavior.

## Decision: Text debug dump is the only visualization deliverable

**Rationale**: Deterministic text output is enough for tests, command-line workflows, and early engine debugging. Graphical visualization is useful later but not needed for the first render graph foundation.

**Alternatives considered**:

- Graphical visualization: rejected as out of scope.
- No debug output: rejected because the roadmap explicitly calls for visualization/debug dump.
