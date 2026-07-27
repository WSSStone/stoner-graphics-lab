# B08-S07: Deferred Frame Plan, Surfaces, And Graph Inspection

## Scope

Inspected Feature 019 deferred surface layout, frame planning, render-graph
declaration, and deterministic planning/graph test coverage. This step stayed
inside Renderer planning/graph responsibilities and did not inspect native
readback, failure injection, CI orchestration, or comparison timing.

Production files read:

- `Source/Renderer/Public/Renderer/FDeferredSurfaceData.h`
- `Source/Renderer/Private/FDeferredSurfaceData.cpp`
- `Source/Renderer/Public/Renderer/FDeferredFramePlan.h`
- `Source/Renderer/Private/FDeferredFramePlan.cpp`
- `Source/Renderer/Public/Renderer/FDeferredRenderer.h`
- `Source/Renderer/Private/FDeferredRenderer.cpp`
- `Source/Renderer/Public/Renderer/FDeferredRenderGraphDeclaration.h`
- `Source/Renderer/Private/FDeferredRenderGraphDeclaration.cpp`
- `Source/Renderer/Public/Renderer/FDeferredFrameExecutor.h`
- `Source/Renderer/Private/FDeferredFrameExecutor.cpp`

Supporting files read:

- `Tests/DeferredRenderingTests.cpp`
- `specs/019-deferred-rendering-pipeline/spec.md`
- `specs/019-deferred-rendering-pipeline/data-model.md`
- `specs/019-deferred-rendering-pipeline/contracts/deferred-renderer-contract.md`

## Requirements Checked

- Feature 019 FR-003: surface-data stage records base color, normalized
  world-space normal, metallic, roughness, emissive, ambient occlusion, and
  depth.
- Feature 019 FR-004: surface layout defines semantics, formats, clear values,
  compatibility, and standard/reversed-Z depth policy.
- Feature 019 FR-012: composition produces exactly one final renderer output.
- Feature 019 FR-014: deferred stages are represented as render graph passes
  with explicit resource reads/writes and ordering.
- Feature 019 FR-015: intermediate resources declare compatible extents,
  semantics, access states, and lifetimes.
- Feature 019 FR-016 and SC-004: equivalent frame inputs produce deterministic
  pass/resource/access order and debug reports.
- Deferred renderer contract: surface geometry visibility/depth work happens
  once, composition follows lighting, optional forward transparency follows
  composition, and no stage reads before a prior write or import.
- Data model: lighting reads surface semantics and writes accumulation;
  composition reads accumulation plus required surface data and writes final
  output.

## Finding

### `CR001-B08-F004`

`FDeferredRenderer::PrepareFrame` declares the `SurfaceData` pass as writing
`LightingAccumulation` along with the three surface color targets and depth.
The executor's surface stage binds the surface render pass/framebuffer and does
not bind the lighting accumulation target. Lighting accumulation is instead the
target of the directional/point/spot lighting stages and is later read by
composition.

Impact: the deferred render graph and debug report can claim that the surface
pass writes a lighting target it does not own. That corrupts lifetime/access
diagnostics and can hide producer/consumer mistakes, especially for no-light
frames where composition still reads `LightingAccumulation`.

Status: Accepted, S2.

## Non-Findings

- `MakeDefaultDeferredSurfaceLayout` declares exactly three color attachments
  plus one `D32_Float` depth attachment, with required semantics represented
  exactly once.
- Standard-Z and reversed-Z policies derive far clear and compare operation
  consistently.
- `TryBuildWorldNormalFromModel` rejects non-finite, non-affine, and singular
  transforms and builds an inverse-transpose normal matrix for valid transforms.
- Invalid view/output/surface-layout preparation returns before producing a
  valid frame plan, and invalid plans return an invalid graph declaration.
- Canonical pass order is surface, directional, point, spot, composition,
  optional forward transparency, optional validation readback.
- Transparent draw candidates are excluded from surface data and handed to the
  forward-transparent path after composition when enabled.
- Graph declaration rejects pass reads or writes of undeclared resources and
  maps exactly one final output resource.
- Existing tests cover surface format/depth policy, canonical pass order, graph
  validity/resource count/final output, empty frames, transparent handoff, and
  20-run deterministic debug dumps. They do not check the exact SurfaceData
  write set, which is why `CR001-B08-F004` escaped.
