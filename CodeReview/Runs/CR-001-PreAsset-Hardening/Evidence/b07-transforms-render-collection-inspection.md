# B07-S13 Evidence: Transforms And Render Collection Inspection

Step: `B07-S13`.

Files inspected:

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
- `Tests/ApplicationSceneEcsTests.cpp`
- `specs/017-scene-graph-ecs/spec.md`
- `specs/017-scene-graph-ecs/plan.md`
- `specs/017-scene-graph-ecs/data-model.md`
- `specs/017-scene-graph-ecs/contracts/scene-graph-ecs-contract.md`

Requirement evidence:

- `specs/017-scene-graph-ecs/spec.md:137` to
  `specs/017-scene-graph-ecs/spec.md:141` define mesh, light, camera, render
  collection ordering, and incomplete item rejection requirements.
- `specs/017-scene-graph-ecs/spec.md:163` defines the representative render
  collection stability success criterion.
- `specs/017-scene-graph-ecs/plan.md:106` records optional sort key ordering
  with entity identity as final tie-breaker.
- `specs/017-scene-graph-ecs/data-model.md:160` to
  `specs/017-scene-graph-ecs/data-model.md:168` define accepted category,
  rejected item, diagnostic, and ordering model fields.

Implementation evidence:

- `Source/Application/Private/FRenderSystem.cpp` rejects missing transforms
  before accepting mesh, light, or camera records.
- `Source/Application/Private/FRenderSystem.cpp` carries mesh id/material id,
  light type/color/intensity/range/placement/direction, and camera projection
  data into the summary.
- `Source/Application/Private/FSceneRenderSummary.cpp` sorts accepted categories
  by optional sort key and final entity identity tie-breaker.
- `Source/Application/Public/Application/FEntity.h` compares entity identity by
  slot index, then generation.

Test evidence:

- `Tests/ApplicationSceneEcsTests.cpp` covers representative 10 renderables, 4
  lights, and 2 cameras.
- `Tests/ApplicationSceneEcsTests.cpp` covers optional sort key priority and
  entity identity tie-breaks.
- `Tests/ApplicationSceneEcsTests.cpp` covers missing-transform rejection,
  diagnostic counting, and byte-stable debug dumps across 20 repeated
  collections.

Finding result:

- No new finding was recorded for B07-S13.
