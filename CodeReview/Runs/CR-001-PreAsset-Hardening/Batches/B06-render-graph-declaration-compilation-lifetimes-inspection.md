# B06-S01: Render Graph Declaration, Compilation, And Lifetimes Inspection

## Scope

Batch `B06` starts the Renderer 013-015 review. This step inspected Feature 013
render graph declaration, compilation, lifetime, transition, culling, and
execution-resource resolution boundaries.

Production files inspected:

- `Source/Renderer/Public/Renderer/FRenderGraph.h`
- `Source/Renderer/Public/Renderer/FRenderGraphCompiler.h`
- `Source/Renderer/Public/Renderer/FRenderGraphExecutor.h`
- `Source/Renderer/Public/Renderer/FRenderGraphPass.h`
- `Source/Renderer/Public/Renderer/FRenderGraphResource.h`
- `Source/Renderer/Private/FRenderGraph.cpp`
- `Source/Renderer/Private/FRenderGraphCompiler.cpp`
- `Source/Renderer/Private/FRenderGraphExecutor.cpp`

Supporting evidence:

- `Tests/RendererRenderGraphTests.cpp`
- `specs/013-render-graph-foundation/spec.md`
- `specs/013-render-graph-foundation/contracts/render-graph-contract.md`
- `specs/013-render-graph-foundation/data-model.md`

## Requirement Focus

- `FR-012`: cull passes that cannot affect requested outputs unless preserved
  for side effects.
- `FR-014`: execute compiled graphs by resolving transient resources,
  validating imported resources, emitting planned transitions, and invoking pass
  callbacks in scheduled order.
- `FR-015`: reject execution with missing or invalid required imported
  resources.
- `SC-007` / `SC-008`: culling and execution ordering must match the compiled
  executable schedule.

## Observations

- Declaration APIs keep handles graph-owned through `GraphId` and reject
  cross-graph resource access during pass declaration.
- Compilation builds deterministic dependency edges, sorted topological order,
  output/side-effect reachability, resource lifetimes, aliasing decisions, and
  transition plans without backend-specific contracts.
- Transition planning is inspectable and tests cover representative transition
  reasons and redundant same-state read elision.
- Execution emits the transition plan and invokes only
  `Compiled.ScheduledPasses`, preserving fail-fast pass behavior.
- Existing tests cover required missing import failure and transient resolution
  failure on the representative executable graph.

## Accepted Finding

`CR001-B06-F001` records an execution/culling mismatch:

- `FRenderGraphCompiler` replaces `Compiled.ScheduledPasses` with a required-only
  culled schedule at `Source/Renderer/Private/FRenderGraphCompiler.cpp:312-324`.
- `FRenderGraphExecutor::Execute` still iterates all `Graph.Resources` at
  `Source/Renderer/Private/FRenderGraphExecutor.cpp:76`, including resources
  used only by culled branches.
- As a result, a missing imported binding or simulated transient-resolution
  failure from an unused branch can fail execution even though no scheduled pass
  can access that resource.

This conflicts with the combined meaning of output-based culling and
`FR-015`'s "required imported resources" language. The fix should derive the
required resource set from `Compiled.ScheduledPasses` and validate/resolve only
that set, while preserving diagnostics for truly scheduled imported/transient
resources.

## Existing Coverage Gap

`Tests/RendererRenderGraphTests.cpp` covers:

- required missing imported resource rejection;
- transient resolution failure on a representative executable graph;
- side-effect preservation and unused pass culling.

It does not cover:

- missing imported resource on a culled branch;
- transient resource on a culled branch;
- execution proving culled-branch resources remain unresolved and cannot affect
  pass callbacks.

## Step Decision

- `CR001-B06-F001`: Accepted S2.
- No production code changed in this inspection step.
- Next B06 step should fix `CR001-B06-F001` with focused tests before moving to
  deeper material/forward renderer review.
