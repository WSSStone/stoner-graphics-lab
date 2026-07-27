# B06-S13: Forward Frame Planning, Lights, Views, And Sorting Inspection

## Scope

This step inspected Feature 015 forward frame planning, view validation, light
selection, draw validation, and transparent sorting behavior.

Production files inspected:

- `Source/Renderer/Public/Renderer/FForwardFramePlan.h`
- `Source/Renderer/Private/FForwardFramePlan.cpp`
- `Source/Renderer/Public/Renderer/FForwardLightData.h`
- `Source/Renderer/Private/FForwardLightData.cpp`
- `Source/Renderer/Public/Renderer/FForwardViewData.h`
- `Source/Renderer/Private/FForwardViewData.cpp`
- `Source/Renderer/Public/Renderer/FMeshDrawCommand.h`
- `Source/Renderer/Private/FMeshDrawCommand.cpp`
- `Source/Renderer/Public/Renderer/FForwardRenderer.h`
- `Source/Renderer/Private/FForwardRenderer.cpp`

Supporting evidence:

- `Tests/RendererForwardPipelineTests.cpp`
- `specs/015-forward-rendering-pipeline/spec.md`
- `specs/015-forward-rendering-pipeline/contracts/forward-rendering-contract.md`
- `specs/015-forward-rendering-pipeline/data-model.md`
- `specs/015-forward-rendering-pipeline/tasks.md`

## Requirement Focus

- `FR-005`: accept at most one primary directional light and report additional
  primary candidates.
- `FR-006`: configurable point light limit defaults to 4 and selects the front
  N lights after deterministic influence ordering.
- `FR-007`: validate camera/view data, view-projection information, viewport
  dimensions, and camera position.
- `FR-008`: validate light fields and preserve stable light ordering.
- `FR-011`: sort transparent draws by camera-space depth descending, then stable
  material id and object id for ties.
- `FR-017`: preserve deterministic behavior across repeated equivalent frame
  inputs.
- `FR-018`: allow frames with geometry but no accepted lights only with
  constant ambient-only fallback and deterministic diagnostics.

## Observations

- `FForwardViewData::IsValid` rejects empty view names, non-finite view
  matrices, non-finite camera positions, and non-positive viewport extents.
- `FForwardOutputTarget::IsValid` rejects missing color target names,
  non-positive output extents, and output/view extent mismatch.
- `PrepareForwardLightSet` accepts the first valid primary directional light
  and emits `FWD-DIR-MULTIPLE-PRIMARY` for later valid primary candidates.
- Point lights are validated for stable id/name, finite position, finite
  non-negative color/intensity, and positive range; accepted candidates are
  sorted by influence score descending, then light id and name.
- Opaque draw sorting uses material id, mesh id, and object id.
- Transparent draw sorting uses camera-space depth descending, then material id
  and object id.
- Existing tests cover default and configured point light limits, influence
  ordering, invalid light fields, zero-limit ambient fallback with the default
  fallback setting, depth-based transparent sorting, material-id transparent
  ties, incompatible transparent material rejection, no-geometry frames, and a
  20-run dump stability loop that constructs a new renderer per iteration.

## Accepted Findings

`CR001-B06-F004` records a transparent final tie-breaker gap:

- `SortForwardTransparentDraws` compares camera-space depth, material id, and
  object id only.
- If one object submits multiple transparent mesh draw commands with the same
  material at the same depth, the comparator treats those commands as
  equivalent and final ordering can depend on the caller/input arrangement.
- This conflicts with the contract's requirement that final transparent ties do
  not rely on caller submission order and with `FMeshDrawCommand` carrying mesh
  id as part of stable draw identity.
- Existing tests cover material-id ties, but not object-id/final ties or
  same-object multi-mesh transparent draws.

`CR001-B06-F005` records an ambient fallback configuration gap:

- `FForwardRendererConfiguration` exposes `bEnableAmbientFallback`.
- `FForwardRenderer::PrepareFrame` only emits `FWD-AMBIENT-FALLBACK` when that
  flag is true.
- With valid geometry, no accepted lights, and `bEnableAmbientFallback=false`,
  the renderer can return a valid plan with no accepted lights, inactive
  fallback, and no fallback diagnostic.
- This conflicts with Feature 015's clarified no-light behavior, data model,
  and contract text that geometry with no accepted lights remains valid only
  with exactly one ambient-only fallback diagnostic.

## Step Decision

- `CR001-B06-F004`: Accepted S2.
- `CR001-B06-F005`: Accepted S2.
- No production or test source changed in this inspection step.
