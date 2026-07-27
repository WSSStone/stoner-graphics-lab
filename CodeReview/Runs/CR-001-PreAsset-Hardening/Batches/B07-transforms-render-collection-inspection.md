# B07-S13: Transforms And Render Collection Inspection

## Scope

Inspected Feature 017 world-transform query behavior, render collection,
accepted/rejected scene summary data, stable sorting, diagnostics, and existing
tests.

Production files read:

- `Source/Application/Private/FRenderSystem.cpp`
- `Source/Application/Public/Application/FSceneRenderSummary.h`
- `Source/Application/Private/FSceneRenderSummary.cpp`
- `Source/Application/Public/Application/FEntity.h`
- `Source/Application/Private/FEntity.cpp`
- `Source/Application/Public/Application/FMeshComponent.h`
- `Source/Application/Private/FMeshComponent.cpp`
- `Source/Application/Public/Application/FLightComponent.h`
- `Source/Application/Private/FLightComponent.cpp`
- `Source/Application/Public/Application/FCameraComponent.h`
- `Source/Application/Private/FCameraComponent.cpp`
- `Source/Application/Private/FWorld.cpp`

Supporting files read:

- `Tests/ApplicationSceneEcsTests.cpp`
- `specs/017-scene-graph-ecs/spec.md`
- `specs/017-scene-graph-ecs/plan.md`
- `specs/017-scene-graph-ecs/data-model.md`
- `specs/017-scene-graph-ecs/contracts/scene-graph-ecs-contract.md`

## Requirements Checked

- Feature 017 FR-010: mesh scene data identifies renderable objects without
  live graphics resource ownership.
- Feature 017 FR-011: light scene data includes type, color, intensity, and
  meaningful range.
- Feature 017 FR-012: camera scene data includes projection intent, near/far,
  and active-camera identification.
- Feature 017 FR-013: render collection produces deterministic per-category
  summaries, ordered by optional sort key and entity identity tie-breaker.
- Feature 017 FR-014: incomplete renderables are rejected or omitted without
  blocking valid items.
- Feature 017 SC-003: representative 10 mesh, 4 light, 2 camera collection is
  stable across repeated runs.

## Findings

No new findings.

## Non-Findings

- `FRenderSystem::Collect` collects from `FWorld::GetLiveEntities` and does not
  expose Renderer, RHI, backend, graphics API, asset, or live GPU resource
  objects.
- Mesh summaries carry entity identity, world transform, mesh id, material id,
  and optional sort key.
- Light summaries carry entity identity, light type, color, intensity, range,
  world position, world direction, and optional sort key.
- Camera summaries carry entity identity, world transform, camera projection
  data, active-camera flag, and optional sort key.
- Missing transform state is rejected with `SCENE-RENDER-MISSING-TRANSFORM`
  while valid items continue to be collected.
- Accepted categories sort by optional sort key first, then by entity identity
  using slot index and generation as the final tie-breaker.
- Rejected items and diagnostics are sorted deterministically before dump
  generation.
- Existing tests cover stable accepted/rejected counts, optional sort-key
  priority, entity identity tie-breaks, missing-transform diagnostics, and
  byte-stable debug dumps across 20 repeated collections.
