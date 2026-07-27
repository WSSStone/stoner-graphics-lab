# B06-S01 Evidence: Render Graph Declaration, Compilation, And Lifetimes Inspection

Step: `B06-S01`.

Inspected eight production files covering render graph public contracts,
compiler state, culling/lifetime/transition planning, and executor resource
resolution.

Key evidence:

- `Source/Renderer/Private/FRenderGraphCompiler.cpp:312-324` replaces the full
  topological order with required-only `Compiled.ScheduledPasses` and records
  culled pass indices.
- `Source/Renderer/Private/FRenderGraphExecutor.cpp:76` iterates all
  `Graph.Resources` before pass execution.
- `Source/Renderer/Private/FRenderGraphExecutor.cpp:123` invokes callbacks only
  for `Graph.GetCompiledGraph().ScheduledPasses`.
- Feature 013 `FR-012`, `FR-014`, and `FR-015` require culling, scheduled graph
  execution, and rejection for missing or invalid required imported resources.
- Existing tests cover required missing import and transient failure, but not
  culled-branch resource isolation.

Finding:

- `CR001-B06-F001`: Render graph execution resolves resources from culled
  branches. Severity S2, Accepted.

No production or test source changed in this inspection step.
