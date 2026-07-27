# B07-S10 Evidence: Hierarchy, Reparenting, And Destruction Inspection

Step: `B07-S10`.

Files inspected:

- `Source/Application/Public/Application/FWorld.h`
- `Source/Application/Private/FWorld.cpp`
- `Source/Application/Public/Application/FTransformComponent.h`
- `Source/Core/Public/Core/FTransform.h`
- `Tests/ApplicationSceneEcsTests.cpp`
- `specs/017-scene-graph-ecs/spec.md`
- `specs/017-scene-graph-ecs/contracts/scene-graph-ecs-contract.md`

Requirement evidence:

- `specs/017-scene-graph-ecs/spec.md` FR-006, FR-007, and FR-009 require
  hierarchy-derived world transforms, explicit reparent preservation modes, and
  deterministic topological operation order.
- `specs/017-scene-graph-ecs/contracts/scene-graph-ecs-contract.md` states
  that hierarchy operation order is public behavior and must not depend on
  internal storage layout.

Finding evidence:

- `Source/Application/Private/FWorld.cpp:589` checks the immediate parent slot.
- `Source/Application/Private/FWorld.cpp:590` treats a parent without transform
  as a terminal case.
- `Source/Application/Private/FWorld.cpp:592` returns the child local transform
  directly and does not continue through the transformless parent's ancestors.
- `Source/Application/Private/FWorld.cpp:347` to
  `Source/Application/Private/FWorld.cpp:361` use the same parent-world
  availability assumption when computing reparent preservation.
- `Tests/ApplicationSceneEcsTests.cpp:140` covers direct transformed
  parent-child composition, but the current tests do not cover
  `Root(transform) -> Group(no transform) -> Child(transform)`.

Finding recorded:

- `CR001-B07-F006`: Transform propagation stops at transformless intermediate
  parents.
- Severity: S2.
- Disposition: Accepted.

Next step:

- Fix `CR001-B07-F006` in the next B07 fix step by making transformless
  hierarchy nodes behave as identity local transforms while still inheriting
  ancestor world transforms, then add regression tests for transform propagation
  and preserve-world reparenting through a transformless group.
