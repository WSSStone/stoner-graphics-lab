# B07-S07 Evidence: Entity Slots And Components Inspection

Step: `B07-S07`.

Evidence commands:

- `wc -l Source/Application/Public/Application/FEntity.h ... Tests/ApplicationSceneEcsTests.cpp specs/017-scene-graph-ecs/*.md`
- `rg -n "struct FEntitySlot|CreateEntity|DestroyEntity|IsEntityLive|Add.*Component|Update.*Component|Replace.*Component|Remove.*Component|Get.*Component|ValidateEntity|Validate.*Component" Source/Application/Private/FWorld.cpp Source/Application/Public/Application/FWorld.h Tests/ApplicationSceneEcsTests.cpp`
- `nl -ba` over the production and test files listed in the batch summary.

Finding opened and accepted:

- `CR001-B07-F005`: Mesh light and camera component mutation bypasses data
  validation.

Key source evidence:

- `Source/Application/Private/FWorld.cpp:150` through
  `Source/Application/Private/FWorld.cpp:201` validate transform component data
  during add and replace.
- `Source/Application/Private/FWorld.cpp:235` through
  `Source/Application/Private/FWorld.cpp:297` define generic mesh/light/camera
  add/replace/remove/get paths without component data validation.
- `Source/Application/Private/FMeshComponent.cpp:6` through
  `Source/Application/Private/FMeshComponent.cpp:9` reject empty mesh ids.
- `Source/Application/Private/FLightComponent.cpp:6` through
  `Source/Application/Private/FLightComponent.cpp:19` reject invalid light data.
- `Source/Application/Private/FCameraComponent.cpp:6` through
  `Source/Application/Private/FCameraComponent.cpp:25` reject invalid camera
  data.
- `Tests/ApplicationSceneEcsTests.cpp:91` through
  `Tests/ApplicationSceneEcsTests.cpp:95` cover invalid transform rejection,
  but mesh/light/camera invalid component mutation coverage is absent.

Non-finding evidence:

- `Source/Application/Private/FWorld.cpp:641` through
  `Source/Application/Private/FWorld.cpp:692` clear subtree component state,
  increment generation, and free destroyed slots.
- `Tests/ApplicationSceneEcsTests.cpp:50` through
  `Tests/ApplicationSceneEcsTests.cpp:60` cover stale handle invalidation and
  slot reuse.

Result:

- B07-S07 completed with one Accepted S2 finding for the next B07 fix step.
