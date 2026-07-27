# B06-S16: Forward Execution, Graph Declaration, And Diagnostics Inspection

## Scope

This step inspected Feature 015 forward render graph declaration, diagnostics,
and the later Feature 018 forward frame executor bridge.

Production files inspected:

- `Source/Renderer/Public/Renderer/FForwardRenderGraphDeclaration.h`
- `Source/Renderer/Private/FForwardRenderGraphDeclaration.cpp`
- `Source/Renderer/Public/Renderer/FForwardDiagnostics.h`
- `Source/Renderer/Private/FForwardDiagnostics.cpp`
- `Source/Renderer/Public/Renderer/FForwardFrameExecutor.h`
- `Source/Renderer/Private/FForwardFrameExecutor.cpp`

Supporting evidence:

- `Tests/RendererForwardPipelineTests.cpp`
- `specs/015-forward-rendering-pipeline/spec.md`
- `specs/015-forward-rendering-pipeline/contracts/forward-rendering-contract.md`
- `specs/015-forward-rendering-pipeline/data-model.md`
- `specs/015-forward-rendering-pipeline/tasks.md`
- `specs/018-triangle-demo-integration/contracts/triangle-demo-runtime-contract.md`
- `specs/018-triangle-demo-integration/research.md`
- `specs/019-deferred-rendering-pipeline/research.md`

## Requirement Focus

- `FR-001`: prepare deterministic frame plans and render graph-compatible
  pass/resource declarations.
- `FR-013`: integrate material resource requirements into frame resource
  declarations without backend-specific resource handles.
- `FR-014`: provide deterministic diagnostics for invalid inputs, excessive
  lights, incompatible transparent objects, and fallback behavior.
- `FR-015`: include accepted passes, rejected items, draw counts, light counts,
  resource requirements, and ordering decisions in human-readable dumps.
- Data model: graph declarations expose pass declarations, resource
  declarations, and access declarations; access declarations represent
  read/write intent for each pass/resource relationship.
- Contract: opaque declarations include material resource requirements; access
  declarations match pass/resource relationships.

## Observations

- `FForwardDiagnosticLog` stores structured records, supports deterministic
  stable sorting by code, subject, and message, and formats records without
  pointer/backend handle values.
- `BuildForwardFrameDebugDump` includes frame summary, view/output summary,
  pass order, opaque and transparent draw lists, light counts, background,
  ambient fallback, graph declaration dump, and diagnostics.
- `BuildForwardRenderGraphDeclaration` emits output color/depth resources,
  optional light data, optional environment resource, material resource
  declarations from accepted opaque/transparent draw bindings, pass summaries,
  stage-specific output/depth/light accesses, and final output summaries.
- Existing forward tests assert final output declaration, graph dump presence,
  fallback summary presence, stable debug dumps, and executor deterministic RHI
  recording.
- `FForwardFrameExecutor` lives in Renderer but depends only on RHI interfaces.
  Its one-output/one-triangle command recording is a Feature 018 triangle-demo
  bridge: Feature 018 runtime contract says it records exactly three vertices
  and one instance, and Feature 019 research explicitly describes its
  one-output/one-draw assumptions as deliberately narrow. This inspection does
  not treat that narrow executor as a Feature 015 S2 defect.

## Accepted Finding

`CR001-B06-F006` records a graph declaration access gap:

- Material resource requirements from accepted draws are added to
  `FForwardRenderGraphDeclaration::Resources`.
- The pass loop never emits `FForwardAccessDeclaration` records from opaque or
  transparent passes to those material resources.
- Existing tests only verify the final output declaration and do not assert
  that `AlbedoTexture` or other material resource requirements have matching
  read access edges.

This conflicts with Feature 015's render-graph integration contract because
graph consumers can see material resources without knowing which pass requires
them.

## Step Decision

- `CR001-B06-F006`: Accepted S2.
- No production or test source changed in this inspection step.
